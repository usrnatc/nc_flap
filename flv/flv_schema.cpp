#include "nc_defines.h"
#include "nc_types.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "fli.h"
#include "flv_schema.h"
#include "flv_stdlib.h"
#include "flv_symbol_table.h"
#include "flv_intern.h"
#include "flv_type.h"

#define FLI_TABLE_FIRST_INDEX 1

struct SchemaBuildField {
    NameIndex Name;
    TypeIndex Elem;
};

struct SchemaBuildMessage {
    NameIndex        Name;
    u32              MaxInstance;
    u32              FieldCount;
    SchemaBuildField Fields[FLV_SCHEMA_FIELD_MAX];
};

INTERNAL TypeIndex
ElemFromScalarKind(FLIScalarKind Kind)
{
    TypeIndex Result = {
        BUILTIN_i64
    };

    switch (Kind) {
        default: {} break;

        case FLI_SCALAR_KIND_F32:
        case FLI_SCALAR_KIND_F64: {
            Result.V = BUILTIN_f64;
        } break;

        case FLI_SCALAR_KIND_CHAR4:
        case FLI_SCALAR_KIND_CHAR16:
        case FLI_SCALAR_KIND_CHAR64: {
            Result = TypeString();
        } break;
    }

    return Result;
}

INTERNAL TypeIndex
ElemWiden(TypeIndex Old, TypeIndex New)
{
    TypeIndex Result = Old;

    if (Old.V == BUILTIN_i64 && New.V == BUILTIN_f64)
        Result = New;

    return Result;
}

INTERNAL Str8
FLIStr(FLIParsed* FLI, u32 StringIndex)
{
    u64 Length = 0;
    u8* Base = FLIStrFromIndex(FLI, StringIndex, &Length);

    return Str(Base, Length);
}

INTERNAL u32
MaxInstanceFromMessageType(FLIParsed* FLI, FLIMessageType const* MessageType)
{
    u32 Result = 0;

    if (MessageType->InstanceField) {
        FLIField* Field = FLIElementFromNameIndex(
            FLI, 
            Fields, 
            FIELDS, 
            MessageType->HeadField + MessageType->InstanceField - 1
        );
        FLIColumn* Column = FLIElementFromNameIndex(
            FLI, 
            Columns, 
            COLUMNS, 
            Field->ColumnIndex
        );

        Result = (u32) Column->Max + 1;
    }

    return Result;
}

INTERNAL void
SchemaBuildAddFlight(
    FLIParsed* FLI, 
    SchemaBuildMessage* Messages, 
    u32* MessageCount, 
    u32* MessageFromName, 
    u32 NameCapacity
) {
    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;
    u64 MessageTypeCount = 0;
    FLIMessageType* MessageTypes = FLITableFromName(
        FLI, 
        MessageTypes, 
        MESSAGE_TYPES, 
        &MessageTypeCount
    );

    for (
        u64 Index = FLI_TABLE_FIRST_INDEX; 
        Index < MessageTypeCount; 
        ++Index
    ) {
        FLIMessageType* MessageType = &MessageTypes[Index];
        NameIndex Name = InternName(
            Names, 
            FLIStr(FLI, MessageType->NameStringIndex)
        );

        ASSERT_ALWAYS(Name.V < NameCapacity);

        u32 MessageIndex = MessageFromName[Name.V];

        if (MessageIndex == FLV_SCHEMA_NOT_FOUND) {
            ASSERT_ALWAYS(*MessageCount < FLV_SCHEMA_MESSAGE_MAX);

            MessageIndex = (*MessageCount)++;
            MessageFromName[Name.V] = MessageIndex;
            Messages[MessageIndex].Name = Name;
            Messages[MessageIndex].MaxInstance = 0;
            Messages[MessageIndex].FieldCount = 0;
        }

        SchemaBuildMessage* Message = &Messages[MessageIndex];

        Message->MaxInstance = MAX(
            Message->MaxInstance, 
            MaxInstanceFromMessageType(FLI, MessageType)
        );

        for (
            u32 FieldIndex = 0; 
            FieldIndex < MessageType->FieldCount; 
            ++FieldIndex
        ) {
            FLIField* Field = FLIElementFromNameIndex(
                FLI, 
                Fields, 
                FIELDS, 
                MessageType->HeadField + FieldIndex
            );
            NameIndex FieldName = InternName(
                Names, 
                FLIStr(FLI, Field->NameStringIndex)
            );
            TypeIndex Elem = ElemFromScalarKind(Field->StoredKind);
            u32 Found = Message->FieldCount;

            for (
                u32 Existing = 0; 
                Existing < Message->FieldCount; 
                ++Existing
            ) {
                if (Message->Fields[Existing].Name.V == FieldName.V) {
                    Found = Existing;
                    break;
                }
            }

            if (Found == Message->FieldCount) {
                ASSERT_ALWAYS(Message->FieldCount < FLV_SCHEMA_FIELD_MAX);

                Message->Fields[Found].Name = FieldName;
                Message->Fields[Found].Elem = Elem;
                ++Message->FieldCount;
            } else {
                Message->Fields[Found].Elem = ElemWiden(
                    Message->Fields[Found].Elem, 
                    Elem
                );
            }
        }
    }
}

