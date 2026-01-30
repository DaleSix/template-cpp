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

## 编译

```
make
./shm_migrate_demo
```

## 使用说明

### 1. 引入头文件

```
#include "shm_migrate.h"
```

### 2. 定义字段 tag

字段 tag 用于新旧结构体字段对齐，需保证同名字段使用同一个 tag。

```
namespace shm_migrate {
SHM_DEFINE_FIELD(id);
SHM_DEFINE_FIELD(count);
SHM_DEFINE_FIELD(status);
}
```

### 3. 为旧结构体和新结构体提供元信息

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

namespace shm_migrate {
template <>
struct StructMeta<OldB> {
    using Fields = TypeList<
        SHM_FIELD(OldB, id),
        SHM_FIELD(OldB, count)
    >;
};

template <>
struct StructMeta<NewB> {
    using Fields = TypeList<
        SHM_FIELD(NewB, id),
        SHM_FIELD(NewB, status),
        SHM_FIELD(NewB, count)
    >;
};
}
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
