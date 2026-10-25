#include "nc_defines.h"
#include "nc_types.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_log.h"
#include "nc_tls.h"
#include "nc_file.h"
#include "flv_parser.h"
#include "flv_ast.h"
#include "flv_token.h"
#include "flv_symbol_table.h"
#include "flv_intern.h"
#include "flv_type.h"
#include "flv_type_check.h"
#include "flv_lexer.h"

#define PARSE_ERROR(Tok, Fmt, ...)         \
    do {                                   \
        SourceLoc __Loc = LocFromTok(Tok); \
        LogError(                          \
            "%S:%u:%u: " Fmt,              \
            PRINT_STR(__Loc.Path),         \
            __Loc.Line,                    \
            __Loc.Column,                  \
            ##__VA_ARGS__                  \
        );                                 \
    } while (FALSE)
 
#define PARSE_PANIC(Tok, Fmt, ...)         \
    do {                                   \
        SourceLoc __Loc = LocFromTok(Tok); \
        LogPanic(                          \
            "%S:%u:%u: " Fmt,              \
            PRINT_STR(__Loc.Path),         \
            __Loc.Line,                    \
            __Loc.Column,                  \
            ##__VA_ARGS__                  \
        );                                 \
        UNREACHABLE();                     \
    } while (FALSE)

global const OpName OP_NAME_TABLE[] = {
    {
        Str8Lit("+"),
        TOKEN_KIND_PLUS
    },
    {
        Str8Lit("-"),
        TOKEN_KIND_MINUS
    },
    {
        Str8Lit("*"),
        TOKEN_KIND_STAR
    },
    {
        Str8Lit("/"),
        TOKEN_KIND_SLASH
    },
    {
        Str8Lit("%"),
        TOKEN_KIND_PERCENT
    },
    {
        Str8Lit("&"),
        TOKEN_KIND_BIT_AND
    },
    {
        Str8Lit("|"),
        TOKEN_KIND_BIT_OR
    },
    {
        Str8Lit("^"),
        TOKEN_KIND_CARET
    },
    {
        Str8Lit("<"),
        TOKEN_KIND_LESS
    },
    {
        Str8Lit(">"),
        TOKEN_KIND_GREATER
    },
    {
        Str8Lit("!"),
        TOKEN_KIND_EXCLAIM
    },
    {
        Str8Lit("=="),
        TOKEN_KIND_EQ_EQ
    },
    {
        Str8Lit("!="),
        TOKEN_KIND_EXCLAIM_EQ
    },
    {
        Str8Lit("<="),
        TOKEN_KIND_LESS_EQ
    },
    {
        Str8Lit(">="),
        TOKEN_KIND_GREATER_EQ
    },
    {
        Str8Lit("&&"),
        TOKEN_KIND_LOG_AND
    },
    {
        Str8Lit("||"),
        TOKEN_KIND_LOG_OR
    }
};

global NameIndex OP_NAMES[256] = {};

INTERNAL Token
CurrentToken(void)
{
    return GetTLS()->PARSER_STATE->Tokens[
        GetTLS()->PARSER_STATE->CurrentIndex.V
    ];
}
 
INTERNAL Token
PeekToken(u32 Skip = 1)
{
    return GetTLS()->PARSER_STATE->Tokens[
        GetTLS()->PARSER_STATE->CurrentIndex.V + Skip
    ];
}
 
INTERNAL Token
ConsumeToken(void)
{
    Token Result = CurrentToken();
 
    ++GetTLS()->PARSER_STATE->CurrentIndex.V;
 
    return Result;
}
 
INTERNAL b32
MatchToken(u8 Kind)
{
    if (CurrentToken().Kind ==  Kind) {
        ConsumeToken();
 
        return TRUE;
    }
 
    return FALSE;
}
 
INTERNAL NameIndex
CurrentName(void)
{
    return GetTLS()->PARSER_STATE->TokenNames[
        GetTLS()->PARSER_STATE->CurrentIndex.V
    ];
}
 
INTERNAL TokIndex
CurrentTok(void)
{
    TokIndex Result = {
        (
            (GetTLS()->PARSER_STATE->CurrentFile << TOK_FILE_SHIFT) |
            GetTLS()->PARSER_STATE->CurrentIndex.V
        )
    };
 
    return Result;
}
 
SourceLoc
LocFromFiles(ParsedFile const* Files, TokIndex Tok)
{
    ParsedFile const* File = &Files[Tok.V >> TOK_FILE_SHIFT];
    u32 Offset = File->Tokens.Tokens[Tok.V & TOK_INDEX_MASK].Offset;
    SourceLoc Result = {};
 
    Result.Path = File->Path;
    Result.Line = LineFromOffset(File->LineOffsets, File->LineCount, Offset);
    Result.Column = Offset - File->LineOffsets[Result.Line - 1] + 1;
 
    return Result;
}
 
INTERNAL SourceLoc
LocFromTok(TokIndex Tok)
{
    return LocFromFiles(GetTLS()->PARSER_STATE->Files, Tok);
}
 
INTERNAL Str8
CurrentTokStr(void)
{
    Token Tok = CurrentToken();
    Str8 Result = Str(
        GetTLS()->PARSER_STATE->Source.Str + Tok.Offset, 
        Tok.Count
    );
 
    return Result;
}
 
API_INTERNAL void
ExpectToken(u8 Kind)
{
    if (CurrentToken().Kind != Kind) {
        PARSE_PANIC(
            CurrentTok(),
            "expected [%c] (%u) but got [%c] (%u) instead",
            Kind,
            Kind,
            CurrentToken().Kind,
            CurrentToken().Kind
        );
    }
 
    ConsumeToken();
}
 
INTERNAL void
PushWork(i32 Item)
{
    i32* Slot = ArenaPushArray(
        GetTLS()->PARSER_STATE->WorkMemPool, 
        i32, 
        1
    );

    *Slot = Item;
    ++GetTLS()->PARSER_STATE->Tree->WorkCount;
}
 
