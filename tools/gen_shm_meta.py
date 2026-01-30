#!/usr/bin/env python3
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description="Generate shm_migrate metadata from structs")
    parser.add_argument("--input", required=True, help="Input header containing structs")
    parser.add_argument("--output", required=True, help="Output generated header")
    parser.add_argument("--clang", default="clang", help="Clang binary path")
    parser.add_argument("--clang-args", nargs=argparse.REMAINDER, default=[], help="Extra args passed to clang")
    return parser.parse_args()


def run_clang_ast_json(input_path, clang, clang_args):
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
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if result.returncode != 0:
        return None, result.stderr
    try:
        return json.loads(result.stdout), None
    except json.JSONDecodeError as exc:
        return None, f"failed to parse clang ast json: {exc}"


def run_clang_ast_text(input_path, clang, clang_args):
    cmd = [
        clang,
        "-x",
        "c++",
        "-fsyntax-only",
        "-Xclang",
        "-ast-dump",
    ]
    cmd.extend(clang_args)
    cmd.append(input_path)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        sys.exit(result.returncode)
    return result.stdout


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
        is_complete = node.get("isCompleteDefinition") is True or node.get("completeDefinition") is True
        if is_struct and is_complete:
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


def line_depth(line):
    depth = 0
    for ch in line:
        if ch.isalpha():
            break
        if ch == "|":
            depth += 1
    return depth


def collect_structs_text(text, structs, warnings):
    namespace_stack = []
    record_stack = []
    ns_re = re.compile(r"\bNamespaceDecl\b.*\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)")
    ns_tail_re = re.compile(r"\bNamespaceDecl\b.*\b([A-Za-z_][A-Za-z0-9_]*)\b\s*$")
    record_re = re.compile(r"\b(CXXRecordDecl|RecordDecl)\b.*\bstruct\b\s+([A-Za-z_][A-Za-z0-9_]*)\b")
    field_re = re.compile(r"\bFieldDecl\b[^']*?\b([A-Za-z_][A-Za-z0-9_]*)\b\s*'")

    for raw_line in text.splitlines():
        line = raw_line.rstrip("\n")
        if not line:
            continue
        depth = line_depth(line)
        while namespace_stack and depth <= namespace_stack[-1][0]:
            namespace_stack.pop()
        while record_stack and depth <= record_stack[-1][0]:
            record_stack.pop()

        ns_match = ns_re.search(line)
        if not ns_match:
            ns_match = ns_tail_re.search(line)
        if ns_match:
            namespace_stack.append((depth, ns_match.group(1)))
            continue

        record_match = record_re.search(line)
        if record_match and "implicit" not in line:
            struct_name = record_match.group(2)
            if namespace_stack:
                full_name = "::".join([name for _, name in namespace_stack] + [struct_name])
            else:
                full_name = struct_name
            if full_name not in structs:
                structs[full_name] = []
            record_stack.append((depth, full_name))
            continue

        field_match = field_re.search(line)
        if field_match:
            if "bitfield" in line:
                warnings.append(f"skip bitfield {field_match.group(1)}")
                continue
            if record_stack:
                owner = record_stack[-1][1]
                if field_match.group(1) not in structs[owner]:
                    structs[owner].append(field_match.group(1))
            else:
                warnings.append(f"skip field without owner {field_match.group(1)}")


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

    structs = {}
    warnings = []
    ast, json_err = run_clang_ast_json(str(input_path), args.clang, clang_args)
    if ast is not None:
        collect_structs(ast, [], structs, warnings)
    else:
        if json_err and "unknown argument: '-ast-dump=json'" not in json_err:
            sys.stderr.write(json_err + "\n")
            sys.exit(1)
        text = run_clang_ast_text(str(input_path), args.clang, clang_args)
        collect_structs_text(text, structs, warnings)

    if not structs:
        sys.stderr.write("no structs found in input header\n")
        sys.exit(1)

    write_output(output_path, input_path, structs, warnings)


if __name__ == "__main__":
    main()
