#if !defined(__FLV_LOWER_H__)
#define __FLV_LOWER_H__

#include "nc_types.h"
#include "nc_string.h"
#include "flv_base_types.h"
#include "flv_schema.h"
#include "flv_kernel.h"

struct Arena;
struct AST;

// @defines____________________________________________________________________
#define FLV_TEMP_MAX             64
#define FLV_PROGRAM_OP_MAX     4096
#define FLV_PROGRAM_REGION_MAX  512
#define FLV_PROGRAM_CONST_MAX   512
#define FLV_PROGRAM_EMIT_MAX    256
#define FLV_PROGRAM_STRING_MAX  256
#define FLV_REGION_OP_MAX       256
#define FLV_FILTER_DEPTH_MAX     32
#define FLV_LOWER_VAR_MAX       128

#define LOWER_KIND_MESSAGE 0xF0
#define LOWER_KIND_MODE    0xF1
#define LOWER_KIND_MISSING 0xF3

#define FLV_LAYOUT_MAX  1024
#define FLV_PROGRAM_MAX 256

typedef u8 FLVConstKind;
enum : u8 {
    FLV_CONST_KIND_I64,
    FLV_CONST_KIND_F64,
    FLV_CONST_KIND_TIME
};

typedef u8 FLVRegionKind;
enum : u8 {
    FLV_REGION_KIND_RUN,
    FLV_REGION_KIND_SCALAR,
    FLV_REGION_KIND_JUMP_IF_FALSE,
    FLV_REGION_KIND_JUMP
};

#define LOWER_ERROR(S, Header, ...)                                                   \
    do {                                                                              \
        FLVLogErrorAt(LocFromFiles((S)->Tree->Files, (Header)->Tok), __VA_ARGS__);    \
        (S)->HasError = TRUE;                                                         \
    } while (FALSE)

// @types______________________________________________________________________
struct FLVConst {
    union {
        i64      I64;
        f64      F64;
        u64      TimeUSecs;
    };

    FLVConstKind Kind;
    u8           __Padding[7];
};

struct FLVEmit {
    u32 LabelStringIndex;
    u8  DisplayKind;
    u8  Axis;
    u8  ElementKind;
    u8  __Padding;
};

struct FLVRegion {
    u32        HeadOp;
    u32        OpCount;
    u32        BaseSlot;
    FLVOperand RangeSelection;
    FLVOperand Mask;
    FLVOperand Operand;
    u16        Target;
    u8         Kind;
    u8         __Padding[3];
};

struct FLVProgram {
    FLVOp*     Ops;
    FLVRegion* Regions;
    FLVConst*  Consts;
    FLVEmit*   Emits;
    Str8*      Strings;
    u8*        TempKind;
    u32*       TempHash;
    u32        OpCount;
    u32        RegionCount;
    u32        ConstCount;
    u32        EmitCount;
    u32        StringCount;
    u32        TempCount;
    u32        AnalysisIndex;
};

struct FLVLayoutNode {
    u32       Parent;
    u32       HeadChild;
    u32       ChildCount;
    u32       Depth;
    NameIndex Name;
    u32       Ordinal;
    u32       AnalysisIndex;
};

struct FLVLowered {
    FLVProgram*    Program;
    FLVLayoutNode* Layout;
    u32            ProgramCount;
    u32            LayoutCount;
    b32            HasError;
};

struct LowerValue {
    FLVOperand Op;
    u8         Kind;
    u8         __Padding;
    TypeIndex  Type;
};

struct LowerRegion {
    LowerRegion* Prev;
    FLVOp        Ops[FLV_REGION_OP_MAX];
    u32          OpCount;
    u32          BaseSlot;
    TypeIndex    Base;
    FLVOperand   RangeSelection;
    FLVOperand   Mask;
};

struct LowerVar {
    BindIndex  Bind;
    FLVOperand Op;
    u8         Kind;
    u8         __Padding[3];
};

struct LowerDecl {
    BindIndex Bind;
    u32       Depth;
};

struct LowerState {
    AST*             Tree;
    FLVSchema const* Schema;
    FLVSlot const*   Slots;
    u32              SlotCount;
    Arena*           MemPool;
    Arena*           Scratch;
    FLVOp*           Ops;
    FLVRegion*       Regions;
    FLVConst*        Consts;
    FLVEmit*         Emits;
    Str8*            Strings;
    u32              OpCount;
    u32              RegionCount;
    u32              ConstCount;
    u32              EmitCount;
    u32              StringCount;
    u8               TempKind[FLV_TEMP_MAX];
    u32              TempHash[FLV_TEMP_MAX];
    u32              TempCount;
    LowerVar         Vars[FLV_LOWER_VAR_MAX];
    u32              VarCount;
    LowerDecl        Decls[FLV_LOWER_VAR_MAX];
    u32              DeclCount;
    ExprIndex        Filters[FLV_FILTER_DEPTH_MAX];
    FLVOperand       CtxSel[FLV_FILTER_DEPTH_MAX + 1];
    u32              FilterDepth;
    u32              CtxDepth;
    u32              Missing;
    LowerRegion*     Region;
    b32              InAnalysis;
    b32              HasError;
    FLVProgram*      Programs;
    FLVLayoutNode*   Layout;
    u32*             ChildCursor;
    u32              ProgramCount;
    u32              LayoutCount;
};

// @runtime____________________________________________________________________
extern FLVKernelInfo const FLV_KERNEL_INFO_TABLE[FLV_KERNEL_COUNT];
extern Str8 const FLV_KERNEL_NAME_TABLE[FLV_KERNEL_COUNT];

// @functions__________________________________________________________________
FLVLowered FLVLower(Arena* MemPool, AST* Tree, FLVSchema const* Schema, FLVSlot const* Slots, u32 SlotsCount);
Str8List FLVProgramPrint(Arena* MemPool, FLVProgram const* Program, FLVSlot const* Slots);

#endif // __FLV_LOWER_H__