INTERNAL void
ArenaExInit(ArenaEx* Stream, u64 ReserveSize)
{
    u8* Base = (u8*) MemReserve(ReserveSize);
 
    ASSERT_ALWAYS(Base);
 
    Stream->End = Base + ReserveSize;
    Stream->Reserved = ReserveSize;
    Stream->Committed = 0;
    Stream->Count = 0;
}
 
INTERNAL void*
ArenaExPushSize(ArenaEx* Stream, u64 Size)
{
    void* Result = NULL;

    Stream->Count += Size;
 
    if (Stream->Count > Stream->Committed) {
        u64 NewComitted = ALIGNTO(Stream->Count, EXPR_ARENA_COMMIT_SIZE);
 
        ASSERT_ALWAYS(NewComitted <= Stream->Reserved);
 
        b32 Committed = MemCommit(
            Stream->End - NewComitted,
            NewComitted - Stream->Committed
        );
 
        ASSERT_ALWAYS(Committed);
 
        Stream->Committed = NewComitted;
    }

    Result = Stream->End - Stream->Count;
 
    return Result;
}
 
INTERNAL void
DeferIdent(
    NameIndex Name, 
    TokIndex Tok, 
    i32 Index, 
    DeferredArenaKind ArenaKind
) {
    DeferredIdent* Entry = ArenaPushStruct(
        GetTLS()->PARSER_STATE->DeferredMemPool,
        DeferredIdent
    );
 
    ++GetTLS()->PARSER_STATE->DeferredCount;
    Entry->Name = Name;
    Entry->Tok = Tok;
    Entry->Index = Index;
    Entry->ArenaKind = ArenaKind;
}
 
INTERNAL void
ResolveDeferred(void)
{
    AST* Tree = GetTLS()->PARSER_STATE->Tree;
 
    for (
        u32 Index = 0; 
        Index < GetTLS()->PARSER_STATE->DeferredCount; 
        ++Index
    ) {
        DeferredIdent* Entry = &GetTLS()->PARSER_STATE->Deferred[Index];
        BindIndex Bind = FindBinding(Entry->Name);
 
        if (Bind.V == BINDING_INVALID_VALUE)
            continue;
 
        switch (Entry->ArenaKind) {
            default: {} break;
 
            case DEFERRED_ARENA_KIND_EXPR: {
                ASTNodeExprIdent* Node = (ASTNodeExprIdent*) (
                    Tree->Exprs + Entry->Index
                );
 
                Node->Bind = Bind;
            } break;
        }
    }
 
    GetTLS()->PARSER_STATE->DeferredCount = 0;
    ArenaClear(GetTLS()->PARSER_STATE->DeferredMemPool);
}
 
void 
ParserInternNames(NamePool* Names)
{
    for (u32 Index = 0; Index < ARRAY_COUNT(OP_NAMES); ++Index)
        OP_NAMES[Index].V = NAME_INDEX_INVALID_VALUE;
 
    for (u32 Index = 0; Index < ARRAY_COUNT(OP_NAME_TABLE); ++Index) {
        OpName const* Entry = &OP_NAME_TABLE[Index];
 
        OP_NAMES[Entry->Kind] = InternName(Names, Entry->Name);
    }
}
 
void*
ASTNodeStmtAlloc(u32 Size)
{
    u32 SizeAligned = ALIGNTO(Size, sizeof(u32));
    void* Result = ArenaPushSize(
        GetTLS()->PARSER_STATE->StmtMemPool, 
        SizeAligned, 
        sizeof(u32),
        FALSE
    );
 
    GetTLS()->PARSER_STATE->Tree->StmtsCount += SizeAligned;
 
    return Result;
}
 
void*
ASTNodeExprAlloc(u32 Size, u8 NodeType)
{
    u32 SizeAligned = ALIGNTO(Size, sizeof(u32));
    void* Result = ArenaExPushSize(
        &GetTLS()->PARSER_STATE->ExprMemPool, 
        SizeAligned
    );
    ASTNodeHeader* ResultHeader = (ASTNodeHeader*) Result;
 
    ResultHeader->Type = NodeType;
    ResultHeader->Count = 0;
    ResultHeader->Variant = 0;
 
    if (ASTExprIsTyped(NodeType))
        ((ASTNodeExprAccel*) Result)->Type.V = TYPE_INDEX_INVALID_TYPE;
 
    GetTLS()->PARSER_STATE->ExprCount = SafeTruncateU64(
        GetTLS()->PARSER_STATE->ExprMemPool.Count
    );
    GetTLS()->PARSER_STATE->Tree->ExprsCount = GetTLS()->PARSER_STATE->ExprCount;
    PushWork(-(i32) GetTLS()->PARSER_STATE->ExprCount);
 
    return Result;
}
 
void*
ASTNodeDeclAlloc(u32 Size)
{
    u32 SizeAligned = ALIGNTO(Size, sizeof(u32));
    void* Result = ArenaPushSize(
        GetTLS()->PARSER_STATE->DeclMemPool, 
        SizeAligned, 
        sizeof(u32),
        FALSE
    );
 
    GetTLS()->PARSER_STATE->Tree->DeclsCount += SizeAligned;
 
    return Result;
}
 
void*
ASTNodeInitAlloc(u32 Size)
{
    u32 SizeAligned = ALIGNTO(Size, sizeof(u32));
    void* Result = ArenaPushSize(
        GetTLS()->PARSER_STATE->InitMemPool, 
        SizeAligned, 
        sizeof(u32),
        FALSE
    );
 
    GetTLS()->PARSER_STATE->Tree->InitsCount += SizeAligned;
 
    return Result;
}
 
INTERNAL ExprIndex
ExprIndexOf(void const* Node)
{
    ExprIndex Result = {
        (i32) ((u8 const*) Node - GetTLS()->PARSER_STATE->Tree->Exprs)
    };
 
    return Result;
}
 
INTERNAL StmtIndex
StmtIndexOf(void const* Node)
{
    StmtIndex Result = {
        SafeTruncateU64((u8 const*) Node - GetTLS()->PARSER_STATE->Tree->Stmts)
    };
 
    return Result;
}
 
INTERNAL DeclIndex
DeclIndexOf(void const* Node)
{
    DeclIndex Result = {
        SafeTruncateU64((u8 const*) Node - GetTLS()->PARSER_STATE->Tree->Decls)
    };
 
    return Result;
}
 
