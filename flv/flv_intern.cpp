#include "nc_defines.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_profile.h"
#include "nc_simd.h"
#include "flv_intern.h"

INTERNAL u64
LoadBytes(u8 const* Bytes, u64 Count)
{
    u64 Result = 0;

    if (Count >= 8) {
        MemCpy(&Result, Bytes, 8);
    } else {
        for (u64 Index = 0; Index < Count; ++Index)
            Result |= (u64) Bytes[Index] << (Index << 3ULL);
    }

    return Result;
}

INTERNAL u32
Hash(Str8 String, OUT u64* Prefix)
{
    u8 const* Bytes = String.Str;
    u64 Remaining = String.Size;
    u64 Chunk = LoadBytes(Bytes, Remaining);
    u64 StringHash = (Chunk ^ String.Size) * NAME_HASH_MUL;

    *Prefix = Chunk;

    if (Remaining > 8)
        Remaining -= 8;
    else
        Remaining = 0;

    Bytes += 8;

    while (Remaining) {
        Chunk = LoadBytes(Bytes, Remaining);
        StringHash = ((StringHash ^ (StringHash >> 29)) ^ Chunk) * NAME_HASH_MUL;

        if (Remaining > 8)
            Remaining -= 8;
        else
            Remaining = 0;

        Bytes += 8;
    }

    StringHash ^= StringHash >> 32;

    u32 Result = (u64) StringHash;

    return Result;
}

NamePool*
NamePoolInit(void)
{
    Arena* MemPool = ArenaAlloc();
    NamePool* Result = ArenaPushArrayZero(MemPool, NamePool, 1);

    Result->MemPool = MemPool;
    Result->NamesMemPool = ArenaAllocEx(GB(2));
    Result->StringMemPool = ArenaAllocEx(GB(2));

    for (u32 Index = 0; Index < ARRAY_COUNT(Result->HashMemPool); ++Index)
        Result->HashMemPool[Index] = ArenaAllocEx(GB(1));

    Result->Names = (Name*) ArenaExBase(Result->NamesMemPool);
    Result->StringData = ArenaExBase(Result->StringMemPool);
    Result->HashTableMask = (NAME_POOL_HASH_SIZE - 1);
    Result->HashTable = ArenaPushArray(
        Result->HashMemPool[0],
        NameSlot,
        NAME_POOL_HASH_SIZE
    );
    MemSet(Result->HashTable, 0xFF, NAME_POOL_HASH_SIZE * sizeof(NameSlot));

    return Result;
}

INTERNAL void
NamePoolExpand(NamePool* Pool)
{
    u32 OldSize = Pool->HashTableMask + 1;
    u32 NewSize = OldSize << 1;
    NameSlot* OldTable = Pool->HashTable;
    u32 NewPoolIndex = Pool->HashMemPoolIndex ^ 1;
    Arena* NewPool = Pool->HashMemPool[NewPoolIndex];

    ArenaClear(NewPool);
    Pool->HashTable = ArenaPushArray(
        NewPool,
        NameSlot,
        NewSize
    );
    Pool->HashTableMask = NewSize - 1;
    Pool->HashMemPoolIndex = NewPoolIndex;
    MemSet(Pool->HashTable, 0xFF, NewSize * sizeof(NameSlot));

    for (u32 Index = 0; Index < OldSize; ++Index) {
        NameSlot Slot = OldTable[Index];

        if (Slot.Index.V == NAME_INDEX_INVALID_VALUE)
            continue;

        u32 SlotIndex = Slot.Hash & Pool->HashTableMask;

        while (Pool->HashTable[SlotIndex].Index.V != NAME_INDEX_INVALID_VALUE) {
            SlotIndex = (SlotIndex + 1) & Pool->HashTableMask;
        }

        Pool->HashTable[SlotIndex] = Slot;
    }
}

NameIndex 
InternName(NamePool* Pool, Str8 String)
{
    NameIndex Result = {
        NAME_INDEX_INVALID_VALUE
    };
    u64 Prefix = 5381;
    u32 StringHash = Hash(String, &Prefix);
    u32 SlotIndex = StringHash & Pool->HashTableMask;

    NC_CPU_PREFETCH(Pool->HashTable);

    for (;;) {
        NameSlot Entry = Pool->HashTable[SlotIndex];

        if (Entry.Index.V == NAME_INDEX_INVALID_VALUE) {
            if (Pool->Count > ((Pool->HashTableMask + 1) * 7) / 10) {
                NamePoolExpand(Pool);
                SlotIndex = StringHash & Pool->HashTableMask;
                continue;
            }

            Name* NewName = ArenaPushArray(Pool->NamesMemPool, Name, 1);
            u8* Bytes = ArenaPushArrayEx(
                Pool->StringMemPool, 
                u8, 
                String.Size, 
                1
            );

            Result.V = Pool->Count++;
            NewName->Prefix = Prefix;
            NewName->Length = SafeTruncateU64(String.Size);
            NewName->Offset = Pool->StringDataCount;
            MemCpy(Bytes, String.Str, String.Size);
            Pool->StringDataCount += SafeTruncateU64(String.Size);
            Pool->HashTable[SlotIndex].Index = Result;
            Pool->HashTable[SlotIndex].Hash = StringHash;
            break;
        }

        if (Entry.Hash == StringHash) {
            Name* Candidate = &Pool->Names[Entry.Index.V];

            if (
                Candidate->Length == String.Size && 
                Candidate->Prefix == Prefix
            ) {
                if (
                    String.Size <= 8 ||
                    MemCmp(
                        Pool->StringData + Candidate->Offset + 8,
                        String.Str + 8,
                        String.Size - 8
                    )
                ) {
                    Result = Entry.Index;
                    break;
                }
            }
        }

        SlotIndex = (SlotIndex + 1) & Pool->HashTableMask;
    }

    return Result;
}

Str8 
GetName(NamePool* Pool, NameIndex Index)
{
    Name* N = &Pool->Names[Index.V];

    return Str(Pool->StringData + N->Offset, (u64) N->Length);
}
