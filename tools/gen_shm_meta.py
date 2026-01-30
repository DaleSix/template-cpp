#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description="Generate shm_migrate metadata from structs")
    parser.add_argument("--input", required=True, help="Input header containing structs")
    parser.add_argument("--output", required=True, help="Output generated header")
    parser.add_argument("--clang", default="clang", help="Clang binary path")
    parser.add_argument("--clang-args", nargs="*", default=[], help="Extra args passed to clang")
    return parser.parse_args()


def run_clang_ast(input_path, clang, clang_args):
    cmd = [
        clang,
        "-x",
        "c++",
        "-fsyntax-only",
        "-Xclang",
        "-ast-dump=json",
    ]
    cmd.extend(clang_args)
    cmd.append(input_path)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        sys.exit(result.returncode)
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        sys.stderr.write("failed to parse clang ast json\n")
        sys.stderr.write(str(exc) + "\n")
        sys.exit(1)


def make_guard(output_path):
    name = output_path.name.upper()
    guard = []
    for ch in name:
        if ch.isalnum():
            guard.append(ch)
        else:
            guard.append("_")
    return "".join(guard)


def collect_structs(node, scope, structs, warnings):
    kind = node.get("kind")
    if kind == "NamespaceDecl":
        name = node.get("name")
        if name:
            scope.append(name)
        for child in node.get("inner", []) or []:
            collect_structs(child, scope, structs, warnings)
        if name:
            scope.pop()
        return

    if kind in ("RecordDecl", "CXXRecordDecl"):
        tag_used = node.get("tagUsed")
        is_struct = tag_used == "struct" or node.get("isStruct") is True
        if is_struct and node.get("isCompleteDefinition"):
            name = node.get("name")
            if name:
                fields = []
                for child in node.get("inner", []) or []:
                    if child.get("kind") != "FieldDecl":
                        continue
                    if child.get("isBitfield"):
                        warnings.append(f"skip bitfield {name}::{child.get('name')}")
                        continue
                    field_name = child.get("name")
                    if field_name:
                        fields.append(field_name)
                full_name = "::".join(scope + [name]) if scope else name
                if full_name not in structs:
                    structs[full_name] = fields
        name = node.get("name")
        if name:
            scope.append(name)
        for child in node.get("inner", []) or []:
            collect_structs(child, scope, structs, warnings)
        if name:
            scope.pop()
        return

    for child in node.get("inner", []) or []:
        collect_structs(child, scope, structs, warnings)


def write_output(output_path, input_path, structs, warnings):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    include_rel = os.path.relpath(str(input_path), str(output_path.parent))
    include_rel = Path(include_rel).as_posix()
    guard = make_guard(output_path) + "_"

    field_names = []
    for fields in structs.values():
        field_names.extend(fields)
    unique_fields = sorted(set(field_names))

    lines = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("// 自动生成文件，请勿手工修改")
    lines.append('#include "shm_migrate.h"')
    lines.append(f'#include "{include_rel}"')
    lines.append("")
    lines.append("namespace shm_migrate {")
    lines.append("")

    for name in unique_fields:
        lines.append(f"SHM_DEFINE_FIELD({name});")

    for struct_name in sorted(structs.keys()):
        fields = structs[struct_name]
        lines.append("")
        lines.append("template <>")
        lines.append(f"struct StructMeta<{struct_name}> {{")
        lines.append("    using Fields = TypeList<")
        for i, field in enumerate(fields):
            suffix = "," if i < len(fields) - 1 else ""
            lines.append(f"        SHM_FIELD({struct_name}, {field}){suffix}")
        lines.append("    >;")
        lines.append("};")

    lines.append("")
    lines.append("}  // namespace shm_migrate")
    lines.append("")
    lines.append(f"#endif")
    lines.append("")

    output_path.write_text("\n".join(lines), encoding="utf-8")

    for warn in warnings:
        sys.stderr.write(warn + "\n")


def main():
    args = parse_args()
    input_path = Path(args.input).resolve()
    output_path = Path(args.output).resolve()
    clang_args = list(args.clang_args)
    clang_args.insert(0, f"-I{input_path.parent}")

    ast = run_clang_ast(str(input_path), args.clang, clang_args)
    structs = {}
    warnings = []
    collect_structs(ast, [], structs, warnings)

    if not structs:
        sys.stderr.write("no structs found in input header\n")
        sys.exit(1)

    write_output(output_path, input_path, structs, warnings)


if __name__ == "__main__":
    main()
