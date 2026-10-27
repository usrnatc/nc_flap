#include "nc_log.h"
#include "nc_memory.h"
#include "nc_print.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "flv_type_check.h"
#include "flv_ast.h"
#include "flv_type.h"
#include "flv_token.h"
#include "flv_symbol_table.h"
#include "flv_parser.h"
#include "flv_schema.h"
#include "flv_stdlib.h"

internal const u8 SCALAR_RANK[BUILTIN_b64 + 1] = {
     0,
     2, 
     5, 
     8, 
    11,
     3, 
     6, 
     9, 
    12,
    13, 
    14,
     1, 
     4,
     7,
    10
};

INTERNAL b32
IsScalar(TypeIndex Type)
{
    return (Type.V - 1U) < (u32) BUILTIN_b64;
}

INTERNAL b32
IsNumericScalar(TypeIndex Type)
{
    return (Type.V - 1U) < (u32) (BUILTIN_b8 - 1);
}

INTERNAL b32
IsBoolScalar(TypeIndex Type)
{
    return (Type.V - (u32) BUILTIN_b8) < (u32) (BUILTIN_b64 - BUILTIN_b8 + 1);
}

INTERNAL b32
IsUntyped(TypeIndex Type)
{
    return Type.V == TYPE_INDEX_INVALID_TYPE;
}

INTERNAL b32
IsVoid(TypeIndex Type)
{
    return Type.V == BUILTIN_void;
}

INTERNAL b32
IsKind(TypeIndex Type, TypeKind Kind)
{
    return (
        !IsUntyped(Type) && 
        TypeAt(Type)->Kind == Kind
    );
}

INTERNAL b32
IsSeries(TypeIndex Type)
{
    return IsKind(Type, TYPE_KIND_SERIES);
}

INTERNAL b32
IsSelection(TypeIndex Type)
{
    return IsKind(Type, TYPE_KIND_SELECTION);
}

INTERNAL b32
IsMessage(TypeIndex Type)
{
    return IsKind(Type, TYPE_KIND_MESSAGE);
}

INTERNAL b32
IsTimeLike(TypeIndex Type)
{
    return (
        IsKind(Type, TYPE_KIND_TIME) || 
        IsKind(Type, TYPE_KIND_DURATION)
    );
}

INTERNAL TypeIndex
SeriesElem(TypeIndex Type)
{
    TypeIndex Result = {
        TypeAt(Type)->A
    };

    return Result;
}

INTERNAL TypeIndex
BaseOf(TypeIndex Type)
{
    TypeIndex Result = {
        TypeAt(Type)->B
    };

    return Result;
}

INTERNAL b32
IsBoolSeries(TypeIndex Type)
{
    return (
        IsSeries(Type) && 
        IsBoolScalar(SeriesElem(Type))
    );
}

INTERNAL b32
IsNumericSeries(TypeIndex Type)
{
    return (
        IsSeries(Type) && 
        IsNumericScalar(SeriesElem(Type))
    );
}

INTERNAL b32
IsStringSeries(TypeIndex Type)
{
    return (
        IsSeries(Type) && 
        IsKind(SeriesElem(Type), TYPE_KIND_STRING)
    );
}

INTERNAL TypeIndex
BoolSeries(TypeIndex Base)
{
    TypeIndex Elem = {
        CHECK_BOOL_TYPE
    };

    return TypeSeries(Elem, Base);
}

INTERNAL NameIndex
MessageName(TypeIndex Message)
{
    NameIndex Result = {
        TypeAt(Message)->A
    };

    return Result;
}

INTERNAL u32
MessageInstance(TypeIndex Message)
{
    u16 Count = TypeAt(Message)->Count;

    return Count 
        ? (u32) (Count - 1) 
        : FLV_INSTANCE_ANY;
}

INTERNAL Str8
NameStr(NameIndex Name)
{
    return GetName(GetTLS()->SYMBOL_TABLE->Names, Name);
}

INTERNAL u32
ResolvedHash(ExprIndex Expr)
{
    return (u32) Expr.V * 2654435761U;
}

BindIndex
ASTResolvedFind(AST *Tree, ExprIndex Expr)
{
    BindIndex Result = {
        BINDING_INVALID_VALUE
    };

    if (Tree->ResolvedMask) {
        u32 ExprHash = ResolvedHash(Expr);
        u32 SlotIndex = ExprHash & Tree->ResolvedMask;

        while (Tree->Resolved[SlotIndex].Expr.V) {
            if (Tree->Resolved[SlotIndex].Expr.V == Expr.V) {
                Result = Tree->Resolved[SlotIndex].Bind;
                break;
            }

            SlotIndex = (SlotIndex + 1) & Tree->ResolvedMask;
        }
    }

    return Result;
}

internal void
ResolvedInsert(AST* Tree, ExprIndex Expr, BindIndex Bind)
{
    ASSERT(Expr.V != CHECK_EXPR_NONE);

    if ((Tree->ResolvedCount + 1) * 2 > Tree->ResolvedMask + 1) {
        u32 OldMask = Tree->ResolvedMask;
        ASTResolved* Old = Tree->Resolved;
        u32 NewMask = OldMask ? (OldMask * 2 + 1) : 63;

        Tree->Resolved = ArenaPushArrayZero(
            Tree->MemPool,
            ASTResolved,
            NewMask + 1
        );
        Tree->ResolvedMask = NewMask;

        for (u32 Index = 0; Old && Index <= OldMask; ++Index) {
            if (!Old[Index].Expr.V)
                continue;

            u32 OldExprHash = ResolvedHash(Old[Index].Expr);
            u32 SlotIndex = OldExprHash & NewMask;

            while (Tree->Resolved[SlotIndex].Expr.V)
                SlotIndex = (SlotIndex + 1) & NewMask;

            Tree->Resolved[SlotIndex] = Old[Index];
        }
    }

    u32 ExprHash = ResolvedHash(Expr);
    u32 SlotIndex = ExprHash & Tree->ResolvedMask;

    while (
        Tree->Resolved[SlotIndex].Expr.V && 
        Tree->Resolved[SlotIndex].Expr.V != Expr.V
    ) {
        SlotIndex = (SlotIndex + 1) & Tree->ResolvedMask;
    }

    if (!Tree->Resolved[SlotIndex].Expr.V)
        ++Tree->ResolvedCount;

    Tree->Resolved[SlotIndex].Expr = Expr;
    Tree->Resolved[SlotIndex].Bind = Bind;
}

