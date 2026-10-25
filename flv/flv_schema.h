#if !defined(__FLV_SCHEMA_H__)
#define __FLV_SCHEMA_H__

#include "nc_types.h"
#include "flv_base_types.h"

struct Arena;
struct FLIParsed;

// @defines____________________________________________________________________
#define FLV_SCHEMA_NOT_FOUND   U32_MAX
#define FLV_INSTANCE_ANY       U32_MAX
#define FLV_SCHEMA_FIELD_MAX     32
#define FLV_SCHEMA_MESSAGE_MAX 1024

typedef u8 FLVSlotKind;
enum : u8 {
    FLV_SLOT_KIND_MESSAGE,
    FLV_SLOT_KIND_FIELD,
    FLV_SLOT_KIND_MODE
};

// @types______________________________________________________________________
struct FLVSchemaField {
    NameIndex Name;
    TypeIndex Element;
};

struct FLVSchemaMessage {
    NameIndex Name;
    u32       HeadField;
    u32       FieldCount;
    u32       MaxInstance;
};

struct FLVSchema {
    FLVSchemaMessage* Messages;
    FLVSchemaField*   Fields;
    u32*              MessageFromName;
    u32               MessageCount;
    u32               FieldCount;
    u32               NameCount;
    u64               Hash;
};

struct FLVSlot {
    NameIndex   Message;
    NameIndex   Field;
    u32         Instance;
    FLVSlotKind Kind;
    u8          __Padding[3];
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
void FLVSchemaBuild(Arena* MemPool, FLIParsed** Flights, u32 FlightsCount, OUT FLVSchema* Schema);

INTERNAL u32
FLVSchemaFindMessage(FLVSchema const* Schema, NameIndex Name)
{
    u32 Result = FLV_SCHEMA_NOT_FOUND;

    if (Name.V < Schema->NameCount)
        Result = Schema->MessageFromName[Name.V];

    return Result;
}

INTERNAL u32
FLVSchemaFindField(FLVSchema const* Schema, u32 Message, NameIndex Name)
{
    FLVSchemaMessage const* M = &Schema->Messages[Message];
    u32 Low = M->HeadField;
    u32 High = M->HeadField + M->FieldCount;

    while (Low < High) {
        u32 Mid = Low + (High - Low) / 2;
        u32 Key = Schema->Fields[Mid].Name.V;

        if (Key < Name.V)
            Low = Mid + 1;
        else if (Key > Name.V)
            High = Mid;
        else
            return Mid;
    }

    return FLV_SCHEMA_NOT_FOUND;
}

#endif // __FLV_SCHEMA_H__