INTERNAL ASTNodeHeader*
ExprHeaderAt(ExprIndex Index)
{
    return (ASTNodeHeader*) (GetTLS()->PARSER_STATE->Tree->Exprs + Index.V);
}
 
INTERNAL b32
ExprIsStrLit(ExprIndex Index)
{
    ASTNodeHeader* Header = ExprHeaderAt(Index);
 
    return (
        Header->Type == AST_LITERAL &&
        Header->Variant == TOKEN_KIND_STR_LIT
    );
}
 
INTERNAL u64
Pow10U64(u32 Exp)
{
    u64 Result = 1;
 
    while (Exp--)
        Result *= 10;
 
    return Result;
}
 
// NOTE(nathan): microseconds per unit, 0 = not a unit
INTERNAL u64
UnitScaleFromStr(Str8 Unit)
{
    u64 Result = 0;
 
    if (Unit.Size == 1) {
        switch (Unit.Str[0]) {
            default: {} break;
 
            case 's': {
                Result = 1000000ULL;
            } break;
 
            case 'm': {
                Result = 60000000ULL;
            } break;
 
            case 'h': {
                Result = 3600000000ULL;
            } break;
        }
    } else if (Unit.Size == 2 && Unit.Str[1] == 's') {
        if (Unit.Str[0] == 'u')
            Result = 1;
        else if (Unit.Str[0] == 'm')
            Result = 1000;
    }
 
    return Result;
}
 
INTERNAL ExprIndex
TimeLiteral(TokIndex Tok, u64 TimeUS)
{
    ASTNodeLiteral* Node = ExprAlloc(ASTNodeLiteral, AST_LITERAL);
 
    Node->Header.Tok = Tok;
    Node->Type = TypeTime();
    Node->Val.Unsigned = TimeUS;
 
    return ExprIndexOf(Node);
}
 
ExprIndex 
ParsePrimary(void)
{
    Token Tok = CurrentToken();
    TokIndex TIndex = CurrentTok();
    ExprIndex Result = {};
 
    switch (Tok.Kind) {
        default: {
            PARSE_ERROR(
                TIndex,
                "expected an expression, got token kind [%u]",
                Tok.Kind
            );
            ConsumeToken();
 
            ASTNodeError* Node = ExprAlloc(ASTNodeError, AST_ERROR);
 
            Node->Header.Tok = TIndex;
            Result = ExprIndexOf(Node);
        } break;
 
        case TOKEN_KIND_NUMBER: {
            Str8 Raw = CurrentTokStr();
            u64 Cursor = 0;
            u64 Int = 0;
            u64 Frac = 0;
            u32 FracDigits = 0;
            b32 HasFrac = FALSE;
 
            ConsumeToken();
 
            while (Cursor < Raw.Size && Raw.Str[Cursor] >= '0' && Raw.Str[Cursor] <= '9')
                Int = Int * 10 + (Raw.Str[Cursor++] - '0');
 
            if (Cursor < Raw.Size && Raw.Str[Cursor] == '.') {
                HasFrac = TRUE;
                ++Cursor;
 
                while (Cursor < Raw.Size && Raw.Str[Cursor] >= '0' && Raw.Str[Cursor] <= '9') {
                    Frac = Frac * 10 + (Raw.Str[Cursor++] - '0');
                    ++FracDigits;
                }
            }
 
            Str8 Unit = StrSkip(Raw, Cursor);
            u64 Scale = UnitScaleFromStr(Unit);
 
            if (Unit.Size && !Scale) {
                PARSE_ERROR(
                    TIndex,
                    "unknown unit \"%S\" (expected us, ms, s, m or h)",
                    PRINT_STR(Unit)
                );
            }
 
            ASTNodeLiteral* Node = ExprAlloc(ASTNodeLiteral, AST_LITERAL);
 
            Node->Header.Tok = TIndex;
 
            if (Scale) {
                Node->Type = TypeDuration();
                Node->Val.Unsigned = Int * Scale;
 
                if (FracDigits)
                    Node->Val.Unsigned += Frac * Scale / Pow10U64(FracDigits);
            } else if (HasFrac) {
                Node->Type.V = BUILTIN_f64;
                Node->Val.Double = (f64) Int + (f64) Frac / (f64) Pow10U64(FracDigits);
            } else {
                Node->Val.Unsigned = Int;
            }
 
            Result = ExprIndexOf(Node);
        } break;
 
        case TOKEN_KIND_STR_LIT: {
            ConsumeToken();
 
            ASTNodeLiteral* Node = ExprAlloc(ASTNodeLiteral, AST_LITERAL);
 
            Node->Header.Tok = TIndex;
            Node->Header.Variant = TOKEN_KIND_STR_LIT;
            Node->Type.V = BUILTIN_void;
            Node->Val.String.Str = GetTLS()->PARSER_STATE->Source.Str + Tok.Offset + 1;
            Node->Val.String.Count = Tok.Count - 2;
            Result = ExprIndexOf(Node);
        } break;
 
        case TOKEN_KIND_IDENT: {
            NameIndex Name = CurrentName();
 
            ConsumeToken();
 
            ASTNodeExprIdent* Node = ExprAlloc(ASTNodeExprIdent, AST_IDENT);
 
            Node->Header.Tok = TIndex;
            Node->Name = Name;
            Node->Bind = FindBinding(Name);
            Node->Slot = U32_MAX;
            Node->ScopeBase = GetTLS()->PARSER_STATE->IndexBase;
            Result = ExprIndexOf(Node);
 
            if (Node->Bind.V == BINDING_INVALID_VALUE) {
                DeferIdent(
                    Name, 
                    TIndex, 
                    Result.V, 
                    DEFERRED_ARENA_KIND_EXPR
                );
            }
        } break;
 
        case TOKEN_KIND_OPEN_PAREN: {
            ConsumeToken();
            Result = ParseExpr();
            ExpectToken(TOKEN_KIND_ClOSE_PAREN);
        } break;
    }
 
    return Result;
}
 