INTERNAL ASTNodeExprAccel*
ExprAt(AST* Tree, ExprIndex Index)
{
    return (ASTNodeExprAccel*) (Tree->Exprs + Index.V);
}

INTERNAL ExprIndex
ImplicitChild(AST* Tree, void* Node, u32 NodeSize)
{
    ExprIndex Result = {
        (i32) (((u8*) Node + NodeSize) - Tree->Exprs)
    };

    return Result;
}

INTERNAL TypeIndex
TypeOf(AST* Tree, ExprIndex Index)
{
    ASTNodeExprAccel* Node = ExprAt(Tree, Index);
    TypeIndex Result = {
        BUILTIN_void
    };

    if (ASTExprIsTyped(Node->Header.Type))
        Result = Node->Type;

    return Result;
}

INTERNAL AdoptFrame*
AdoptPush(Arena* MemPool, AdoptFrame* Head, ExprIndex Expr)
{
    AdoptFrame* Result = ArenaPushStruct(MemPool, AdoptFrame);

    Result->Prev = Head;
    Result->Expr = Expr;

    return Result;
}

internal void
Adopt(AST* Tree, ExprIndex Root, TypeIndex TIndex)
{
    TempArena Scratch = GetScratch(NULL, 0);
    AdoptFrame* Head = NULL;
    ExprIndex EIndex = Root;

    for (;;) {
        ASTNodeExprAccel* Node = ExprAt(Tree, EIndex);
        ExprIndex Next = {
            CHECK_EXPR_NONE
        };

        if (ASTExprIsTyped(Node->Header.Type) && IsUntyped(Node->Type)) {
            Node->Type = TIndex;

            switch (Node->Header.Type) {
                default: {} break;

                case AST_UNARY_EXPR: {
                    Next = ((ASTNodeUnary*) Node)->Expr;
                } break;

                case AST_BINARY_EXPR: {
                    Head = AdoptPush(
                        Scratch.MemPool,
                        Head,
                        ((ASTNodeBinary*) Node)->Left
                    );
                    Next = ImplicitChild(Tree, Node, sizeof(ASTNodeBinary));
                } break;

                case AST_TERNARY_EXPR: {
                    Head = AdoptPush(
                        Scratch.MemPool,
                        Head,
                        ((ASTNodeTernary*) Node)->Then
                    );
                    Next = ImplicitChild(Tree, Node, sizeof(ASTNodeTernary));
                } break;
            }
        }

        if (Next.V != CHECK_EXPR_NONE) {
            EIndex = Next;
        } else if (Head) {
            EIndex = Head->Expr;
            Head = Head->Prev;
        } else {
            break;
        }
    }

    ReleaseScratch(Scratch);
}

INTERNAL TypeIndex
Defaulted(TypeIndex TIndex)
{
    TypeIndex Result = {
        CHECK_DEFAULT_INT_TYPE
    };

    if (!IsUntyped(TIndex))
        Result = TIndex;

    return Result;
}

INTERNAL b32
IsConvertible(TypeIndex From, TypeIndex To)
{
    if (
        From.V == To.V || 
        IsUntyped(From) || 
        IsVoid(From) || 
        IsVoid(To)
    ) {
        return TRUE;
    }

    return IsScalar(From) && IsScalar(To);
}

internal TypeIndex
Promote(TypeIndex LHS, TypeIndex RHS)
{
    TypeIndex Result = {
        BUILTIN_void
    };

    if (IsUntyped(LHS)) {
        Result = RHS;
    } else if (IsUntyped(RHS)) {
        Result = LHS;
    } else if (LHS.V == RHS.V) {
        Result = LHS;
    } else if (
        IsVoid(LHS) ||
        IsVoid(RHS)
    ) {
        Result.V = BUILTIN_void;
    } else if (IsScalar(LHS) && IsScalar(RHS)) {
        if (SCALAR_RANK[LHS.V] >= SCALAR_RANK[RHS.V])
            Result = LHS;
        else
            Result = RHS;
    }

    return Result;
}

INTERNAL Str8
NearestMessageName(CheckState* State, Str8 Name)
{
    Str8 Result = {};

    for (u32 Index = 0; Index < State->Schema->MessageCount; ++Index) {
        Str8 Candidate = NameStr(State->Schema->Messages[Index].Name);

        if (StrMatch(Candidate, Name, STR_MATCH_ALL_CASES)) {
            Result = Candidate;
            break;
        }
    }

    return Result;
}

INTERNAL Str8
NearestFieldName(CheckState* State, u32 Message, Str8 Name)
{
    FLVSchemaMessage const* M = &State->Schema->Messages[Message];
    Str8 Result = {};

    for (u32 Index = 0; Index < M->FieldCount; ++Index) {
        Str8 Candidate = NameStr(State->Schema->Fields[M->HeadField + Index].Name);

        if (StrMatch(Candidate, Name, STR_MATCH_ALL_CASES)) {
            Result = Candidate;
            break;
        }
    }

    return Result;
}

