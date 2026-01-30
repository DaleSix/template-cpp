# 共享内存结构体迁移工具 (C++17)

该仓库提供一个 header-only 的结构体迁移工具，用于在结构体版本升级后，
在不清理共享内存的前提下，把旧布局的数据迁移到新布局。工具通过模板元编程
在编译期生成字段拷贝逻辑，避免手写繁琐的版本迁移代码。

## 核心能力

- 同名字段自动拷贝
- 新增字段自动填默认值 (value-initialize)
- 删除字段自动跳过
- 支持嵌套结构体
- 支持数组以及结构体数组
- 结构体数组长度变化按最小长度拷贝，多余元素默认值
- 当数组元素类型一致且可平凡拷贝时使用 memcpy

## 编译

```
make
./shm_migrate_demo
```

## 使用说明

### 1. 准备结构体定义

把新旧结构体放到 `include/shm_structs.h`，例如：

```
struct OldB {
    int id;
    int count;
};

struct NewB {
    int id;
    int status;
    int count;
};
```

### 2. 自动生成字段元信息

构建时会自动运行生成脚本，输出 `include/shm_meta_generated.h`。
生成脚本依赖 `clang` 和 `python3`。
建议使用 clang 9 及以上以启用 JSON AST。
如果只装得到 clang 7，脚本会自动退化为解析文本 AST。

手动运行示例：

```
python3 tools/gen_shm_meta.py \
  --input include/shm_structs.h \
  --output include/shm_meta_generated.h \
  --clang clang \
  --clang-args -std=c++17 -Iinclude
```

### 3. 在代码中使用

```
#include "shm_migrate.h"
#include "shm_structs.h"
#include "shm_meta_generated.h"
```

### 4. 执行迁移

```
OldB oldObj{10, 20};
NewB newObj{};
shm_migrate::migrate(oldObj, newObj);
```

### 5. 嵌套结构体与数组

支持结构体内含结构体数组、数组长度变化等场景，工具会递归迁移并填充默认值。
完整示例见 `src/main.cpp`。
