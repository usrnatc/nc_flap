#include "flv_type.h"
#include "nc_print.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "flv_symbol_table.h"

TypeTable* TYPE_TABLE = NULL;

internal force_inline u32
HashMix(u32 BaseHash, u32 Value)
{
    BaseHash = (BaseHash ^ Value) * 0x9E3779B1U;
    BaseHash ^= BaseHash >> 15;

    return BaseHash;
}

internal force_inline u32
Hash(Type const* Key, u32 const* Args, u32 ArgCount)
{
    u32 Result = HashMix(
        0x811C9DC5U,
        Key->Kind | (Key->Flags << 8) | (Key->Count << 16)
    );

    if (Key->Kind != TYPE_KIND_FUNC)
        Result = HashMix(Result, Key->A);

    if (Key->Kind != TYPE_KIND_INSTANCE)
        Result = HashMix(Result, Key->B);

    for (u32 Index = 0; Index < ArgCount; ++Index)
        Result = HashMix(Result, Args[Index]);

    return Result;
}

internal b32
TypeKeyCmp(
    Type const* Candidate, 
    Type const* Key, 
    u32 const* Args, 
    u32 ArgsCount
) {
    b32 Result = FALSE;

    if (
        Candidate->Kind != Key->Kind ||
        Candidate->Flags != Key->Flags ||
        Candidate->Count != Key->Count
    ) {
        return Result;
    }

    switch (Key->Kind) {
        default: {
            Result = (
                Candidate->A == Key->A &&
                Candidate->B == Key->B
            );
        } break;

        case TYPE_KIND_FUNC: {
            Result = MemCmp(
                TYPE_TABLE->Args + Candidate->A,
                Args,
                ArgsCount * sizeof(u32)
            );
        } break;

        case TYPE_KIND_INSTANCE: {
            Result = (
                Candidate->A == Key->A &&
                MemCmp(
                    TYPE_TABLE->Args + Candidate->B,
                    Args,
                    ArgsCount * sizeof(u32)
                )
            );
        } break;
    }

    return Result;
}

internal void
TypeTableExpand(void)
{
    u32 NewSize = (TYPE_TABLE->HashTableMask + 1) * 2;

    TYPE_TABLE->HashTableMask = NewSize - 1;
    ArenaClear(TYPE_TABLE->HashMemPool);
    TYPE_TABLE->HashTable = ArenaPushArray(
        TYPE_TABLE->HashMemPool,
        TypeIndex,
        NewSize
    );
    MemSet(TYPE_TABLE->HashTable, 0xFF, NewSize * sizeof(TypeIndex));

    for (u32 Index = 0; Index < TYPE_TABLE->Count; ++Index) {
        u32 SlotIndex = TYPE_TABLE->Types[Index].Hash & TYPE_TABLE->HashTableMask;

        while (TYPE_TABLE->HashTable[SlotIndex].V != TYPE_INDEX_INVALID_TYPE)
            SlotIndex = (SlotIndex + 1) & TYPE_TABLE->HashTableMask;

        TYPE_TABLE->HashTable[SlotIndex] = {
            Index
        };
    }
}

internal TypeIndex
TypeIntern(Type Key, u32 const* Args = NULL, u32 ArgsCount = 0)
{
    u32 THash = Hash(&Key, Args, ArgsCount);
    u32 SlotIndex = THash & TYPE_TABLE->HashTableMask;

    for (;;) {
        TypeIndex Candidate = TYPE_TABLE->HashTable[SlotIndex];

        if (Candidate.V == TYPE_INDEX_INVALID_TYPE) {
            if ((TYPE_TABLE->Count + 1) * 10 > (TYPE_TABLE->HashTableMask + 1) * 7) {
                TypeTableExpand();
                SlotIndex = THash & TYPE_TABLE->HashTableMask;
                continue;
            }

            break;
        }

        Type const* T = &TYPE_TABLE->Types[Candidate.V];

        if (T->Hash == THash && TypeKeyCmp(T, &Key, Args, ArgsCount))
            return Candidate;

        SlotIndex = (SlotIndex + 1) & TYPE_TABLE->HashTableMask;
    }

    if (ArgsCount) {
        u32* Stored = ArenaPushArray(
            TYPE_TABLE->ArgMemPool,
            u32,
            ArgsCount
        );

        MemCpy(Stored, Args, ArgsCount * sizeof(u32));

        if (Key.Kind == TYPE_KIND_INSTANCE)
            Key.B = TYPE_TABLE->ArgsCount;
        else
            Key.A = TYPE_TABLE->ArgsCount;

        TYPE_TABLE->ArgsCount += ArgsCount;
    }

    TypeIndex Result = {
        TYPE_TABLE->Count++
    };
    Type* NewType = ArenaPushStruct(
        TYPE_TABLE->TypeMemPool,
        Type
    );
    TypeIndex* PtrSlot = ArenaPushStruct(
        TYPE_TABLE->PtrToMemPool,
        TypeIndex
    );

    Key.Hash = THash;
    *NewType = Key;
    PtrSlot->V = TYPE_INDEX_INVALID_TYPE;
    TYPE_TABLE->HashTable[SlotIndex] = Result;

    return Result;
}

