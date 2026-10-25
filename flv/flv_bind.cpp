#include "nc_defines.h"
#include "nc_types.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "fli.h"
#include "flv_bind.h"
#include "flv_schema.h"
#include "flv_stdlib.h"
#include "flv_symbol_table.h"
#include "flv_intern.h"
 
#define EMPTY_FLV_INDEX_VALUE 0
 
INTERNAL b32
FLIIndexIsValid(u32 Index)
{
    return Index != EMPTY_FLV_INDEX_VALUE && Index != U32_MAX;
}
 
INTERNAL u32
MessageTypeIndexFromName(FLIParsed* FLI, FLIParsedNameMap* Map, Str8 Name)
{
    FLINameMapNode* Node = FLINameMapLookup(FLI, Map, Name.Str, Name.Size);
    u32 Count = 0;
    u32* Run = FLIMatchesFromMapNode(FLI, Node, &Count);
    u32 Result = U32_MAX;
 
    if (Count && FLIIndexIsValid(Run[0]))
        Result = Run[0];
 
    return Result;
}
 
INTERNAL void
BindMessageType(FLIParsed* FLI, FLVBoundSlot* Bound, u32 MessageTypeIndex)
{
    FLIMessageType* MessageType = FLIElementFromNameIndex(
        FLI, 
        MessageTypes, 
        MESSAGE_TYPES, 
        MessageTypeIndex
    );
 
    Bound->MessageType = MessageTypeIndex;
    Bound->TimeColumn = MessageType->TimeColumn;
    Bound->InstanceColumn = FLV_COLUMN_NONE;
 
    if (MessageType->InstanceField) {
        FLIField* Field = FLIElementFromNameIndex(
            FLI, 
            Fields, 
            FIELDS, 
            MessageType->HeadField + MessageType->InstanceField - 1
        );
 
        Bound->InstanceColumn = Field->ColumnIndex;
    }
 
    if (
        Bound->Instance != FLV_INSTANCE_ANY && 
        Bound->InstanceColumn == FLV_COLUMN_NONE
    ) {
        Bound->IsMissing = TRUE;
    }
}
 
FLVBoundSlot*
FLVBindSlots(
    Arena* MemPool, 
    FLIParsed* FLI, 
    FLVSlot const* Slots, 
    u32 SlotCount
) {
    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;
    FLVBoundSlot* Result = ArenaPushArray(MemPool, FLVBoundSlot, SlotCount);
    FLINameMap* MessageMap = FLIElementFromNameIndex(
        FLI, 
        NameMaps, 
        NAME_MAPS, 
        FLI_NAME_MAP_KIND_MESSAGE_TYPES
    );
    FLIParsedNameMap ParsedMessageMap = {};
    FLVVehicleKind VehicleKind = FLV_VEHICLE_KIND_UNKNOWN;
    u64 InfoCount = 0;
    FLIFileInfo* Infos = FLITableFromName(
        FLI, 
        FileInfo, 
        FILE_INFO, 
        &InfoCount
    );
 
    FLIParsedFromNameMap(FLI, MessageMap, &ParsedMessageMap);
 
    if (InfoCount) {
        u64 Length = 0;
        u8* Base = FLIStrFromIndex(
            FLI, 
            Infos[InfoCount - 1].VehicleStringIndex, 
            &Length
        );
 
        VehicleKind = FLVVehicleKindFromStr(Str(Base, Length));
    }
 
    for (u32 Index = 0; Index < SlotCount; ++Index) {
        FLVSlot const* Slot = &Slots[Index];
        FLVBoundSlot* Bound = &Result[Index];
 
        Bound->Column = FLV_COLUMN_NONE;
        Bound->TimeColumn = FLV_COLUMN_NONE;
        Bound->InstanceColumn = FLV_COLUMN_NONE;
        Bound->MessageType = U32_MAX;
        Bound->Instance = Slot->Instance;
        Bound->ModeNumber = FLV_MODE_NUMBER_NONE;
        Bound->Kind = FLI_SCALAR_KIND_NULL;
        Bound->IsMissing = FALSE;
 
        switch (Slot->Kind) {
            default: {
                ASSERT(FALSE);
            } break;
 
            case FLV_SLOT_KIND_MESSAGE: {
                u32 MessageTypeIndex = MessageTypeIndexFromName(
                    FLI, 
                    &ParsedMessageMap, 
                    GetName(Names, Slot->Message)
                );
 
                if (MessageTypeIndex == U32_MAX)
                    Bound->IsMissing = TRUE;
                else
                    BindMessageType(FLI, Bound, MessageTypeIndex);
            } break;
 
            case FLV_SLOT_KIND_FIELD: {
                u32 ColumnIndex = FLIColumnIndexFromNames(
                    FLI, 
                    GetName(Names, Slot->Message), 
                    GetName(Names, Slot->Field)
                );
 
                if (!FLIIndexIsValid(ColumnIndex)) {
                    Bound->IsMissing = TRUE;
                } else {
                    FLIColumn* Column = FLIElementFromNameIndex(
                        FLI, 
                        Columns, 
                        COLUMNS, 
                        ColumnIndex
                    );
 
                    Bound->Column = ColumnIndex;
                    Bound->Kind = Column->Kind;
                    BindMessageType(FLI, Bound, Column->MessageTypeIndex);
                }
            } break;
 
            case FLV_SLOT_KIND_MODE: {
                Bound->ModeNumber = FLVModeNumberFromName(
                    VehicleKind, 
                    GetName(Names, Slot->Field)
                );
                Bound->IsMissing = (Bound->ModeNumber == FLV_MODE_NUMBER_NONE);
            } break;
        }
    }
 
    return Result;
}
