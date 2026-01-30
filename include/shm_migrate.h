#ifndef SHM_MIGRATE_H
#define SHM_MIGRATE_H

#include <cstddef>
#include <type_traits>

namespace shm_migrate {

template <typename... Ts>
struct TypeList {};

template <typename Struct, typename Name, typename Type, Type Struct::*Member>
struct Field {
    using struct_type = Struct;
    using name = Name;
    using type = Type;
    static constexpr Type Struct::* ptr = Member;
};

template <typename T>
struct StructMeta;

template <typename T, typename = void>
struct has_meta : std::false_type {};

template <typename T>
struct has_meta<T, std::void_t<typename StructMeta<T>::Fields>> : std::true_type {};

template <typename T>
struct is_reflectable : has_meta<T> {};

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

template <typename T>
void set_default(T& value);

template <typename OldT, typename NewT>
void copy_value(const OldT& oldVal, NewT& newVal);

template <typename OldT, std::size_t N, typename NewT, std::size_t M>
void copy_array(const OldT (&oldArr)[N], NewT (&newArr)[M]);

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

template <typename Old, typename New>
void migrate(const Old& oldObj, New& newObj) {
    static_assert(is_reflectable<Old>::value, "Old type missing StructMeta");
    static_assert(is_reflectable<New>::value, "New type missing StructMeta");
    using OldFields = typename StructMeta<Old>::Fields;
    using NewFields = typename StructMeta<New>::Fields;
    CopyFields<Old, New, OldFields, NewFields>::apply(oldObj, newObj);
}

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

#define SHM_DEFINE_FIELD(name) \
    struct name##_tag {}

#define SHM_FIELD(struct_type, member) \
    ::shm_migrate::Field<struct_type, member##_tag, decltype(struct_type::member), &struct_type::member>

#endif