internal u32
ParseWidth(Str8 String, u64* Cursor)
{
    u32 Result = 0;

    while (
        *Cursor < String.Size &&
        String.Str[*Cursor] >= '0' &&
        String.Str[*Cursor] <= '9'
    ) {
        Result = Result * 10 + (String.Str[*Cursor] - '0');
        ++*Cursor;
    }

    return Result;
}

internal Type
BuiltinKeyFromName(NamePool* Names, Str8 Name)
{
    Type Result = {};

    if (Name.Size == 4 && StrMatch(Name, "void"_s8, 0)) {
        Result.Kind = TYPE_KIND_VOID;

        return Result;
    }

    u8 Lead = Name.Str[0];
    u64 Cursor = 1;

    switch (Lead) {
        case 'v':
        case 'm':
        case 'r': {
            Result.Kind = (Lead == 'v')
                ? TYPE_KIND_VEC
                : (Lead == 'm')
                    ? TYPE_KIND_MAT
                    : TYPE_KIND_RNG;
            Result.Count = SafeTruncateU32(ParseWidth(Name, &Cursor));

            NameIndex Underlying = InternName(
                Names,
                Str(
                    Name.Str + Cursor,
                    Name.Size - Cursor
                )
            );

            ASSERT_ALWAYS(Underlying.V < BUILTIN_COUNT);

            Result.A = Underlying.V;
        } break;

        case 'z': {
            Result.Kind = TYPE_KIND_SIMD;
            Result.A = ParseWidth(Name, &Cursor);
            Result.B = (Cursor >= Name.Size)
                ? BUILTIN_void
                : (Name.Str[Cursor] == 'f')
                    ? BUILTIN_f32
                    : BUILTIN_f64;
        } break;

        case 'i':
        case 'u': {
            Result.Kind = TYPE_KIND_INT;
            Result.Flags = (Lead == 'i')
                ? TYPE_FLAG_SIGNED
                : 0;
            Result.A = ParseWidth(Name, &Cursor);
        } break;

        case 'f': {
            Result.Kind = TYPE_KIND_FLOAT;
            Result.A = ParseWidth(Name, &Cursor);
        } break;

        case 'b': {
            Result.Kind = TYPE_KIND_BOOL;
            Result.A = ParseWidth(Name, &Cursor);
        } break;

        default: {
            UNREACHABLE();
        } break;
    }

    return Result;
}

#define X(N) #N,