INTERNAL void
SortFieldsByName(SchemaBuildField* Fields, u32 Count)
{
    for (u32 Index = 1; Index < Count; ++Index) {
        SchemaBuildField Key = Fields[Index];
        u32 Cursor = Index;

        while (Cursor && Fields[Cursor - 1].Name.V > Key.Name.V) {
            Fields[Cursor] = Fields[Cursor - 1];
            --Cursor;
        }

        Fields[Cursor] = Key;
    }
}

INTERNAL void
InternFlightNames(FLIParsed* FLI)
{
    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;
    u64 MessageTypeCount = 0;
    FLIMessageType* MessageTypes = FLITableFromName(
        FLI, 
        MessageTypes, 
        MESSAGE_TYPES, 
        &MessageTypeCount
    );

    for (
        u64 Index = FLI_TABLE_FIRST_INDEX; 
        Index < MessageTypeCount; 
        ++Index
    ) {
        FLIMessageType* MessageType = &MessageTypes[Index];

        InternName(Names, FLIStr(FLI, MessageType->NameStringIndex));

        for (
            u32 FieldIndex = 0; 
            FieldIndex < MessageType->FieldCount; 
            ++FieldIndex
        ) {
            FLIField* Field = FLIElementFromNameIndex(
                FLI, 
                Fields, 
                FIELDS, 
                MessageType->HeadField + FieldIndex
            );

            InternName(Names, FLIStr(FLI, Field->NameStringIndex));
        }
    }
}

INTERNAL FLVVehicleKind
VehicleFromFlight(FLIParsed* FLI)
{
    u64 Count = 0;
    FLIFileInfo* Infos = FLITableFromName(FLI, FileInfo, FILE_INFO, &Count);
    FLVVehicleKind Result = FLV_VEHICLE_KIND_UNKNOWN;

    if (Count) {
        Result = FLVVehicleKindFromStr(
            FLIStr(FLI, Infos[Count - 1].VehicleStringIndex)
        );
    }

    return Result;
}

void
FLVSchemaBuild(
    Arena* MemPool, 
    FLIParsed** Flights, 
    u32 FlightCount, 
    OUT FLVSchema* Schema
) {
    TempArena Scratch = GetScratch(&MemPool, 1);
    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;

    for (u32 Index = 0; Index < FlightCount; ++Index)
        InternFlightNames(Flights[Index]);

    SymTabPrepare();

    u32 NameCount = Names->Count;
    u32* MessageFromName = ArenaPushArray(MemPool, u32, NameCount);
    SchemaBuildMessage* Build = ArenaPushArray(
        Scratch.MemPool, 
        SchemaBuildMessage, 
        FLV_SCHEMA_MESSAGE_MAX
    );
    u32 MessageCount = 0;
    u32 FieldCount = 0;
    u32 VehicleMask = 0;

    MemSet(MessageFromName, U8_MAX, NameCount * sizeof(u32));

    for (u32 Index = 0; Index < FlightCount; ++Index) {
        SchemaBuildAddFlight(
            Flights[Index], 
            Build, 
            &MessageCount, 
            MessageFromName, 
            NameCount
        );
        VehicleMask |= 1U << VehicleFromFlight(Flights[Index]);
    }

    for (u32 Index = 0; Index < MessageCount; ++Index)
        FieldCount += Build[Index].FieldCount;

    Schema->Messages = ArenaPushArray(MemPool, FLVSchemaMessage, MessageCount);
    Schema->Fields = ArenaPushArray(MemPool, FLVSchemaField, FieldCount);
    Schema->MessageFromName = MessageFromName;
    Schema->MessageCount = MessageCount;
    Schema->FieldCount = FieldCount;
    Schema->NameCount = NameCount;

    u32 FieldCursor = 0;

    for (u32 Index = 0; Index < MessageCount; ++Index) {
        SchemaBuildMessage* Src = &Build[Index];
        FLVSchemaMessage* Dst = &Schema->Messages[Index];

        SortFieldsByName(Src->Fields, Src->FieldCount);
        Dst->Name = Src->Name;
        Dst->HeadField = FieldCursor;
        Dst->FieldCount = Src->FieldCount;
        Dst->MaxInstance = Src->MaxInstance;

        for (u32 FieldIndex = 0; FieldIndex < Src->FieldCount; ++FieldIndex) {
            Schema->Fields[FieldCursor].Name = Src->Fields[FieldIndex].Name;
            Schema->Fields[FieldCursor].Element = Src->Fields[FieldIndex].Elem;
            ++FieldCursor;
        }
    }

    Schema->Hash = Hash(
        (u8*) Schema->Messages,
        MessageCount * sizeof(FLVSchemaMessage)
    );
    Schema->Hash ^= Hash(
        (u8*) Schema->Fields,
        FieldCount * sizeof(FLVSchemaField)
    ) * NAME_HASH_MUL;

    for (u32 Vehicle = 0; Vehicle < FLV_VEHICLE_KIND_COUNT; ++Vehicle) {
        if (VehicleMask & (1U << Vehicle))
            FLVStdLibBindModes((FLVVehicleKind) Vehicle);
    }

    ReleaseScratch(Scratch);
}