INTERNAL u32
SlotFind(
    CheckState* State, 
    FLVSlotKind Kind, 
    NameIndex Message, 
    NameIndex Field, 
    u32 Instance
) {
    for (u32 Index = 0; Index < State->SlotsCount; ++Index) {
        FLVSlot* Slot = &State->Slots[Index];

        if (
            Slot->Kind == Kind &&
            Slot->Message.V == Message.V &&
            Slot->Field.V == Field.V &&
            Slot->Instance == Instance
        ) {
            return Index;
        }
    }

    ASSERT_ALWAYS(State->SlotsCount < CHECK_SLOT_MAX);

    FLVSlot* Slot = &State->Slots[State->SlotsCount];

    Slot->Message = Message;
    Slot->Field = Field;
    Slot->Instance = Instance;
    Slot->Kind = Kind;

    return State->SlotsCount++;
}

internal BindIndex
ResolveCall(
    AST* Tree,
    BindIndex Head,
    ExprIndex const* Expressions,
    i32 Step,
    TypeIndex const* Types,
    u32 Count,
    OUT TypeIndex* Return
) {
    BindIndex Result = {
        BINDING_INVALID_VALUE
    };
    TypeIndex CandidateField = {
        TYPE_INDEX_INVALID_TYPE
    };
    TypeIndex CandidateBound[TYPE_GENERIC_MAX] = {};
    u32 CandidateScore = 0;
    b32 CandidateGeneric = FALSE;

    for (BindIndex Candidate = Head; Candidate.V != BINDING_INVALID_VALUE; ) {
        Binding* B = &GetTLS()->SYMBOL_TABLE->Bindings[Candidate.V];
        TypeIndex F = B->Type;
        BindIndex CurrentCandidate = Candidate;

        Candidate = B->Overload;

        if (
            IsUntyped(F) ||
            TypeAt(F)->Kind != TYPE_KIND_FUNC ||
            TypeAt(F)->Count != Count
        ) {
            continue;
        }

        TypeIndex const* Sig = TypeArgs(TypeAt(F));
        b32 IsGeneric = !!(TypeAt(F)->Flags & TYPE_FLAG_GENERIC);
        TypeIndex Bound[TYPE_GENERIC_MAX] = {};
        u32 Score = 0;
        b32 IsOkay = TRUE;

        if (IsGeneric) {
            for (u32 Index = 0; Index < TYPE_GENERIC_MAX; ++Index)
                Bound[Index].V = TYPE_INDEX_INVALID_TYPE;
        }

        for (u32 Index = 0; Index < Count && IsOkay; ++Index) {
            TypeIndex P = Sig[Index + 1];
            TypeIndex A = Types
                ? Types[Index]
                : TypeOf(Tree, Expressions[(i32) Index * Step]);

            if (TypeAt(P)->Flags & TYPE_FLAG_GENERIC) {
                IsOkay = TypeUnify(P, A, Bound, TYPE_GENERIC_MAX);
                Score += !IsUntyped(A) + (TypeAt(P)->Kind != TYPE_KIND_PARAM);
            } else if (P.V == A.V) {
                ++Score;
            } else {
                IsOkay = IsConvertible(A, P);
            }
        }

        if (
            IsOkay && 
            (
                Result.V == BINDING_INVALID_VALUE || 
                Score >= CandidateScore
            )
        ) {
            Result = CurrentCandidate;
            CandidateField = F;
            CandidateScore = Score;
            CandidateGeneric = IsGeneric;

            if (IsGeneric)
                MemCpy(CandidateBound, Bound, sizeof(Bound));
        }
    }

    if (Result.V != BINDING_INVALID_VALUE) {
        TypeIndex const* Sig = TypeArgs(TypeAt(CandidateField));

        for (u32 Index = 0; Index < Count; ++Index) {
            TypeIndex P = Sig[Index + 1];
            ExprIndex EIndex = Expressions[(i32) Index * Step];

            if (CandidateGeneric)
                P = TypeSub(P, CandidateBound, TYPE_GENERIC_MAX);

            if (EIndex.V != CHECK_EXPR_NONE)
                Adopt(Tree, EIndex, P);
        }

        if (CandidateGeneric)
            *Return = TypeSub(Sig[0], CandidateBound, TYPE_GENERIC_MAX);
        else
            *Return = Sig[0];
    }

    return Result;
}

INTERNAL void
CheckIdent(CheckState* State, ASTNodeExprIdent* Ident)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Ident;

    if (Ident->ScopeBase.V != CHECK_EXPR_NONE) {
        TypeIndex Base = TypeOf(Tree, Ident->ScopeBase);

        if (IsVoid(Base)) {
            Node->Type.V = BUILTIN_void;
            return;
        }

        if (IsMessage(Base)) {
            u32 Message = FLVSchemaFindMessage(State->Schema, MessageName(Base));
            u32 Field = FLVSchemaFindField(State->Schema, Message, Ident->Name);

            if (Field != FLV_SCHEMA_NOT_FOUND) {
                Node->Type = TypeSeries(State->Schema->Fields[Field].Element, Base);
                Ident->Slot = SlotFind(
                    State, 
                    FLV_SLOT_KIND_FIELD, 
                    MessageName(Base), 
                    Ident->Name, 
                    MessageInstance(Base)
                );
                return;
            }
        }
    }

    NameIndex NoName = {
        NAME_INDEX_INVALID_VALUE
    };

    if (Ident->Bind.V != BINDING_INVALID_VALUE) {
        Binding* B = &GetTLS()->SYMBOL_TABLE->Bindings[Ident->Bind.V];

        if (B->Kind != BINDING_KIND_VALUE && B->Kind != BINDING_KIND_FUNC) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "type name used as a value"
            );
            Node->Type.V = BUILTIN_void;
        } else if (IsUntyped(B->Type)) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "type of \"%S\" is not known at this point", 
                PRINT_STR(NameStr(Ident->Name))
            );
            Node->Type.V = BUILTIN_void;
        } else {
            Node->Type = B->Type;

            if (IsKind(B->Type, TYPE_KIND_MODE)) {
                Ident->Slot = SlotFind(
                    State, 
                    FLV_SLOT_KIND_MODE, 
                    NoName, 
                    Ident->Name, 
                    FLV_INSTANCE_ANY
                );
            }
        }

        return;
    }

    u32 Message = FLVSchemaFindMessage(State->Schema, Ident->Name);

    if (Message != FLV_SCHEMA_NOT_FOUND) {
        Node->Type = TypeMessage(Ident->Name, 0);
        Ident->Slot = SlotFind(
            State, 
            FLV_SLOT_KIND_MESSAGE, 
            Ident->Name, 
            NoName, 
            FLV_INSTANCE_ANY
        );

        return;
    }

    if (State->Schema->MessageCount) {
        Str8 Name = NameStr(Ident->Name);
        Str8 Nearest = NearestMessageName(State, Name);

        if (Nearest.Size) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "unknown name \"%S\"; did you mean \"%S\"?", 
                PRINT_STR(Name), 
                PRINT_STR(Nearest)
            );
        } else {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "unknown name \"%S\"", 
                PRINT_STR(Name)
            );
        }
    }

    Node->Type.V = BUILTIN_void;
}

