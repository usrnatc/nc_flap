#if !defined(__FLI_BIN_H__)
#define __FLI_BIN_H__

#include "nc_types.h"
#include "nc_string.h"
#include "fli.h"

// @defines____________________________________________________________________
#define BIN_HEAD_BYTE_0      0xA3
#define BIN_HEAD_BYTE_1      0x95
#define BIN_HEAD_SIZE        3
#define BIN_FMT_TYPE         0x80
#define BIN_FMT_LENGTH       89
#define BIN_TYPE_COUNT       256
#define BIN_MAX_FIELD_COUNT  16

typedef u32 B2FSpecialKind;
enum : u32 {
    B2F_SPECIAL_KIND_NONE,
    B2F_SPECIAL_KIND_FMT,
    B2F_SPECIAL_KIND_FMTU,
    B2F_SPECIAL_KIND_UNIT,
    B2F_SPECIAL_KIND_MULT,
    B2F_SPECIAL_KIND_PARM,
    B2F_SPECIAL_KIND_MSG,
    B2F_SPECIAL_KIND_MODE,
    B2F_SPECIAL_KIND_EV,
    B2F_SPECIAL_KIND_ERR,
    B2F_SPECIAL_KIND_COUNT
};

// @types______________________________________________________________________
struct B2FConvertParams {
    Str8 InputBINName;
    Str8 InputBINData;
};

struct B2FFormatCharInfo {
    FLIScalarKind Kind;
    u8            Size;
    f64           Scale;
};

struct B2FOffsetChunk {
    B2FOffsetChunk* Next;
    u64*            V;
    u64             Count;
    u64             Capacity;
};

struct B2FFormat {
    u8                  Type;
    u8                  Length;
    B2FSpecialKind      Special;
    b32                 Supported;
    b32                 HasTime;
    Str8                Name;
    Str8                FormatString;
    u32                 FieldCount;
    u32                 TimeField;
    u32                 InstanceField;
    Str8                FieldNames[BIN_MAX_FIELD_COUNT];
    u8                  FieldChars[BIN_MAX_FIELD_COUNT];
    u8                  FieldOffsets[BIN_MAX_FIELD_COUNT];
    u8                  UnitIds[BIN_MAX_FIELD_COUNT];
    u8                  MultIds[BIN_MAX_FIELD_COUNT];
    u64                 RecordCount;
    B2FOffsetChunk*     HeadOffsets;
    B2FOffsetChunk*     TailOffsets;
    FLIBakeMessageType* MessageType;
};

// @functions__________________________________________________________________
FLIBakeParams B2FConvert(Arena* MemPool, B2FConvertParams* Params);

#endif // __FLI_BIN_H__