void 
TypeTableInit(NamePool* Names)
{
    local char const* BuiltinNames[] = {
        BUILTIN_TYPES(X)
    };
    Arena* MemPool = ArenaAlloc(); // FIXME(nc): it is a little crazy to spin up a whole arena for one allocation but we only run once so who cares i guess?

    TYPE_TABLE = ArenaPushStructZero(MemPool, TypeTable);
    TYPE_TABLE->TypeMemPool = ArenaAllocEx(GB(1));
    TYPE_TABLE->ArgMemPool = ArenaAllocEx(GB(1));
    TYPE_TABLE->PtrToMemPool = ArenaAllocEx(GB(1));
    TYPE_TABLE->HashMemPool = ArenaAllocEx(GB(1));
    TYPE_TABLE->Types = (Type*) ArenaExBase(TYPE_TABLE->TypeMemPool);
    TYPE_TABLE->Args = (u32*) ArenaExBase(TYPE_TABLE->ArgMemPool);
    TYPE_TABLE->PtrTo = (TypeIndex*) ArenaExBase(TYPE_TABLE->PtrToMemPool);
    TYPE_TABLE->HashTableMask = TYPE_TABLE_HASH_SIZE - 1;
    TYPE_TABLE->HashTable = ArenaPushArray(
        TYPE_TABLE->HashMemPool,
        TypeIndex,
        TYPE_TABLE_HASH_SIZE
    );
    MemSet(
        TYPE_TABLE->HashTable,
        0xFF,
        TYPE_TABLE_HASH_SIZE * sizeof(TypeIndex)
    );

    for (u32 Index = 0; Index < BUILTIN_COUNT; ++Index) {
        Type Key = BuiltinKeyFromName(Names, Str((char*) BuiltinNames[Index]));
        TypeIndex Result = TypeIntern(Key);

        if (Result.V != Index) {
            PrintOut(
                "builtin %u \"%s\": got %u, key kind %u flags %u count %u A %u B %u, names %u\n",
                Index, BuiltinNames[Index], Result.V,
                Key.Kind, Key.Flags, Key.Count, Key.A, Key.B,
                Names->Count
            );
        }

        ASSERT_ALWAYS(Result.V == Index);
    }
}

#undef X

void 
TypeTableRelease(void)
{
    if (TYPE_TABLE) {
        ArenaRelease(TYPE_TABLE->TypeMemPool);
        ArenaRelease(TYPE_TABLE->ArgMemPool);
        ArenaRelease(TYPE_TABLE->PtrToMemPool);
        ArenaRelease(TYPE_TABLE->HashMemPool);
        TYPE_TABLE = NULL;
    }
}

TypeIndex 
TypePtrTo(TypeIndex Underlying)
{
    TypeIndex Cached = TYPE_TABLE->PtrTo[Underlying.V];

    if (Cached.V != TYPE_INDEX_INVALID_TYPE)
        return Cached;

    Type Key = {};

    Key.Kind = TYPE_KIND_PTR;
    Key.Flags = TypeAt(Underlying)->Flags & TYPE_FLAG_GENERIC;
    Key.A = Underlying.V;

    TypeIndex Result = TypeIntern(Key);

    TYPE_TABLE->PtrTo[Underlying.V] = Result;

    return Result;
}

TypeIndex 
TypeArrayOf(TypeIndex Underlying, u32 Count, b32 Unsized)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_ARR;
    Key.Flags = (
        (Unsized ? TYPE_FLAG_UNSIZED : 0) | 
        (TypeAt(Underlying)->Flags & TYPE_FLAG_GENERIC)
    );
    Key.A = Underlying.V;
    Key.B = Unsized ? 0 : Count;

    return TypeIntern(Key);
}

TypeIndex 
TypeFunc(TypeIndex const* Sig, u32 ParamsCount)
{
    ASSERT(ParamsCount <= U16_MAX);

    u8 Generic = 0;

    for (u32 Index = 0; Index <= ParamsCount; ++Index)
        if (Sig[Index].V != TYPE_INDEX_INVALID_TYPE) 
            Generic |= TypeAt(Sig[Index])->Flags & TYPE_FLAG_GENERIC;

    Type Key = {};

    Key.Kind = TYPE_KIND_FUNC;
    Key.Flags = Generic;
    Key.Count = SafeTruncateU32(ParamsCount);

    TypeIndex Result = TypeIntern(Key, (u32 const*) Sig, ParamsCount + 1);

    return Result;
}

TypeIndex 
TypeNominal(TypeKind Kind, DeclIndex Decl)
{
    ASSERT(
        Kind == TYPE_KIND_STRUCT ||
        Kind == TYPE_KIND_UNION ||
        Kind == TYPE_KIND_ENUM
    );

    Type Key = {};

    Key.Kind = Kind;
    Key.A = Decl.V;

    return TypeIntern(Key);
}

