#if !defined(__FLV_TYPE_H__)
#define __FLV_TYPE_H__

#include "nc_types.h"
#include "nc_arena.h"
#include "flv_base_types.h"
#include "flv_intern.h"

// @defines____________________________________________________________________
#define TYPE_INDEX_INVALID_TYPE U32_MAX
#define TYPE_TABLE_HASH_SIZE    4096

typedef u8 TypeKind;
enum : u8 {
    TYPE_KIND_VOID,
    TYPE_KIND_INT,
    TYPE_KIND_FLOAT,
    TYPE_KIND_BOOL,
    TYPE_KIND_SIMD,
    TYPE_KIND_VEC,
    TYPE_KIND_MAT,
    TYPE_KIND_RNG,
    TYPE_KIND_PTR,
    TYPE_KIND_ARR,
    TYPE_KIND_FUNC,
    TYPE_KIND_STRUCT,
    TYPE_KIND_UNION,
    TYPE_KIND_ENUM,
    TYPE_KIND_INSTANCE,
    TYPE_KIND_PARAM,
    TYPE_KIND_MESSAGE,
    TYPE_KIND_SERIES,
    TYPE_KIND_SELECTION,
    TYPE_KIND_TIME,
    TYPE_KIND_DURATION,
    TYPE_KIND_MODE,
    TYPE_KIND_STRING,
    TYPE_KIND_COUNT
};

typedef u8 TypeFlag;
enum : u8 {
    TYPE_FLAG_SIGNED  = (1 << 0),
    TYPE_FLAG_UNSIZED = (1 << 1),
    TYPE_FLAG_GENERIC = (1 << 2)
};

#define TYPE_GENERIC_MAX 16
#define TYPE_WALK_DEPTH  32
#define TYPE_WALK_VALUES 64

// @types______________________________________________________________________
struct TypeWalkFrame {
    TypeIndex T;
    u32       Child;
    u32       Count;
};

struct TypeWalk {
    TypeWalkFrame Stack[TYPE_WALK_DEPTH];
    u32           Depth;
};

struct TypeRecord {
    TypeIndex Next;
    i32       PushCount;
    i32       PopCount;
};

struct Type {
    u32      Hash;
    u32      A;
    u32      B;
    u16      Count;
    TypeKind Kind;
    TypeFlag Flags;
};
STATIC_ASSERT(sizeof(Type) == 16, TypeSizeCheck);

struct TypeTable {
    Arena*     TypeMemPool;
    Arena*     ArgMemPool;
    Arena*     PtrToMemPool;
    Arena*     HashMemPool;
    Type*      Types;
    u32*       Args;
    TypeIndex* PtrTo;
    TypeIndex* HashTable;
    u32        Count;
    u32        ArgsCount;
    u32        HashTableMask;
};

// @runtime____________________________________________________________________
extern TypeTable* TYPE_TABLE;

// @functions__________________________________________________________________
void TypeTableInit(NamePool* Names);
void TypeTableRelease(void);
TypeIndex TypePtrTo(TypeIndex Underlying);
TypeIndex TypeArrayOf(TypeIndex Underlying, u32 Count, b32 Unsized);
TypeIndex TypeFunc(TypeIndex const* Sig, u32 ParamsCount);
TypeIndex TypeNominal(TypeKind Kind, DeclIndex Decl);
TypeIndex TypeInstance(DeclIndex Generic, TypeIndex const* Args, u32 ArgsCount);
TypeIndex TypeEnum(DeclIndex Decl, TypeIndex Underlying);
TypeIndex TypeParam(BindIndex Bind, u32 Ordinal);
TypeIndex TypeMessage(NameIndex Name, u32 InstanceOrZero);
TypeIndex TypeSeries(TypeIndex Elem, TypeIndex Message);
TypeIndex TypeSelection(TypeIndex Message);
TypeIndex TypeTime(void);
TypeIndex TypeString(void);
TypeIndex TypeDuration(void);
TypeIndex TypeMode(void);
TypeIndex TypeSub(TypeIndex T, TypeIndex const* Args, u32 ArgsCount);
b32 TypeUnify(TypeIndex Param, TypeIndex Arg, TypeIndex* Bound, u32 BoundCount);
TypeRecord TypeRecordDepthFirst(TypeWalk* Walk, TypeIndex Node);

INTERNAL Type*
TypeAt(TypeIndex Index)
{
    return &TYPE_TABLE->Types[Index.V];
}

INTERNAL TypeIndex const*
TypeArgs(Type const* _Type)
{
    u32 Offset = (_Type->Kind == TYPE_KIND_INSTANCE)
        ? _Type->B
        : _Type->A;

    return (TypeIndex const*) (TYPE_TABLE->Args + Offset);
}

INTERNAL u32
TypeChildCount(Type const* _Type)
{
    u32 Result = 0;

    if (_Type->Flags & TYPE_FLAG_GENERIC) {
        switch (_Type->Kind) {
            default: {} break;

            case TYPE_KIND_PTR:
            case TYPE_KIND_ARR: {
                Result = 1;
            } break;

            case TYPE_KIND_FUNC: {
                Result = _Type->Count + 1U;
            } break;

            case TYPE_KIND_INSTANCE: {
                Result = _Type->Count;
            } break;

            case TYPE_KIND_SELECTION: {
                Result = 1;
            } break;

            case TYPE_KIND_SERIES: {
                Result = 2;
            } break;
        }
    }

    return Result;
}

INTERNAL TypeIndex
TypeChild(Type const* _Type, u32 Index)
{
    TypeIndex Result = {};

    switch (_Type->Kind) {
        default: {
            Result = TypeArgs(_Type)[Index];
        } break;

        case TYPE_KIND_PTR:
        case TYPE_KIND_ARR: {
            Result.V = _Type->A;
        } break;

        case TYPE_KIND_SELECTION: {
            Result.V = _Type->B;
        } break;

        case TYPE_KIND_SERIES: {
            Result.V = Index 
                ? _Type->B 
                : _Type->A;
        } break;
    }

    return Result;
}

#endif // __FLV_TYPE_H__
