#include "nc_defines.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_tls.h"
#include "flv_symbol_table.h"
#include "flv_token.h"
#include "flv_intern.h"
#include "flv_type.h"

#if !defined(AST_NODE_INDEX_INVALID)
    #define AST_NODE_INDEX_INVALID U32_MAX
#endif

internal force_inline void
EnsureCurrentBindingCapacity(u32 Required)
{
    if (Required <= GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity)
        return;

    u32 OldCapacity = GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity;
    u32 NewCapacity = MAX(OldCapacity * 2, Required);
    BindIndex* NewTable = ArenaPushArray(
        GetTLS()->SYMBOL_TABLE->MemPool,
        BindIndex,
        NewCapacity
    );

    MemCpy(
        NewTable, 
        GetTLS()->SYMBOL_TABLE->CurrentBinding, 
        OldCapacity * sizeof(BindIndex)
    );
    MemSet(
        NewTable + OldCapacity,
        0xFF,
        (NewCapacity - OldCapacity) * sizeof(BindIndex)
    );
    GetTLS()->SYMBOL_TABLE->CurrentBinding = NewTable;
    GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity = NewCapacity;
}

void
SymTabPrepare(void)
{
    EnsureCurrentBindingCapacity(GetTLS()->SYMBOL_TABLE->Names->Count);
}

void 
SymTabInit(NamePool* Names)
{
    Arena* MemPool = ArenaAlloc();

    GetTLS()->SYMBOL_TABLE = ArenaPushArrayZero(
        MemPool,
        SymTab,
        1
    );
    GetTLS()->SYMBOL_TABLE->MemPool = MemPool;
    GetTLS()->SYMBOL_TABLE->Names = Names;
    GetTLS()->SYMBOL_TABLE->BindingsMemPool = ArenaAllocEx(GB(2));
    GetTLS()->SYMBOL_TABLE->UndoMemPool = ArenaAllocEx(GB(1));
    GetTLS()->SYMBOL_TABLE->Bindings = (Binding*) ArenaExBase(
        GetTLS()->SYMBOL_TABLE->BindingsMemPool
    );
    GetTLS()->SYMBOL_TABLE->UndoStack = (ScopeUndo*) ArenaExBase(
        GetTLS()->SYMBOL_TABLE->UndoMemPool
    );
    GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity = CURRENT_BIND_INIT_COUNT;
    GetTLS()->SYMBOL_TABLE->CurrentBinding = ArenaPushArray(
        MemPool,
        BindIndex,
        GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity
    );
    MemSet(
        GetTLS()->SYMBOL_TABLE->CurrentBinding, 
        0xFF, 
        GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity * sizeof(BindIndex)
    );
    ScopeEnter();       // NOTE(nc): enter global scope
}

#define X(N) #N,

void 
BuiltinTypesInit(void)
{
    local char const* BuiltinNames[] = {
        BUILTIN_TYPES(X)
    };

    ScopeIndex Root = {};

    for (u32 Index = 0; Index < BUILTIN_COUNT; ++Index) {
        Str8 NameString = Str((char*) BuiltinNames[Index]);
        NameIndex Name = InternName(GetTLS()->SYMBOL_TABLE->Names, NameString);

        ASSERT_ALWAYS(Name.V == Index);

        DeclIndex Decl = {
            AST_NODE_INDEX_INVALID
        };
        TypeIndex TIndex = {
            Index
        };
        BindIndex Bind = AddBinding(
            Root, 
            Name, 
            Decl, 
            TIndex, 
            BINDING_KIND_TYPE
        );

        ASSERT_ALWAYS(Bind.V == Index);
        ASSERT_ALWAYS(GetTLS()->SYMBOL_TABLE->CurrentBinding[Name.V].V == Index);
    }
}

#undef X

void 
SymTabReset(void)
{
    if (GetTLS()->SYMBOL_TABLE) {
        ArenaRelease(GetTLS()->SYMBOL_TABLE->BindingsMemPool);
        ArenaRelease(GetTLS()->SYMBOL_TABLE->UndoMemPool);
        ArenaRelease(GetTLS()->SYMBOL_TABLE->MemPool);
        GetTLS()->SYMBOL_TABLE = NULL;
    }
}

ScopeIndex 
ScopeEnter(void)
{
    ScopeIndex Result = {
        GetTLS()->SYMBOL_TABLE->UndoCount
    };

    ++GetTLS()->SYMBOL_TABLE->Depth;

    return Result;
}

