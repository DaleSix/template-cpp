#include "shm_migrate.h"

#include <iostream>

struct OldB {
    int id;
    int count;
};

struct NewB {
    int id;
    int status;
    int count;
};

struct OldA {
    int flag;
    OldB items[2];
    int values[3];
};

struct NewA {
    int flag;
    NewB items[3];
    int values[2];
    double price;
};

namespace shm_migrate {

SHM_DEFINE_FIELD(id);
SHM_DEFINE_FIELD(count);
SHM_DEFINE_FIELD(status);
SHM_DEFINE_FIELD(flag);
SHM_DEFINE_FIELD(items);
SHM_DEFINE_FIELD(values);
SHM_DEFINE_FIELD(price);

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

template <>
struct StructMeta<OldA> {
    using Fields = TypeList<
        SHM_FIELD(OldA, flag),
        SHM_FIELD(OldA, items),
        SHM_FIELD(OldA, values)
    >;
};

template <>
struct StructMeta<NewA> {
    using Fields = TypeList<
        SHM_FIELD(NewA, flag),
        SHM_FIELD(NewA, items),
        SHM_FIELD(NewA, values),
        SHM_FIELD(NewA, price)
    >;
};

}  // namespace shm_migrate

int main() {
    OldA oldA{};
    oldA.flag = 1;
    oldA.items[0] = OldB{10, 20};
    oldA.items[1] = OldB{11, 21};
    oldA.values[0] = 7;
    oldA.values[1] = 8;
    oldA.values[2] = 9;

    NewA newA{};
    shm_migrate::migrate(oldA, newA);

    std::cout << "flag=" << newA.flag << "\n";
    for (std::size_t i = 0; i < 3; ++i) {
        std::cout << "items[" << i << "].id=" << newA.items[i].id
                  << ", status=" << newA.items[i].status
                  << ", count=" << newA.items[i].count << "\n";
    }
    for (std::size_t i = 0; i < 2; ++i) {
        std::cout << "values[" << i << "]=" << newA.values[i] << "\n";
    }
    std::cout << "price=" << newA.price << "\n";
    return 0;
}
