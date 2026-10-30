#if !defined(__FLV_RUN_H__)
#define __FLV_RUN_H__

#include "nc_types.h"
#include "nc_string.h"
#include "fli.h"
#include "flv_lower.h"
#include "flv_bind.h"

struct Arena;

// @defines____________________________________________________________________
#define FLV_CHUNK_SAMPLES    4096
#define FLV_CHUNK_MASK_WORDS (FLV_CHUNK_SAMPLES / 64)

#define FLV_I64_HOLE I64_MAX

#define FLV_LOAD_LOOP(SrcType, DstType)                              \
    do {                                                             \
        SrcType const* __Src = (SrcType const*) Column->Data + Base; \
        DstType* __Dst = (DstType*) Result;                          \
                                                                     \
        for (u64 Index = 0; Index < Count; ++Index)                  \
            __Dst[Index] = (DstType) __Src[Index];                   \
    } while (FALSE)

#define FLV_ARITH_HEAD(Type)                                                \
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) { \
        FLVTempSetMissing(Lane, Op.Dst);                                    \
                                                                            \
        return;                                                             \
    }                                                                       \
                                                                            \
    Type* Dst = (Type*) FLVTempPtr(Lane, Op.Dst);                           \
    u64   Count = Lane->ChunkCount

#define FLV_OP_ADD(A, B)     ((A) + (B))
#define FLV_OP_SUB(A, B)     ((A) - (B))
#define FLV_OP_MUL(A, B)     ((A) * (B))
#define FLV_OP_DIV_I64(A, B) ((B) ? (A) / (B) : FLV_I64_HOLE)
#define FLV_OP_DIV_F64(A, B) ((A) / (B))
#define FLV_OP_MOD_I64(A, B) ((B) ? (A) % (B) : FLV_I64_HOLE)
#define FLV_OP_MOD_F64(A, B) FMod((A), (B))

#define FLV_EVENT_ID_ARMED    10
#define FLV_EVENT_ID_DISARMED 11

typedef u8 FLVRenderKind;
enum : u8 {
    FLV_RENDER_KIND_NULL,
    FLV_RENDER_KIND_CURVE,
    FLV_RENDER_KIND_TABLE,
    FLV_RENDER_KIND_TEXT,
    FLV_RENDER_KIND_FLAG,
    FLV_RENDER_KIND_COUNT
};

// @types______________________________________________________________________
struct FLVTimeRange {
    u64 StartUSecs;
    u64 EndUSecs;
};

struct FLVSelection {
    FLVTimeRange* V;
    u64           Count;
};

struct FLVIndexRange {
    u64 Head;
    u64 Tail;
};

struct FLVColumn {
    void*         Data;
    u64           Count;
    FLIScalarKind Kind;
    u8            ElementSize;
    u8            __Padding[6];
};

struct FLVSelectionBuild {
    FLVTimeRange* V;
    u64           Count;
    u64           Capacity;
    u64           OpenStartUSecs;
    u64           OpenEndUSecs;
    b32           IsOpen;
    u32           __Padding;
};

struct FLVRenderCurve {
    u64* TimeUSecs;
    f64* Values;
    u64  Count;
    u32  LabelStringIndex;
    u8   Axis;
    u8   __Padding[3];
};

struct FLVRenderFlag {
    FLVTimeRange* Ranges;
    u64           Count;
    u64           TotalDurationUSecs;
    u32           LabelStringIndex;
    u32           __Padding;
};

struct FLVRenderItem {
    FLVRenderKind Kind;
    u8            IsMissing;
    u16           __Padding;
    u32           AnalysisIndex;
    u32           FlightIndex;
    u32           EmitIndex;

    union {
        FLVRenderCurve Curve;
        FLVRenderFlag  Flag;
        Str8           Text;
    };
};

union FLVNumber {
    f64 F64;
    i64 I64;
};

struct FLVReduction {
    FLVNumber Summation;
    FLVNumber Minimum;
    FLVNumber Maximum;
    FLVNumber Head;
    FLVNumber Tail;
    f64       SummationSquared;
    f64       Shift;
    u64       Count;
};

struct FLVWindow {
    u64*       Times;
    FLVNumber* Values;
    u64        Capacity;
    u64        Head;
    u64        Count;
    FLVNumber  Summation;
    FLVNumber  Prev;
    u64        PrevTimeUSecs;
    b32        HasPrev;
    u32        __Padding;
};

struct FLVLane {
    FLVProgram const*   Program;
    FLVBoundSlot const* Slots;
    FLIParsed*          FLI;
    u32                 SlotsCount;
    u32                 FlightIndex;
    u64                 FlightEndUSecs;
    Arena*              MemPool;
    Arena*              ResultMemPool;
    FLVColumn*          Columns;
    FLVColumn*          InstanceColumns;
    void**              Temps;
    FLVSelectionBuild*  SelectionBuilds;
    FLVReduction*       Reductions;
    FLVWindow*          Windows;
    u64                 MissingTemps;
    FLVRenderItem*      Items;
    FLVRegion const*    Region;
    FLVColumn const*    BaseTime;
    u64 const*          Mask;
    FLVIndexRange*      Ranges;
    u64                 RangesCount;
    u64                 RegionSampleCount;
    u64                 ChunkBase;
    u64                 ChunkCount;
};

#define FLV_KERNEL_FUNC_SIG(X) void X(FLVLane* Lane, FLVOp Op)
typedef FLV_KERNEL_FUNC_SIG(FLVKernelFuncType);

// @runtime____________________________________________________________________
extern FLVKernelFuncType* FLV_KERNEL_TABLE[FLV_KERNEL_COUNT];

// @functions__________________________________________________________________
void FLVKernelInit(void);
void FLVLaneInit(FLVLane* Lane, Arena* MemPool, Arena* ResultMemPool, FLVProgram const* Program, FLIParsed* FLI, FLVBoundSlot const* Slots, u32 SlotsCount, u32 FlightIndex);
void FLVLaneRun(FLVLane* Lane);
Str8List FLVRenderItemsPrint(Arena* MemPool, FLVLane const* Lane);

INTERNAL void*
FLVTempPtr(FLVLane* Lane, FLVOperand Operand)
{
    ASSERT(FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_TEMP);

    return Lane->Temps[FLVGetOperandIndex(Operand)];
}

INTERNAL b32
FLVTempIsMissing(FLVLane const* Lane, FLVOperand Operand)
{
    b32 Result = FALSE;

    if (FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_TEMP)
        Result = (Lane->MissingTemps >> FLVGetOperandIndex(Operand)) & 1;

    return Result;
}

INTERNAL void
FLVTempSetMissing(FLVLane* Lane, FLVOperand Operand)
{
    if (FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_TEMP)
        Lane->MissingTemps |= 1ULL << FLVGetOperandIndex(Operand);
}

INTERNAL b32
FLVMaskBit(u64 const* Mask, u64 Index)
{
    return (Mask[Index >> 6] >> (Index & 63)) & 1;
}

#endif // __FLV_RUN_H__
