#include "fli_bin.h"
#include "nc_memory.h"
#include "nc_atomics.h"
#include "nc_profile.h"
#include "nc_hash.h"

INTERNAL B2FFormatCharInfo
B2FFormatCharInfoFromChar(u8 C)
{
    B2FFormatCharInfo Result = {};

    Result.Scale = 1.0;

    switch (C) {
        default: {} break;

        case 'b': { 
            Result.Kind = FLI_SCALAR_KIND_I8;
            Result.Size = 1; 
        } break;

        case 'B': {
            Result.Kind = FLI_SCALAR_KIND_U8;
            Result.Size = 1; 
        } break;

        case 'M': {
            Result.Kind = FLI_SCALAR_KIND_U8;
            Result.Size = 1; 
        } break;

        case 'h': {
            Result.Kind = FLI_SCALAR_KIND_I16;
            Result.Size = 2; 
        } break;

        case 'H': {
            Result.Kind = FLI_SCALAR_KIND_U16;
            Result.Size = 2; 
        } break;

        case 'i': {
            Result.Kind = FLI_SCALAR_KIND_I32;
            Result.Size = 4; 
        } break;

        case 'I': {
            Result.Kind = FLI_SCALAR_KIND_U32;
            Result.Size = 4; 
        } break;

        case 'q': {
            Result.Kind = FLI_SCALAR_KIND_I64;
            Result.Size = 8; 
        } break;

        case 'Q': {
            Result.Kind = FLI_SCALAR_KIND_U64;
            Result.Size = 8; 
        } break;

        case 'f': {
            Result.Kind = FLI_SCALAR_KIND_F32;
            Result.Size = 4; 
        } break;

        case 'd': {
            Result.Kind = FLI_SCALAR_KIND_F64;
            Result.Size = 8; 
        } break;

        case 'n': {
            Result.Kind = FLI_SCALAR_KIND_CHAR4;
            Result.Size = 4; 
        } break;

        case 'N': {
            Result.Kind = FLI_SCALAR_KIND_CHAR16;
            Result.Size = 16; 
        } break;

        case 'Z': {
            Result.Kind = FLI_SCALAR_KIND_CHAR64;
            Result.Size = 64; 
        } break;

        case 'c': {
            Result.Kind = FLI_SCALAR_KIND_I16;
            Result.Size = 2;
            Result.Scale = 0.01; 
        } break;

        case 'C': {
            Result.Kind = FLI_SCALAR_KIND_U16;
            Result.Size = 2;
            Result.Scale = 0.01; 
        } break;

        case 'e': {
            Result.Kind = FLI_SCALAR_KIND_I32;
            Result.Size = 4;
            Result.Scale = 0.01; 
        } break;

        case 'E': {
            Result.Kind = FLI_SCALAR_KIND_U32;
            Result.Size = 4;
            Result.Scale = 0.01; 
        } break;

        case 'L': {
            Result.Kind = FLI_SCALAR_KIND_I32;
            Result.Size = 4;
            Result.Scale = 1.0e-7; 
        } break;
    }

    return Result;
}

INTERNAL b32
B2FScalarKindIsChar(FLIScalarKind Kind)
{
    return (
        Kind == FLI_SCALAR_KIND_CHAR4 ||
        Kind == FLI_SCALAR_KIND_CHAR16 ||
        Kind == FLI_SCALAR_KIND_CHAR64
    );
}

INTERNAL Str8
B2FStrFromFixedChars(u8* Chars, u64 Capacity)
{
    u64 Size = 0;

    for ( ; Size < Capacity && Chars[Size]; ++Size);

    return Str(Chars, Size);
}

