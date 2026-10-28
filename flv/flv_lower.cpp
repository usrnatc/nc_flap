#include "nc_defines.h"
#include "nc_types.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "flv_lower.h"
#include "flv_kernel.h"
#include "flv_ast.h"
#include "flv_type.h"
#include "flv_token.h"
#include "flv_symbol_table.h"
#include "flv_parser.h"
#include "flv_type_check.h"
#include "flv_schema.h"
#include "flv_stdlib.h"
#include "flv_intern.h"
#include "flv_type_check.h"

#define X(Name, Inputs, Dst, Flags) { Inputs, FLV_ELEMENT_KIND_##Dst, Flags, 0 },

FLVKernelInfo const FLV_KERNEL_INFO_TABLE[FLV_KERNEL_COUNT] = {
    FLV_KERNEL_LIST(X)
};

#undef X

#define X(Name, Inputs, Dst, Flags) Str8Lit(#Name),

Str8 const FLV_KERNEL_NAME_TABLE[FLV_KERNEL_COUNT] = {
    FLV_KERNEL_LIST(X)
};

#undef X

INTERNAL ASTNodeExprAccel*
ExprAt(LowerState* State, ExprIndex Index)
{
    return ExprAt(State->Tree, Index);
}

INTERNAL ExprIndex
ImplicitChild(LowerState* State, void* Node, u32 NodeSize)
{
    return ImplicitChild(State->Tree, Node, NodeSize);
}

INTERNAL TypeIndex
TypeOf(LowerState* State, ExprIndex Index)
{
    return TypeOf(State->Tree, Index);
}


INTERNAL FLVOperand
TempAlloc(LowerState* State, u8 Kind)
{
    ASSERT_ALWAYS(State->TempCount < FLV_TEMP_MAX);

    u32 Index = State->TempCount++;
    FLVOperand Result = FLV_OPERAND_NONE;

    State->TempKind[Index] = Kind;
    State->TempHash[Index] = 0;
    Result = FLVOperandAlloc(FLV_OPERAND_KIND_TEMP, Index);

    return Result;
}

INTERNAL u32
Hash(LowerState* State, FLVOperand Op)
{
    u32 Result = (u32) Op * 2654435761U;

    switch (FLVGetOperandKind(Op)) {
        default: {} break;

        case FLV_OPERAND_KIND_TEMP: {
            Result = State->TempHash[FLVGetOperandIndex(Op)];
        } break;

        case FLV_OPERAND_KIND_SLOT: {
            FLVSlot const* Slot = &State->Slots[FLVGetOperandIndex(Op)];

            Result = (
                (Slot->Message.V * 2654435761U) ^ 
                (Slot->Field.V * 0x9E3779B1U) ^ 
                Slot->Instance
            );
        } break;

        case FLV_OPERAND_KIND_CONST: {
            FLVConst const* Const = &State->Consts[FLVGetOperandIndex(Op)];

            Result = (u32) (Const->TimeUSecs ^ (Const->TimeUSecs >> 32)) ^ Const->Kind;
        } break;
    }

    return Result;
}

INTERNAL FLVOperand
ConstAdd(LowerState* State, FLVConst Const)
{
    FLVOperand Result = FLV_OPERAND_NONE;

    for (u32 Index = 0; Index < State->ConstCount; ++Index) {
        if (
            State->Consts[Index].Kind == Const.Kind && 
            State->Consts[Index].TimeUSecs == Const.TimeUSecs
        ) {
            Result = FLVOperandAlloc(FLV_OPERAND_KIND_CONST, Index);
            break;
        }
    }

    if (Result == FLV_OPERAND_NONE) {
        ASSERT_ALWAYS(State->ConstCount < FLV_PROGRAM_CONST_MAX);

        State->Consts[State->ConstCount] = Const;
        Result = FLVOperandAlloc(FLV_OPERAND_KIND_CONST, State->ConstCount++);
    }

    return Result;
}

INTERNAL FLVOperand
ConstI64(LowerState* State, i64 Value)
{
    FLVConst Const = {};
    FLVOperand Result = FLV_OPERAND_NONE;

    Const.I64 = Value;
    Const.Kind = FLV_CONST_KIND_I64;
    Result = ConstAdd(State, Const);

    return Result;
}

INTERNAL FLVOperand
ConstF64(LowerState* State, f64 Value)
{
    FLVConst Const = {};
    FLVOperand Result = FLV_OPERAND_NONE;

    Const.F64 = Value;
    Const.Kind = FLV_CONST_KIND_F64;
    Result = ConstAdd(State, Const);

    return Result;
}

INTERNAL FLVOperand
ConstTime(LowerState* State, u64 TimeUSecs)
{
    FLVConst Const = {};
    FLVOperand Result = FLV_OPERAND_NONE;

    Const.TimeUSecs = TimeUSecs;
    Const.Kind = FLV_CONST_KIND_TIME;
    Result = ConstAdd(State, Const);

    return Result;
}

INTERNAL u32
StringAdd(LowerState* State, Str8 String)
{
    ASSERT_ALWAYS(State->StringCount < FLV_PROGRAM_STRING_MAX);

    State->Strings[State->StringCount] = ArenaPushStrCpy(
        State->MemPool, 
        String
    );

    return State->StringCount++;
}

INTERNAL u32
EmitAdd(
    LowerState* State, 
    u8 DisplayKind, 
    u8 Axis, 
    u8 ElementKind, 
    Str8 Label
) {
    ASSERT_ALWAYS(State->EmitCount < FLV_PROGRAM_EMIT_MAX);

    FLVEmit* E = &State->Emits[State->EmitCount];

    E->LabelStringIndex = StringAdd(State, Label);
    E->DisplayKind = DisplayKind;
    E->Axis = Axis;
    E->ElementKind = ElementKind;
    E->__Padding = 0;

    return State->EmitCount++;
}

INTERNAL Str8
ArgLabel(LowerState* State, ASTDisplayArg const* Arg)
{
    ParsedFile const* File = &State->Tree->Files[
        Arg->HeadTok.V >> TOK_FILE_SHIFT
    ];
    Token const* Head = &File->Tokens.Tokens[Arg->HeadTok.V & TOK_INDEX_MASK];
    Token const* Tail = &File->Tokens.Tokens[Arg->TailTok.V & TOK_INDEX_MASK];
    u32 Start = Head->Offset;
    u32 End = Tail->Offset + Tail->Count;
    Str8 Result = Str(File->Source.Str + Start, End - Start);

    return Result;
}

INTERNAL FLVRegion*
RegionPush(LowerState* State, u8 Kind)
{
    ASSERT_ALWAYS(State->RegionCount < FLV_PROGRAM_REGION_MAX);

    FLVRegion* Result = &State->Regions[State->RegionCount++];

    Result->HeadOp = State->OpCount;
    Result->OpCount = 0;
    Result->BaseSlot = U32_MAX;
    Result->RangeSelection = FLV_OPERAND_NONE;
    Result->Mask = FLV_OPERAND_NONE;
    Result->Operand = FLV_OPERAND_NONE;
    Result->Target = 0;
    Result->Kind = Kind;

    return Result;
}

INTERNAL FLVOperand
EmitOp(LowerState* State, u16 Kernel, FLVOperand LHS, FLVOperand RHS)
{
    FLVKernelInfo const* Info = &FLV_KERNEL_INFO_TABLE[Kernel];
    FLVOp Op = {};
    FLVOperand Result = FLV_OPERAND_NONE;

    Op.Kernel = Kernel;
    Op.Dst = (Info->DstKind != FLV_ELEMENT_KIND_NONE) 
        ? TempAlloc(State, Info->DstKind) 
        : FLV_OPERAND_NONE;
    Op.LHS = LHS;
    Op.RHS = RHS;

    if (Info->Flags & FLV_KERNEL_FLAG_SCALAR) {
        FLVRegion* Region = State->RegionCount 
            ? &State->Regions[State->RegionCount - 1] 
            : NULL;

        if (
            !Region || 
            Region->Kind != FLV_REGION_KIND_SCALAR || 
            Region->HeadOp + Region->OpCount != State->OpCount
        ) {
            Region = RegionPush(State, FLV_REGION_KIND_SCALAR);
        }

        ASSERT_ALWAYS(State->OpCount < FLV_PROGRAM_OP_MAX);

        State->Ops[State->OpCount++] = Op;
        ++Region->OpCount;
    } else {
        ASSERT_ALWAYS(State->Region);
        ASSERT_ALWAYS(State->Region->OpCount < FLV_REGION_OP_MAX);

        State->Region->Ops[State->Region->OpCount++] = Op;
    }

    if (Op.Dst != FLV_OPERAND_NONE) {
        u32 H = (u32) Kernel * 0x9E3779B1U;

        H ^= Hash(State, LHS) * 2654435761U;
        H ^= Hash(State, RHS) * 0x85EBCA6BU;
        State->TempHash[FLVGetOperandIndex(Op.Dst)] = H;
    }

    Result = Op.Dst;

    return Result;
}

INTERNAL u32
SlotForBase(LowerState* State, TypeIndex Base)
{
    NameIndex Name = {
        TypeAt(Base)->A
    };
    u32 Instance = MessageInstance(Base);
    u32 Fallback = U32_MAX;

    for (u32 Index = 0; Index < State->SlotCount; ++Index) {
        FLVSlot const* Slot = &State->Slots[Index];

        if (
            Slot->Kind == FLV_SLOT_KIND_MODE || 
            Slot->Message.V != Name.V
        ) {
            continue;
        }

        if (Slot->Instance == Instance)
            return Index;

        Fallback = Index;
    }

    ASSERT_ALWAYS(Fallback != U32_MAX);

    return Fallback;
}

