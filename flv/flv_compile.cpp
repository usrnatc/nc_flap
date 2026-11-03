#include "nc_types.h"
#include "nc_memory.h"
#include "nc_content.h"
#include "nc_file_stream.h"

#include "flv_compile.h"
#include "flv_schema.h"
#include "flv_run.h"
#include "flv_token.h"
#include "flv_lexer.h"
#include "flv_parser.h"
#include "flv_type_check.h"

INTERNAL b32
FLVCompilerSchemaUpdate(
    FLVCompiler* Compiler,
    Access* Acc,
    FLIKey const* Keys,
    u64 Count,
    u64 KeyHash
) {
    if (Compiler->SchemaKeyHash == KeyHash && Compiler->Schema)
        return TRUE;

    TempArena Scratch = GetScratch(NULL, 0);
    FLIParsed** Flights = ArenaPushArray(Scratch.MemPool, FLIParsed*, Count);
    b32 Ready = TRUE;

    for (u64 Index = 0; Index < Count; ++Index) {
        Flights[Index] = FLIParsedFromKey(Acc, Keys[Index], TRUE, 0);

        if (Flights[Index] == EMPTY_FLI_PARSED_VALUE)
            Ready = FALSE;
    }

    if (Ready) {
        ArenaClear(Compiler->SchemaMemPool);
        Compiler->Schema = ArenaPushStructZero(
            Compiler->SchemaMemPool, 
            FLVSchema
        );
        FLVSchemaBuild(
            Compiler->SchemaMemPool, 
            Flights, 
            (u32) Count, 
            Compiler->Schema
        );
        Compiler->SchemaKeyHash = KeyHash;
    }

    ReleaseScratch(Scratch);

    return Ready;
}

INTERNAL Str8
FLVUnitKey(Arena* MemPool, Str8 Path, FLIKey const* Keys, u64 KeysCount)
{
    Str8List Parts = {};

    ListPush(MemPool, &Parts, Str8Struct(&Path.Size));
    ListPush(MemPool, &Parts, Path);
    ListPush(MemPool, &Parts, FLVFlightSetKey(MemPool, Keys, KeysCount));

    return StrListJoin(MemPool, &Parts, NULL);
}

INTERNAL u128
FLVSourceHash(Str8 Path)
{
    return FSHashFromPathRange(Path, Rng((u64) 0, U64_MAX), 0);
}

