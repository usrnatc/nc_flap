#if !defined(__FLV_TYPE_CHECK_H__)
#define __FLV_TYPE_CHECK_H__

#include "nc_types.h"
#include "flv_base_types.h"
#include "flv_schema.h"

struct AST;
struct Arena;

// @defines___________________________________________________________________
#define CHECK_WORK_KIND_OFFSET 30
#define CHECK_WORK_ARG_MASK    0x3FFFFFFFU
#define CHECK_DEFAULT_INT_TYPE BUILTIN_i64
#define CHECK_BOOL_TYPE        BUILTIN_b32
#define CHECK_SLOT_MAX         1024

typedef u32 CheckWorkKind;
enum : u32 {
    CHECK_WORK_KIND_BIND
};

// @types_____________________________________________________________________
struct AdoptFrame {
    AdoptFrame* Prev;
    ExprIndex   Expr;
};

struct CheckState {
    AST*             Tree;
    FLVSchema const* Schema;
    FLVSlot*         Slots;
    u32              SlotsCount;
};

// @runtime___________________________________________________________________


// @functions_________________________________________________________________
void CheckAST(Arena* MemPool, AST* Tree, FLVSchema const* Schema, OUT FLVSlot** Slots, OUT u32* SlotsCount);
BindIndex ASTResolvedFind(AST* Tree, ExprIndex Expr);
u32 StmtNext(AST* Tree, u32 Offset);

#endif // __FLV_TYPE_CHECK_H__