INTERNAL void
RegionAddRanges(LowerState* State, LowerRegion* Region, FLVOperand Selection)
{
    if (Region->RangeSelection == FLV_OPERAND_NONE) {
        Region->RangeSelection = Selection;
    } else {
        Region->RangeSelection = EmitOp(
            State, 
            FLV_KERNEL_SEL_AND, 
            Region->RangeSelection, 
            Selection
        );
    }
}

INTERNAL void
RegionAddMask(LowerState* State, LowerRegion* Region, FLVOperand Mask)
{
    if (Region->Mask == FLV_OPERAND_NONE) {
        Region->Mask = Mask;
    } else {
        Region->Mask = EmitOp(
            State, 
            FLV_KERNEL_MASK_AND, 
            Region->Mask, 
            Mask
        );
    }
}

INTERNAL LowerRegion*
RegionOpen(LowerState* State, TypeIndex Base)
{
    LowerRegion* Result = ArenaPushStruct(State->Scratch, LowerRegion);

    Result->Prev = State->Region;
    Result->OpCount = 0;
    Result->Base = Base;
    Result->BaseSlot = SlotForBase(State, Base);
    Result->RangeSelection = State->CtxSel[State->CtxDepth];
    Result->Mask = FLV_OPERAND_NONE;
    State->Region = Result;

    if (MessageInstance(Base) != FLV_INSTANCE_ANY) {
        FLVOperand Inst = EmitOp(
            State, 
            FLV_KERNEL_LOAD_INSTANCE, 
            FLV_OPERAND_NONE, 
            FLV_OPERAND_NONE
        );

        RegionAddMask(
            State, 
            Result, 
            EmitOp(
                State, 
                FLV_KERNEL_CMP_EQ_I64_S, 
                Inst, 
                ConstI64(State, (i64) MessageInstance(Base))
            )
        );
    }

    return Result;
}

INTERNAL void
RegionClose(LowerState* State)
{
    LowerRegion* Region = State->Region;
    FLVRegion* Result = RegionPush(State, FLV_REGION_KIND_RUN);

    ASSERT_ALWAYS(State->OpCount + Region->OpCount <= FLV_PROGRAM_OP_MAX);

    MemCpy(
        State->Ops + State->OpCount, 
        Region->Ops, 
        Region->OpCount * sizeof(FLVOp)
    );
    State->OpCount += Region->OpCount;
    Result->OpCount = Region->OpCount;
    Result->BaseSlot = Region->BaseSlot;
    Result->RangeSelection = Region->RangeSelection;
    Result->Mask = Region->Mask;
    State->Region = Region->Prev;
}

INTERNAL LowerValue LowerExpr(LowerState* State, ExprIndex Expr);
INTERNAL FLVOperand LowerSelection(LowerState* State, ExprIndex Expr);

INTERNAL LowerValue
ValueAlloc(FLVOperand Op, u8 Kind, TypeIndex Type)
{
    LowerValue Result = {};

    Result.Op = Op;
    Result.Kind = Kind;
    Result.Type = Type;

    return Result;
}

INTERNAL LowerValue
ValueMissing(void)
{
    TypeIndex Void = {
        BUILTIN_void
    };
    LowerValue Result = ValueAlloc(
        FLV_OPERAND_NONE, 
        LOWER_KIND_MISSING, 
        Void
    );

    return Result;
}

INTERNAL b32
IsChunk(LowerValue Value)
{
    b32 Result = (
        Value.Kind == FLV_ELEMENT_KIND_I64 || 
        Value.Kind == FLV_ELEMENT_KIND_F64
    );

    return Result;
}

INTERNAL b32
IsF64Value(LowerValue Value)
{
    b32 Result = (
        Value.Kind == FLV_ELEMENT_KIND_F64 || 
        Value.Kind == FLV_ELEMENT_KIND_SINGLE_F64
    );

    return Result;
}

INTERNAL LowerValue
ToF64(LowerState* State, LowerValue Value)
{
    if (IsF64Value(Value))
        return Value;

    TypeIndex F64 = {
        BUILTIN_f64
    };

    if (FLVGetOperandKind(Value.Op) == FLV_OPERAND_KIND_CONST) {
        FLVConst const* Const = &State->Consts[FLVGetOperandIndex(Value.Op)];

        return ValueAlloc(
            ConstF64(State, (f64) Const->I64), 
            FLV_ELEMENT_KIND_SINGLE_F64, 
            F64
        );
    }

    if (Value.Kind == FLV_ELEMENT_KIND_I64) {
        return ValueAlloc(
            EmitOp(
                State, 
                FLV_KERNEL_CONVERT_I64_F64, 
                Value.Op, 
                FLV_OPERAND_NONE
            ), 
            FLV_ELEMENT_KIND_F64, 
            F64
        );
    }

    return ValueAlloc(
        EmitOp(
            State, 
            FLV_KERNEL_S_CONVERT_I64_F64, 
            Value.Op, 
            FLV_OPERAND_NONE
        ), 
        FLV_ELEMENT_KIND_SINGLE_F64, 
        F64
    );
}

INTERNAL u8
ElemKindOf(TypeIndex T)
{
    u8 Result = FLV_ELEMENT_KIND_I64;

    if (T.V == BUILTIN_f32 || T.V == BUILTIN_f64)
        Result = FLV_ELEMENT_KIND_F64;

    return Result;
}

INTERNAL b32
IsF64Elem(TypeIndex T)
{
    return ElemKindOf(T) == FLV_ELEMENT_KIND_F64;
}

no_inline LowerValue
LowerLiteral(LowerState* State, ASTNodeLiteral* Literal)
{
    TypeIndex Type = Literal->Type;

    if (IsKind(Type, TYPE_KIND_TIME) || IsKind(Type, TYPE_KIND_DURATION)) {
        return ValueAlloc(
            ConstTime(State, Literal->Val.Unsigned), 
            FLV_ELEMENT_KIND_SINGLE_I64, 
            Type
        );
    }

    if (IsF64Elem(Type)) {
        f64 Value = (Literal->Header.Variant == LITERAL_VARIANT_FLOAT)
            ? Literal->Val.Double
            : (f64) Literal->Val.Signed;

        return ValueAlloc(
            ConstF64(State, Value), 
            FLV_ELEMENT_KIND_SINGLE_F64, 
            Type
        );
    }

    return ValueAlloc(
        ConstI64(State, Literal->Val.Signed), 
        FLV_ELEMENT_KIND_SINGLE_I64, 
        Type
    );
}

no_inline LowerValue
LowerFieldLoad(
    LowerState* State, 
    ASTNodeHeader* Header, 
    u32 Slot, 
    TypeIndex Type
) {
    TypeIndex Elem = SeriesElem(Type);

    if (!State->Region) {
        LOWER_ERROR(
            State, 
            Header, 
            "a series value here has no display, reduction or selection to run in"
        );

        return ValueMissing();
    }

    if (IsKind(Elem, TYPE_KIND_STRING)) {
        LOWER_ERROR(
            State, 
            Header, 
            "a string field can only be a table column"
        );

        return ValueMissing();
    }

    FLVOperand SlotOp = FLVOperandAlloc(FLV_OPERAND_KIND_SLOT, Slot);
    u16 Kernel = IsF64Elem(Elem) 
        ? FLV_KERNEL_LOAD_F64 
        : FLV_KERNEL_LOAD_I64;

    return ValueAlloc(
        EmitOp(
            State, 
            Kernel, 
            SlotOp, 
            FLV_OPERAND_NONE
        ), 
        ElemKindOf(Elem), 
        Type
    );
}

INTERNAL u32
DeclDepthOf(LowerState* State, BindIndex Bind)
{
    u32 Result = 0;

    for (u32 Index = 0; Index < State->DeclCount; ++Index) {
        if (State->Decls[Index].Bind.V == Bind.V) {
            Result = State->Decls[Index].Depth;
            break;
        }
    }

    return Result;
}

INTERNAL ExprIndex
VarInitExpr(LowerState* State, Binding const* Bind)
{
    AST* Tree = State->Tree;
    ASTNodeDecl* Decl = (ASTNodeDecl*) (Tree->Decls + Bind->Decl.V);
    ASTNodeDeclValued* Valued = (ASTNodeDeclValued*) (
        Tree->Decls + Decl->FirstDecl.V
    );
    ASTNodeInitialiserExpr* Init = (ASTNodeInitialiserExpr*) (
        Tree->Inits + Valued->Init.V
    );

    return Init->Val;
}

INTERNAL LowerValue
LowerVarUse(LowerState* State, ASTNodeExprIdent* Ident)
{
    Binding const* Bind = &GetTLS()->SYMBOL_TABLE->Bindings[Ident->Bind.V];
    b32 IsSeries = IsKind(Bind->Type, TYPE_KIND_SERIES);

    if (!IsSeries) {
        for (u32 Index = 0; Index < State->VarCount; ++Index) {
            if (State->Vars[Index].Bind.V == Ident->Bind.V)
                return ValueAlloc(
                    State->Vars[Index].Op, 
                    State->Vars[Index].Kind, 
                    Bind->Type
                );
        }
    }

    u32 SavedDepth = State->CtxDepth;

    State->CtxDepth = DeclDepthOf(State, Ident->Bind);

    LowerValue Result = LowerExpr(State, VarInitExpr(State, Bind));

    State->CtxDepth = SavedDepth;

    if (!IsSeries) {
        ASSERT_ALWAYS(State->VarCount < FLV_LOWER_VAR_MAX);

        State->Vars[State->VarCount].Bind = Ident->Bind;
        State->Vars[State->VarCount].Op = Result.Op;
        State->Vars[State->VarCount].Kind = Result.Kind;
        ++State->VarCount;
    }

    return Result;
}

