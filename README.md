# Shared-memory struct migration (C++17)

This repo contains a header-only helper to migrate data between an old struct
layout and a new struct layout without clearing shared memory. It generates
copy logic at compile time based on a small metadata list per struct.

Key features:
- Copies same-named fields
- Defaults added fields (value-initialize)
- Skips deleted fields
- Supports nested structs and arrays (including arrays of structs)

## Build

```
make
./shm_migrate_demo
```

## Usage sketch

1. Define field tags once (same tag for both versions).
2. Provide `StructMeta<T>` with a `Fields` typelist.
3. Call `shm_migrate::migrate(oldObj, newObj)`.

See `src/main.cpp` for a complete example, including nested arrays of structs.