INTERNAL void
CheckFieldAccess(CheckState* State, ASTNodeFieldAccess* Access)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Access;
    ExprIndex Base = ImplicitChild(Tree, Node, sizeof(ASTNodeFieldAccess));
    TypeIndex Type = TypeOf(Tree, Base);

    if (IsVoid(Type)) {
        Node->Type = Type;
        return;
    }

    if (!IsMessage(Type)) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "field access on something that is not a message type"
        );
        Node->Type.V = BUILTIN_void;

        return;
    }

    u32 Message = FLVSchemaFindMessage(State->Schema, MessageName(Type));

    ASSERT(Message != FLV_SCHEMA_NOT_FOUND);

    u32 Field = FLVSchemaFindField(State->Schema, Message, Access->Name);

    if (Field == FLV_SCHEMA_NOT_FOUND) {
        Str8 Name = NameStr(Access->Name);
        Str8 Nearest = NearestFieldName(State, Message, Name);

        if (Nearest.Size) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "%S has no field \"%S\"; did you mean \"%S\"?", 
                PRINT_STR(NameStr(MessageName(Type))), 
                PRINT_STR(Name), 
                PRINT_STR(Nearest)
            );
        } else {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "%S has no field \"%S\"", 
                PRINT_STR(NameStr(MessageName(Type))), 
                PRINT_STR(Name)
            );
        }

        Node->Type.V = BUILTIN_void;

        return;
    }

    Node->Type = TypeSeries(State->Schema->Fields[Field].Element, Type);
    Access->Slot = SlotFind(
        State, 
        FLV_SLOT_KIND_FIELD, 
        MessageName(Type), 
        Access->Name, 
        MessageInstance(Type)
    );
}

INTERNAL void
CheckUnary(CheckState* State, ASTNodeUnary* Unary)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Unary;
    TypeIndex Type = TypeOf(Tree, Unary->Expr);

    if (IsVoid(Type)) {
        Node->Type = Type;

        return;
    }

    switch (Node->Header.Variant) {
        default: {
            ASSERT(FALSE);
        } break;

        case TOKEN_KIND_MINUS: {
            if (
                IsUntyped(Type) || 
                IsNumericScalar(Type) || 
                IsNumericSeries(Type)
            ) {
                Node->Type = Type;
            } else {
                CHECK_ERROR(
                    State, 
                    &Node->Header, 
                    "unary - needs a number or a numeric series"
                );
                Node->Type.V = BUILTIN_void;
            }
        } break;

        case TOKEN_KIND_EXCLAIM: {
            if (IsSelection(Type) || IsBoolSeries(Type)) {
                Node->Type = Type;
            } else if (IsUntyped(Type) || IsScalar(Type)) {
                Adopt(Tree, Unary->Expr, Defaulted(Type));
                Node->Type.V = CHECK_BOOL_TYPE;
            } else {
                CHECK_ERROR(
                    State, 
                    &Node->Header, 
                    "! needs a boolean series, a selection or a scalar"
                );
                Node->Type.V = BUILTIN_void;
            }
        } break;
    }
}