no_inline LowerValue
LowerIdent(LowerState* State, ASTNodeExprIdent* Ident)
{
    TypeIndex Type = Ident->Type;
    LowerValue Result = ValueMissing();

    if (Ident->Slot != U32_MAX) {
        FLVSlot const* Slot = &State->Slots[Ident->Slot];

        switch (Slot->Kind) {
            default: {
                ASSERT(FALSE);
            } break;

            case FLV_SLOT_KIND_FIELD: {
                Result = LowerFieldLoad(
                    State, 
                    &Ident->Header, 
                    Ident->Slot, 
                    Type
                );
            } break;

            case FLV_SLOT_KIND_MESSAGE: {
                Result = ValueAlloc(
                    FLVOperandAlloc(
                        FLV_OPERAND_KIND_SLOT, 
                        Ident->Slot
                    ), 
                    LOWER_KIND_MESSAGE, 
                    Type
                );
            } break;

            case FLV_SLOT_KIND_MODE: {
                Result = ValueAlloc(
                    FLVOperandAlloc(
                        FLV_OPERAND_KIND_SLOT, 
                        Ident->Slot
                    ), 
                    LOWER_KIND_MODE, 
                    Type
                );
            } break;
        }

        return Result;
    }

    if (Ident->Bind.V == FLV_STDLIB_ARMED.V) {
        Result = ValueAlloc(
            EmitOp(
                State, 
                FLV_KERNEL_SEL_ARMED, 
                FLV_OPERAND_NONE, 
                FLV_OPERAND_NONE
            ), 
            FLV_ELEMENT_KIND_SELECTION,
            Type
        );
    } else if (Ident->Bind.V != BINDING_INVALID_VALUE) {
        Result = LowerVarUse(State, Ident);
    }

    return Result;
}

INTERNAL u16
ArithmeticKernel(u8 Op, b32 IsF64, u32 Form)
{
    u16 Result = FLV_KERNEL_NONE;

    switch (Op) {
        default: {
            ASSERT(FALSE);
        } break;

        case TOKEN_KIND_PLUS: {
            Result = (u16) (FLV_KERNEL_ADD_I64_T + IsF64 * 2 + Form); 
        } break;

        case TOKEN_KIND_STAR: {
            Result = (u16) (FLV_KERNEL_MUL_I64_T + IsF64 * 2 + Form); 
        } break;

        case TOKEN_KIND_MINUS: {
            Result = (u16) (FLV_KERNEL_SUB_I64_T + IsF64 * 3 + Form); 
        } break;

        case TOKEN_KIND_SLASH: {
            Result = (u16) (FLV_KERNEL_DIV_I64_T + IsF64 * 3 + Form); 
        } break;

        case TOKEN_KIND_PERCENT: {
            Result = (u16) (FLV_KERNEL_MOD_I64_T + IsF64 * 3 + Form); 
        } break;
    }

    return Result;
}

INTERNAL u16
ScalarArithKernel(u8 Op, b32 IsF64)
{
    u16 Result = FLV_KERNEL_NONE;

    switch (Op) {
        default: {
            ASSERT(FALSE);
        } break;

        case TOKEN_KIND_PLUS: { 
            Result = IsF64 
                ? FLV_KERNEL_S_ADD_F64 
                : FLV_KERNEL_S_ADD_I64; 
        } break;

        case TOKEN_KIND_MINUS: { 
            Result = IsF64 
                ? FLV_KERNEL_S_SUB_F64 
                : FLV_KERNEL_S_SUB_I64; 
        } break;

        case TOKEN_KIND_STAR: { 
            Result = IsF64 
                ? FLV_KERNEL_S_MUL_F64 
                : FLV_KERNEL_S_MUL_I64; 
        } break;

        case TOKEN_KIND_SLASH: { 
            Result = IsF64 
                ? FLV_KERNEL_S_DIV_F64 
                : FLV_KERNEL_S_DIV_I64; 
        } break;

        case TOKEN_KIND_PERCENT: { 
            Result = IsF64 
                ? FLV_KERNEL_S_MOD_F64 
                : FLV_KERNEL_S_MOD_I64; 
        } break;
    }

    return Result;
}

INTERNAL u32
CmpOrdinal(u8 Op)
{
    u32 Result = 0;

    switch (Op) {
        default: {
            ASSERT(FALSE);
        } break;

        case TOKEN_KIND_LESS: { 
            Result = 0; 
        } break;

        case TOKEN_KIND_LESS_EQ: { 
            Result = 1; 
        } break;

        case TOKEN_KIND_GREATER: { 
            Result = 2; 
        } break;

        case TOKEN_KIND_GREATER_EQ: { 
            Result = 3; 
        } break;

        case TOKEN_KIND_EQ_EQ: { 
            Result = 4; 
        } break;

        case TOKEN_KIND_EXCLAIM_EQ: { 
            Result = 5; 
        } break;
    }

    return Result;
}

INTERNAL u32
CmpOrdinalFlipped(u32 Ordinal)
{
    u32 Result = Ordinal;

    if (Ordinal < 4)
        Result = (Ordinal ^ 2);

    return Result;
}

INTERNAL b32
IsCompare(u8 Op)
{
    b32 Result = (
        Op == TOKEN_KIND_LESS || 
        Op == TOKEN_KIND_LESS_EQ ||
        Op == TOKEN_KIND_GREATER || 
        Op == TOKEN_KIND_GREATER_EQ ||
        Op == TOKEN_KIND_EQ_EQ || 
        Op == TOKEN_KIND_EXCLAIM_EQ
    );

    return Result;
}

INTERNAL b32
IsLogical(u8 Op)
{
    b32 Result = (
        Op == TOKEN_KIND_LOG_AND || 
        Op == TOKEN_KIND_LOG_OR ||
        Op == TOKEN_KIND_BIT_AND || 
        Op == TOKEN_KIND_BIT_OR
    );

    return Result;
}

INTERNAL b32
IsAnd(u8 Op)
{
    b32 Result = (
        Op == TOKEN_KIND_LOG_AND || 
        Op == TOKEN_KIND_BIT_AND
    );

    return Result;
}

INTERNAL LowerValue
LowerBinarySeries(
    LowerState* State, 
    ASTNodeBinary* Binary, 
    LowerValue LHS, 
    LowerValue RHS
) {
    u8 Op = Binary->Header.Variant;
    TypeIndex Type = Binary->Type;

    if (IsLogical(Op)) {
        ASSERT(
            LHS.Kind == FLV_ELEMENT_KIND_MASK && 
            RHS.Kind == FLV_ELEMENT_KIND_MASK
        );

        return ValueAlloc(
            EmitOp(
                State, 
                IsAnd(Op) 
                    ? FLV_KERNEL_MASK_AND 
                    : FLV_KERNEL_MASK_OR, 
                LHS.Op, 
                RHS.Op
            ), 
            FLV_ELEMENT_KIND_MASK, 
            Type
        );
    }

    b32 IsF64 = IsF64Value(LHS) || IsF64Value(RHS);

    if (IsF64) {
        LHS = ToF64(State, LHS);
        RHS = ToF64(State, RHS);
    }

    b32 LChunk = IsChunk(LHS);
    b32 RChunk = IsChunk(RHS);
    FLVOperand A = LHS.Op;
    FLVOperand B = RHS.Op;
    u16 Kernel;

    ASSERT(LChunk || RChunk);

    if (IsCompare(Op)) {
        u32 Ordinal = CmpOrdinal(Op);

        if (!LChunk) {
            Ordinal = CmpOrdinalFlipped(Ordinal);
            A = RHS.Op;
            B = LHS.Op;
        }

        Kernel = (u16) (
            FLV_KERNEL_CMP_LT_I64_T + 
            Ordinal * 4 + 
            IsF64 * 2 + 
            !(LChunk && RChunk)
        );

        return ValueAlloc(
            EmitOp(
                State, 
                Kernel, 
                A, 
                B
            ), 
            FLV_ELEMENT_KIND_MASK, 
            Type
        );
    }

    if (LChunk && RChunk) {
        Kernel = ArithmeticKernel(Op, IsF64, 0);
    } else if (LChunk) {
        Kernel = ArithmeticKernel(Op, IsF64, 1);
    } else if (
        Op == TOKEN_KIND_PLUS || 
        Op == TOKEN_KIND_STAR
    ) {
        Kernel = ArithmeticKernel(Op, IsF64, 1);
        A = RHS.Op;
        B = LHS.Op;
    } else {
        Kernel = ArithmeticKernel(Op, IsF64, 2);
    }

    return ValueAlloc(
        EmitOp(
            State, 
            Kernel, 
            A, 
            B
        ), 
        IsF64 
            ? FLV_ELEMENT_KIND_F64 
            : FLV_ELEMENT_KIND_I64, 
        Type
    );
}