// NOTE(nathan): the high bound is the implicit child, so it is parsed last
INTERNAL ExprIndex
ParseRangeHigh(TokIndex RangeTok, ExprIndex Low)
{
    ExprIndex High = {};
 
    if (CurrentToken().Kind == TOKEN_KIND_CLOSE_SQ_BRACK)
        High = TimeLiteral(RangeTok, U64_MAX);
    else
        High = ParseExpr();
 
    ASTNodeBinary* Node = ExprAlloc(ASTNodeBinary, AST_BINARY_EXPR);
 
    Node->Header.Variant = TOKEN_KIND_RANGE;
    Node->Header.Tok = RangeTok;
    Node->Left = Low;
 
    return ExprIndexOf(Node);
}
 
ExprIndex 
ParsePostfix(void)
{
    ExprIndex Left = ParsePrimary();
 
    for (;;) {
        Token Tok = CurrentToken();
        TokIndex OpTok = CurrentTok();
 
        switch (Tok.Kind) {
            default: {
                return Left;
            } break;
 
            case TOKEN_KIND_OPEN_PAREN: {
                ConsumeToken();
 
                u32 ArgCount = 0;
                ExprIndex Args[128] = {};
 
                if (CurrentToken().Kind != TOKEN_KIND_ClOSE_PAREN) {
                    do {
                        ASSERT(ArgCount < ARRAY_COUNT(Args));
 
                        Args[ArgCount++] = ParseExpr();
                    } while (MatchToken(TOKEN_KIND_COMMA));
                }
 
                ExpectToken(TOKEN_KIND_ClOSE_PAREN);
 
                u32 NodeSize = OFFSETOF(ASTNodeFuncCall, Arg) + ArgCount * sizeof(ExprIndex);
                ASTNodeFuncCall* Node = (ASTNodeFuncCall*) ASTNodeExprAlloc(NodeSize, AST_FUNC_CALL);
 
                Node->Header.Count = ArgCount;
                Node->Header.Tok = OpTok;
                Node->Func = Left;
 
                for (u32 Index = 0; Index < ArgCount; ++Index)
                    Node->Arg[Index] = Args[ArgCount - 1 - Index];
 
                Left = ExprIndexOf(Node);
            } break;
 
            case TOKEN_KIND_PERIOD: {
                ConsumeToken();
 
                NameIndex FieldName = CurrentName();
 
                ExpectToken(TOKEN_KIND_IDENT);
 
                ASTNodeFieldAccess* Node = ExprAlloc(ASTNodeFieldAccess, AST_FIELD_ACCESS);
 
                Node->Header.Tok = OpTok;
                Node->Name = FieldName;
                Node->Slot = U32_MAX;
                Left = ExprIndexOf(Node);
            } break;
 
            case TOKEN_KIND_OPEN_SQ_BRACK: {
                ConsumeToken();
 
                ExprIndex BaseRestore = GetTLS()->PARSER_STATE->IndexBase;

                GetTLS()->PARSER_STATE->IndexBase = Left;

                TokIndex RangeTok = CurrentTok();
 
                if (MatchToken(TOKEN_KIND_RANGE)) {
                    ParseRangeHigh(RangeTok, TimeLiteral(RangeTok, 0));
                } else {
                    ExprIndex Low = ParseExpr();
 
                    RangeTok = CurrentTok();
 
                    if (MatchToken(TOKEN_KIND_RANGE))
                        ParseRangeHigh(RangeTok, Low);
                }
 
                GetTLS()->PARSER_STATE->IndexBase = BaseRestore;
                ExpectToken(TOKEN_KIND_CLOSE_SQ_BRACK);
 
                ASTNodeBinary* Node = ExprAlloc(ASTNodeBinary, AST_INDEX);
 
                Node->Header.Tok = OpTok;
                Node->Left = Left;
                Left = ExprIndexOf(Node);
            } break;
        }
    }
}
 
ExprIndex 
ParseUnary(void)
{
    Token Tok = CurrentToken();
 
    switch (Tok.Kind) {
        default: {
            return ParsePostfix();
        } break;
 
        case TOKEN_KIND_MINUS:
        case TOKEN_KIND_EXCLAIM: {
            u8 OpKind = Tok.Kind;
            TokIndex OpTok = CurrentTok();
 
            ConsumeToken();
 
            ExprIndex Operand = ParseUnary();
            ASTNodeUnary* Node = ExprAlloc(ASTNodeUnary, AST_UNARY_EXPR);
 
            Node->Header.Variant = OpKind;
            Node->Header.Tok = OpTok;
            Node->Expr = Operand;
 
            return ExprIndexOf(Node);
        } break;
    }
}
 
ExprIndex 
ParseBinary(u8 MinPrecedence)
{
    ExprIndex Left = ParseUnary();
 
    for (;;) {
        Token Op = CurrentToken();
        TokIndex TIndex = CurrentTok();
        u8 Precedence = 0;
 
        switch (Op.Kind) {
            default: {
                Precedence = 0;
            } break;
 
            case TOKEN_KIND_STAR:
            case TOKEN_KIND_SLASH:
            case TOKEN_KIND_PERCENT: {
                Precedence = 10;
            } break;
 
            case TOKEN_KIND_PLUS:
            case TOKEN_KIND_MINUS: {
                Precedence = 9;
            } break;
 
            case TOKEN_KIND_LESS:
            case TOKEN_KIND_GREATER:
            case TOKEN_KIND_LESS_EQ:
            case TOKEN_KIND_GREATER_EQ: {
                Precedence = 7;
            } break;
 
            case TOKEN_KIND_EQ_EQ:
            case TOKEN_KIND_EXCLAIM_EQ: {
                Precedence = 6;
            } break;
 
            case TOKEN_KIND_BIT_AND: {
                Precedence = 5;
            } break;
 
            case TOKEN_KIND_CARET: {
                Precedence = 4;
            } break;
 
            case TOKEN_KIND_BIT_OR: {
                Precedence = 3;
            } break;
 
            case TOKEN_KIND_LOG_AND: {
                Precedence = 2;
            } break;
 
            case TOKEN_KIND_LOG_OR: {
                Precedence = 1;
            } break;
        }
 
        if (Precedence < MinPrecedence)
            break;
 
        u8 OpKind = Op.Kind;
 
        ConsumeToken();
        ParseBinary(Precedence + 1);
 
        ASTNodeBinary* Node = ExprAlloc(ASTNodeBinary, AST_BINARY_EXPR);
 
        Node->Header.Variant = OpKind;
        Node->Header.Tok = TIndex;
        Node->Left = Left;
        Left = ExprIndexOf(Node);
    }
 
    return Left;
}
 