INTERNAL void
CheckBinary(CheckState* State, ASTNodeBinary* Binary)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Binary;
    ExprIndex Left = Binary->Left;
    ExprIndex Right = ImplicitChild(Tree, Node, sizeof(ASTNodeBinary));
    TypeIndex LHS = TypeOf(Tree, Left);
    TypeIndex RHS = TypeOf(Tree, Right);
    u8 Op = Node->Header.Variant;

    if (Op == TOKEN_KIND_RANGE) {
        if (
            !(IsVoid(LHS) || IsTimeLike(LHS)) ||
            !(IsVoid(RHS) || IsTimeLike(RHS))
        ) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "range bounds must be times with a unit (1s, 250ms, 2m)"
            );
        }

        Node->Type = TypeTime();

        return;
    }

    if (IsVoid(LHS) || IsVoid(RHS)) {
        Node->Type.V = BUILTIN_void;
        return;
    }

    if (IsMessage(LHS) || IsMessage(RHS)) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "a message type is not a value; use a field (MSG.Field)"
        );
        Node->Type.V = BUILTIN_void;

        return;
    }

    if (IsStringSeries(LHS) || IsStringSeries(RHS)) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "string fields cannot be used in expressions"
        );
        Node->Type.V = BUILTIN_void;

        return;
    }

    b32 LSel = IsSelection(LHS);
    b32 RSel = IsSelection(RHS);

    if (LSel || RSel) {
        if (Op != TOKEN_KIND_BIT_AND && Op != TOKEN_KIND_BIT_OR) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "only & and | apply to a selection"
            );
            Node->Type.V = BUILTIN_void;
        } else if (
            (!LSel && !IsBoolSeries(LHS)) || 
            (!RSel && !IsBoolSeries(RHS))
        ) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "& and | need selections or boolean series on both sides"
            );
            Node->Type.V = BUILTIN_void;
        } else {
            Node->Type = LSel ? LHS : RHS;
        }

        return;
    }

    b32 LSer = IsSeries(LHS);
    b32 RSer = IsSeries(RHS);

    if (!LSer && !RSer) {
        switch (Op) {
            case TOKEN_KIND_LESS:
            case TOKEN_KIND_GREATER:
            case TOKEN_KIND_LESS_EQ:
            case TOKEN_KIND_GREATER_EQ:
            case TOKEN_KIND_EQ_EQ:
            case TOKEN_KIND_EXCLAIM_EQ: {
                TypeIndex Common = Defaulted(Promote(LHS, RHS));

                if (IsVoid(Common)) {
                    CHECK_ERROR(
                        State, 
                        &Node->Header, 
                        "cannot compare these types"
                    );
                } else {
                    Adopt(Tree, Left, Common);
                    Adopt(Tree, Right, Common);
                }

                Node->Type.V = CHECK_BOOL_TYPE;
            } break;

            case TOKEN_KIND_LOG_AND:
            case TOKEN_KIND_LOG_OR: {
                Adopt(Tree, Left, Defaulted(LHS));
                Adopt(Tree, Right, Defaulted(RHS));
                Node->Type.V = CHECK_BOOL_TYPE;
            } break;

            default: {
                TypeIndex Common = Promote(LHS, RHS);

                if (IsVoid(Common)) {
                    CHECK_ERROR(
                        State, 
                        &Node->Header, 
                        "operands have incompatible types"
                    );
                } else if (!IsUntyped(Common)) {
                    Adopt(Tree, Left, Common);
                    Adopt(Tree, Right, Common);
                }

                Node->Type = Common;
            } break;
        }

        return;
    }

    if (LSer && RSer && BaseOf(LHS).V != BaseOf(RHS).V) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "%S and %S are different streams; align is not implemented yet",
            PRINT_STR(NameStr(MessageName(BaseOf(LHS)))),
            PRINT_STR(NameStr(MessageName(BaseOf(RHS))))
        );
        Node->Type.V = BUILTIN_void;
        return;
    }

    TypeIndex Base = LSer 
        ? BaseOf(LHS) 
        : BaseOf(RHS);
    TypeIndex LE = LSer 
        ? SeriesElem(LHS) 
        : LHS;
    TypeIndex RE = RSer 
        ? SeriesElem(RHS) 
        : RHS;

    switch (Op) {
        default: {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "this operator is not defined on a series"
            );
            Node->Type.V = BUILTIN_void;
        } break;

        case TOKEN_KIND_LESS:
        case TOKEN_KIND_GREATER:
        case TOKEN_KIND_LESS_EQ:
        case TOKEN_KIND_GREATER_EQ:
        case TOKEN_KIND_EQ_EQ:
        case TOKEN_KIND_EXCLAIM_EQ: {
            TypeIndex Common = Defaulted(Promote(LE, RE));

            if (IsVoid(Common)) {
                CHECK_ERROR(
                    State, 
                    &Node->Header, 
                    "cannot compare these types"
                );
                Node->Type.V = BUILTIN_void;
            } else {
                if (!LSer)
                    Adopt(Tree, Left, Common);

                if (!RSer)
                    Adopt(Tree, Right, Common);

                Node->Type = BoolSeries(Base);
            }
        } break;

        case TOKEN_KIND_LOG_AND:
        case TOKEN_KIND_LOG_OR:
        case TOKEN_KIND_BIT_AND:
        case TOKEN_KIND_BIT_OR: {
            if (!LSer || !RSer) {
                CHECK_ERROR(
                    State, 
                    &Node->Header, 
                    "a scalar on one side of && || & | with a series is not supported in version 1"
                );
                Node->Type.V = BUILTIN_void;
            } else if (!IsBoolScalar(LE) || !IsBoolScalar(RE)) {
                CHECK_ERROR(
                    State, 
                    &Node->Header, 
                    "&& || & | need boolean series"
                );
                Node->Type.V = BUILTIN_void;
            } else {
                Node->Type = BoolSeries(Base);
            }
        } break;

        case TOKEN_KIND_PLUS:
        case TOKEN_KIND_MINUS:
        case TOKEN_KIND_STAR:
        case TOKEN_KIND_SLASH:
        case TOKEN_KIND_PERCENT: {
            b32 LOkay = IsUntyped(LE) || IsNumericScalar(LE);
            b32 ROkay = IsUntyped(RE) || IsNumericScalar(RE);

            if (!LOkay || !ROkay) {
                CHECK_ERROR(
                    State, 
                    &Node->Header, 
                    "arithmetic needs numeric series or numbers"
                );
                Node->Type.V = BUILTIN_void;
            } else {
                TypeIndex Common = Defaulted(Promote(LE, RE));

                if (!LSer)
                    Adopt(Tree, Left, Common);

                if (!RSer)
                    Adopt(Tree, Right, Common);

                Node->Type = TypeSeries(Common, Base);
            }
        } break;
    }
}

INTERNAL void
CheckAssignment(CheckState* State, ASTNodeBinary* Binary)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Binary;
    ExprIndex Left = Binary->Left;
    ExprIndex Right = ImplicitChild(Tree, Node, sizeof(ASTNodeBinary));
    ASTNodeExprAccel* LeftNode = ExprAt(Tree, Left);
    TypeIndex LHS = TypeOf(Tree, Left);
    TypeIndex RHS = TypeOf(Tree, Right);
    b32 IsVar = FALSE;

    if (LeftNode->Header.Type == AST_IDENT) {
        ASTNodeExprIdent* Ident = (ASTNodeExprIdent*) LeftNode;

        if (
            Ident->Bind.V != BINDING_INVALID_VALUE && 
            Ident->Slot == U32_MAX
        ) {
            Binding* B = &GetTLS()->SYMBOL_TABLE->Bindings[Ident->Bind.V];

            IsVar = (
                B->Kind == BINDING_KIND_VALUE && 
                B->Decl.V != AST_NODE_INDEX_INVALID
            );
        }
    }

    if (!IsVar) {
        CHECK_ERROR(State, &Node->Header, "can only assign to a var");
        Node->Type.V = BUILTIN_void;
        return;
    }

    if (!IsVoid(RHS)) {
        if (IsScalar(LHS))
            Adopt(Tree, Right, LHS);

        if (!IsConvertible(RHS, LHS))
            CHECK_ERROR(State, &Node->Header, "cannot convert in assignment");
    }

    Node->Type = LHS;
}