INTERNAL LowerValue
LowerBinaryScalar(
    LowerState* State, 
    ASTNodeBinary* Binary, 
    LowerValue LHS, 
    LowerValue RHS
) {
    u8 Op = Binary->Header.Variant;
    TypeIndex Type = Binary->Type;

    if (IsLogical(Op)) {
        return ValueAlloc(
            EmitOp(
                State, 
                IsAnd(Op) 
                    ? FLV_KERNEL_S_AND 
                    : FLV_KERNEL_S_OR, 
                LHS.Op, 
                RHS.Op
            ), 
            FLV_ELEMENT_KIND_SINGLE_I64, 
            Type
        );
    }

    b32 IsF64 = IsF64Value(LHS) || IsF64Value(RHS);

    if (IsF64) {
        LHS = ToF64(State, LHS);
        RHS = ToF64(State, RHS);
    }

    if (IsCompare(Op)) {
        return ValueAlloc(
            EmitOp(
                State, 
                (u16) (FLV_KERNEL_S_CMP_LT_I64 + CmpOrdinal(Op) * 2 + IsF64), 
                LHS.Op, 
                RHS.Op
            ), 
            FLV_ELEMENT_KIND_SINGLE_I64, 
            Type
        );
    }

    return ValueAlloc(
        EmitOp(
            State, 
            ScalarArithKernel(
                Op, 
                IsF64
            ), 
            LHS.Op, 
            RHS.Op
        ), 
        IsF64 
            ? FLV_ELEMENT_KIND_SINGLE_F64 
            : FLV_ELEMENT_KIND_SINGLE_I64, 
        Type
    );
}

INTERNAL LowerValue
LowerBinarySelection(
    LowerState* State, 
    ASTNodeBinary* Binary, 
    ExprIndex Left, 
    ExprIndex Right
) {
    FLVOperand LHS = LowerSelection(State, Left);
    FLVOperand RHS = LowerSelection(State, Right);

    if (LHS == FLV_OPERAND_NONE || RHS == FLV_OPERAND_NONE)
        return ValueMissing();

    u16 Kernel = (Binary->Header.Variant == TOKEN_KIND_BIT_AND) 
        ? FLV_KERNEL_SEL_AND 
        : FLV_KERNEL_SEL_OR;

    return ValueAlloc(
        EmitOp(
            State, 
            Kernel, 
            LHS, 
            RHS
        ), 
        FLV_ELEMENT_KIND_SELECTION, 
        Binary->Type
    );
}

no_inline LowerValue
LowerBinary(LowerState* State, ASTNodeBinary* Binary)
{
    ExprIndex Left = Binary->Left;
    ExprIndex Right = ImplicitChild(State, Binary, sizeof(ASTNodeBinary));
    TypeIndex Type = Binary->Type;

    if (IsKind(Type, TYPE_KIND_SELECTION))
        return LowerBinarySelection(State, Binary, Left, Right);

    LowerValue LHS = LowerExpr(State, Left);
    LowerValue RHS = LowerExpr(State, Right);

    if (LHS.Kind == LOWER_KIND_MISSING || RHS.Kind == LOWER_KIND_MISSING)
        return ValueMissing();

    if (IsKind(Type, TYPE_KIND_SERIES))
        return LowerBinarySeries(State, Binary, LHS, RHS);

    return LowerBinaryScalar(State, Binary, LHS, RHS);
}

no_inline LowerValue
LowerUnary(LowerState* State, ASTNodeUnary* Unary)
{
    TypeIndex Type = Unary->Type;

    if (
        Unary->Header.Variant == TOKEN_KIND_EXCLAIM && 
        IsKind(Type, TYPE_KIND_SELECTION)
    ) {
        FLVOperand Selection = LowerSelection(State, Unary->Expr);

        if (Selection == FLV_OPERAND_NONE)
            return ValueMissing();

        return ValueAlloc(
            EmitOp(
                State, 
                FLV_KERNEL_SEL_NOT, 
                Selection, 
                FLV_OPERAND_NONE
            ), 
            FLV_ELEMENT_KIND_SELECTION,
            Type
        );
    }

    LowerValue Value = LowerExpr(State, Unary->Expr);

    if (Value.Kind == LOWER_KIND_MISSING)
        return Value;

    if (Unary->Header.Variant == TOKEN_KIND_EXCLAIM) {
        if (Value.Kind == FLV_ELEMENT_KIND_MASK) {
            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_MASK_NOT, 
                    Value.Op, 
                    FLV_OPERAND_NONE
                ), 
                FLV_ELEMENT_KIND_MASK, 
                Type
            );
        }

        return ValueAlloc(
            EmitOp(
                State, 
                FLV_KERNEL_S_NOT, 
                Value.Op, 
                FLV_OPERAND_NONE
            ), 
            FLV_ELEMENT_KIND_SINGLE_I64, 
            Type
        );
    }

    if (FLVGetOperandKind(Value.Op) == FLV_OPERAND_KIND_CONST) {
        FLVConst C = State->Consts[FLVGetOperandIndex(Value.Op)];

        if (C.Kind == FLV_CONST_KIND_F64)
            C.F64 = -C.F64;
        else
            C.I64 = -C.I64;

        return ValueAlloc(ConstAdd(State, C), Value.Kind, Type);
    }

    switch (Value.Kind) {
        default: {
            ASSERT(FALSE);
        } break;

        case FLV_ELEMENT_KIND_I64: { 
            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_NEG_I64, 
                    Value.Op, 
                    FLV_OPERAND_NONE
                ), 
                Value.Kind, 
                Type
            ); 
        } break;

        case FLV_ELEMENT_KIND_F64: { 
            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_NEG_F64, 
                    Value.Op, 
                    FLV_OPERAND_NONE
                ), 
                Value.Kind, 
                Type
            ); 
        } break;

        case FLV_ELEMENT_KIND_SINGLE_I64: { 
            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_S_NEG_I64, 
                    Value.Op, 
                    FLV_OPERAND_NONE
                ), 
                Value.Kind, 
                Type
            ); 
        } break;

        case FLV_ELEMENT_KIND_SINGLE_F64: { 
            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_S_NEG_F64, 
                    Value.Op, 
                    FLV_OPERAND_NONE
                ), 
                Value.Kind, 
                Type
            ); 
        } break;
    }

    return ValueMissing();
}

INTERNAL FLVOperand
LowerSelectionFromBool(LowerState* State, ExprIndex Expr)
{
    RegionOpen(State, BaseOf(TypeOf(State, Expr)));

    LowerValue Mask = LowerExpr(State, Expr);
    FLVOperand Result = FLV_OPERAND_NONE;

    if (Mask.Kind == FLV_ELEMENT_KIND_MASK) {
        Result = EmitOp(
            State, 
            FLV_KERNEL_SEL_FROM_MASK, 
            Mask.Op, 
            FLV_OPERAND_NONE
        );
    }

    RegionClose(State);

    return Result;
}

INTERNAL FLVOperand
LowerSelection(LowerState* State, ExprIndex Expr)
{
    if (IsKind(TypeOf(State, Expr), TYPE_KIND_SERIES))
        return LowerSelectionFromBool(State, Expr);

    LowerValue Value = LowerExpr(State, Expr);
    FLVOperand Result = FLV_OPERAND_NONE;

    if (Value.Kind == FLV_ELEMENT_KIND_SELECTION)
        Result = Value.Op;

    return Result;
}

no_inline LowerValue
LowerIndex(LowerState* State, ASTNodeBinary* Index)
{
    ExprIndex Idx = ImplicitChild(State, Index, sizeof(ASTNodeBinary));
    LowerRegion* Region = State->Region;
    LowerValue Result = LowerExpr(State, Index->Left);

    if (Result.Kind == LOWER_KIND_MISSING)
        return Result;

    Result.Type = Index->Type;

    if (Index->Header.Variant == INDEX_KIND_INSTANCE)
        return Result;

    if (!Region) {
        LOWER_ERROR(
            State, 
            &Index->Header, 
            "a selection here has no display, reduction or selection to run in"
        );

        return ValueMissing();
    }

    switch (Index->Header.Variant) {
        default: {
            ASSERT(FALSE);
        } break;

        case INDEX_KIND_TIME: {
            ASTNodeBinary* Range = (ASTNodeBinary*) ExprAt(State, Idx);
            LowerValue Low = LowerExpr(State, Range->Left);
            LowerValue High = LowerExpr(
                State, 
                ImplicitChild(State, Range, sizeof(ASTNodeBinary))
            );

            RegionAddRanges(
                State, 
                Region, 
                EmitOp(
                    State, 
                    FLV_KERNEL_SEL_FROM_TIME_RANGE, 
                    Low.Op, 
                    High.Op
                )
            );
        } break;

        case INDEX_KIND_CONDITION: {
            if (BaseOf(TypeOf(State, Idx)).V == Region->Base.V) {
                LowerValue Mask = LowerExpr(State, Idx);

                if (Mask.Kind == FLV_ELEMENT_KIND_MASK)
                    RegionAddMask(State, Region, Mask.Op);
            } else {
                FLVOperand Selection = LowerSelectionFromBool(State, Idx);

                if (Selection != FLV_OPERAND_NONE)
                    RegionAddRanges(State, Region, Selection);
            }
        } break;

        case INDEX_KIND_SELECTION: {
            FLVOperand Selection = LowerSelection(State, Idx);

            if (Selection != FLV_OPERAND_NONE)
                RegionAddRanges(State, Region, Selection);
        } break;
    }

    return Result;
}

