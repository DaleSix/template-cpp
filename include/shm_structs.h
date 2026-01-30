#ifndef SHM_STRUCTS_H
#define SHM_STRUCTS_H

// 旧版本结构体
struct OldB {
    int id;
    int count;
};

// 新版本结构体，新增字段 status
struct NewB {
    int id;
    int status;
    int count;
};

// 旧版本结构体，包含结构体数组和基础数组
struct OldA {
    int flag;
    OldB items[2];
    int values[3];
};

// 新版本结构体：数组长度变化，新增字段 price
struct NewA {
    int flag;
    NewB items[3];
    int values[2];
    double price;
};

#endif
