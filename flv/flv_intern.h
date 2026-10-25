#if !defined(__FLV_INTERN_H__)
#define __FLV_INTERN_H__

#include "nc_types.h"
#include "nc_string.h"
#include "flv_base_types.h"

struct Arena;

#define NAME_POOL_HASH_SIZE      4096
#define NAME_INDEX_INVALID_VALUE U32_MAX
#define NAME_HASH_MUL            0x9E3779B97F4A7C15ULL

struct Name {
    u64 Prefix;
    u32 Offset;
    u32 Length;
};

struct NameSlot {
    NameIndex Index;
    u32       Hash;
};

struct NamePool {
    Arena*     HashMemPool[2];
    Arena*     MemPool;
    Arena*     NamesMemPool;
    Arena*     StringMemPool;
    Name*      Names;
    NameSlot*  HashTable;
    u8*        StringData;
    u32        Count;
    u32        HashTableMask;
    u32        StringDataCount;
    u32        HashMemPoolIndex;
};

NamePool* NamePoolInit(void);
NameIndex InternName(NamePool* Pool, Str8 String);
Str8 GetName(NamePool* Pool, NameIndex Index);

#endif // __FLV_INTERN_H__