ExprIndex 
ParseTernary(void)
{
    ExprIndex Cond = ParseBinary(1);
    TokIndex TIndex = CurrentTok();
 
    if (MatchToken(TOKEN_KIND_QUESTION)) {
        ExprIndex Then = ParseExpr();
 
        ExpectToken(TOKEN_KIND_COLON);
        ParseTernary();
 
        ASTNodeTernary* Node = ExprAlloc(ASTNodeTernary, AST_TERNARY_EXPR);
 
        Node->Header.Tok = TIndex;
        Node->Cond = Cond;
        Node->Then = Then;
 
        return ExprIndexOf(Node);
    }
 
    return Cond;
}
 
ExprIndex 
ParseAssign(void)
{
    ExprIndex Left = ParseTernary();
    TokIndex TIndex = CurrentTok();
 
    if (MatchToken(TOKEN_KIND_EQUAL)) {
        ParseAssign();
 
        ASTNodeBinary* Node = ExprAlloc(ASTNodeBinary, AST_BINARY_ASSIGNMENT);
 
        Node->Header.Variant = TOKEN_KIND_EQUAL;
        Node->Header.Tok = TIndex;
        Node->Left = Left;
 
        return ExprIndexOf(Node);
    }
 
    return Left;
}
 
ExprIndex 
ParseExpr(void)
{
    return ParseAssign();
}
 
StmtIndex 
ParseIf(void)
{
    TokIndex IfTok = CurrentTok();
 
    ExpectToken(TOKEN_KIND_IF);
    ExpectToken(TOKEN_KIND_OPEN_PAREN);
 
    ExprIndex Cond = ParseExpr();
 
    ExpectToken(TOKEN_KIND_ClOSE_PAREN);
 
    ASTNodeIf* Node = StmtAlloc(ASTNodeIf);
 
    Node->Header.Type = AST_IF;
    Node->Header.Tok = IfTok;
    Node->Header.Count = 0;
    Node->Header.Variant = 0;
    Node->Cond = Cond;
    Node->Else = {
        AST_NODE_INDEX_INVALID
    };
 
    StmtIndex Result = StmtIndexOf(Node);
 
    ParseBlock();
 
    if (MatchToken(TOKEN_KIND_ELSE)) {
        if (CurrentToken().Kind == TOKEN_KIND_IF)
            Node->Else = ParseIf();
        else
            Node->Else = ParseBlock();
    }
 
    return Result;
}
 
StmtIndex
ParseDisplay(void)
{
    Token Keyword = CurrentToken();
    TokIndex KeywordTok = CurrentTok();
    ASTDisplayArg Args[DISPLAY_ARG_MAX] = {};
    u32 Count = 0;
 
    ConsumeToken();
    ExpectToken(TOKEN_KIND_OPEN_PAREN);
 
    do {
        ASSERT(Count < DISPLAY_ARG_MAX);
 
        ASTDisplayArg* Arg = &Args[Count++];
        TokIndex ArgTok = CurrentTok();
 
        Arg->Expr = ParseExpr();
        Arg->Axis = 1;
        Arg->IsLabel = (u8) ExprIsStrLit(Arg->Expr);
 
        if (Arg->IsLabel && !(Keyword.Kind == TOKEN_KIND_FLAG && Count == 2))
            PARSE_ERROR(ArgTok, "a string literal is only legal as the second argument of flag()");
 
        if (MatchToken(TOKEN_KIND_COLON)) {
            TokIndex AxisTok = CurrentTok();
            Str8 AxisStr = CurrentTokStr();
 
            ExpectToken(TOKEN_KIND_NUMBER);
 
            u64 Axis = U64FromStr(AxisStr);
 
            if (Keyword.Kind != TOKEN_KIND_GRAPH)
                PARSE_ERROR(AxisTok, "an axis is only legal on a graph() argument");
            else if (Axis != 1 && Axis != 2)
                PARSE_ERROR(AxisTok, "axis must be 1 or 2");
            else
                Arg->Axis = (u8) Axis;
        }
    } while (MatchToken(TOKEN_KIND_COMMA));
 
    ExpectToken(TOKEN_KIND_ClOSE_PAREN);
    ExpectToken(TOKEN_KIND_SEMICOLON);
 
    u32 NodeSize = OFFSETOF(ASTNodeDisplay, Arg) + Count * sizeof(ASTDisplayArg);
    ASTNodeDisplay* Node = (ASTNodeDisplay*) ASTNodeStmtAlloc(NodeSize);
 
    Node->Header.Type = AST_DISPLAY;
    Node->Header.Tok = KeywordTok;
    Node->Header.Variant = Keyword.Kind;
    Node->Header.Count = SafeTruncateU32(Count);
    MemCpy(Node->Arg, Args, Count * sizeof(ASTDisplayArg));
 
    if (GetTLS()->PARSER_STATE->CurrentScopeNode)
        GetTLS()->PARSER_STATE->CurrentScopeNode->Header.Variant |= SCOPE_FLAG_IS_ANALYSIS;
 
    return StmtIndexOf(Node);
}
 
StmtIndex
ParseScope(void)
{
    TokIndex Tok = CurrentTok();
    NameIndex Name = {
        NAME_INDEX_INVALID_VALUE
    };
 
    if (CurrentToken().Kind == TOKEN_KIND_IDENT) {
        Name = CurrentName();
        ConsumeToken();
    }
 
    ExpectToken(TOKEN_KIND_OPEN_BRACK);
 
    StmtIndex Result = ParseScopeBody(Name, Tok, FALSE);
 
    ExpectToken(TOKEN_KIND_CLOSE_BRACK);
 
    return Result;
}
 
