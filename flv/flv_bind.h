#if !defined(__FLV_BIND_H__)
#define __FLV_BIND_H__

#include "nc_types.h"
#include "fli.h"
#include "flv_schema.h"

struct Arena;

// @defines____________________________________________________________________
#define FLV_COLUMN_NONE U32_MAX

// @types______________________________________________________________________
struct FLVBoundSlot {
    u32           Column;
    u32           TimeColumn;
    u32           InstanceColumn;
    u32           MessageType;
    u32           Instance;
    u32           ModeNumber;
    FLIScalarKind Kind;
    u8            IsMissing;
    u8            __Padding[2];
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
FLVBoundSlot* FLVBindSlots(Arena* MemPool, FLIParsed* FLI, FLVSlot const* Flots, u32 SlotsCount);

#endif // __FLV_BIND_H__
