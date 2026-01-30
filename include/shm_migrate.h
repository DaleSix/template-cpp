#ifndef SHM_MIGRATE_H
#define SHM_MIGRATE_H

#include <cstddef>
#include <type_traits>

namespace shm_migrate {

// 简单的类型列表，用于描述结构体字段序列
template <typename... Ts>
struct TypeList {};

// 字段描述：结构体类型、字段名标记、字段类型、成员指针
template <typename Struct, typename Name, typename Type, Type Struct::*Member>
struct Field {
    using struct_type = Struct;
    using name = Name;
    using type = Type;
    static constexpr Type Struct::* ptr = Member;
};

template <typename T>
struct StructMeta;

// 是否提供了结构体元信息
template <typename T, typename = void>
struct has_meta : std::false_type {};

template <typename T>
struct has_meta<T, std::void_t<typename StructMeta<T>::Fields>> : std::true_type {};

template <typename T>
struct is_reflectable : has_meta<T> {};

// 在字段列表中按名字查找
template <typename Name, typename Fields>
struct FindField;

template <typename Name>
struct FindField<Name, TypeList<>> {
    using type = void;
};

template <typename Name, typename Head, typename... Tail>
struct FindField<Name, TypeList<Head, Tail...>> {
    using type = std::conditional_t<std::is_same<typename Head::name, Name>::value,
                                    Head,
                                    typename FindField<Name, TypeList<Tail...>>::type>;
};

// 默认值填充
template <typename T>
void set_default(T& value);

// 迁移单个值，支持嵌套结构体和数组
template <typename OldT, typename NewT>
void copy_value(const OldT& oldVal, NewT& newVal);

// 迁移数组，按最小长度拷贝，剩余新元素填默认
template <typename OldT, std::size_t N, typename NewT, std::size_t M>
void copy_array(const OldT (&oldArr)[N], NewT (&newArr)[M]);

// 拷贝字段：如果旧字段存在则拷贝，否则填默认
template <typename Old, typename New, typename OldField, typename NewField, bool HasOld>
struct CopyFieldImpl;

template <typename Old, typename New, typename OldField, typename NewField>
struct CopyFieldImpl<Old, New, OldField, NewField, true> {
    static void apply(const Old& oldObj, New& newObj) {
        copy_value(oldObj.*(OldField::ptr), newObj.*(NewField::ptr));
    }
};

template <typename Old, typename New, typename OldField, typename NewField>
struct CopyFieldImpl<Old, New, OldField, NewField, false> {
    static void apply(const Old&, New& newObj) {
        set_default(newObj.*(NewField::ptr));
    }
};

// 遍历新结构体字段列表，按名字匹配旧字段
template <typename Old, typename New, typename OldFields, typename NewFields>
struct CopyFields;

template <typename Old, typename New, typename OldFields>
struct CopyFields<Old, New, OldFields, TypeList<>> {
    static void apply(const Old&, New&) {}
};

template <typename Old, typename New, typename OldFields, typename NewHead, typename... NewTail>
struct CopyFields<Old, New, OldFields, TypeList<NewHead, NewTail...>> {
    static void apply(const Old& oldObj, New& newObj) {
        using OldField = typename FindField<typename NewHead::name, OldFields>::type;
        constexpr bool kHasOld = !std::is_void<OldField>::value;
        CopyFieldImpl<Old, New, OldField, NewHead, kHasOld>::apply(oldObj, newObj);
        CopyFields<Old, New, OldFields, TypeList<NewTail...>>::apply(oldObj, newObj);
    }
};

// 入口函数：从旧对象迁移到新对象
template <typename Old, typename New>
void migrate(const Old& oldObj, New& newObj) {
    static_assert(is_reflectable<Old>::value, "Old type missing StructMeta");
    static_assert(is_reflectable<New>::value, "New type missing StructMeta");
    using OldFields = typename StructMeta<Old>::Fields;
    using NewFields = typename StructMeta<New>::Fields;
    CopyFields<Old, New, OldFields, NewFields>::apply(oldObj, newObj);
}

// 默认值实现：数组递归填充，其它类型 value-initialize
template <typename T>
void set_default(T& value) {
    if constexpr (std::is_array<T>::value) {
        for (std::size_t i = 0; i < std::extent<T>::value; ++i) {
            set_default(value[i]);
        }
    } else {
        value = T{};
    }
}

// 数组迁移
template <typename OldT, std::size_t N, typename NewT, std::size_t M>
void copy_array(const OldT (&oldArr)[N], NewT (&newArr)[M]) {
    constexpr std::size_t kMin = (N < M) ? N : M;
    for (std::size_t i = 0; i < kMin; ++i) {
        copy_value(oldArr[i], newArr[i]);
    }
    for (std::size_t i = kMin; i < M; ++i) {
        set_default(newArr[i]);
    }
}

// 值迁移：数组 -> 数组，结构体 -> 结构体，其它可赋值类型直接拷贝
template <typename OldT, typename NewT>
void copy_value(const OldT& oldVal, NewT& newVal) {
    if constexpr (std::is_array<OldT>::value && std::is_array<NewT>::value) {
        copy_array(oldVal, newVal);
    } else if constexpr (is_reflectable<OldT>::value && is_reflectable<NewT>::value) {
        migrate(oldVal, newVal);
    } else if constexpr (std::is_assignable<NewT&, OldT>::value) {
        newVal = oldVal;
    } else {
        set_default(newVal);
    }
}

}  // namespace shm_migrate

// 定义字段名 tag，用于新旧结构体字段对齐
#define SHM_DEFINE_FIELD(name) \
    struct name##_tag {}

// 生成字段描述，需与 SHM_DEFINE_FIELD 的 tag 名一致
#define SHM_FIELD(struct_type, member) \
    ::shm_migrate::Field<struct_type, member##_tag, decltype(struct_type::member), &struct_type::member>

#endif
