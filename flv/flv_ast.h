#if !defined(__FLV_AST_H__)
#define __FLV_AST_H__

// AST Allocation strategy:
// ============================================================================
// We store the AST in a collection of arrays for different types of the parse:
// expressions, statements, declarations, and initialisers.
// Each array provides storage for that type, and 'pointers' are indices into one 
// of the  different array types. (We use 32-bit indices instead of 64 bit 
// pointers to keep the data structures smaller).

// We reallocate the array as needed so the indices stay valid. In each node, the
// first 'child pointer' is omitted if that child always appears as the next item 
// sequentially in the array. (note we could make all child pointers implicit and
// still be able to traverse, but this approach allows you to traverse node children
// in any order).

// For example, if there is a node at index 8, and the node is 4 u32s in size, then
// the first child of the that node is at index 24.

// The result of this is that once built, when we traverse the tree in depth-first
// order we will traverse the array in linear order, for good cache and hardware
// prefetching, and we don't waste storage for the first child.

// For statement parsing, this is mostly straightforward. As we recurse down, we use
// an expanding array, and just allocate each node sequentially as we parse top-down.
// Because we have to allocate a given node before we parse its descendants,
// this only works for prefix-parse-able things, so we have some special logic for
// handling non-prefix parse-able cases like array declarations.

// For expressions, we use a more strange strategy. We use another expandable array, 
// and we allocate from the END of the array. We only allocate nodes after parsing
// all of their children. This results in expressions being properly tree-packed, 
// but it reverses all of their descendants; e.g. "X+Y" is coded as "(+ y x)" ,
// and in particular function arguments and comma expressions are naively in
// reverse order. The rest of the code base then works with these intentionally 
// reversed trees; for most code, e.g. type analysis, it doesn't matter, and we can 
// continue to traverse in depth-first order. For things like code generation
// for and "l l" we will traverse the tree in not depth-first order. We store 
// indices relative to the end of the array so that resizing doesn't renumber them.
// Declarations mostly use explicit first child pointers because we prefer to use 
// forward allocation in this case. Initialisers use a regular expanding array, 
// which mostly consists of "pointers" to expression sub-trees with the actual
// initialiser value.

#include "nc_types.h"
#include "flv_base_types.h"

struct Arena;
struct ParsedFile;

// @defines____________________________________________________________________
typedef u8 ASTNodeType;
enum : u8 {
    AST_ERROR,
    AST_DECLARATION,
    AST_DECLARATOR_VALUED,
    AST_DECLARATOR_IDENT,
    AST_STATEMENT_LIST,
    AST_IF,
    AST_EXPR_STATEMENT,
    AST_DECL_STATEMENT,
    AST_EMPTY,                                                                  // NOTE(nathan): we parse the empty statement explicitly to avoid complexity in the parse
    AST_SCOPE,
    AST_DISPLAY,
    AST_INDEX,
    AST_INITIALISER_EXPR,
    AST_LITERAL,
    AST_IDENT,
    AST_EXPRESSION_LIST,
    AST_BINARY_ASSIGNMENT,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_TERNARY_EXPR,
    AST_FUNC_CALL,
    AST_FIELD_ACCESS
};

#define AST_TYPED_EXPR_MASK (         \
    (1ULL << AST_LITERAL)           | \
    (1ULL << AST_IDENT)             | \
    (1ULL << AST_BINARY_ASSIGNMENT) | \
    (1ULL << AST_BINARY_EXPR)       | \
    (1ULL << AST_UNARY_EXPR)        | \
    (1ULL << AST_TERNARY_EXPR)      | \
    (1ULL << AST_INDEX)             | \
    (1ULL << AST_FUNC_CALL)         | \
    (1ULL << AST_FIELD_ACCESS)        \
)

#define AST_EXPR_FLAG_OVERLOADED (1 << 0)

#define SCOPE_FLAG_IS_ANALYSIS  (1 << 0)
#define SCOPE_FLAG_IS_CONTAINER (1 << 1)
#define SCOPE_FLAG_IS_IMPORTED  (1 << 2)

#define INDEX_KIND_INSTANCE  1
#define INDEX_KIND_TIME      2
#define INDEX_KIND_CONDITION 3
#define INDEX_KIND_SELECTION 4

#define IF_VARIANT_BRANCH 1
#define IF_VARIANT_FILTER 2