INTERNAL u16
ReductionKernel(u16 ID, b32 IsF64)
{
    u16 Result = FLV_KERNEL_NONE;

    switch (ID) {
        default: {
            ASSERT(FALSE);
        } break;

        case FLV_STDLIB_ID_MEAN: { 
            Result = IsF64 
                ? FLV_KERNEL_MEAN_F64 
                : FLV_KERNEL_MEAN_I64; 
        } break;

        case FLV_STDLIB_ID_STDDEV: { 
            Result = IsF64 
                ? FLV_KERNEL_STDDEV_F64 
                : FLV_KERNEL_STDDEV_I64; 
        } break;

        case FLV_STDLIB_ID_MIN: { 
            Result = IsF64 
                ? FLV_KERNEL_MIN_F64 
                : FLV_KERNEL_MIN_I64; 
        } break;

        case FLV_STDLIB_ID_MAX: { 
            Result = IsF64 
                ? FLV_KERNEL_MAX_F64 
                : FLV_KERNEL_MAX_I64; 
        } break;

        case FLV_STDLIB_ID_SUM: { 
            Result = IsF64 
                ? FLV_KERNEL_SUM_F64 
                : FLV_KERNEL_SUM_I64; 
        } break;

        case FLV_STDLIB_ID_HEAD: { 
            Result = IsF64 
                ? FLV_KERNEL_FIRST_F64 
                : FLV_KERNEL_FIRST_I64; 
        } break;

        case FLV_STDLIB_ID_TAIL: { 
            Result = IsF64 
                ? FLV_KERNEL_LAST_F64 
                : FLV_KERNEL_LAST_I64; 
        } break;

        case FLV_STDLIB_ID_COUNT: { 
            Result = FLV_KERNEL_REDUCE_COUNT; 
        } break;
    }

    return Result;
}

INTERNAL u16
WindowKernel(u16 ID, b32 IsF64)
{
    u16 Result = FLV_KERNEL_NONE;

    switch (ID) {
        default: {
            ASSERT(FALSE);
        } break;

        case FLV_STDLIB_ID_DIFF: { 
            Result = IsF64 
                ? FLV_KERNEL_DIFF_F64 
                : FLV_KERNEL_DIFF_I64; 
        } break;

        case FLV_STDLIB_ID_RATE: { 
            Result = IsF64 
                ? FLV_KERNEL_RATE_F64 
                : FLV_KERNEL_RATE_I64; 
        } break;

        case FLV_STDLIB_ID_ROLLING_MEAN: { 
            Result = IsF64 
                ? FLV_KERNEL_ROLLING_MEAN_F64 
                : FLV_KERNEL_ROLLING_MEAN_I64; 
        } break;

        case FLV_STDLIB_ID_ROLLING_MIN: { 
            Result = IsF64 
                ? FLV_KERNEL_ROLLING_MIN_F64 
                : FLV_KERNEL_ROLLING_MIN_I64; 
        } break;

        case FLV_STDLIB_ID_ROLLING_MAX: { 
            Result = IsF64 
                ? FLV_KERNEL_ROLLING_MAX_F64 
                : FLV_KERNEL_ROLLING_MAX_I64; 
        } break;
    }

    return Result;
}

no_inline LowerValue
LowerCall(LowerState* State, ASTNodeFuncCall* Call, ExprIndex Self)
{
    BindIndex Chosen = ASTResolvedFind(State->Tree, Self);
    TypeIndex Type = Call->Type;
    u32 ArgCount = Call->Header.Count;

    ASSERT_ALWAYS(Chosen.V != BINDING_INVALID_VALUE);

    u16 ID = (u16) GetTLS()->SYMBOL_TABLE->Bindings[Chosen.V].Decl.V;
    ExprIndex Head = Call->Arg[ArgCount - 1];

    switch (ID) {
        default: {
            ASSERT(FALSE);
        } break;

        case FLV_STDLIB_ID_MEAN:
        case FLV_STDLIB_ID_MIN:
        case FLV_STDLIB_ID_MAX:
        case FLV_STDLIB_ID_SUM:
        case FLV_STDLIB_ID_STDDEV:
        case FLV_STDLIB_ID_COUNT:
        case FLV_STDLIB_ID_HEAD:
        case FLV_STDLIB_ID_TAIL: {
            RegionOpen(State, BaseOf(TypeOf(State, Head)));

            LowerValue Value = LowerExpr(State, Head);
            LowerValue Result = ValueMissing();

            if (IsChunk(Value)) {
                u16 Kernel = ReductionKernel(ID, IsF64Value(Value));

                Result = ValueAlloc(
                    EmitOp(
                        State, 
                        Kernel, 
                        Value.Op, 
                        FLV_OPERAND_NONE
                    ), 
                    FLV_KERNEL_INFO_TABLE[Kernel].DstKind, 
                    Type
                );
            }

            RegionClose(State);

            return Result;
        } break;

        case FLV_STDLIB_ID_ABS_SERIES:
        case FLV_STDLIB_ID_ABS_SCALAR: {
            LowerValue Value = LowerExpr(State, Head);

            switch (Value.Kind) {
                default: {
                    return Value;
                } break;

                case FLV_ELEMENT_KIND_I64: { 
                    return ValueAlloc(
                        EmitOp(
                            State, 
                            FLV_KERNEL_ABS_I64, 
                            Value.Op, 
                            FLV_OPERAND_NONE
                        ), 
                        Value.Kind, 
                        Type
                    ); 
                } break;

                case FLV_ELEMENT_KIND_F64: { 
                    return ValueAlloc(
                        EmitOp(
                            State, 
                            FLV_KERNEL_ABS_F64, 
                            Value.Op, 
                            FLV_OPERAND_NONE
                        ), 
                        Value.Kind, 
                        Type
                    ); 
                } break;

                case FLV_ELEMENT_KIND_SINGLE_I64: { 
                    return ValueAlloc(
                        EmitOp(
                            State, 
                            FLV_KERNEL_S_ABS_I64, 
                            Value.Op, 
                            FLV_OPERAND_NONE
                        ), 
                        Value.Kind, 
                        Type
                    ); 
                } break;

                case FLV_ELEMENT_KIND_SINGLE_F64: { 
                    return ValueAlloc(
                        EmitOp(
                            State, 
                            FLV_KERNEL_S_ABS_F64, 
                            Value.Op, 
                            FLV_OPERAND_NONE
                        ), 
                        Value.Kind, 
                        Type
                    ); 
                } break;
            }
        } break;

        case FLV_STDLIB_ID_DIFF:
        case FLV_STDLIB_ID_RATE:
        case FLV_STDLIB_ID_ROLLING_MEAN:
        case FLV_STDLIB_ID_ROLLING_MIN:
        case FLV_STDLIB_ID_ROLLING_MAX: {
            LowerValue Value = LowerExpr(State, Head);
            FLVOperand Window = FLV_OPERAND_NONE;

            if (!IsChunk(Value))
                return ValueMissing();

            if (ArgCount == 2) {
                LowerValue W = LowerExpr(State, Call->Arg[0]);

                if (FLVGetOperandKind(W.Op) != FLV_OPERAND_KIND_CONST) {
                    LOWER_ERROR(
                        State, 
                        &Call->Header, 
                        "a rolling window must be a duration literal"
                    );

                    return ValueMissing();
                }

                Window = W.Op;
            }

            u16 Kernel = WindowKernel(ID, IsF64Value(Value));

            return ValueAlloc(
                EmitOp(
                    State, 
                    Kernel, 
                    Value.Op, 
                    Window
                ), FLV_KERNEL_INFO_TABLE[Kernel].DstKind, 
                Type
            );
        } break;

        case FLV_STDLIB_ID_MODE: {
            LowerValue Mode = LowerExpr(State, Head);

            if (Mode.Kind != LOWER_KIND_MODE)
                return ValueMissing();

            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_SEL_MODE, 
                    Mode.Op, 
                    FLV_OPERAND_NONE
                ), 
                FLV_ELEMENT_KIND_SELECTION, 
                Type
            );
        } break;

        case FLV_STDLIB_ID_EVENT: {
            LowerValue Event = LowerExpr(State, Head);

            if (FLVGetOperandKind(Event.Op) != FLV_OPERAND_KIND_CONST) {
                LOWER_ERROR(
                    State, 
                    &Call->Header, 
                    "event() takes an integer literal"
                );

                return ValueMissing();
            }

            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_SEL_EVENT, 
                    Event.Op, 
                    FLV_OPERAND_NONE
                ), 
                FLV_ELEMENT_KIND_SELECTION, 
                Type
            );
        } break;

        case FLV_STDLIB_ID_DURATION: {
            FLVOperand Sel = LowerSelection(State, Head);

            if (Sel == FLV_OPERAND_NONE)
                return ValueMissing();

            return ValueAlloc(
                EmitOp(
                    State, 
                    FLV_KERNEL_SEL_DURATION, 
                    Sel, 
                    FLV_OPERAND_NONE
                ), 
                FLV_ELEMENT_KIND_SINGLE_I64, 
                Type
            );
        } break;
    }

    return ValueMissing();
}

