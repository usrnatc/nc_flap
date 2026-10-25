#if !defined(__FLV_PARSER_H__)
#define __FLV_PARSER_H__

#include "nc_types.h"
#include "nc_string.h"
#include "flv_base_types.h"
#include "flv_type.h"
#include "flv_token.h"

struct AST;
struct ASTNodeScope;
struct Arena;
struct NamePool;

// @defines____________________________________________________________________
#if !defined(AST_NODE_INDEX_INVALID)
    #define AST_NODE_INDEX_INVALID U32_MAX
#endif

#define TOK_NEXT 1

#define EXPR_ARENA_RESERVE_SIZE GB(4)
#define EXPR_ARENA_COMMIT_SIZE  KB(256)

#define TOK_FILE_SHIFT 24
#define TOK_INDEX_MASK 0x00FFFFFFU

#define DISPLAY_ARG_MAX 128

#define StmtAlloc(X)    (X*) ASTNodeStmtAlloc(sizeof(X))
#define ExprAlloc(X, T) (X*) ASTNodeExprAlloc(sizeof(X), T)
#define DeclAlloc(X)    (X*) ASTNodeDeclAlloc(sizeof(X))
#define InitAlloc(X)    (X*) ASTNodeInitAlloc(sizeof(X))

typedef u8 DeferredArenaKind;
enum : u8 {
    DEFERRED_ARENA_KIND_EXPR,
    DEFERRED_ARENA_KIND_STMT
};

// @types______________________________________________________________________
struct DeferredIdent {
    NameIndex         Name;
    TokIndex          Tok;
    i32               Index;
    DeferredArenaKind ArenaKind;
};

struct ArenaEx {
    u8* End;
    u64 Reserved;
    u64 Committed;
    u64 Count;
};

struct OpName {
    Str8 Name;
    u8   Kind;
};

struct ParserState {
    ArenaEx        ExprMemPool;
    Str8           Source;
    Token*         Tokens;
    NameIndex*     TokenNames;
    AST*           Tree;
    ParsedFile*    Files;
    DeferredIdent* Deferred;
    Str8 const*    SearchPaths;
    Arena*         StmtMemPool;
    Arena*         DeclMemPool;
    Arena*         InitMemPool;
    Arena*         DeferredMemPool;
    Arena*         WorkMemPool;
    Arena*         FilesMemPool;
    ASTNodeScope*  CurrentScopeNode;
    TokIndex       CurrentIndex;
    ScopeIndex     CurrentScope;
    ScopeIndex     ModuleScope;
    ExprIndex      IndexBase;
    u32            CurrentFile;
    u32            FilesCount;
    u32            SearchPathsCount;
    u32            DeferredCount;
    u32            ExprCount;
    u32            ScopeDepth;
};

// @runtime____________________________________________________________________
extern const OpName OP_NAME_TABLE[];
extern NameIndex OP_NAMES[256];

// @functions__________________________________________________________________
ExprIndex ParsePrimary(void);
ExprIndex ParsePostfix(void);
ExprIndex ParseUnary(void);
ExprIndex ParseBinary(u8 MinPrecedence);
ExprIndex ParseTernary(void);
ExprIndex ParseAssign(void);
ExprIndex ParseExpr(void);
StmtIndex ParseIf(void);
StmtIndex ParseDisplay(void);
StmtIndex ParseScope(void);
StmtIndex ParseScopeBody(NameIndex Name, TokIndex Tok, b32 IsRoot);
StmtIndex ParseStmt(void);
StmtIndex ParseStmtList(void);
StmtIndex ParseBlock(void);
InitIndex ParseInit(void);
DeclIndex ParseDecl(void);
void ParserInternNames(NamePool* Names);
void ParseImport(void);
void ParseProg(b32 IsImported);
SourceLoc LocFromFiles(ParsedFile const* Files, TokIndex Tok);
AST* ParseTokens(Arena* MemPool, ParsedFile const* Root, Str8 const* SearchPaths, u32 SearchPathCount);
void* ASTNodeStmtAlloc(u32 Size);
void* ASTNodeExprAlloc(u32 Size, u8 NodeType);
void* ASTNodeDeclAlloc(u32 Size);
void* ASTNodeInitAlloc(u32 Size);
 
#endif // __FLV_PARSER_H__