INTERNAL void
CheckTernary(CheckState* State, ASTNodeTernary* Ternary)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Ternary;
    ExprIndex Else = ImplicitChild(Tree, Node, sizeof(ASTNodeTernary));
    TypeIndex Cond = TypeOf(Tree, Ternary->Cond);
    TypeIndex Then = TypeOf(Tree, Ternary->Then);
    TypeIndex ElseT = TypeOf(Tree, Else);

    UNUSED(Cond);
    UNUSED(Then);
    UNUSED(ElseT);

    CHECK_ERROR(
        State, 
        &Node->Header, 
        "the ternary operator is not supported in version 1"
    );
    Node->Type.V = BUILTIN_void;
}

INTERNAL void
CheckIndex(CheckState* State, ASTNodeBinary* Binary)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Binary;
    ExprIndex Base = Binary->Left;
    ExprIndex Index = ImplicitChild(Tree, Node, sizeof(ASTNodeBinary));
    ASTNodeExprAccel* IndexNode = ExprAt(Tree, Index);
    TypeIndex BaseType = TypeOf(Tree, Base);
    TypeIndex IndexType = TypeOf(Tree, Index);
    b32 IsRange = (
        IndexNode->Header.Type == AST_BINARY_EXPR &&
        IndexNode->Header.Variant == TOKEN_KIND_RANGE
    );

    if (IsVoid(BaseType) || IsVoid(IndexType)) {
        Node->Type.V = BUILTIN_void;
        return;
    }

    if (!IsMessage(BaseType) && !IsSeries(BaseType)) {
        CHECK_ERROR(State, &Node->Header, "cannot index this type");
        Node->Type.V = BUILTIN_void;
        return;
    }

    if (IsRange) {
        Node->Header.Variant = INDEX_KIND_TIME;
        Node->Type = BaseType;
    } else if (IsBoolSeries(IndexType)) {
        Node->Header.Variant = INDEX_KIND_CONDITION;
        Node->Type = BaseType;
    } else if (IsSelection(IndexType)) {
        Node->Header.Variant = INDEX_KIND_SELECTION;
        Node->Type = BaseType;
    } else if (IsUntyped(IndexType)) {
        if (!IsMessage(BaseType)) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "instance select is only legal on a message type or a field"
            );
            Node->Type.V = BUILTIN_void;
        } else if (IndexNode->Header.Type != AST_LITERAL) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "an instance index must be an integer literal"
            );
            Node->Type.V = BUILTIN_void;
        } else if (TypeAt(BaseType)->Count) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "instance already selected"
            );
            Node->Type.V = BUILTIN_void;
        } else {
            TypeIndex I64 = {
                CHECK_DEFAULT_INT_TYPE
            };
            u64 Instance = ((ASTNodeLiteral*) IndexNode)->Val.Unsigned;

            Adopt(Tree, Index, I64);
            Node->Header.Variant = INDEX_KIND_INSTANCE;
            Node->Type = TypeMessage(
                MessageName(BaseType), 
                SafeTruncateU64(Instance + 1)
            );
        }
    } else {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "an index must be an instance, a time range, a boolean series or a selection"
        );
        Node->Type.V = BUILTIN_void;
    }
}

INTERNAL void
CheckCall(CheckState* State, ASTNodeFuncCall* Call, ExprIndex Self)
{
    AST* Tree = State->Tree;
    ASTNodeExprAccel* Node = (ASTNodeExprAccel*) Call;
    ASTNodeExprAccel* FuncNode = ExprAt(Tree, Call->Func);
    u32 ArgCount = Call->Header.Count;
    BindIndex Head = {
        BINDING_INVALID_VALUE
    };

    if (FuncNode->Header.Type == AST_IDENT)
        Head = ((ASTNodeExprIdent*) FuncNode)->Bind;

    if (
        Head.V == BINDING_INVALID_VALUE ||
        GetTLS()->SYMBOL_TABLE->Bindings[Head.V].Kind != BINDING_KIND_FUNC
    ) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "call of something that is not a function"
        );
        Node->Type.V = BUILTIN_void;
        return;
    }

    for (u32 Index = 0; Index < ArgCount; ++Index) {
        if (IsVoid(TypeOf(Tree, Call->Arg[Index]))) {
            Node->Type.V = BUILTIN_void;
            return;
        }
    }

    TypeIndex Return = {
        BUILTIN_void
    };
    BindIndex Chosen = ResolveCall(
        Tree,
        Head,
        Call->Arg + (ArgCount - 1),
        -1,
        NULL,
        ArgCount,
        &Return
    );
    Str8 FuncName = NameStr(((ASTNodeExprIdent*) FuncNode)->Name);

    if (Chosen.V == BINDING_INVALID_VALUE) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "no matching overload of %S for these arguments", 
            PRINT_STR(FuncName)
        );
        Node->Type.V = BUILTIN_void;

        return;
    }

    if (IsUntyped(Return)) {
        CHECK_ERROR(
            State, 
            &Node->Header, 
            "cannot infer the type of %S from its arguments", 
            PRINT_STR(FuncName)
        );
        Node->Type.V = BUILTIN_void;

        return;
    }

    FLVBuiltin const* Row = &FLV_STDLIB_TABLE[
        GetTLS()->SYMBOL_TABLE->Bindings[Chosen.V].Decl.V
    ];

    if (Row->AcceptsNumericOnly) {
        TypeIndex First = TypeOf(Tree, Call->Arg[ArgCount - 1]);
        b32 Okay = IsNumericSeries(First) || IsNumericScalar(First);

        if (!Okay) {
            CHECK_ERROR(
                State, 
                &Node->Header, 
                "%S needs a numeric series", 
                PRINT_STR(FuncName)
            );
            Node->Type.V = BUILTIN_void;

            return;
        }
    }

    ResolvedInsert(Tree, Self, Chosen);
    Node->Type = Return;
}