// @types______________________________________________________________________
struct ASTNodeHeader {
    TokIndex Tok;

    union {
        u16  Count;
        u16  Flags;
    };

    u8       Type;
    u8       Variant;
};

struct ASTResolved {
    ExprIndex Expr;
    BindIndex Bind;
};

struct ASTNodeStmtList {
    ASTNodeHeader Header;
    u32           Count;
}; // NOTE(nathan): followed by `Count` statements

struct ASTNodeScope {
    ASTNodeHeader Header;
    NameIndex     Name;
    u32           StmtCount;
    u32           Depth;
}; // NOTE(nathan): followed by StmtCount statements

struct ASTDisplayArg {
    ExprIndex Expr;
    u8        Axis;
    u8        IsLabel;
    u16       __Padding;
};

struct ASTNodeDisplay {
    ASTNodeHeader Header;
    ASTDisplayArg Arg[1];
};

struct ASTNodeExprStmt {
    ASTNodeHeader Header;
    ExprIndex     Expr;
};

struct ASTNodeDeclStmt {
    ASTNodeHeader Header;
    DeclIndex     Decl;
};

struct ASTNodeDeclIdent {
    ASTNodeHeader Header;
    NameIndex     Name;
    BindIndex     Bind;
};

struct ASTNodeInitialiserExpr {
    ASTNodeHeader Header;
    ExprIndex     Val;
};

struct ASTNodeFuncCall {
    ASTNodeHeader Header;
    TypeIndex     Type;
    ExprIndex     Func;
    ExprIndex     Arg[1];
}; // NOTE(nathan): followed by arguments in reverse order

struct ASTNodeIf {
    ASTNodeHeader Header;
    ExprIndex     Cond;
    StmtIndex     Else;
    // NOTE(nathan): followed by Then
};

struct ASTNodeDecl {
    ASTNodeHeader Header;
    TypeIndex     Type;
    u32           DeclCount;
    DeclIndex     FirstDecl;
};

struct ASTNodeDeclValued {
    ASTNodeHeader Header;
    DeclIndex     Child;
    InitIndex     Init;
};

PACKED_STRUCT_BEGIN;
struct ASTNodeExprAccel {
    ASTNodeHeader Header;
    TypeIndex     Type;
} PACKED;
PACKED_STRUCT_END;

struct ASTNodeFieldAccess {
    ASTNodeHeader Header;
    TypeIndex     Type;
    NameIndex     Name;
    u32           Slot;
};

struct ASTNodeUnary {
    ASTNodeHeader Header;
    TypeIndex     Type;
    ExprIndex     Expr;
};

struct ASTNodeBinary {
    ASTNodeHeader Header;
    TypeIndex     Type;
    ExprIndex     Left;
}; // NOTE(nathan): followed by Right

struct ASTNodeTernary {
    ASTNodeHeader Header;
    TypeIndex     Type;
    ExprIndex     Cond;
    ExprIndex     Then;
}; // NOTE(nathan): followed by Else

struct ASTNodeExprList {
    ASTNodeHeader Header;
    i32           Count;
    ExprIndex     ExprList[1];                                                  // NOTE(nathan): expressions in left-to-right order
};

struct ASTNodeLiteral {
    ASTNodeHeader Header;
    TypeIndex     Type;
    Value         Val;
};

struct ASTNodeExprIdent {
    ASTNodeHeader Header;
    TypeIndex     Type;
    NameIndex     Name;
    BindIndex     Bind;
    ExprIndex     ScopeBase;
    u32           Slot;
};

struct ASTNodeError {
    ASTNodeHeader Header;
};

struct AST {
    Arena*       MemPool;
    u8*          Stmts;
    u8*          Exprs;
    u8*          Decls;
    u8*          Inits;
    i32*         Work;
    ASTResolved* Resolved;
    ParsedFile*  Files;
    StmtIndex    Root;
    u32          FilesCount;
    u32          StmtsCount;
    u32          ExprsCount;
    u32          DeclsCount;
    u32          InitsCount;
    u32          WorkCount;
    u32          ResolvedCount;
    u32          ResolvedMask;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
API_INTERNAL b32
ASTExprIsTyped(u8 NodeType)
{
    b32 Result = (AST_TYPED_EXPR_MASK >> NodeType) & 1ULL;

    return Result;
}

#endif // __FLV_AST_H__