no_inline LowerValue
LowerExpr(LowerState* State, ExprIndex Expr)
{
    ASTNodeExprAccel* Node = ExprAt(State, Expr);
    LowerValue Result = ValueMissing();

    if (ASTExprIsTyped(Node->Header.Type) && IsVoid(Node->Type))
        return Result;

    switch (Node->Header.Type) {
        default: {
            ASSERT(FALSE);
        } break;

        case AST_LITERAL: { 
            Result = LowerLiteral(State, (ASTNodeLiteral*) Node); 
        } break;

        case AST_IDENT: { 
            Result = LowerIdent(State, (ASTNodeExprIdent*) Node); 
        } break;

        case AST_INDEX: { 
            Result = LowerIndex(State, (ASTNodeBinary*) Node); 
        } break;

        case AST_UNARY_EXPR: { 
            Result = LowerUnary(State, (ASTNodeUnary*) Node); 
        } break;

        case AST_BINARY_EXPR: { 
            Result = LowerBinary(State, (ASTNodeBinary*) Node); 
        } break;

        case AST_FUNC_CALL: { 
            Result = LowerCall(State, (ASTNodeFuncCall*) Node, Expr); 
        } break;

        case AST_FIELD_ACCESS: {
            ASTNodeFieldAccess* Access = (ASTNodeFieldAccess*) Node;

            Result = LowerFieldLoad(
                State, 
                &Access->Header, 
                Access->Slot, 
                Access->Type
            );
        } break;
    }

    return Result;
}

INTERNAL b32
IsPlainField(LowerState* State, ExprIndex Expr, OUT u32* Slot)
{
    ASTNodeExprAccel* Node = ExprAt(State, Expr);
    u32 Result = U32_MAX;

    if (Node->Header.Type == AST_FIELD_ACCESS)
        Result = ((ASTNodeFieldAccess*) Node)->Slot;
    else if (Node->Header.Type == AST_IDENT)
        Result = ((ASTNodeExprIdent*) Node)->Slot;

    if (
        Result != U32_MAX && 
        State->Slots[Result].Kind != FLV_SLOT_KIND_FIELD
    ) {
        Result = U32_MAX;
    }

    *Slot = Result;

    return Result != U32_MAX;
}

INTERNAL void
EmitMissing(LowerState* State, u8 DisplayKind, Str8 Label)
{
    EmitOp(
        State, 
        FLV_KERNEL_EMIT_MISSING, 
        FLV_OPERAND_NONE, 
        (FLVOperand) EmitAdd(
            State, 
            DisplayKind, 
            1, 
            FLV_ELEMENT_KIND_NONE, 
            Label
        )
    );
}

INTERNAL void
LowerGraph(LowerState* State, ASTNodeDisplay* Display)
{
    for (u32 Index = 0; Index < Display->Header.Count; ++Index) {
        ASTDisplayArg* Arg = &Display->Arg[Index];
        TypeIndex Type = TypeOf(State, Arg->Expr);
        Str8 Label = ArgLabel(State, Arg);

        if (IsVoid(Type) || State->Missing) {
            EmitMissing(State, TOKEN_KIND_GRAPH, Label);
            continue;
        }

        RegionOpen(State, BaseOf(Type));

        LowerValue Value = LowerExpr(State, Arg->Expr);

        if (IsChunk(Value)) {
            Value = ToF64(State, Value);
            EmitOp(
                State, 
                FLV_KERNEL_EMIT_CURVE, 
                Value.Op, 
                (FLVOperand) EmitAdd(
                    State, 
                    TOKEN_KIND_GRAPH, 
                    Arg->Axis, 
                    FLV_ELEMENT_KIND_F64, 
                    Label
                )
            );
        }

        RegionClose(State);
    }
}

INTERNAL u32
FieldSlotFind(
    LowerState* State, 
    NameIndex Message, 
    NameIndex Field, 
    u32 Instance
) {
    u32 Result = U32_MAX;

    for (u32 Index = 0; Index < State->SlotCount; ++Index) {
        FLVSlot const* Slot = &State->Slots[Index];

        if (
            Slot->Kind == FLV_SLOT_KIND_FIELD &&
            Slot->Message.V == Message.V &&
            Slot->Field.V == Field.V &&
            Slot->Instance == Instance
        ) {
            Result = Index;
            break;
        }
    }

    return Result;
}

INTERNAL void
EmitTableColumnFromSlot(
    LowerState* State, 
    u32 Slot, 
    TypeIndex Elem, 
    Str8 Label
) {
    u8 ElementKind = FLV_ELEMENT_KIND_NONE; 

    if (!IsKind(Elem, TYPE_KIND_STRING))
        ElementKind = ElemKindOf(Elem);

    EmitOp(
        State, 
        FLV_KERNEL_EMIT_TABLE_COLUMN, 
        FLVOperandAlloc(
            FLV_OPERAND_KIND_SLOT, 
            Slot
        ), 
        (FLVOperand) EmitAdd(
            State, 
            TOKEN_KIND_TABLE, 
            1, 
            ElementKind, 
            Label
        )
    );
}


no_inline void
LowerTable(LowerState* State, ASTNodeDisplay* Display)
{
    u32 Count = Display->Header.Count;
    TypeIndex FirstType = TypeOf(State, Display->Arg[0].Expr);

    if (IsVoid(FirstType) || State->Missing) {
        for (u32 Index = 0; Index < Count; ++Index) {
            EmitMissing(
                State, 
                TOKEN_KIND_TABLE, 
                ArgLabel(
                    State, 
                    &Display->Arg[Index]
                )
            );
        }

        return;
    }

    if (IsKind(FirstType, TYPE_KIND_MESSAGE)) {
        NameIndex MessageName = {
            TypeAt(FirstType)->A
        };
        u32 Instance = MessageInstance(FirstType);
        FLVSchemaMessage const* M = &State->Schema->Messages[
            FLVSchemaFindMessage(State->Schema, MessageName)
        ];

        RegionOpen(State, FirstType);
        LowerExpr(State, Display->Arg[0].Expr);

        for (u32 Index = 0; Index < M->FieldCount; ++Index) {
            FLVSchemaField const* Field = &State->Schema->Fields[
                M->HeadField + Index
            ];
            u32 Slot = FieldSlotFind(
                State, 
                MessageName, 
                Field->Name, 
                Instance
            );

            ASSERT_ALWAYS(Slot != U32_MAX);

            EmitTableColumnFromSlot(
                State, 
                Slot, 
                Field->Element, 
                NameStr(Field->Name)
            );
        }

        RegionClose(State);

        return;
    }

    RegionOpen(State, BaseOf(FirstType));

    for (u32 Index = 0; Index < Count; ++Index) {
        ASTDisplayArg* Arg = &Display->Arg[Index];
        TypeIndex Type = TypeOf(State, Arg->Expr);
        Str8 Label = ArgLabel(State, Arg);
        u32 Slot = U32_MAX;

        if (IsVoid(Type)) {
            EmitMissing(State, TOKEN_KIND_TABLE, Label);
        } else if (IsPlainField(State, Arg->Expr, &Slot)) {
            EmitTableColumnFromSlot(State, Slot, SeriesElem(Type), Label);
        } else {
            LowerValue Value = LowerExpr(State, Arg->Expr);

            if (IsChunk(Value)) {
                EmitOp(
                    State, 
                    FLV_KERNEL_EMIT_TABLE_COLUMN, 
                    Value.Op, 
                    (FLVOperand) EmitAdd(
                        State, 
                        TOKEN_KIND_TABLE, 
                        1, 
                        Value.Kind, 
                        Label
                    )
                );
            }
        }
    }

    RegionClose(State);
}

INTERNAL void
LowerFlag(LowerState* State, ASTNodeDisplay* Display)
{
    ASTDisplayArg* Cond = &Display->Arg[0];
    Str8 Label = ArgLabel(State, Cond);

    if (Display->Header.Count > 1 && Display->Arg[1].IsLabel) {
        ASTNodeLiteral* Lit = (ASTNodeLiteral*) ExprAt(
            State, 
            Display->Arg[1].Expr
        );

        Label = Str(
            Lit->Val.String.Str, 
            Lit->Val.String.Count
        );
    }

    FLVOperand Sel = FLV_OPERAND_NONE;

    if (
        !IsVoid(TypeOf(State, Cond->Expr)) && 
        !State->Missing
    ) {
        Sel = LowerSelection(State, Cond->Expr);
    }

    if (Sel == FLV_OPERAND_NONE) {
        EmitMissing(State, TOKEN_KIND_FLAG, Label);
    } else {
        EmitOp(
            State, 
            FLV_KERNEL_EMIT_FLAG, 
            Sel, 
            (FLVOperand) EmitAdd(
                State, 
                TOKEN_KIND_FLAG, 
                1, 
                FLV_ELEMENT_KIND_SELECTION, 
                Label
            )
        );
    }
}

INTERNAL void
LowerDisplay(LowerState* State, ASTNodeDisplay* Display)
{
    switch (Display->Header.Variant) {
        default: {} break;

        case TOKEN_KIND_TEXT: {} break;

        case TOKEN_KIND_GRAPH: {
            LowerGraph(State, Display);
        } break;

        case TOKEN_KIND_TABLE: {
            LowerTable(State, Display);
        } break;

        case TOKEN_KIND_FLAG: {
            LowerFlag(State, Display);
        } break;
    }
}