internal void
CheckExpr(CheckState* State, ASTNodeExprAccel* Node, ExprIndex Self)
{
    switch (Node->Header.Type) {
        default: {} break;

        case AST_LITERAL: {} break;

        case AST_IDENT: {
            CheckIdent(State, (ASTNodeExprIdent*) Node);
        } break;

        case AST_UNARY_EXPR: {
            CheckUnary(State, (ASTNodeUnary*) Node);
        } break;

        case AST_BINARY_EXPR: {
            CheckBinary(State, (ASTNodeBinary*) Node);
        } break;

        case AST_BINARY_ASSIGNMENT: {
            CheckAssignment(State, (ASTNodeBinary*) Node);
        } break;

        case AST_TERNARY_EXPR: {
            CheckTernary(State, (ASTNodeTernary*) Node);
        } break;

        case AST_INDEX: {
            CheckIndex(State, (ASTNodeBinary*) Node);
        } break;

        case AST_FUNC_CALL: {
            CheckCall(State, (ASTNodeFuncCall*) Node, Self);
        } break;

        case AST_FIELD_ACCESS: {
            CheckFieldAccess(State, (ASTNodeFieldAccess*) Node);
        } break;
    }
}

internal void
FinaliseBinding(CheckState* State, BindIndex Bind)
{
    AST* Tree = State->Tree;
    Binding* B = &GetTLS()->SYMBOL_TABLE->Bindings[Bind.V];
    ASTNodeDecl* Decl = (ASTNodeDecl*) (Tree->Decls + B->Decl.V);
    ASTNodeDeclValued* Valued = (ASTNodeDeclValued*) (Tree->Decls + Decl->FirstDecl.V);
    ASTNodeInitialiserExpr* Init = (ASTNodeInitialiserExpr*) (Tree->Inits + Valued->Init.V);
    TypeIndex Type = Defaulted(TypeOf(Tree, Init->Val));

    ASSERT(Valued->Header.Type == AST_DECLARATOR_VALUED);
    ASSERT(Init->Header.Type == AST_INITIALISER_EXPR);

    if (IsMessage(Type)) {
        CHECK_ERROR(
            State, 
            &Decl->Header, 
            "a var cannot hold a message type; use a field (MSG.Field)"
        );
        Type.V = BUILTIN_void;
    }

    Adopt(Tree, Init->Val, Type);
    B->Type = Type;
    Decl->Type = Type;
}

u32
StmtNext(AST* Tree, u32 Offset)
{
    ASTNodeHeader* Header = (ASTNodeHeader*) (Tree->Stmts + Offset);
    u32 Next = Offset;

    switch (Header->Type) {
        default: {
            ASSERT(FALSE);
        } break;

        case AST_STATEMENT_LIST: {
            ASTNodeStmtList* List = (ASTNodeStmtList*) Header;

            Next += sizeof(ASTNodeStmtList);

            for (u32 Index = 0; Index < List->Count; ++Index)
                Next = StmtNext(Tree, Next);
        } break;

        case AST_SCOPE: {
            ASTNodeScope* Scope = (ASTNodeScope*) Header;

            Next += sizeof(ASTNodeScope);

            for (u32 Index = 0; Index < Scope->StmtCount; ++Index)
                Next = StmtNext(Tree, Next);
        } break;

        case AST_IF: {
            ASTNodeIf* If = (ASTNodeIf*) Header;

            Next = StmtNext(Tree, Next + sizeof(ASTNodeIf));

            if (If->Else.V != AST_NODE_INDEX_INVALID) {
                ASSERT(If->Else.V == Next);
                Next = StmtNext(Tree, Next);
            }
        } break;

        case AST_DISPLAY: {
            Next += ALIGNTO(
                OFFSETOF(
                    ASTNodeDisplay, 
                    Arg
                ) + Header->Count * sizeof(ASTDisplayArg), 
                sizeof(u32)
            );
        } break;

        case AST_EXPR_STATEMENT: {
            Next += sizeof(ASTNodeExprStmt);
        } break;

        case AST_DECL_STATEMENT: {
            Next += sizeof(ASTNodeDeclStmt);
        } break;

        case AST_EMPTY: {
            Next += sizeof(ASTNodeHeader);
        } break;
    }

    return Next;
}