StmtIndex
ParseScopeBody(NameIndex Name, TokIndex Tok, b32 IsRoot)
{
    ASTNodeScope* Parent = GetTLS()->PARSER_STATE->CurrentScopeNode;
    ScopeIndex SavedScope = GetTLS()->PARSER_STATE->CurrentScope;
    ASTNodeScope* Node = StmtAlloc(ASTNodeScope);
    StmtIndex Result = StmtIndexOf(Node);
    u32 Count = 0;
 
    Node->Header.Type = AST_SCOPE;
    Node->Header.Tok = Tok;
    Node->Header.Count = 0;
    Node->Header.Variant = 0;
    Node->Name = Name;
    Node->StmtCount = 0;
    Node->Depth = GetTLS()->PARSER_STATE->ScopeDepth;
 
    if (Parent) {
        Parent->Header.Variant |= SCOPE_FLAG_IS_CONTAINER;
        ++Parent->Header.Count;
    }
 
    GetTLS()->PARSER_STATE->CurrentScopeNode = Node;
    ++GetTLS()->PARSER_STATE->ScopeDepth;
 
    if (!IsRoot)
        GetTLS()->PARSER_STATE->CurrentScope = ScopeEnter();
 
    while (
        CurrentToken().Kind != TOKEN_KIND_CLOSE_BRACK &&
        CurrentToken().Kind != TOKEN_KIND_EOF
    ) {
        ParseStmt();
        ++Count;
    }
 
    Node->StmtCount = Count;
 
    if (
        (Node->Header.Variant & SCOPE_FLAG_IS_ANALYSIS) &&
        (Node->Header.Variant & SCOPE_FLAG_IS_CONTAINER)
    ) {
        PARSE_ERROR(CurrentTok(), "a scope cannot hold both display statements and child scopes");
    }
 
    if (!IsRoot) {
        ScopeExit(GetTLS()->PARSER_STATE->CurrentScope);
        GetTLS()->PARSER_STATE->CurrentScope = SavedScope;
    }
 
    --GetTLS()->PARSER_STATE->ScopeDepth;
    GetTLS()->PARSER_STATE->CurrentScopeNode = Parent;
 
    return Result;
}
 
StmtIndex 
ParseStmt(void)
{
    StmtIndex Result = {};
    Token Tok = CurrentToken();
 
    switch (Tok.Kind) {
        default: {
            ASTNodeExprStmt* Node = StmtAlloc(ASTNodeExprStmt);
 
            Node->Header.Type = AST_EXPR_STATEMENT;
            Node->Header.Tok = CurrentTok();
            Node->Expr = ParseExpr();
            ExpectToken(TOKEN_KIND_SEMICOLON);
            Result = StmtIndexOf(Node);
        } break;
 
        case TOKEN_KIND_IF: {
            Result = ParseIf();
        } break;
 
        case TOKEN_KIND_OPEN_BRACK: {
            Result = ParseScope();
        } break;
 
        case TOKEN_KIND_GRAPH:
        case TOKEN_KIND_TABLE:
        case TOKEN_KIND_TEXT:
        case TOKEN_KIND_FLAG: {
            Result = ParseDisplay();
        } break;
 
        case TOKEN_KIND_SEMICOLON: {
            ASTNodeHeader* Node = StmtAlloc(ASTNodeHeader);
 
            Node->Type = AST_EMPTY;
            Node->Tok = CurrentTok();
            ConsumeToken();
            Result = StmtIndexOf(Node);
        } break;
 
        case TOKEN_KIND_VAR: {
            ASTNodeDeclStmt* Node = StmtAlloc(ASTNodeDeclStmt);
 
            Node->Header.Type = AST_DECL_STATEMENT;
            Node->Header.Tok = CurrentTok();
            Node->Decl = ParseDecl();
            Result = StmtIndexOf(Node);
        } break;
 
        case TOKEN_KIND_IMPORT: {
            PARSE_PANIC(CurrentTok(), "import must be at the top of the file");
        } break;
 
        case TOKEN_KIND_IDENT: {
            if (PeekToken().Kind == TOKEN_KIND_OPEN_BRACK) {
                Result = ParseScope();
            } else {
                ASTNodeExprStmt* Node = StmtAlloc(ASTNodeExprStmt);
 
                Node->Header.Type = AST_EXPR_STATEMENT;
                Node->Header.Tok = CurrentTok();
                Node->Expr = ParseExpr();
                ExpectToken(TOKEN_KIND_SEMICOLON);
                Result = StmtIndexOf(Node);
            }
        } break;
    }
 
    return Result;
}
 
StmtIndex
ParseBlock(void)
{
    ExpectToken(TOKEN_KIND_OPEN_BRACK);
 
    ScopeIndex Saved = GetTLS()->PARSER_STATE->CurrentScope;
 
    GetTLS()->PARSER_STATE->CurrentScope = ScopeEnter();
 
    StmtIndex Result = ParseStmtList();
 
    ScopeExit(GetTLS()->PARSER_STATE->CurrentScope);
    GetTLS()->PARSER_STATE->CurrentScope = Saved;
    ExpectToken(TOKEN_KIND_CLOSE_BRACK);
 
    return Result;
}
 
StmtIndex 
ParseStmtList(void)
{
    u32 StartPosition = GetTLS()->PARSER_STATE->Tree->StmtsCount;
    u32 Count = 0;
    ASTNodeStmtList* Node = StmtAlloc(ASTNodeStmtList);
    StmtIndex Result = {StartPosition};
 
    Node->Header.Type = AST_STATEMENT_LIST;
    Node->Header.Tok = CurrentTok();
 
    while (
        CurrentToken().Kind != TOKEN_KIND_CLOSE_BRACK &&
        CurrentToken().Kind != TOKEN_KIND_EOF
    ) {
        ParseStmt();
        ++Count;
    }
 
    Node->Count = Count;
 
    return Result;
}
 
InitIndex 
ParseInit(void)
{
    u32 StartPosition = GetTLS()->PARSER_STATE->Tree->InitsCount;
    ASTNodeInitialiserExpr* Node = InitAlloc(ASTNodeInitialiserExpr);
 
    Node->Header.Type = AST_INITIALISER_EXPR;
    Node->Header.Tok = CurrentTok();
    Node->Val = ParseAssign();
 
    InitIndex Result = {StartPosition};
 
    return Result;
}
 
