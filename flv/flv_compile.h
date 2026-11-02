#if !defined(__FLV_COMPILE_H__)
#define __FLV_COMPILE_H__

#include "nc_types.h"
#include "nc_string.h"
#include "nc_thread.h"

#include "fli.h"

#include "flv_compiler.h"
#include "flv_lower.h"
#include "flv_visualisation.h"
#include "flv_run.h"

struct Arena;

// @types______________________________________________________________________
struct FLVUnit {
    Arena*       MemPool;
    u64          Hash;
    Str8         Path;
    FLVLowered   Lowered;
    Str8*        LayoutNames;
    FLVSlot*     Slots;
    FLVSlotName* SlotNames;
    u32          SlotsCount;
    u32          ImportCount;
    Str8*        ImportPaths;
    b32          HasError;
    u32          __Padding;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
Str8 FLVFlightSetKey(Arena* MemPool, FLIKey const* Keys, u64 Count);
FLVUnit* FLVUnitFromPath(Access* Acc, Str8 Path, FLIKey const* Keys, u64 KeysCount, u64 EndTimeUSecs);
FLVRenderItem* FLVResultFromUnit(Access* Acc, FLVUnit const* Unit, FLIKey const* Keys, u64 KeysCount, FLIKey Flight, u32 Program, u64 EndTimeUSecs);

#endif // __FLV_COMPILE_H__