INTERNAL OCObject
FLVUnitObjectCreate(
    Str8 Key, 
    b32* CancelSignal, 
    OUT b32* Retry, 
    OUT u64* Generation
) {
    OCObject Result = {};
    TempArena Scratch = GetScratch(NULL, 0);
    Access* Acc = AccessOpen();
    u64 Offset = 0;
    Str8 Path = {};
    u64 KeysCount = 0;

    *Generation = FSChangeGeneration();
    Offset += SerialPopStruct(Key, Offset, &Path.Size);
    Path.Str = ArenaPushArray(Scratch.MemPool, u8, Path.Size);
    Offset += SerialPop(Key, Offset, Path.Str, Path.Size, 1);
    Offset += SerialPopStruct(Key, Offset, &KeysCount);

    FLIKey* Keys = ArenaPushArray(Scratch.MemPool, FLIKey, KeysCount);

    Offset += SerialPop(Key, Offset, Keys, KeysCount * sizeof(FLIKey), 1);

    FLVCompiler* Compiler = FLVCompilerSelected();
    u64 SetHash = Hash(FLVFlightSetKey(Scratch.MemPool, Keys, KeysCount));
    b32 SchemaReady = FLVCompilerSchemaUpdate(Compiler, Acc, Keys, KeysCount, SetHash);
    u128 SourceHash = FLVSourceHash(Path);
    Str8 Source = CDataFromHash(Acc, SourceHash);

    if (!SchemaReady || !Source.Size) {
        *Retry = TRUE;
        AccessClose(Acc);
        ReleaseScratch(Scratch);

        return Result;
    }

    Arena* MemPool = ArenaAlloc();
    FLVUnit* Unit = ArenaPushStructZero(MemPool, FLVUnit);
    ParsedFile Root = {};

    Unit->MemPool = MemPool;
    Unit->Path = ArenaPushStrCpy(MemPool, Path);
    Root.Path = Unit->Path;
    Root.Source.Size = Source.Size;
    Root.Source.Str = (u8*) ArenaPushSize(
        MemPool, 
        Source.Size + 64, 
        NC_SIMD_ALIGN256, 
        FALSE
    );
    MemCpy(Root.Source.Str, Source.Str, Source.Size);
    MemZero(Root.Source.Str + Source.Size, 64);
    Root.Tokens = LexString(MemPool, Root.Source, Compiler->Names);
    Root.LineOffsets = LineOffsetsFromSource(
        MemPool, 
        Root.Source, 
        &Root.LineCount
    );

    AST* Tree = ParseTokens(MemPool, &Root, NULL, 0);

    CheckAST(
        MemPool,
        Tree,
        Compiler->Schema,
        &Unit->Slots,
        &Unit->SlotsCount
    );

    Unit->Lowered = FLVLower(
        MemPool, 
        Tree, 
        Compiler->Schema, 
        Unit->Slots, 
        Unit->SlotsCount
    );
    Unit->HasError = Unit->Lowered.HasError;
    Unit->SlotNames = ArenaPushArrayZero(
        MemPool, 
        FLVSlotName, 
        Unit->SlotsCount
    );

    for (u32 Index = 0; Index < Unit->SlotsCount; ++Index) {
        FLVSlot const* Slot = &Unit->Slots[Index];
        FLVSlotName* Name = &Unit->SlotNames[Index];

        if (Slot->Kind == FLV_SLOT_KIND_MODE) {
            Name->Mode = ArenaPushStrCpy(
                MemPool, 
                GetName(Compiler->Names, Slot->Field)
            );
        } else {
            Name->Message = ArenaPushStrCpy(
                MemPool, 
                GetName(Compiler->Names, Slot->Message)
            );

            if (Slot->Field.V != NAME_INDEX_INVALID_VALUE) {
                Name->Field = ArenaPushStrCpy(
                    MemPool, 
                    GetName(Compiler->Names, Slot->Field)
                );
            }
        }
    }

    Unit->LayoutNames = ArenaPushArrayZero(
        MemPool, 
        Str8, 
        Unit->Lowered.LayoutCount
    );

    for (u32 Index = 0; Index < Unit->Lowered.LayoutCount; ++Index) {
        NameIndex Name = Unit->Lowered.Layout[Index].Name;

        if (Name.V != NAME_INDEX_INVALID_VALUE) {
            Unit->LayoutNames[Index] = ArenaPushStrCpy(
                MemPool, 
                GetName(Compiler->Names, Name)
            );
        }
    }

    Unit->ImportCount = Tree->FilesCount - 1;
    Unit->ImportPaths = ArenaPushArray(MemPool, Str8, Unit->ImportCount);

    u64 UnitHash = Hash(Str8Struct(&SourceHash)) ^ Compiler->Schema->Hash;

    for (u32 Index = 0; Index < Unit->ImportCount; ++Index) {
        Str8 ImportPath = Tree->Files[Index + 1].Path;
        u128 ImportHash = FLVSourceHash(ImportPath);

        Unit->ImportPaths[Index] = ArenaPushStrCpy(MemPool, ImportPath);
        UnitHash = (
            (UnitHash * 0x9E3779B97F4A7C15ULL) ^ 
            Hash(Str8Struct(&ImportHash))
        );
    }

    Unit->Hash = UnitHash;

    Result.V[0] = INT_FROM_PTR(MemPool);
    Result.V[1] = INT_FROM_PTR(Unit);
    AccessClose(Acc);
    ReleaseScratch(Scratch);

    return Result;
}

INTERNAL void
FLVUnitObjectDestroy(OCObject Object)
{
    if (Object.V[0])
        ArenaRelease((Arena*) PTR_FROM_INT(Object.V[0]));
}

INTERNAL Str8
FLVResultKey(
    Arena* MemPool,
    Str8 Path,
    FLIKey const* Keys,
    u64 KeysCount,
    u64 UnitHash,
    FLIKey Flight,
    u32 Program
) {
    Str8List Parts = {};
 
    ListPush(MemPool, &Parts, FLVUnitKey(MemPool, Path, Keys, KeysCount));
    ListPush(MemPool, &Parts, Str8Struct(&UnitHash));
    ListPush(MemPool, &Parts, Str8Struct(&Flight));
    ListPush(MemPool, &Parts, Str8Struct(&Program));
 
    return StrListJoin(MemPool, &Parts, NULL);
}
 
