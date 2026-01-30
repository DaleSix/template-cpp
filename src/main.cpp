#include "shm_migrate.h"
#include "shm_structs.h"
#include "shm_meta_generated.h"

#include <iostream>

int main() {
    // 模拟从共享内存读取旧结构体并迁移到新结构体
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