TypeIndex 
TypeInstance(DeclIndex Generic, TypeIndex const* Args, u32 ArgsCount)
{
    ASSERT(ArgsCount && ArgsCount <= U16_MAX);

    Type Key = {};
    u8 GenericFlag = 0;

    for (u32 Index = 0; Index < ArgsCount; ++Index)
        GenericFlag |= TypeAt(Args[Index])->Flags & TYPE_FLAG_GENERIC;

    Key.Kind = TYPE_KIND_INSTANCE;
    Key.Flags = GenericFlag;
    Key.Count = SafeTruncateU32(ArgsCount);
    Key.A = Generic.V;

    return TypeIntern(Key, (u32 const*) Args, ArgsCount);
}

TypeIndex 
TypeEnum(DeclIndex Decl, TypeIndex Underlying)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_ENUM;
    Key.A = Decl.V;
    Key.B = Underlying.V;

    return TypeIntern(Key);
}

TypeIndex 
TypeParam(BindIndex Bind, u32 Ordinal)
{
    ASSERT(Ordinal < TYPE_GENERIC_MAX);

    Type Key = {};

    Key.Kind = TYPE_KIND_PARAM;
    Key.Flags = TYPE_FLAG_GENERIC;
    Key.A = Bind.V;
    Key.B = Ordinal;

    return TypeIntern(Key);
}

TypeIndex 
TypeMessage(NameIndex Name, u32 InstanceOrZero)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_MESSAGE;
    Key.Count = SafeTruncateU32(InstanceOrZero);
    Key.A = Name.V;

    return TypeIntern(Key);
}

TypeIndex 
TypeSeries(TypeIndex Elem, TypeIndex Message)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_SERIES;
    Key.Flags = (
        TypeAt(Elem)->Flags |
        TypeAt(Message)->Flags
    ) & TYPE_FLAG_GENERIC;
    Key.A = Elem.V;
    Key.B = Message.V;

    return TypeIntern(Key);
}

TypeIndex 
TypeSelection(TypeIndex Message)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_SELECTION;
    Key.Flags = TypeAt(Message)->Flags & TYPE_FLAG_GENERIC;
    Key.B = Message.V;

    return TypeIntern(Key);
}

TypeIndex 
TypeTime(void)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_TIME;

    return TypeIntern(Key);
}

TypeIndex 
TypeString(void)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_STRING;

    return TypeIntern(Key);
}

TypeIndex 
TypeDuration(void)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_DURATION;

    return TypeIntern(Key);
}

TypeIndex 
TypeMode(void)
{
    Type Key = {};

    Key.Kind = TYPE_KIND_MODE;

    return TypeIntern(Key);
}

internal TypeIndex
TypeRebuild(TypeIndex Src, TypeIndex const* Values, u32 ValuesCount)
{
    Type Info = *TypeAt(Src);
    TypeIndex Result = Src;

    switch (Info.Kind) {
        default: {} break;

        case TYPE_KIND_PTR: {
            Result = TypePtrTo(Values[0]);
        } break;

        case TYPE_KIND_ARR: {
            Result = TypeArrayOf(
                Values[0],
                Info.B,
                Info.Flags & TYPE_FLAG_UNSIZED
            );
        } break;

        case TYPE_KIND_FUNC: {
            Result = TypeFunc(Values, ValuesCount - 1);
        } break;

        case TYPE_KIND_INSTANCE: {
            DeclIndex Generic = {
                Info.A
            };

            Result = TypeInstance(Generic, Values, ValuesCount);
        } break;

        case TYPE_KIND_SERIES: {
            Result = TypeSeries(Values[0], Values[1]);
        } break;

        case TYPE_KIND_SELECTION: {
            Result = TypeSelection(Values[0]);
        } break;
    }

    return Result;
}