DeclIndex 
ParseDecl(void)
{
    ExpectToken(TOKEN_KIND_VAR);
 
    TokIndex NameTok = CurrentTok();
    NameIndex Name = CurrentName();
 
    ExpectToken(TOKEN_KIND_IDENT);
 
    u32 DeclPosition = GetTLS()->PARSER_STATE->Tree->DeclsCount;
    ASTNodeDecl* DeclNode = DeclAlloc(ASTNodeDecl);
 
    DeclNode->Header.Type = AST_DECLARATION;
    DeclNode->Header.Tok = NameTok;
    DeclNode->Type = {TYPE_INDEX_INVALID_TYPE};
    DeclNode->DeclCount = 1;
 
    ASTNodeDeclIdent* IdentNode = DeclAlloc(ASTNodeDeclIdent);
 
    IdentNode->Header.Type = AST_DECLARATOR_IDENT;
    IdentNode->Header.Tok = NameTok;
    IdentNode->Name = Name;
    IdentNode->Bind = {BINDING_INVALID_VALUE};
 
    ExpectToken(TOKEN_KIND_EQUAL);
 
    InitIndex Init = ParseInit();
    ASTNodeDeclValued* ValuedNode = DeclAlloc(ASTNodeDeclValued);
 
    ValuedNode->Header.Type = AST_DECLARATOR_VALUED;
    ValuedNode->Header.Tok = NameTok;
    ValuedNode->Child = DeclIndexOf(IdentNode);
    ValuedNode->Init = Init;
    DeclNode->FirstDecl = DeclIndexOf(ValuedNode);
    ExpectToken(TOKEN_KIND_SEMICOLON);
 
    IdentNode->Bind = AddBinding(
        GetTLS()->PARSER_STATE->CurrentScope, 
        Name, 
        {
            DeclPosition
        },
        {
            TYPE_INDEX_INVALID_TYPE
        },
        BINDING_KIND_VALUE
    );
 
    if (IdentNode->Bind.V != BINDING_INVALID_VALUE) {
        PushWork(
            (CHECK_WORK_KIND_BIND << CHECK_WORK_KIND_OFFSET) |
            IdentNode->Bind.V
        );
    } else {
        Str8 NameString = GetName(GetTLS()->SYMBOL_TABLE->Names, Name);
 
        PARSE_ERROR(
            NameTok,
            "redeclaration of \"%S\"",
            PRINT_STR(NameString)
        );
    }
 
    DeclIndex Result = {
        DeclPosition
    };
 
    return Result;
}
 
// NOTE(nathan): the directory of the importing file first, then the search path
INTERNAL Str8
ImportPathFromName(Arena* MemPool, Str8 Name)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    Str8 FileName = ArenaPushStrCat(Scratch.MemPool, Name, ".flv"_s8);
    Str8 Result = {};
    Str8 Current = GetTLS()->PARSER_STATE->Files[GetTLS()->PARSER_STATE->CurrentFile].Path;
    Str8 Dir = StrChopLastSlash(Current);
 
    if (Dir.Size == Current.Size)
        Dir = "."_s8;
 
    for (u32 Index = 0; Index <= GetTLS()->PARSER_STATE->SearchPathsCount; ++Index) {
        Str8 Base = Index 
            ? GetTLS()->PARSER_STATE->SearchPaths[Index - 1] 
            : Dir;
        Str8 Candidate = ArenaPushStrCat(
            Scratch.MemPool,
            ArenaPushStrCat(Scratch.MemPool, StrTrimLastSlash(Base), "/"_s8),
            FileName
        );
 
        if (SystemFileExists(Candidate)) {
            Result = ArenaPushStrCpy(MemPool, Candidate);
            break;
        }
    }
 
    ReleaseScratch(Scratch);
 
    return Result;
}
 
INTERNAL u32
PushFile(Str8 Path, Str8 Source)
{
    u32 Result = GetTLS()->PARSER_STATE->FilesCount++;
    ParsedFile* File = ArenaPushStruct(
        GetTLS()->PARSER_STATE->FilesMemPool, 
        ParsedFile
    );
 
    ASSERT_ALWAYS(Result < (1U << (32 - TOK_FILE_SHIFT)));
 
    File->Path = Path;
    File->Source = Source;
    File->Tokens = LexString(
        GetTLS()->PARSER_STATE->Tree->MemPool, 
        Source, 
        GetTLS()->SYMBOL_TABLE->Names
    );
    File->LineOffsets = LineOffsetsFromSource(
        GetTLS()->PARSER_STATE->Tree->MemPool,
        Source,
        &File->LineCount
    );
    File->IsOpen = TRUE;
 
    ASSERT_ALWAYS(File->Tokens.TokensCount <= TOK_INDEX_MASK);
    SymTabPrepare();
    GetTLS()->PARSER_STATE->Tree->FilesCount = GetTLS()->PARSER_STATE->FilesCount;
 
    return Result;
}
 
INTERNAL void
SelectFile(u32 Index)
{
    ParsedFile* File = &GetTLS()->PARSER_STATE->Files[Index];
 
    GetTLS()->PARSER_STATE->CurrentFile = Index;
    GetTLS()->PARSER_STATE->Source = File->Source;
    GetTLS()->PARSER_STATE->Tokens = File->Tokens.Tokens;
    GetTLS()->PARSER_STATE->TokenNames = File->Tokens.Names;
}
 