void 
ScopeExit(ScopeIndex Mark)
{
    while (GetTLS()->SYMBOL_TABLE->UndoCount > Mark.V) {
        ScopeUndo* Undo = &GetTLS()->SYMBOL_TABLE->UndoStack[--GetTLS()->SYMBOL_TABLE->UndoCount];

        GetTLS()->SYMBOL_TABLE->CurrentBinding[Undo->Name.V] = Undo->OldBinding;
    }

    ArenaPopTo(
        GetTLS()->SYMBOL_TABLE->UndoMemPool,
        ARENA_HEADER_SIZE + GetTLS()->SYMBOL_TABLE->UndoCount * sizeof(ScopeUndo)
    );
    --GetTLS()->SYMBOL_TABLE->Depth;
}

BindIndex
PushBinding(
    ScopeIndex Current, 
    NameIndex Name, 
    DeclIndex Decl, 
    TypeIndex Type, 
    BindingKind Kind,
    BindIndex Overload
) {
    // BindIndex Result = {
    //     BINDING_INVALID_VALUE
    // };
    // BindIndex LocalIndex = FindLocalBinding(Current, Name);

    // if (LocalIndex.V != BINDING_INVALID_VALUE)
    //     return Result;

    // ASSERT(Name.V < GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity);
    BindIndex Result = {
        BINDING_INVALID_VALUE
    };

    ASSERT(Name.V < GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity);

    ScopeUndo* Undo = ArenaPushStruct(GetTLS()->SYMBOL_TABLE->UndoMemPool, ScopeUndo);

    ++GetTLS()->SYMBOL_TABLE->UndoCount;
    Undo->Name = Name;
    Undo->OldBinding = GetTLS()->SYMBOL_TABLE->CurrentBinding[Name.V];
    Result.V =  GetTLS()->SYMBOL_TABLE->BindingsCount++;

    Binding* NewBinding = ArenaPushStruct(
        GetTLS()->SYMBOL_TABLE->BindingsMemPool, 
        Binding
    );

    NewBinding->Name = Name;
    NewBinding->Decl = Decl;
    NewBinding->Type = Type;
    NewBinding->Scope = Current;
    NewBinding->Depth = SafeTruncateU32(GetTLS()->SYMBOL_TABLE->Depth);
    NewBinding->Kind = Kind;
    NewBinding->Overload = Overload;
    GetTLS()->SYMBOL_TABLE->CurrentBinding[Name.V] = Result;

    return Result;
}

BindIndex
AddBinding(
    ScopeIndex Current,
    NameIndex Name,
    DeclIndex Decl,
    TypeIndex Type,
    BindingKind Kind
) {
    BindIndex Result = {
        BINDING_INVALID_VALUE
    };
    BindIndex LocalIndex = FindLocalBinding(Current, Name);

    if (LocalIndex.V == BINDING_INVALID_VALUE)
        Result = PushBinding(Current, Name, Decl, Type, Kind, Result);

    return Result;
}

BindIndex
AddFuncBinding(
    ScopeIndex Current, 
    NameIndex Name, 
    DeclIndex Decl, 
    TypeIndex Type
) {
    BindIndex Result = {
        BINDING_INVALID_VALUE
    };
    BindIndex LocalIndex = FindLocalBinding(Current, Name);
    b32 CanBind = (
        LocalIndex.V == BINDING_INVALID_VALUE ||
        GetTLS()->SYMBOL_TABLE->Bindings[LocalIndex.V].Kind == BINDING_KIND_FUNC
    );

    if (CanBind) {
        Result = PushBinding(
            Current,
            Name,
            Decl,
            Type,
            BINDING_KIND_FUNC,
            LocalIndex
        );
    }

    return Result;
}

BindIndex 
FindBinding(NameIndex Name)
{
    BindIndex Result = {
        BINDING_INVALID_VALUE
    };

    ASSERT(Name.V < GetTLS()->SYMBOL_TABLE->CurrentBindingCapacity);

    Result = GetTLS()->SYMBOL_TABLE->CurrentBinding[Name.V];

    return Result;
}

BindIndex 
FindLocalBinding(ScopeIndex Current, NameIndex Name)
{
    BindIndex Result = FindBinding(Name);

    if (Result.V == BINDING_INVALID_VALUE)
        return Result;

    if (GetTLS()->SYMBOL_TABLE->Bindings[Result.V].Scope.V != Current.V) {
        Result = {
            BINDING_INVALID_VALUE
        };
    }

    return Result;
}