INTERNAL OCObject
FLVResultObjectCreate(
    Str8 Key, 
    b32* CancelSignal, 
    OUT b32* Retry, 
    OUT u64* Generation
) {
    OCObject Result = {};
    TempArena Scratch = GetScratch(NULL, 0);
    Access* Acc = AccessOpen();
    u64 Offset = 0;
    Str8 Path = {};
    u64 KeysCount = 0;
    u64 UnitHash = 0;
    FLIKey Flight = {};
    u32 Program = 0;
 
    *Generation = FLILoadGeneration();
    Offset += SerialPopStruct(Key, Offset, &Path.Size);
    Path.Str = ArenaPushArray(Scratch.MemPool, u8, Path.Size);
    Offset += SerialPop(Key, Offset, Path.Str, Path.Size, 1);
    Offset += SerialPopStruct(Key, Offset, &KeysCount);
 
    FLIKey* Keys = ArenaPushArray(Scratch.MemPool, FLIKey, KeysCount);
 
    Offset += SerialPop(Key, Offset, Keys, KeysCount * sizeof(FLIKey), 1);
    Offset += SerialPopStruct(Key, Offset, &UnitHash);
    Offset += SerialPopStruct(Key, Offset, &Flight);
    Offset += SerialPopStruct(Key, Offset, &Program);
 
    FLVUnit* Unit = FLVUnitFromPath(Acc, Path, Keys, KeysCount, 0);
    FLIParsed* FLI = FLIParsedFromKey(Acc, Flight, TRUE, 0);
 
    if (!Unit || FLI == EMPTY_FLI_PARSED_VALUE) {
        *Retry = TRUE;
    } else if (Unit->Hash == UnitHash && Program < Unit->Lowered.ProgramCount) {
        Arena* MemPool = ArenaAlloc();
        FLVBoundSlot* Bound = FLVBindSlots(
            MemPool,
            FLI,
            Unit->Slots,
            Unit->SlotNames,
            Unit->SlotsCount
        );
        FLVLane CompilerLane = {};
 
        FLVLaneInit(
            &CompilerLane,
            Scratch.MemPool,
            MemPool,
            &Unit->Lowered.Program[Program],
            FLI,
            Bound,
            Unit->SlotsCount,
            0
        );
        FLVLaneRun(&CompilerLane);
 
        Result.V[0] = INT_FROM_PTR(MemPool);
        Result.V[1] = INT_FROM_PTR(CompilerLane.Items);
        Result.V[2] = Unit->Lowered.Program[Program].EmitCount;
    }
 
    AccessClose(Acc);
    ReleaseScratch(Scratch);
 
    return Result;
}
 
INTERNAL void
FLVResultObjectDestroy(OCObject Object)
{
    if (Object.V[0])
        ArenaRelease((Arena*) PTR_FROM_INT(Object.V[0]));
}
 
FLVRenderItem*
FLVResultFromUnit(
    Access* Acc,
    FLVUnit const* Unit,
    FLIKey const* Keys,
    u64 KeysCount,
    FLIKey Flight,
    u32 Program,
    u64 EndTimeUSecs
) {
    TempArena Scratch = GetScratch(NULL, 0);
    Str8 Key = FLVResultKey(
        Scratch.MemPool,
        Unit->Path,
        Keys,
        KeysCount,
        Unit->Hash,
        Flight,
        Program
    );
    OCObject Object = OCObjectFromKey(
        Acc,
        Key,
        FLVResultObjectCreate,
        FLVResultObjectDestroy,
        EndTimeUSecs,
        __Params.Generation = FLILoadGeneration()
    );
 
    ReleaseScratch(Scratch);
 
    return (FLVRenderItem*) PTR_FROM_INT(Object.V[1]);
}

INTERNAL void
FLVSortFLIKeys(FLIKey* Keys, u64 Count)
{
    for (u64 Index = 1; Index < Count; ++Index) {
        FLIKey Key = Keys[Index];
        u64 At = Index;

        while (
            At &&
            (
                Keys[At - 1].U64[0] > Key.U64[0] ||
                (Keys[At - 1].U64[0] == Key.U64[0] && Keys[At - 1].U64[1] > Key.U64[1])
            )
        ) {
            Keys[At] = Keys[At - 1];
            --At;
        }

        Keys[At] = Key;
    }
}

Str8
FLVFlightSetKey(Arena* MemPool, FLIKey const* Keys, u64 Count)
{
    FLIKey* Sorted = ArenaPushArray(MemPool, FLIKey, Count);
    Str8List Parts = {};

    MemCpy(Sorted, Keys, Count * sizeof(FLIKey));
    FLVSortFLIKeys(Sorted, Count);
    ListPush(MemPool, &Parts, Str8Struct(&Count));
    ListPush(MemPool, &Parts, Str((u8*) Sorted, Count * sizeof(FLIKey)));

    return StrListJoin(MemPool, &Parts, NULL);
}

FLVUnit*
FLVUnitFromPath(
    Access* Acc, 
    Str8 Path, 
    FLIKey const* Keys, 
    u64 KeysCount, 
    u64 EndTimeUSecs
) {
    TempArena Scratch = GetScratch(NULL, 0);
    Str8 Key = FLVUnitKey(Scratch.MemPool, Path, Keys, KeysCount);
    OCObject Object = OCObjectFromKey(
        Acc,
        Key,
        FLVUnitObjectCreate,
        FLVUnitObjectDestroy,
        EndTimeUSecs,
        __Params.Generation = FSChangeGeneration()
    );

    ReleaseScratch(Scratch);

    return (FLVUnit*) PTR_FROM_INT(Object.V[1]);
}