void
ParseImport(void)
{
    TokIndex ImportTok = CurrentTok();
 
    ExpectToken(TOKEN_KIND_IMPORT);
 
    TokIndex NameTok = CurrentTok();
    Str8 Name = CurrentTokStr();
 
    ExpectToken(TOKEN_KIND_IDENT);
    ExpectToken(TOKEN_KIND_SEMICOLON);
 
    Str8 Path = ImportPathFromName(GetTLS()->PARSER_STATE->Tree->MemPool, Name);
 
    if (!Path.Size) {
        PARSE_ERROR(NameTok, "cannot find import \"%S\"", PRINT_STR(Name));
        return;
    }
 
    for (u32 Index = 0; Index < GetTLS()->PARSER_STATE->FilesCount; ++Index) {
        ParsedFile* File = &GetTLS()->PARSER_STATE->Files[Index];
 
        if (!PathMatchNormalised(File->Path, Path))
            continue;
 
        if (File->IsOpen) {
            PARSE_ERROR(
                ImportTok, 
                "import cycle through \"%S\"", 
                PRINT_STR(Path)
            );
        }
 
        return;
    }
 
    Str8 Source = ReadFileContentsEx(
        GetTLS()->PARSER_STATE->Tree->MemPool, 
        Path, 
        NC_SIMD_ALIGN256
    );
 
    if (!Source.Size) {
        PARSE_ERROR(NameTok, "cannot read import \"%S\"", PRINT_STR(Path));
        return;
    }
 
    u32 SavedFile = GetTLS()->PARSER_STATE->CurrentFile;
    TokIndex SavedIndex = GetTLS()->PARSER_STATE->CurrentIndex;
    ASTNodeScope* SavedScopeNode = GetTLS()->PARSER_STATE->CurrentScopeNode;
    u32 SavedDepth = GetTLS()->PARSER_STATE->ScopeDepth;
    u32 FileIndex = PushFile(Path, Source);
 
    SelectFile(FileIndex);
    GetTLS()->PARSER_STATE->CurrentIndex = {};
    GetTLS()->PARSER_STATE->CurrentScopeNode = NULL;
    GetTLS()->PARSER_STATE->ScopeDepth = 0;
    ParseProg(TRUE);
    GetTLS()->PARSER_STATE->Files[FileIndex].IsOpen = FALSE;
    SelectFile(SavedFile);
    GetTLS()->PARSER_STATE->CurrentIndex = SavedIndex;
    GetTLS()->PARSER_STATE->CurrentScopeNode = SavedScopeNode;
    GetTLS()->PARSER_STATE->ScopeDepth = SavedDepth;
}
 
void
ParseProg(b32 IsImported)
{
    while (CurrentToken().Kind == TOKEN_KIND_IMPORT)
        ParseImport();
 
    NameIndex NoName = {
        NAME_INDEX_INVALID_VALUE
    };
    StmtIndex Root = ParseScopeBody(NoName, CurrentTok(), TRUE);
 
    if (CurrentToken().Kind != TOKEN_KIND_EOF)
        PARSE_PANIC(CurrentTok(), "unexpected '}'");
 
    if (IsImported) {
        ASTNodeScope* Node = (ASTNodeScope*) (
            GetTLS()->PARSER_STATE->Tree->Stmts + Root.V
        );
 
        Node->Header.Variant |= SCOPE_FLAG_IS_IMPORTED;
    } else {
        GetTLS()->PARSER_STATE->Tree->Root = Root;
    }
 
    ResolveDeferred();
}
 
AST*
ParseTokens(
    Arena* MemPool, 
    ParsedFile const* Root, 
    Str8 const* SearchPaths, 
    u32 SearchPathCount
) {
    AST* Tree;
 
    GetTLS()->PARSER_STATE = ArenaPushArrayZero(MemPool, ParserState, 1);
    GetTLS()->PARSER_STATE->StmtMemPool = ArenaAllocEx();
    GetTLS()->PARSER_STATE->DeclMemPool = ArenaAllocEx();
    GetTLS()->PARSER_STATE->InitMemPool = ArenaAllocEx();
    GetTLS()->PARSER_STATE->DeferredMemPool = ArenaAllocEx(GB(1));
    GetTLS()->PARSER_STATE->WorkMemPool = ArenaAllocEx(GB(1));
    GetTLS()->PARSER_STATE->FilesMemPool = ArenaAllocEx(MB(1));
    ArenaExInit(&GetTLS()->PARSER_STATE->ExprMemPool, EXPR_ARENA_RESERVE_SIZE);
    Tree = ArenaPushArrayZero(MemPool, AST, 1);
    Tree->MemPool = MemPool;
    Tree->Stmts = ArenaExBase(GetTLS()->PARSER_STATE->StmtMemPool);
    Tree->Exprs = GetTLS()->PARSER_STATE->ExprMemPool.End;
    Tree->Decls = ArenaExBase(GetTLS()->PARSER_STATE->DeclMemPool);
    Tree->Inits = ArenaExBase(GetTLS()->PARSER_STATE->InitMemPool);
    Tree->Work = (i32*) ArenaExBase(GetTLS()->PARSER_STATE->WorkMemPool);
    Tree->Files = (ParsedFile*) ArenaExBase(GetTLS()->PARSER_STATE->FilesMemPool);
    GetTLS()->PARSER_STATE->Tree = Tree;
    GetTLS()->PARSER_STATE->Files = Tree->Files;
    GetTLS()->PARSER_STATE->SearchPaths = SearchPaths;
    GetTLS()->PARSER_STATE->SearchPathsCount = SearchPathCount;
    GetTLS()->PARSER_STATE->Deferred = (DeferredIdent*) ArenaExBase(
        GetTLS()->PARSER_STATE->DeferredMemPool
    );
 
    ParsedFile* RootFile = ArenaPushStruct(
        GetTLS()->PARSER_STATE->FilesMemPool, 
        ParsedFile
    );
 
    *RootFile = *Root;
    RootFile->IsOpen = TRUE;
    GetTLS()->PARSER_STATE->FilesCount = 1;
    Tree->FilesCount = 1;
 
    ASSERT_ALWAYS(RootFile->Tokens.TokensCount <= TOK_INDEX_MASK);
    SelectFile(0);
    GetTLS()->PARSER_STATE->CurrentIndex = {};
    GetTLS()->PARSER_STATE->ModuleScope = ScopeEnter();
    GetTLS()->PARSER_STATE->CurrentScope = GetTLS()->PARSER_STATE->ModuleScope;
    ParseProg(FALSE);
    RootFile->IsOpen = FALSE;
    Tree->ExprsCount = GetTLS()->PARSER_STATE->ExprCount;
 
    return GetTLS()->PARSER_STATE->Tree;
}