INTERNAL f64
B2FF64FromScalar(u8* Ptr, FLIScalarKind Kind)
{
    f64 Result = 0.0;

    switch (Kind) {
        default: {} break;

        case FLI_SCALAR_KIND_I8: { 
            i8  V; 
            
            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_U8: { 
            u8  V; 
            
            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_I16: { 
            i16 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_U16: { 
            u16 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_I32: { 
            i32 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_U32: { 
            u32 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_I64: { 
            i64 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_U64: { 
            u64 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_F32: { 
            f32 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = (f64) V; 
        } break;

        case FLI_SCALAR_KIND_F64: { 
            f64 V; 

            MemCpy(&V, Ptr, sizeof(V)); 
            Result = V; 
        } break;
    }

    return Result;
}

INTERNAL void
B2FScalarFromF64(u8* Ptr, FLIScalarKind Kind, f64 Value)
{
    switch (Kind) {
        default: {} break;

        case FLI_SCALAR_KIND_I8: { 
            i8  V = (i8) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_U8: { 
            u8  V = (u8) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_I16: { 
            i16 V = (i16) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_U16: { 
            u16 V = (u16) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_I32: { 
            i32 V = (i32) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_U32: { 
            u32 V = (u32) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_I64: { 
            i64 V = (i64) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_U64: { 
            u64 V = (u64) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_F32: { 
            f32 V = (f32) Value; 

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;

        case FLI_SCALAR_KIND_F64: { 
            f64 V = Value;

            MemCpy(Ptr, &V, sizeof(V)); 
        } break;
    }
}

INTERNAL u32
B2FFieldIndexFromName(B2FFormat* Format, Str8 Name)
{
    u32 Result = BIN_MAX_FIELD_COUNT;

    for (u32 Index = 0; Index < Format->FieldCount; ++Index) {
        if (StrMatch(Format->FieldNames[Index], Name, 0)) {
            Result = Index;
            break;
        }
    }

    return Result;
}

INTERNAL f64
B2FF64FromField(u8* Body, B2FFormat* Format, u32 FieldIndex)
{
    f64 Result = 0.0;

    if (FieldIndex < Format->FieldCount) {
        B2FFormatCharInfo Info = B2FFormatCharInfoFromChar(
            Format->FieldChars[FieldIndex]
        );

        Result = B2FF64FromScalar(
            Body + Format->FieldOffsets[FieldIndex], 
            Info.Kind
        ) * Info.Scale;
    }

    return Result;
}

INTERNAL u64
B2FU64FromField(u8* Body, B2FFormat* Format, u32 FieldIndex)
{
    u64 Result = 0;

    if (FieldIndex < Format->FieldCount) {
        B2FFormatCharInfo Info = B2FFormatCharInfoFromChar(
            Format->FieldChars[FieldIndex]
        );

        MemCpy(
            &Result, 
            Body + Format->FieldOffsets[FieldIndex], 
            MIN(Info.Size, sizeof(Result))
        );
    }

    return Result;
}

INTERNAL Str8
B2FStrFromField(u8* Body, B2FFormat* Format, u32 FieldIndex)
{
    Str8 Result = {};

    if (FieldIndex < Format->FieldCount) {
        B2FFormatCharInfo Info = B2FFormatCharInfoFromChar(
            Format->FieldChars[FieldIndex]
        );

        if (B2FScalarKindIsChar(Info.Kind)) {
            Result = B2FStrFromFixedChars(
                Body + Format->FieldOffsets[FieldIndex], 
                Info.Size
            );
        }
    }

    return Result;
}

INTERNAL void
B2FFormatFromFMTBody(Arena* MemPool, B2FFormat* Formats, u8* Body)
{
    u8 Type = Body[0];
    B2FFormat* Format = &Formats[Type];

    MemSet(Format, 0, sizeof(*Format));
    Format->Type = Type;
    Format->Length = Body[1];
    Format->Name = ArenaPushStrCpy(
        MemPool, 
        B2FStrFromFixedChars(Body + 2, 4)
    );
    Format->FormatString = ArenaPushStrCpy(
        MemPool, 
        B2FStrFromFixedChars(Body + 6, 16)
    );
    Format->Supported = TRUE;
    Format->TimeField = BIN_MAX_FIELD_COUNT;

    Str8 Columns = B2FStrFromFixedChars(Body + 22, 64);
    u8* Cursor = Columns.Str;
    u8* End = Columns.Str + Columns.Size;
    u32 Offset = 0;

    for (
        u32 Index = 0; 
        Index < Format->FormatString.Size && Index < BIN_MAX_FIELD_COUNT; 
        ++Index
    ) {
        u8* NameStart = Cursor;

        for ( ; Cursor < End && *Cursor != ','; ++Cursor);

        B2FFormatCharInfo Info = B2FFormatCharInfoFromChar(
            Format->FormatString.Str[Index]
        );

        Format->FieldNames[Index] = ArenaPushStrCpy(
            MemPool, 
            Str(NameStart, INT_FROM_PTR(Cursor - NameStart))
        );
        Format->FieldChars[Index] = Format->FormatString.Str[Index];
        Format->FieldOffsets[Index] = (u8) Offset;
        Format->UnitIds[Index] = '-';
        Format->MultIds[Index] = '-';
        Offset += Info.Size;
        ++Format->FieldCount;

        if (Info.Kind == FLI_SCALAR_KIND_NULL)
            Format->Supported = FALSE;

        if (Cursor < End)
            ++Cursor;
    }

    if (Format->Length != BIN_HEAD_SIZE + Offset)
        Format->Supported = FALSE;

    Format->TimeField = B2FFieldIndexFromName(Format, "TimeUS"_s8);
    Format->HasTime = (Format->TimeField < Format->FieldCount);

    if (StrMatch(Format->Name, "FMT"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_FMT;
    else if (StrMatch(Format->Name, "FMTU"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_FMTU;
    else if (StrMatch(Format->Name, "UNIT"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_UNIT;
    else if (StrMatch(Format->Name, "MULT"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_MULT;
    else if (StrMatch(Format->Name, "PARM"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_PARM;
    else if (StrMatch(Format->Name, "MSG"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_MSG;
    else if (StrMatch(Format->Name, "MODE"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_MODE;
    else if (StrMatch(Format->Name, "EV"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_EV;
    else if (StrMatch(Format->Name, "ERR"_s8, 0))
        Format->Special = B2F_SPECIAL_KIND_ERR;
}

INTERNAL void
B2FOffsetPush(Arena* MemPool, B2FFormat* Format, u64 Offset)
{
    B2FOffsetChunk* Chunk = Format->TailOffsets;

    if (!Chunk || Chunk->Count >= Chunk->Capacity) {
        Chunk = ArenaPushArrayZero(MemPool, B2FOffsetChunk, 1);
        Chunk->Capacity = 16384;
        Chunk->V = ArenaPushArray(MemPool, u64, Chunk->Capacity);
        SLL_QUEUE_PUSH(Format->HeadOffsets, Format->TailOffsets, Chunk);
    }

    Chunk->V[Chunk->Count] = Offset;
    ++Chunk->Count;
    ++Format->RecordCount;
}

struct B2FShared {
    B2FFormat*     Formats;
    B2FFormat**    Tasks;
    u64            TasksCount;
    u64            TaskTakeCounter;
    u64            StartTimeUS;
    MD5Hash        SourceMD5;
    FLIBakeParams* Result;
};

FLIBakeParams
B2FConvert(Arena* MemPool, B2FConvertParams* Params)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    u8* FileData = Params->InputBINData.Str;
    u64 FileSize = Params->InputBINData.Size;
    B2FShared* Shared = NULL;
    const u64 MessageTypeCapacity = BIN_TYPE_COUNT;
    const u64 FieldCapacity = BIN_TYPE_COUNT * BIN_MAX_FIELD_COUNT;
    const u64 ColumnCapacity = BIN_TYPE_COUNT * BIN_MAX_FIELD_COUNT;
    const u64 ParamCapacity = 4096;
    const u64 MessageCapacity = 1024;
    const u64 ModeCapacity = 256;
    const u64 EventCapacity = 1024;
    const u64 ErrorCapacity = 1024;

    if (!LaneIndex()) {
        Shared = ArenaPushArrayZero(Scratch.MemPool, B2FShared, 1);
        Shared->Formats = ArenaPushArrayZero(
            Scratch.MemPool, 
            B2FFormat, 
            BIN_TYPE_COUNT
        );
        Shared->Result = ArenaPushArrayZero(MemPool, FLIBakeParams, 1);
    }

    LaneSyncU64(&Shared, 0);

    B2FFormat* Formats = Shared->Formats;
    FLIBakeParams* Result = Shared->Result;

    if (LaneIndex() == LaneFromTaskIndex(1)) {
        PROFILE(B2FMD5Timer) {
            Shared->SourceMD5 = MD5FromStr(Params->InputBINData);
        }
    }

    if (!LaneIndex()) {
        PROFILE(B2FScanTimer) {
            Str8 UnitLabels[BIN_TYPE_COUNT] = {};
            f64 MultValues[BIN_TYPE_COUNT] = {};
            u8* Cursor = FileData;
            u8* End = FileData + FileSize;
            u64 RecordCount = 0;
            u64 SkippedRecordCount = 0;
            u64 MinTimeUSecs = U64_MAX;
            u64 MaxTimeUSecs = 0;

            for (u32 Index = 0; Index < BIN_TYPE_COUNT; ++Index)
                MultValues[Index] = 1.0;

            while (Cursor + BIN_HEAD_SIZE <= End) {
                if (
                    Cursor[0] != BIN_HEAD_BYTE_0 || 
                    Cursor[1] != BIN_HEAD_BYTE_1
                ) {
                    ++Cursor;
                    continue;
                }

                u8 Type = Cursor[2];

                if (Type == BIN_FMT_TYPE) {
                    if (Cursor + BIN_FMT_LENGTH > End)
                        break;

                    B2FFormatFromFMTBody(
                        MemPool, 
                        Formats, 
                        Cursor + BIN_HEAD_SIZE
                    );
                    Cursor += BIN_FMT_LENGTH;
                    ++RecordCount;
                    continue;
                }

                B2FFormat* Format = &Formats[Type];

                if (!Format->Length) {
                    ++Cursor;
                    continue;
                }

                if (Cursor + Format->Length > End)
                    break;

                u8* Body = Cursor + BIN_HEAD_SIZE;
                u64 TimeUSecs = 0;

                if (Format->HasTime) {
                    TimeUSecs = B2FU64FromField(Body, Format, Format->TimeField);
                    MinTimeUSecs = MIN(MinTimeUSecs, TimeUSecs);
                    MaxTimeUSecs = MAX(MaxTimeUSecs, TimeUSecs);
                }

                switch (Format->Special) {
                    default: {} break;

                    case B2F_SPECIAL_KIND_NONE: {
                        if (Format->Supported && Format->HasTime) {
                            B2FOffsetPush(
                                Scratch.MemPool, 
                                Format, 
                                INT_FROM_PTR(Cursor - FileData)
                            );
                        } else {
                            ++SkippedRecordCount;
                        }
                    } break;

                    case B2F_SPECIAL_KIND_FMTU: {
                        u8 FmtType = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "FmtType"_s8)
                        );
                        Str8 UnitIds = B2FStrFromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "UnitIds"_s8)
                        );
                        Str8 MultIds = B2FStrFromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "MultIds"_s8)
                        );

                        MemCpy(
                            Formats[FmtType].UnitIds, 
                            UnitIds.Str, 
                            MIN(UnitIds.Size, BIN_MAX_FIELD_COUNT)
                        );
                        MemCpy(
                            Formats[FmtType].MultIds, 
                            MultIds.Str, 
                            MIN(MultIds.Size, BIN_MAX_FIELD_COUNT)
                        );
                    } break;

                    case B2F_SPECIAL_KIND_UNIT: {
                        u8 Id = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Id"_s8)
                        );
                        Str8 Label = B2FStrFromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Label"_s8)
                        );

                        UnitLabels[Id] = ArenaPushStrCpy(MemPool, Label);
                    } break;

                    case B2F_SPECIAL_KIND_MULT: {
                        u8 Id = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Id"_s8)
                        );

                        MultValues[Id] = B2FF64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Mult"_s8)
                        );
                    } break;

                    case B2F_SPECIAL_KIND_PARM: {
                        FLIBakeParam* Param = FLIBakeParamChunkListPush(
                            MemPool, 
                            &Result->Params, 
                            ParamCapacity
                        );

                        Param->Name = ArenaPushStrCpy(
                            MemPool, 
                            B2FStrFromField(
                                Body, 
                                Format, 
                                B2FFieldIndexFromName(Format, "Name"_s8)
                            )
                        );
                        Param->Value = (f32) B2FF64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Value"_s8)
                        );
                        Param->Default = (f32) B2FF64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Default"_s8)
                        );
                        Param->TimeUSecs = TimeUSecs;
                    } break;

                    case B2F_SPECIAL_KIND_MSG: {
                        FLIBakeMessage* Message = FLIBakeMessageChunkListPush(
                            MemPool, 
                            &Result->Messages, 
                            MessageCapacity
                        );

                        Message->Text = ArenaPushStrCpy(
                            MemPool, 
                            B2FStrFromField(
                                Body, 
                                Format, 
                                B2FFieldIndexFromName(Format, "Message"_s8)
                            )
                        );
                        Message->TimeUSecs = TimeUSecs;

                        if (
                            !Result->FileInfo.Firmware.Size &&
                            (
                                StrMatch(
                                    StrPrefix(Message->Text, 4), 
                                    "Ardu"_s8, 
                                    0
                                ) ||
                                StrMatch(
                                    StrPrefix(Message->Text, 7), 
                                    "Antenna"_s8, 
                                    0
                                ) ||
                                StrMatch(
                                    StrPrefix(Message->Text, 5), 
                                    "Blimp"_s8, 
                                    0
                                )
                            )
                        ) {
                            u64 SpaceIndex = 0;

                            for (
                                ;
                                (
                                    SpaceIndex < Message->Text.Size && 
                                    Message->Text.Str[SpaceIndex] != ' '
                                ); 
                                ++SpaceIndex
                            );

                            Result->FileInfo.Firmware = Message->Text;
                            Result->FileInfo.Vehicle = StrPrefix(
                                Message->Text, 
                                SpaceIndex
                            );
                        }
                    } break;

                    case B2F_SPECIAL_KIND_MODE: {
                        FLIBakeMode* Mode = FLIBakeModeChunkListPush(
                            MemPool, 
                            &Result->Modes, 
                            ModeCapacity
                        );

                        Mode->TimeUSecs = TimeUSecs;
                        Mode->Mode = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Mode"_s8)
                        );
                        Mode->ModeNum = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "ModeNum"_s8)
                        );
                        Mode->Reason = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Rsn"_s8)
                        );
                    } break;

                    case B2F_SPECIAL_KIND_EV: {
                        FLIBakeEvent* Event = FLIBakeEventChunkListPush(
                            MemPool, 
                            &Result->Events, 
                            EventCapacity
                        );

                        Event->TimeUSecs = TimeUSecs;
                        Event->ID = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Id"_s8)
                        );
                    } break;

                    case B2F_SPECIAL_KIND_ERR: {
                        FLIBakeError* Error = FLIBakeErrorChunkListPush(
                            MemPool, 
                            &Result->Errors, 
                            ErrorCapacity
                        );

                        Error->TimeUSecs = TimeUSecs;
                        Error->Subsystem = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "Subsys"_s8)
                        );
                        Error->Code = (u8) B2FU64FromField(
                            Body, 
                            Format, 
                            B2FFieldIndexFromName(Format, "ECode"_s8)
                        );
                    } break;
                }

                Cursor += Format->Length;
                ++RecordCount;
            }

            if (MinTimeUSecs == U64_MAX)
                MinTimeUSecs = 0;

            Shared->StartTimeUS = MinTimeUSecs;
            Result->FileInfo.SourcePath = ArenaPushStrCpy(MemPool, Params->InputBINName);
            Result->FileInfo.StartTimeUSecs = MinTimeUSecs;
            Result->FileInfo.EndTimeUSecs = MaxTimeUSecs;
            Result->FileInfo.RecordCount = RecordCount;
            Result->FileInfo.SkippedRecordCount = SkippedRecordCount;

            for (
                FLIBakeParamChunkNode* Node = Result->Params.Head; 
                Node; 
                Node = Node->Next
            ) {
                for (u64 Index = 0; Index < Node->Count; ++Index)
                    Node->V[Index].TimeUSecs-= MinTimeUSecs;
            }

            for (
                FLIBakeMessageChunkNode* Node = Result->Messages.Head; 
                Node; 
                Node = Node->Next
            ) {
                for (u64 Index = 0; Index < Node->Count; ++Index)
                    Node->V[Index].TimeUSecs -= MinTimeUSecs;
            }

            for (
                FLIBakeModeChunkNode* Node = Result->Modes.Head; 
                Node; 
                Node = Node->Next
            ) {
                for (u64 Index = 0; Index < Node->Count; ++Index)
                    Node->V[Index].TimeUSecs -= MinTimeUSecs;
            }

            for (
                FLIBakeEventChunkNode* Node = Result->Events.Head; 
                Node; 
                Node = Node->Next
            ) {
                for (u64 Index = 0; Index < Node->Count; ++Index)
                    Node->V[Index].TimeUSecs -= MinTimeUSecs;
            }

            for (
                FLIBakeErrorChunkNode* Node = Result->Errors.Head; 
                Node; 
                Node = Node->Next
            ) {
                for (u64 Index = 0; Index < Node->Count; ++Index)
                    Node->V[Index].TimeUSecs -= MinTimeUSecs;
            }

            Shared->Tasks = ArenaPushArrayZero(
                Scratch.MemPool, 
                B2FFormat*, 
                BIN_TYPE_COUNT
            );

            for (u32 Type = 0; Type < BIN_TYPE_COUNT; ++Type) {
                B2FFormat* Format = &Formats[Type];

                if (!Format->RecordCount)
                    continue;

                FLIBakeMessageType* MessageType = FLIBakeMessageTypeChunkListPush(
                    MemPool, 
                    &Result->MessageTypes, 
                    MessageTypeCapacity
                );

                Format->MessageType = MessageType;
                MessageType->ID = Format->Type;
                MessageType->Length = Format->Length;
                MessageType->Name = Format->Name;
                MessageType->Format = Format->FormatString;
                MessageType->FieldCount = Format->FieldCount;
                MessageType->ColumnCount = Format->FieldCount;
                MessageType->RecordCount = Format->RecordCount;

                for (u32 Index = 0; Index < Format->FieldCount; ++Index) {
                    B2FFormatCharInfo Info = B2FFormatCharInfoFromChar(
                        Format->FieldChars[Index]
                    );
                    FLIBakeField* Field = FLIBakeFieldChunkListPush(
                        MemPool, 
                        &Result->Fields, 
                        FieldCapacity
                    );
                    FLIBakeColumn* Column = FLIBakeColumnChunkListPush(
                        MemPool, 
                        &Result->Columns, 
                        ColumnCapacity
                    );
                    u8 UnitId = Format->UnitIds[Index];
                    u8 MultId = Format->MultIds[Index];
                    f64 Scale = Info.Scale;

                    if (Scale == 1.0 && MultId != '-' && MultId != '?')
                        Scale = MultValues[MultId];

                    if (!Index) {
                        MessageType->HeadField = Field;
                        MessageType->HeadColumn = Column;
                    }

                    if (Index == Format->TimeField)
                        MessageType->TimeColumn = Column;

                    if (
                        !MessageType->InstanceField &&
                        (
                            UnitId == '#' ||
                            StrMatch(Format->FieldNames[Index], "I"_s8, 0) ||
                            StrMatch(Format->FieldNames[Index], "Instance"_s8, 0) ||
                            StrMatch(Format->FieldNames[Index], "Inst"_s8, 0)
                        )
                    ) {
                        MessageType->InstanceField = Index + 1;
                    }

                    Field->MessageType = MessageType;
                    Field->Name = Format->FieldNames[Index];
                    Field->Unit = UnitLabels[UnitId];
                    Field->FormatChar = Format->FieldChars[Index];
                    Field->LoggedKind = Info.Kind;
                    Field->LoggedSize = Info.Size;
                    Field->Multiplier = Scale;
                    Field->Column = Column;
                    Field->StoredKind = Info.Kind;

                    if (Scale != 1.0 && !B2FScalarKindIsChar(Info.Kind)) {
                        Field->StoredKind = (Info.Size <= 2) 
                            ? FLI_SCALAR_KIND_F32 
                            : FLI_SCALAR_KIND_F64;
                    }

                    Column->MessageType = MessageType;
                    Column->Field = Field;
                    Column->Kind = Field->StoredKind;
                    Column->Count = Format->RecordCount;
                    Column->Data = ArenaPushArray(
                        MemPool,
                        u8,
                        Column->Count * FLI_SCALAR_KIND_SIZE_TABLE[Column->Kind]
                    );
                }

                u64 Insert = Shared->TasksCount++;

                for (
                    ; 
                    (
                        Insert && 
                        Shared->Tasks[Insert - 1]->RecordCount < Format->RecordCount
                    ); 
                    --Insert
                ) {
                    Shared->Tasks[Insert] = Shared->Tasks[Insert - 1];
                }

                Shared->Tasks[Insert] = Format;
            }
        }
    }

    LaneSync();

    if (!LaneIndex()) {
        STATIC_ASSERT(
            sizeof(Result->FileInfo.SourceMD5) == sizeof(Shared->SourceMD5),
            MD5SizeCheck
        );

        MemCpy(
            &Result->FileInfo.SourceMD5,
            &Shared->SourceMD5,
            sizeof(Result->FileInfo.SourceMD5)
        );
    }

    LaneSync();

    PROFILE(B2FGatherTimer) {
        for (;;) {
            u64 TaskIndex = AtomicIncFetchU64(
                &Shared->TaskTakeCounter, 
                MEM_ORDER_SEQ_CST
            ) - 1;

            if (TaskIndex >= Shared->TasksCount)
                break;

            B2FFormat* Format = Shared->Tasks[TaskIndex];
            FLIBakeMessageType* MessageType = Format->MessageType;
            u32 FieldCount = Format->FieldCount;
            FLIBakeColumn* Columns = MessageType->HeadColumn;
            u8* ColumnData[BIN_MAX_FIELD_COUNT] = {};
            FLIScalarKind LoggedKinds[BIN_MAX_FIELD_COUNT] = {};
            FLIScalarKind StoredKinds[BIN_MAX_FIELD_COUNT] = {};
            u8 LoggedSizes[BIN_MAX_FIELD_COUNT] = {};
            u8 StoredSizes[BIN_MAX_FIELD_COUNT] = {};
            u8 Offsets[BIN_MAX_FIELD_COUNT] = {};
            b32 IsRaw[BIN_MAX_FIELD_COUNT] = {};
            b32 IsChar[BIN_MAX_FIELD_COUNT] = {};
            f64 Scales[BIN_MAX_FIELD_COUNT] = {};
            f64 Mins[BIN_MAX_FIELD_COUNT] = {};
            f64 Maxs[BIN_MAX_FIELD_COUNT] = {};
            f64 Sums[BIN_MAX_FIELD_COUNT] = {};
            u64 StartTimeUS = Shared->StartTimeUS;
            u32 TimeField = Format->TimeField;

            for (u32 Index = 0; Index < FieldCount; ++Index) {
                FLIBakeField* Field = Columns[Index].Field;

                ColumnData[Index] = (u8*) Columns[Index].Data;
                LoggedKinds[Index] = Field->LoggedKind;
                StoredKinds[Index] = Field->StoredKind;
                LoggedSizes[Index] = Field->LoggedSize;
                StoredSizes[Index] = (u8) FLI_SCALAR_KIND_SIZE_TABLE[Field->StoredKind];
                Offsets[Index] = Format->FieldOffsets[Index];
                Scales[Index] = Field->Multiplier;
                IsChar[Index] = B2FScalarKindIsChar(Field->LoggedKind);
                IsRaw[Index] = (
                    Field->LoggedKind == Field->StoredKind && 
                    Index != TimeField
                );
                Mins[Index] = F64_MAX;
                Maxs[Index] = -F64_MAX;
                Sums[Index] = 0.0;
            }

            u64 RecordIndex = 0;

            for (
                B2FOffsetChunk* Chunk = Format->HeadOffsets; 
                Chunk; 
                Chunk = Chunk->Next
            ) {
                for (
                    u64 ChunkIndex = 0; 
                    ChunkIndex < Chunk->Count; 
                    ++ChunkIndex, ++RecordIndex
                ) {
                    u8* Body = FileData + Chunk->V[ChunkIndex] + BIN_HEAD_SIZE;

                    for (u32 Index = 0; Index < FieldCount; ++Index) {
                        u8* Src = Body + Offsets[Index];
                        u8* Dst = ColumnData[Index] + RecordIndex * StoredSizes[Index];
                        f64 Value = 0.0;

                        if (IsChar[Index]) {
                            MemCpy(Dst, Src, StoredSizes[Index]);
                            continue;
                        }

                        if (Index == TimeField) {
                            u64 TimeUS = 0;

                            MemCpy(&TimeUS, Src, sizeof(TimeUS));
                            TimeUS -= StartTimeUS;
                            MemCpy(Dst, &TimeUS, sizeof(TimeUS));
                            Value = (f64) TimeUS;
                        } else if (IsRaw[Index]) {
                            MemCpy(Dst, Src, StoredSizes[Index]);
                            Value = B2FF64FromScalar(Src, LoggedKinds[Index]);
                        } else {
                            Value = B2FF64FromScalar(
                                Src, 
                                LoggedKinds[Index]
                            ) * Scales[Index];
                            B2FScalarFromF64(Dst, StoredKinds[Index], Value);
                        }

                        Mins[Index] = MIN(Mins[Index], Value);
                        Maxs[Index] = MAX(Maxs[Index], Value);
                        Sums[Index] += Value;
                    }
                }
            }

            for (u32 Index = 0; Index < FieldCount; ++Index) {
                if (IsChar[Index] || !RecordIndex) {
                    Columns[Index].Min = 0.0;
                    Columns[Index].Max = 0.0;
                    Columns[Index].Mean = 0.0;
                } else {
                    Columns[Index].Min = Mins[Index];
                    Columns[Index].Max = Maxs[Index];
                    Columns[Index].Mean = Sums[Index] / (f64) RecordIndex;
                }
            }
        }
    }

    LaneSync();

    FLIBakeParams Copy = *Result;

    LaneSync();
    ReleaseScratch(Scratch);

    return Copy;
}
