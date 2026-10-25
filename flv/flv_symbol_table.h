#if !defined(__FLV_SYMBOL_TABLE_H__)
#define __FLV_SYMBOL_TABLE_H__

#include "nc_arena.h"
#include "nc_string.h"
#include "flv_base_types.h"
#include "flv_ast.h"

struct NamePool;

#define BINDING_INVALID_VALUE U32_MAX
#define SCOPE_INVALID_VALUE   U32_MAX

#define CURRENT_BIND_INIT_COUNT 1024

#define BUILTIN_ELEMENTS(X, P) \
    X(P##i8)                   \
    X(P##i16)                  \
    X(P##i32)                  \
    X(P##i64)                  \
    X(P##u8)                   \
    X(P##u16)                  \
    X(P##u32)                  \
    X(P##u64)                  \
    X(P##f32)                  \
    X(P##f64)                  \
    X(P##z128)                 \
    X(P##z128f)                \
    X(P##z128d)                \
    X(P##z256)                 \
    X(P##z256f)                \
    X(P##z256d)                \
    X(P##z512)                 \
    X(P##z512f)                \
    X(P##z512d)

#define BUILTIN_TYPES(X)    \
    X(void)                 \
    X(i8)                   \
    X(i16)                  \
    X(i32)                  \
    X(i64)                  \
    X(u8)                   \
    X(u16)                  \
    X(u32)                  \
    X(u64)                  \
    X(f32)                  \
    X(f64)                  \
    X(b8)                   \
    X(b16)                  \
    X(b32)                  \
    X(b64)                  \
    X(z128)                 \
    X(z128f)                \
    X(z128d)                \
    X(z256)                 \
    X(z256f)                \
    X(z256d)                \
    X(z512)                 \
    X(z512f)                \
    X(z512d)                \
    BUILTIN_ELEMENTS(X, v2) \
    BUILTIN_ELEMENTS(X, v3) \
    BUILTIN_ELEMENTS(X, v4) \
    BUILTIN_ELEMENTS(X, m2) \
    BUILTIN_ELEMENTS(X, m3) \
    BUILTIN_ELEMENTS(X, m4) \
    BUILTIN_ELEMENTS(X, r1) \
    BUILTIN_ELEMENTS(X, r2) \
    BUILTIN_ELEMENTS(X, r3) \
    BUILTIN_ELEMENTS(X, r4)

#define X(N) BUILTIN_##N,
enum : u32 {
    BUILTIN_TYPES(X)
    BUILTIN_COUNT
};
#undef X

typedef u8 BindingKind;
enum : u8 {
    BINDING_KIND_VALUE,
    BINDING_KIND_TYPE,
    BINDING_KIND_GENERIC,
    BINDING_KIND_FUNC
};

struct Binding {
    NameIndex   Name;
    DeclIndex   Decl;
    TypeIndex   Type;
    ScopeIndex  Scope;
    BindIndex   Overload;
    u16         Depth;                                                          // NOTE(nc): diagnostics only
    BindingKind Kind;
};

struct ScopeUndo {
    NameIndex Name;
    BindIndex OldBinding;
};

struct SymTab {
    Arena*     MemPool;
    Arena*     BindingsMemPool;
    Arena*     UndoMemPool;
    NamePool*  Names;
    BindIndex* CurrentBinding;
    Binding*   Bindings;
    ScopeUndo* UndoStack;
    u32        CurrentBindingCapacity;
    u32        BindingsCount;
    u32        UndoCount;
    u32        Depth;
};

void SymTabInit(NamePool* Names);
void BuiltinTypesInit(void);
void SymTabPrepare(void);
void SymTabReset(void);
ScopeIndex ScopeEnter(void);
void ScopeExit(ScopeIndex Mark);
BindIndex AddBinding(ScopeIndex Current, NameIndex Name, DeclIndex Decl, TypeIndex Type, BindingKind Kind);
BindIndex AddFuncBinding(ScopeIndex Current, NameIndex Name, DeclIndex Decl, TypeIndex Type);
BindIndex FindBinding(NameIndex Name);
BindIndex FindLocalBinding(ScopeIndex Current, NameIndex Name);

#endif // __FLV_SYMBOL_TABLE_H__