INTERNAL void
CtxPush(LowerState* State, u32 Depth, ExprIndex Cond)
{
    FLVOperand Prev = State->CtxSel[Depth - 1];

    State->CtxDepth = Depth - 1;

    FLVOperand Selection = LowerSelection(State, Cond);

    if (Selection == FLV_OPERAND_NONE) {
        ++State->Missing;
        State->CtxSel[Depth] = Prev;
    } else {
        if (Prev == FLV_OPERAND_NONE) {
            State->CtxSel[Depth] = Selection;
        } else {
            State->CtxSel[Depth] = EmitOp(
                State, 
                FLV_KERNEL_SEL_AND, 
                Prev, 
                Selection
            );
        }
    }

    State->CtxDepth = Depth;
}

INTERNAL void
FilterPush(LowerState* State, ExprIndex Cond)
{
    ASSERT_ALWAYS(State->FilterDepth < FLV_FILTER_DEPTH_MAX);

    State->Filters[State->FilterDepth++] = Cond;

    if (State->InAnalysis)
        CtxPush(State, State->FilterDepth, Cond);
}

INTERNAL void
FilterPop(LowerState* State)
{
    u32 Depth = State->FilterDepth--;

    if (!State->InAnalysis)
        return;

    if (State->CtxSel[Depth] == State->CtxSel[Depth - 1])
        --State->Missing;

    State->CtxDepth = State->FilterDepth;
}

INTERNAL void
ProgramBegin(LowerState* State)
{
    State->Ops = ArenaPushArray(
        State->Scratch, 
        FLVOp, 
        FLV_PROGRAM_OP_MAX
    );
    State->Regions = ArenaPushArray(
        State->Scratch, 
        FLVRegion, 
        FLV_PROGRAM_REGION_MAX
    );
    State->Consts = ArenaPushArray(
        State->Scratch, 
        FLVConst, 
        FLV_PROGRAM_CONST_MAX
    );
    State->Emits = ArenaPushArray(
        State->Scratch, 
        FLVEmit, 
        FLV_PROGRAM_EMIT_MAX
    );
    State->Strings = ArenaPushArray(
        State->Scratch, 
        Str8, 
        FLV_PROGRAM_STRING_MAX
    );
    State->OpCount = 0;
    State->RegionCount = 0;
    State->ConstCount = 0;
    State->EmitCount = 0;
    State->StringCount = 0;
    State->TempCount = 0;
    State->VarCount = 0;
    State->Missing = 0;
    State->Region = NULL;
    State->InAnalysis = TRUE;
    State->CtxSel[0] = FLV_OPERAND_NONE;

    for (u32 Index = 0; Index < State->FilterDepth; ++Index)
        CtxPush(State, Index + 1, State->Filters[Index]);

    State->CtxDepth = State->FilterDepth;
}

INTERNAL u32
ProgramEnd(LowerState* State)
{
    ASSERT_ALWAYS(State->ProgramCount < FLV_PROGRAM_MAX);

    FLVProgram* P = &State->Programs[State->ProgramCount];
    Arena* MemPool = State->MemPool;

    P->Ops = ArenaPushArray(MemPool, FLVOp, State->OpCount);
    P->Regions = ArenaPushArray(MemPool, FLVRegion, State->RegionCount);
    P->Consts = ArenaPushArray(MemPool, FLVConst, State->ConstCount);
    P->Emits = ArenaPushArray(MemPool, FLVEmit, State->EmitCount);
    P->Strings = ArenaPushArray(MemPool, Str8, State->StringCount);
    P->TempKind = ArenaPushArray(MemPool, u8, State->TempCount);
    P->TempHash = ArenaPushArray(MemPool, u32, State->TempCount);
    MemCpy(P->Ops, State->Ops, State->OpCount * sizeof(FLVOp));
    MemCpy(P->Regions, State->Regions, State->RegionCount * sizeof(FLVRegion));
    MemCpy(P->Consts, State->Consts, State->ConstCount * sizeof(FLVConst));
    MemCpy(P->Emits, State->Emits, State->EmitCount * sizeof(FLVEmit));
    MemCpy(P->Strings, State->Strings, State->StringCount * sizeof(Str8));
    MemCpy(P->TempKind, State->TempKind, State->TempCount * sizeof(u8));
    MemCpy(P->TempHash, State->TempHash, State->TempCount * sizeof(u32));
    P->OpCount = State->OpCount;
    P->RegionCount = State->RegionCount;
    P->ConstCount = State->ConstCount;
    P->EmitCount = State->EmitCount;
    P->StringCount = State->StringCount;
    P->TempCount = State->TempCount;
    P->AnalysisIndex = State->ProgramCount;
    State->InAnalysis = FALSE;

    return State->ProgramCount++;
}

INTERNAL u32 LowerStmt(LowerState* State, u32 Offset, u32 LayoutNode);

INTERNAL void
LowerBranch(
    LowerState* State, 
    ASTNodeIf* If, 
    u32 Offset, 
    u32 LayoutNode, 
    OUT u32* Next
) {
    if (!State->InAnalysis) {
        LOWER_ERROR(
            State, 
            &If->Header, 
            "a branch if (scalar) is only legal inside an analysis"
        );
        *Next = StmtNext(State->Tree, Offset);

        return;
    }

    LowerValue Cond = LowerExpr(State, If->Cond);

    if (Cond.Kind != FLV_ELEMENT_KIND_SINGLE_I64) {
        if (Cond.Kind != LOWER_KIND_MISSING) {
            LOWER_ERROR(
                State, 
                &If->Header, 
                "a branch condition must be an integer or boolean scalar"
            );
        }

        *Next = StmtNext(State->Tree, Offset);

        return;
    }

    u32 JumpIndex = State->RegionCount;

    RegionPush(State, FLV_REGION_KIND_JUMP_IF_FALSE)->Operand = Cond.Op;
    *Next = LowerStmt(State, Offset + sizeof(ASTNodeIf), LayoutNode);

    if (If->Else.V != AST_NODE_INDEX_INVALID) {
        u32 SkipElse = State->RegionCount;

        RegionPush(State, FLV_REGION_KIND_JUMP);
        State->Regions[JumpIndex].Target = (u16) State->RegionCount;
        *Next = LowerStmt(State, *Next, LayoutNode);
        State->Regions[SkipElse].Target = (u16) State->RegionCount;
    } else {
        State->Regions[JumpIndex].Target = (u16) State->RegionCount;
    }
}

INTERNAL u32
LayoutAdd(LowerState* State, u32 Parent, ASTNodeScope* Scope, u32 Ordinal)
{
    u32 Index = (Parent == U32_MAX) 
        ? State->LayoutCount++ 
        : State->Layout[Parent].HeadChild + Ordinal;
    FLVLayoutNode* Node = &State->Layout[Index];

    ASSERT_ALWAYS(State->LayoutCount + Scope->Header.Count <= FLV_LAYOUT_MAX);

    Node->Parent = Parent;
    Node->HeadChild = State->LayoutCount;
    Node->ChildCount = Scope->Header.Count;
    Node->Depth = Scope->Depth;
    Node->Name = Scope->Name;
    Node->Ordinal = Ordinal;
    Node->AnalysisIndex = U32_MAX;
    State->LayoutCount += Scope->Header.Count;

    return Index;
}

INTERNAL u32
LowerScope(LowerState* State, ASTNodeScope* Scope, u32 Offset, u32 Parent)
{
    if (Scope->Header.Variant & SCOPE_FLAG_IS_IMPORTED)
        return StmtNext(State->Tree, Offset);

    u32 Ordinal = (Parent == U32_MAX) ? 0 : State->ChildCursor[Parent]++;
    u32 Node = LayoutAdd(State, Parent, Scope, Ordinal);
    b32 IsAnalysis = !!(Scope->Header.Variant & SCOPE_FLAG_IS_ANALYSIS);
    u32 SavedDeclCount = State->DeclCount;
    u32 Next = Offset + sizeof(ASTNodeScope);

    if (IsAnalysis)
        ProgramBegin(State);

    for (u32 Index = 0; Index < Scope->StmtCount; ++Index)
        Next = LowerStmt(State, Next, Node);

    if (IsAnalysis)
        State->Layout[Node].AnalysisIndex = ProgramEnd(State);

    State->DeclCount = SavedDeclCount;

    return Next;
}

INTERNAL u32
LowerStmt(LowerState* State, u32 Offset, u32 LayoutNode)
{
    AST* Tree = State->Tree;
    ASTNodeHeader* Header = (ASTNodeHeader*) (Tree->Stmts + Offset);
    u32 Next = Offset;

    switch (Header->Type) {
        default: {
            Next = StmtNext(Tree, Offset);
        } break;

        case AST_STATEMENT_LIST: {
            ASTNodeStmtList* List = (ASTNodeStmtList*) Header;

            Next += sizeof(ASTNodeStmtList);

            for (u32 Index = 0; Index < List->Count; ++Index)
                Next = LowerStmt(State, Next, LayoutNode);
        } break;

        case AST_SCOPE: {
            Next = LowerScope(
                State, 
                (ASTNodeScope*) Header, 
                Offset, 
                LayoutNode
            );
        } break;

        case AST_IF: {
            ASTNodeIf* If = (ASTNodeIf*) Header;

            if (If->Header.Variant == IF_VARIANT_FILTER) {
                if (If->Else.V != AST_NODE_INDEX_INVALID) {
                    LOWER_ERROR(
                        State, 
                        Header, 
                        "else on a filter if (series) is not supported in version 1"
                    );
                }

                FilterPush(State, If->Cond);
                Next = LowerStmt(
                    State, 
                    Next + sizeof(ASTNodeIf), 
                    LayoutNode
                );
                FilterPop(State);

                if (If->Else.V != AST_NODE_INDEX_INVALID)
                    Next = StmtNext(Tree, Next);
            } else {
                LowerBranch(State, If, Offset, LayoutNode, &Next);
            }
        } break;

        case AST_DECL_STATEMENT: {
            ASTNodeDeclStmt* Stmt = (ASTNodeDeclStmt*) Header;
            ASTNodeDecl* Decl = (ASTNodeDecl*) (Tree->Decls + Stmt->Decl.V);
            ASTNodeDeclValued* Valued = (ASTNodeDeclValued*) (
                Tree->Decls + Decl->FirstDecl.V
            );
            ASTNodeDeclIdent* Ident = (ASTNodeDeclIdent*) (
                Tree->Decls + Valued->Child.V
            );

            ASSERT_ALWAYS(State->DeclCount < FLV_LOWER_VAR_MAX);

            State->Decls[State->DeclCount].Bind = Ident->Bind;
            State->Decls[State->DeclCount].Depth = State->FilterDepth;
            ++State->DeclCount;
            Next = StmtNext(Tree, Offset);
        } break;

        case AST_DISPLAY: {
            if (State->InAnalysis)
                LowerDisplay(State, (ASTNodeDisplay*) Header);

            Next = StmtNext(Tree, Offset);
        } break;
    }

    return Next;
}