TypeIndex 
TypeSub(TypeIndex T, TypeIndex const* Args, u32 ArgsCount)
{
    if (
        T.V == TYPE_INDEX_INVALID_TYPE || 
        !(TypeAt(T)->Flags & TYPE_FLAG_GENERIC)
    ) {
        return T;
    }

    TypeWalk Walk = {};
    TypeRecord Record = {};
    TypeIndex Values[TYPE_WALK_VALUES];
    u32 Base[TYPE_WALK_DEPTH];
    u32 ValuesCount = 0;

    for (
        TypeIndex Node = T; 
        Node.V != TYPE_INDEX_INVALID_TYPE; 
        Node = Record.Next
    ) {
        Record = TypeRecordDepthFirst(&Walk, Node);

        if (Record.PushCount) {
            Base[Walk.Depth - 1] = ValuesCount;
            continue;
        }

        Type const* Info = TypeAt(Node);
        TypeIndex Leaf = Node;

        if (Info->Kind == TYPE_KIND_PARAM) {
            ASSERT(Info->B < ArgsCount);

            Leaf = Args[Info->B];
        }

        ASSERT(ValuesCount < TYPE_WALK_VALUES);

        Values[ValuesCount++] = Leaf;

        for (i32 Pop = Record.PopCount - 1; Pop >= 0; --Pop) {
            u32 FrameIndex = Walk.Depth + Pop;
            u32 FrameBase = Base[FrameIndex];
            TypeIndex Folded = TypeRebuild(
                Walk.Stack[FrameIndex].T,
                Values + FrameBase,
                ValuesCount - FrameBase
            );

            ValuesCount = FrameBase;
            Values[ValuesCount++] = Folded;
        }
    }

    ASSERT(ValuesCount == 1);

    return Values[0];
}

b32 
TypeUnify(TypeIndex Param, TypeIndex Arg, TypeIndex* Bound, u32 BoundCount)
{
    b32 Result = TRUE;

    if (Arg.V == TYPE_INDEX_INVALID_TYPE)
        return Result;

    TypeWalk Walk = {};
    TypeRecord Record = {};
    TypeIndex ArgStack[TYPE_WALK_DEPTH];
    TypeIndex A = Arg;

    for (
        TypeIndex P = Param; 
        P.V != TYPE_INDEX_INVALID_TYPE && Result; 
        P = Record.Next
    ) {
        Type const* PInfo = TypeAt(P);

        if (!(PInfo->Flags & TYPE_FLAG_GENERIC)) {
            Result = (P.V == A.V);
        } else if (PInfo->Kind == TYPE_KIND_PARAM) {
            ASSERT(PInfo->B < BoundCount);

            if (Bound[PInfo->B].V == TYPE_INDEX_INVALID_TYPE)
                Bound[PInfo->B] = A;
            else
                Result = (Bound[PInfo->B].V == A.V);
        } else {
            Type const* AInfo = TypeAt(A);

            Result = (
                AInfo->Kind == PInfo->Kind &&
                AInfo->Count == PInfo->Count &&
                (PInfo->Kind != TYPE_KIND_INSTANCE || AInfo->A == PInfo->A) &&
                (
                    PInfo->Kind != TYPE_KIND_ARR ||
                    (
                        AInfo->B == PInfo->B &&
                        (AInfo->Flags & TYPE_FLAG_UNSIZED) == (PInfo->Flags & TYPE_FLAG_UNSIZED)
                    )
                )
            );
        }

        if (!Result)
            break;

        Record = TypeRecordDepthFirst(&Walk, P);

        if (Record.PushCount) {
            ArgStack[Walk.Depth - 1] = A;
            A = TypeChild(TypeAt(A), 0);
        } else if (Record.Next.V != TYPE_INDEX_INVALID_TYPE) {
            u32 Top = Walk.Depth - 1;

            A = TypeChild(
                TypeAt(ArgStack[Top]),
                Walk.Stack[Top].Child
            );
        }
    }

    return Result;
}

TypeRecord 
TypeRecordDepthFirst(TypeWalk* Walk, TypeIndex Node)
{
    TypeRecord Result = {
        {
            TYPE_INDEX_INVALID_TYPE
        }
    };
    Type const* Info = TypeAt(Node);
    u32 ChildCount = TypeChildCount(Info);

    if (ChildCount) {
        ASSERT(Walk->Depth < TYPE_WALK_DEPTH);

        TypeWalkFrame* Frame = &Walk->Stack[Walk->Depth++];

        Frame->T = Node;
        Frame->Child = 0;
        Frame->Count = ChildCount;
        Result.Next = TypeChild(Info, 0);
        Result.PushCount = 1;
    } else {
        while (Walk->Depth) {
            TypeWalkFrame* Frame = &Walk->Stack[Walk->Depth - 1];

            if (++Frame->Child < Frame->Count) {
                Result.Next = TypeChild(TypeAt(Frame->T), Frame->Child);
                break;
            }

            --Walk->Depth;
            ++Result.PopCount;
        }
    }

    return Result;
}
