#if !defined(__FLV_COMPILE_H__)
#define __FLV_COMPILE_H__

#include "nc_types.h"
#include "nc_string.h"
#include "nc_thread.h"

#include "fli.h"

#include "flv_compiler.h"
#include "flv_lower.h"
#include "flv_visualisation.h"

struct Arena;

// @types______________________________________________________________________
struct FLVUnit {
    Arena*       MemPool;
    u64          Hash;
    Str8         Key;
    Str8         Path;
    FLVLowered   Lowered;
    Str8*        LayoutNames;
    FLVSlot*     Slots;
    FLVSlotName* SlotNames;
    u32          SlotsCount;
    u32          ImportCount;
    Str8*        ImportPaths;
    u128*        ImportHashes;
    b32          HasError;
    u32          __Padding;
};

struct FLVClosure {
    u64   Key;
    Str8  Path;
    Str8* ImportPaths;
    u32   ImportCount;
    u32   __Padding;
};

struct FLVClosureNode {
    FLVClosureNode* Next;
    FLVClosureNode* Prev;
    FLVClosure      V;
};

struct FLVClosureSlot {
    FLVClosureNode* Head;
    FLVClosureNode* Tail;
};

struct FLVClosureTable {
    Arena*          MemPool;
    FLVClosureSlot* Slots;
    u32             SlotsCount;
};

// @runtime____________________________________________________________________
extern FLVClosureTable FLV_CLOSURES;

// @functions__________________________________________________________________
Str8 FLVFlightSetKey(Arena* MemPool, FLIKey const* Keys, u64 Count);
FLVUnit* FLVUnitFromPath(Access* Acc, Str8 Path, FLIKey const* Keys, u64 KeysCount, u64 EndTimeUSecs);
FLVRenderItem* FLVResultFromUnit(Access* Acc, FLVUnit const* Unit, FLIKey const* Keys, u64 KeysCount, FLIKey Flight, u32 Program, u64 EndTimeUSecs);
Str8 FLVImportSource(Arena* MemPool, Str8 Path, OUT u128* Hash);

#endif // __FLV_COMPILE_H__