INTERNAL void
CheckDisplay(CheckState* State, ASTNodeDisplay* Display, u32* ScopeKinds)
{
    AST* Tree = State->Tree;
    u8 Kind = Display->Header.Variant;
    u32 ArgCount = Display->Header.Count;
    TypeIndex TableBase = {
        TYPE_INDEX_INVALID_TYPE
    };

    if (Kind != TOKEN_KIND_FLAG) {
        u32 Bit = 1U << (Kind - TOKEN_KIND_GRAPH);

        if (*ScopeKinds && !(*ScopeKinds & Bit)) {
            CHECK_ERROR(
                State, 
                &Display->Header, 
                "an analysis can hold only one of graph, table or text"
            );
        }

        *ScopeKinds |= Bit;
    }

    for (u32 Index = 0; Index < ArgCount; ++Index) {
        ASTDisplayArg* Arg = &Display->Arg[Index];
        ASTNodeHeader* ArgHeader = &ExprAt(Tree, Arg->Expr)->Header;
        TypeIndex Type = TypeOf(Tree, Arg->Expr);

        if (IsVoid(Type) && !Arg->IsLabel)
            continue;

        switch (Kind) {
            default: {} break;

            case TOKEN_KIND_GRAPH: {
                if (IsMessage(Type))
                    CHECK_ERROR(
                        State, 
                        ArgHeader, 
                        "graph needs a field (MSG.Field), not a message type"
                    );
                else if (IsStringSeries(Type))
                    CHECK_ERROR(
                        State, 
                        ArgHeader, 
                        "cannot graph a string field"
                    );
                else if (!IsSeries(Type))
                    CHECK_ERROR(
                        State, 
                        ArgHeader, 
                        "a graph argument must be a series"
                    );
            } break;

            case TOKEN_KIND_TABLE: {
                if (IsMessage(Type)) {
                    if (ArgCount > 1) {
                        CHECK_ERROR(
                            State, 
                            ArgHeader, 
                            "table(MSG) takes the message type alone"
                        );
                    } else {
                        u32 Message = FLVSchemaFindMessage(
                            State->Schema, 
                            MessageName(Type)
                        );
                        FLVSchemaMessage const* M = &State->Schema->Messages[Message];

                        for (u32 FieldIndex = 0; FieldIndex < M->FieldCount; ++FieldIndex) {
                            SlotFind(
                                State, 
                                FLV_SLOT_KIND_FIELD, 
                                MessageName(Type), 
                                State->Schema->Fields[M->HeadField + FieldIndex].Name, 
                                MessageInstance(Type)
                            );
                        }
                    }
                } else if (IsSeries(Type)) {
                    if (IsUntyped(TableBase)) {
                        TableBase = BaseOf(Type);
                    } else if (TableBase.V != BaseOf(Type).V) {
                        CHECK_ERROR(
                            State, 
                            ArgHeader, 
                            "%S and %S are different streams; align is not implemented yet",
                            PRINT_STR(NameStr(MessageName(TableBase))),
                            PRINT_STR(NameStr(MessageName(BaseOf(Type))))
                        );
                    }
                } else {
                    CHECK_ERROR(
                        State, 
                        ArgHeader, 
                        "a table argument must be a field or a message type"
                    );
                }
            } break;

            case TOKEN_KIND_FLAG: {
                if (Index == 0) {
                    if (!IsBoolSeries(Type) && !IsSelection(Type)) {
                        CHECK_ERROR(
                            State, 
                            ArgHeader, 
                            "a flag condition must be a boolean series or a selection"
                        );
                    }
                } else if (Index == 1) {
                    if (!Arg->IsLabel) {
                        CHECK_ERROR(
                            State, 
                            ArgHeader, 
                            "a flag label must be a string literal"
                        );
                    }
                } else {
                    CHECK_ERROR(
                        State, 
                        ArgHeader, 
                        "flag takes a condition and an optional label"
                    );
                }
            } break;

            case TOKEN_KIND_TEXT: {} break;
        }
    }
}

INTERNAL void
CheckIf(CheckState* State, ASTNodeIf* If)
{
    AST* Tree = State->Tree;
    TypeIndex Type = TypeOf(Tree, If->Cond);

    If->Header.Variant = IF_VARIANT_BRANCH;

    if (IsVoid(Type))
        return;

    if (IsBoolSeries(Type) || IsSelection(Type)) {
        If->Header.Variant = IF_VARIANT_FILTER;
    } else if (IsUntyped(Type) || IsScalar(Type)) {
        Adopt(Tree, If->Cond, Defaulted(Type));
    } else if (IsSeries(Type)) {
        CHECK_ERROR(
            State, 
            &If->Header, 
            "if on a series needs a boolean series (a comparison)"
        );
    } else {
        CHECK_ERROR(
            State, 
            &If->Header, 
            "an if condition must be a scalar, a boolean series or a selection"
        );
    }
}

INTERNAL u32
CheckStmt(CheckState* State, u32 Offset, u32* ScopeKinds)
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
                Next = CheckStmt(State, Next, ScopeKinds);
        } break;

        case AST_SCOPE: {
            ASTNodeScope* Scope = (ASTNodeScope*) Header;
            u32 Kinds = 0;

            Next += sizeof(ASTNodeScope);

            for (u32 Index = 0; Index < Scope->StmtCount; ++Index)
                Next = CheckStmt(State, Next, &Kinds);
        } break;

        case AST_IF: {
            ASTNodeIf* If = (ASTNodeIf*) Header;

            CheckIf(State, If);
            Next = CheckStmt(State, Next + sizeof(ASTNodeIf), ScopeKinds);

            if (If->Else.V != AST_NODE_INDEX_INVALID) {
                ASSERT(If->Else.V == Next);
                Next = CheckStmt(State, Next, ScopeKinds);
            }
        } break;

        case AST_DISPLAY: {
            CheckDisplay(State, (ASTNodeDisplay*) Header, ScopeKinds);
            Next = StmtNext(Tree, Offset);
        } break;
    }

    return Next;
}

void
CheckAST(
    Arena* MemPool, 
    AST* Tree, 
    FLVSchema const* Schema, 
    OUT FLVSlot** Slots, 
    OUT u32* SlotCount
) {
    CheckState State = {};
    u32 RootKinds = 0;

    State.Tree = Tree;
    State.Schema = Schema;
    State.Slots = ArenaPushArray(MemPool, FLVSlot, CHECK_SLOT_MAX);

    for (u32 Index = 0; Index < Tree->WorkCount; ++Index) {
        i32 Item = Tree->Work[Index];

        if (Item < 0) {
            ExprIndex Expr = {
                Item
            };

            CheckExpr(&State, ExprAt(Tree, Expr), Expr);
            continue;
        }

        u32 Kind = (u32) Item >> CHECK_WORK_KIND_OFFSET;
        u32 Arg = (u32) Item & CHECK_WORK_ARG_MASK;

        ASSERT(Kind == CHECK_WORK_KIND_BIND);

        FinaliseBinding(
            &State, 
            {
                Arg
            }
        );
    }

    CheckStmt(&State, Tree->Root.V, &RootKinds);

    *Slots = State.Slots;
    *SlotCount = State.SlotsCount;
}
