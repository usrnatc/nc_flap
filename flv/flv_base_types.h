#if !defined(__FLV_BASE_TYPES_H__)
#define __FLV_BASE_TYPES_H__

#include "nc_types.h"

struct StmtIndex {
    u32 V;
};

struct ExprIndex {
    i32 V;
};

struct DeclIndex {
    u32 V;
};

struct InitIndex {
    u32 V;
};

struct InfoIndex {
    i32 V;
};

struct TypeIndex {
    u32 V;
};

struct TypedNameIndex {
    u32 V;
};

struct BindIndex {
    u32 V;
};

struct TokIndex {
    u32 V;
};

struct NameIndex {
    u32 V;
};

struct ScopeIndex {
    u32 V;
};

struct Value {
    union {
        i64 Signed;
        u64 Unsigned;
        f64 Double;

        struct { 
            u8* Str; 
            u64 Count; 
        } String;

        void* Pointer;
    };

    u32 Type;
    b32 IsInvalid;
};

#endif // __FLV_BASE_TYPES_H__
