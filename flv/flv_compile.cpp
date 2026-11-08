#include "nc_print.h"
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
#include "flv_symbol_table.h"

FLVClosureTable FLV_CLOSURES = {};

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

INTERNAL u128
FLVSourceHash(Str8 Path)
{
    u128 Result = FSHashFromPathRange(Path, Rng((u64) 0ULL, U64_MAX), 0);

    return Result;
}

INTERNAL u64
FLVFoldHashes(u64 Seed, u128 const* Hashes, u32 Count)
{
    u64 Result = Seed;

    for (u32 Index = 0; Index < Count; ++Index) {
        Result = (
            (Result * 0x9E3779B97F4A7C15ULL) ^
            Hash(Str8Struct(&Hashes[Index]))
        );
    }

    return Result;
}

INTERNAL Str8
FLVSourceCpy(Arena* MemPool, Str8 Source)
{
    Str8 Result = {};

    Result.Size = Source.Size;
    Result.Str = (u8*) ArenaPushSize(
        MemPool,
        Source.Size + 64,
        NC_SIMD_ALIGN256,
        FALSE
    );
    MemCpy(Result.Str, Source.Str, Source.Size);
    MemZero(Result.Str + Source.Size, 64);

    return Result;
}

Str8 FLVImportSource(Arena *MemPool, Str8 Path, u128 *Hash)
{
    Str8 Result = {};
    Access* Acc = AccessOpen();
    u128 SourceHash = FLVSourceHash(Path);
    Str8 Source = CDataFromHash(Acc, SourceHash);

    if (Source.Size) {
        Result = FLVSourceCpy(MemPool, Source);
        *Hash = SourceHash;
    }

    AccessClose(Acc);

    return Result;
}

INTERNAL FLVClosureSlot*
FLVClosureSlotFromKey(u64 Key)
{
    if (!FLV_CLOSURES.MemPool) {
        FLV_CLOSURES.MemPool = ArenaAlloc();
        FLV_CLOSURES.SlotsCount = 256;
        FLV_CLOSURES.Slots = ArenaPushArrayZero(
            FLV_CLOSURES.MemPool,
            FLVClosureSlot,
            FLV_CLOSURES.SlotsCount
        );
    }

    u64 SlotIndex = Key % FLV_CLOSURES.SlotsCount;
    FLVClosureSlot* Result = &FLV_CLOSURES.Slots[SlotIndex];

    return Result;
}

INTERNAL FLVClosure*
FLVClosureFromPath(Str8 Path)
{
    FLVClosure* Result = NULL;
    u64 Key = Hash(Path);
    FLVClosureSlot* Slot = FLVClosureSlotFromKey(Key);

    for (FLVClosureNode* Node = Slot->Head; Node; Node = Node->Next) {
        if (Node->V.Key == Key && StrMatch(Node->V.Path, Path, 0)) {
            Result = &Node->V;
            break;
        }
    }

    return Result;
}

INTERNAL void
FLVClosureUpdate(Str8 Path, FLVUnit const* Unit)
{
    FLVClosure* Closure = FLVClosureFromPath(Path);

    if (!Closure) {
        u64 Key = Hash(Path);
        FLVClosureSlot* Slot = FLVClosureSlotFromKey(Key);
        FLVClosureNode* Node = ArenaPushArrayZero(
            FLV_CLOSURES.MemPool,
            FLVClosureNode,
            1
        );

        Node->V.Key = Key;
        Node->V.Path = ArenaPushStrCpy(
            FLV_CLOSURES.MemPool,
            Path
        );
        DLL_PUSH_BACK(Slot->Head, Slot->Tail, Node);
        Closure = &Node->V;
    }

    b32 IsSame = (Closure->ImportCount == Unit->ImportCount);

    for (u32 Index = 0; IsSame && Index < Unit->ImportCount; ++Index) {
        IsSame = StrMatch(
            Closure->ImportPaths[Index],
            Unit->ImportPaths[Index],
            0
        );
    }

    if (IsSame)
        return;

    Closure->ImportCount = Unit->ImportCount;
    Closure->ImportPaths = ArenaPushArray(
        FLV_CLOSURES.MemPool,
        Str8,
        Unit->ImportCount
    );

    for (u32 Index = 0; Index < Unit->ImportCount; ++Index) {
        Closure->ImportPaths[Index] = ArenaPushStrCpy(
            FLV_CLOSURES.MemPool,
            Unit->ImportPaths[Index]
        );
    }
}