FLVLowered
FLVLower(
    Arena* MemPool, 
    AST* Tree, 
    FLVSchema const* Schema, 
    FLVSlot const* Slots, 
    u32 SlotCount
) {
    TempArena Scratch = GetScratch(&MemPool, 1);
    LowerState* State = ArenaPushStruct(Scratch.MemPool, LowerState);
    FLVLowered Result = {};

    MemZero(State, sizeof(*State));
    State->Tree = Tree;
    State->Schema = Schema;
    State->Slots = Slots;
    State->SlotCount = SlotCount;
    State->MemPool = MemPool;
    State->Scratch = Scratch.MemPool;
    State->Programs = ArenaPushArray(
        Scratch.MemPool, 
        FLVProgram, 
        FLV_PROGRAM_MAX
    );
    State->Layout = ArenaPushArray(
        Scratch.MemPool, 
        FLVLayoutNode, 
        FLV_LAYOUT_MAX
    );
    State->ChildCursor = ArenaPushArrayZero(
        Scratch.MemPool, 
        u32, 
        FLV_LAYOUT_MAX
    );
    LowerScope(
        State, 
        (ASTNodeScope*) (Tree->Stmts + Tree->Root.V), 
        Tree->Root.V, 
        U32_MAX
    );
    Result.Program = ArenaPushArray(
        MemPool, 
        FLVProgram, 
        State->ProgramCount
    );
    Result.Layout = ArenaPushArray(
        MemPool, 
        FLVLayoutNode, 
        State->LayoutCount
    );
    MemCpy(
        Result.Program, 
        State->Programs, 
        State->ProgramCount * sizeof(FLVProgram)
    );
    MemCpy(
        Result.Layout, 
        State->Layout, 
        State->LayoutCount * sizeof(FLVLayoutNode)
    );
    Result.ProgramCount = State->ProgramCount;
    Result.LayoutCount = State->LayoutCount;
    Result.HasError = State->HasError;

    ReleaseScratch(Scratch);

    return Result;
}

INTERNAL Str8
OperandStr(
    Arena* MemPool, 
    FLVProgram const* Program, 
    FLVSlot const* Slots, 
    FLVOperand Op
) {
    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;
    u32 Index = FLVGetOperandIndex(Op);

    switch (FLVGetOperandKind(Op)) {
        default: {} break;

        case FLV_OPERAND_KIND_TEMP: {
            return ArenaPushStrFmt(MemPool, "t%u", Index);
        } break;

        case FLV_OPERAND_KIND_SLOT: {
            FLVSlot const* Slot = &Slots[Index];
            Str8 Message = (Slot->Message.V != NAME_INDEX_INVALID_VALUE) 
                ? GetName(Names, Slot->Message) 
                : "mode"_s8;
            Str8 Field = (Slot->Field.V != NAME_INDEX_INVALID_VALUE) 
                ? GetName(Names, Slot->Field) 
                : "*"_s8;

            if (Slot->Instance == FLV_INSTANCE_ANY) {
                return ArenaPushStrFmt(
                    MemPool, 
                    "slot%u<%S.%S>", 
                    Index, 
                    PRINT_STR(Message), 
                    PRINT_STR(Field)
                );
            }

            return ArenaPushStrFmt(
                MemPool, 
                "slot%u<%S[%u].%S>", 
                Index, 
                PRINT_STR(Message), 
                Slot->Instance, 
                PRINT_STR(Field)
            );
        } break;

        case FLV_OPERAND_KIND_CONST: {
            FLVConst const* Const = &Program->Consts[Index];

            if (Const->Kind == FLV_CONST_KIND_F64) {
                return ArenaPushStrFmt(
                    MemPool, 
                    "%f", 
                    Const->F64
                );
            }

            if (Const->Kind == FLV_CONST_KIND_TIME) {
                return ArenaPushStrFmt(
                    MemPool, 
                    "%lluus", 
                    Const->TimeUSecs
                );
            }

            return ArenaPushStrFmt(
                MemPool, 
                "%lld", 
                Const->I64
            );
        } break;
    }

    return "-"_s8;
}

Str8List
FLVProgramPrint(
    Arena* MemPool, 
    FLVProgram const* Program, 
    FLVSlot const* Slots
) {
    Str8List Result = {};

    ListPush(
        MemPool, 
        &Result, 
        ArenaPushStrFmt(
            MemPool, 
            "analysis %u: %u ops, %u regions, %u temps, %u consts", 
            Program->AnalysisIndex, 
            Program->OpCount, 
            Program->RegionCount, 
            Program->TempCount, 
            Program->ConstCount
        )
    );

    for (u32 RIndex = 0; RIndex < Program->RegionCount; ++RIndex) {
        FLVRegion const* Region = &Program->Regions[RIndex];

        switch (Region->Kind) {
            default: {} break;

            case FLV_REGION_KIND_RUN: {
                ListPush(
                    MemPool, 
                    &Result, 
                    ArenaPushStrFmt(
                        MemPool, 
                        "region %u: RUN base=%S ranges=%S mask=%S", 
                        RIndex, 
                        PRINT_STR(
                            OperandStr(
                                MemPool, 
                                Program, 
                                Slots, 
                                FLVOperandAlloc(
                                    FLV_OPERAND_KIND_SLOT, 
                                    Region->BaseSlot
                                )
                            )
                        ),
                        PRINT_STR(
                            OperandStr(
                                MemPool, 
                                Program, 
                                Slots, 
                                Region->RangeSelection
                            )
                        ),
                        PRINT_STR(
                            OperandStr(
                                MemPool, 
                                Program, 
                                Slots, 
                                Region->Mask
                            )
                        )
                    )
                );
            } break;

            case FLV_REGION_KIND_SCALAR: {
                ListPush(
                    MemPool, 
                    &Result, 
                    ArenaPushStrFmt(
                        MemPool, 
                        "region %u: SCALAR", 
                        RIndex
                    )
                );
            } break;

            case FLV_REGION_KIND_JUMP_IF_FALSE: {
                ListPush(
                    MemPool, 
                    &Result, 
                    ArenaPushStrFmt(
                        MemPool, 
                        "region %u: JUMP_IF_FALSE %S -> %u", 
                        RIndex, 
                        PRINT_STR(
                            OperandStr(
                                MemPool, 
                                Program, 
                                Slots, 
                                Region->Operand
                            )
                        ), 
                        Region->Target
                    )
                );
            } break;

            case FLV_REGION_KIND_JUMP: {
                ListPush(
                    MemPool, 
                    &Result, 
                    ArenaPushStrFmt(
                        MemPool, 
                        "region %u: JUMP -> %u", 
                        RIndex, 
                        Region->Target
                    )
                );
            } break;
        }

        for (
            u32 OIndex = Region->HeadOp; 
            OIndex < Region->HeadOp + Region->OpCount; 
            ++OIndex
        ) {
            FLVOp const* Op = &Program->Ops[OIndex];

            if (FLV_KERNEL_INFO_TABLE[Op->Kernel].Flags & FLV_KERNEL_FLAG_EMIT) {
                FLVEmit const* E = &Program->Emits[Op->RHS];

                ListPush(
                    MemPool, 
                    &Result, 
                    ArenaPushStrFmt(
                        MemPool, 
                        "    %S %S \"%S\" axis=%u", 
                        PRINT_STR(FLV_KERNEL_NAME_TABLE[Op->Kernel]), 
                        PRINT_STR(OperandStr(MemPool, Program, Slots, Op->LHS)),
                        PRINT_STR(Program->Strings[E->LabelStringIndex]),
                        E->Axis
                    )
                );
            } else {
                ListPush(
                    MemPool, 
                    &Result, 
                    ArenaPushStrFmt(
                        MemPool, 
                        "    %S = %S %S %S", 
                        PRINT_STR(OperandStr(MemPool, Program, Slots, Op->Dst)),
                        PRINT_STR(FLV_KERNEL_NAME_TABLE[Op->Kernel]), 
                        PRINT_STR(OperandStr(MemPool, Program, Slots, Op->LHS)),
                        PRINT_STR(OperandStr(MemPool, Program, Slots, Op->RHS))
                    )
                );
            }
        }
    }

    return Result;
}