INTERNAL Str8
FLVUnitKey(
    Arena* MemPool, 
    Str8 Path, 
    FLIKey const* Keys, 
    u64 KeysCount,
    u128 RootHash,
    u64 ImportsHash
) {
    Str8List Parts = {};

    ListPush(MemPool, &Parts, Str8Struct(&Path.Size));
    ListPush(MemPool, &Parts, Path);
    ListPush(MemPool, &Parts, FLVFlightSetKey(MemPool, Keys, KeysCount));
    ListPush(MemPool, &Parts, Str8Struct(&RootHash));
    ListPush(MemPool, &Parts, Str8Struct(&ImportsHash));

    return StrListJoin(MemPool, &Parts, NULL);
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
    u128 RootHash = {};

    Offset += SerialPopStruct(Key, Offset, &Path.Size);
    Path.Str = ArenaPushArray(Scratch.MemPool, u8, Path.Size);
    Offset += SerialPop(Key, Offset, Path.Str, Path.Size, 1);
    Offset += SerialPopStruct(Key, Offset, &KeysCount);

    FLIKey* Keys = ArenaPushArray(Scratch.MemPool, FLIKey, KeysCount);

    Offset += SerialPop(Key, Offset, Keys, KeysCount * sizeof(FLIKey), 1);
    Offset += SerialPopStruct(Key, Offset, &RootHash);

    FLVCompiler* Compiler = FLVCompilerLock();
    u64 SetHash = Hash(FLVFlightSetKey(Scratch.MemPool, Keys, KeysCount));
    b32 SchemaReady = FLVCompilerSchemaUpdate(
        Compiler, 
        Acc, 
        Keys, 
        KeysCount, 
        SetHash
    );
    Str8 Source = CDataFromHash(Acc, RootHash);

    if (!SchemaReady || !Source.Size) {
        *Retry = TRUE;
        FLVCompilerUnlock();
        AccessClose(Acc);
        ReleaseScratch(Scratch);

        return Result;
    }

    SymTabMark BindingsMark = SymTabMarkGet();
    Arena* MemPool = ArenaAlloc();
    FLVUnit* Unit = ArenaPushStructZero(MemPool, FLVUnit);
    ParsedFile Root = {};

    Unit->MemPool = MemPool;
    Unit->Key = ArenaPushStrCpy(MemPool, Key);
    Unit->Path = ArenaPushStrCpy(MemPool, Path);
    Root.Path = Unit->Path;
    Root.Source = FLVSourceCpy(MemPool, Source);
    Root.Hash = RootHash;
    Root.Tokens = LexString(MemPool, Root.Source, Compiler->Names);
    Root.LineOffsets = LineOffsetsFromSource(
        MemPool, 
        Root.Source, 
        &Root.LineCount
    );

    AST* Tree = ParseTokens(MemPool, &Root, NULL, 0);

    if (GetTLS()->PARSER_STATE->ImportNotReady) {
        SymTabPopToMark(BindingsMark);
        FLVCompilerUnlock();
        ArenaRelease(MemPool);
        *Retry = TRUE;
        AccessClose(Acc);
        ReleaseScratch(Scratch);

        return Result;
    }

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
    Unit->ImportHashes = ArenaPushArray(MemPool, u128, Unit->ImportCount);

    for (u32 Index = 0; Index < Unit->ImportCount; ++Index) {
        ParsedFile const* File = &Tree->Files[Index + 1];

        Unit->ImportPaths[Index] = ArenaPushStrCpy(MemPool, File->Path);
        Unit->ImportHashes[Index] = File->Hash;
    }

    Unit->Hash = FLVFoldHashes(
        Hash(Str8Struct(&RootHash)), 
        Unit->ImportHashes, 
        Unit->ImportCount
    ) ^ Compiler->Schema->Hash;
    Result.V[0] = INT_FROM_PTR(MemPool);
    Result.V[1] = INT_FROM_PTR(Unit);
    SymTabPopToMark(BindingsMark);
    FLVCompilerUnlock();
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

INTERNAL FLVUnit*
FLVUnitFromKey(Access* Acc, Str8 Key, u64 EndTimeUSecs)
{
    OCObject Object = OCObjectFromKey(
        Acc,
        Key,
        FLVUnitObjectCreate,
        FLVUnitObjectDestroy,
        EndTimeUSecs,
        __Params.Generation = 0
    );

    return (FLVUnit*) PTR_FROM_INT(Object.V[1]);
}

INTERNAL Str8
FLVResultKey(
    Arena* MemPool,
    Str8 UnitKey,
    FLIKey Flight,
    u32 Program
) {
    Str8List Parts = {};
 
    ListPush(MemPool, &Parts, Str8Struct(&UnitKey.Size));
    ListPush(MemPool, &Parts, UnitKey);
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
    Str8 UnitKey = {};
    FLIKey Flight = {};
    u32 Program = 0;
 
    *Generation = FLILoadGeneration();
    Offset += SerialPopStruct(Key, Offset, &UnitKey.Size);
    UnitKey.Str = ArenaPushArray(Scratch.MemPool, u8, UnitKey.Size);
    Offset += SerialPop(Key, Offset, UnitKey.Str, UnitKey.Size, 1);
    Offset += SerialPopStruct(Key, Offset, &Flight);
    Offset += SerialPopStruct(Key, Offset, &Program);
 
    FLVUnit* Unit = FLVUnitFromKey(Acc, UnitKey, 0);
    FLIParsed* FLI = FLIParsedFromKey(Acc, Flight, TRUE, 0);
 
    if (!Unit || FLI == EMPTY_FLI_PARSED_VALUE) {
        *Retry = TRUE;
    } else if (Program < Unit->Lowered.ProgramCount) {
        Arena* MemPool = ArenaAlloc();
        Arena* RunMemPool = ArenaAlloc();
        FLVBoundSlot* Bound = FLVBindSlots(
            MemPool,
            FLI,
            Unit->Slots,
            Unit->SlotNames,
            Unit->SlotsCount
        );
        FLVLane CompilerLane = {};
        b32 Ready = FLVLaneInit(
            &CompilerLane,
            Acc,
            RunMemPool,
            MemPool,
            &Unit->Lowered.Program[Program],
            Flight,
            FLI,
            Bound,
            Unit->SlotsCount,
            0,
            0
        );

        if (!Ready) {
            *Retry = TRUE;
            ArenaRelease(RunMemPool);
            ArenaRelease(MemPool);
            AccessClose(Acc);
            ReleaseScratch(Scratch);

            return Result;
        }

        FLVLaneRun(&CompilerLane);
        ArenaRelease(RunMemPool);
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
    Str8 Key = FLVResultKey(Scratch.MemPool, Unit->Key, Flight, Program);
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
    u128 RootHash = FSHashFromPathRange(
        Path, 
        Rng((u64) 0, U64_MAX), 
        EndTimeUSecs
    );

    if (RootHash == u128{})
        return NULL;

    TempArena Scratch = GetScratch(NULL, 0);
    FLVClosure* Closure = FLVClosureFromPath(Path);
    u64 ImportsHash = 0;

    if (Closure) {
        u128* Hashes = ArenaPushArray(
            Scratch.MemPool, 
            u128, 
            Closure->ImportCount
        );

        for (u32 Index = 0; Index < Closure->ImportCount; ++Index)
            Hashes[Index] = FLVSourceHash(Closure->ImportPaths[Index]);

        ImportsHash = FLVFoldHashes(0, Hashes, Closure->ImportCount);
    }

    Str8 Key = FLVUnitKey(
        Scratch.MemPool, 
        Path, 
        Keys, 
        KeysCount, 
        RootHash, 
        ImportsHash
    );
    FLVUnit* Unit = FLVUnitFromKey(Acc, Key, EndTimeUSecs);

    if (Unit)
        FLVClosureUpdate(Path, Unit);

    ReleaseScratch(Scratch);

    return Unit;
}
