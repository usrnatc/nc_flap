#include "libnc.h"

#include "fli.h"
#include "fli_bin.h"
#include "fli_main.h"

#include "flv_intern.h"
#include "flv_token.h"
#include "flv_lexer.h"
#include "flv_type.h"
#include "flv_symbol_table.h"
#include "flv_stdlib.h"
#include "flv_schema.h"
#include "flv_parser.h"
#include "flv_type_check.h"
#include "flv_bind.h"
#include "flv_lower.h"
#include "flv_compiler.h"
#include "flv_compile.h"
#include "flv_kernel.h"
#include "flv_run.h"

#include "fli.cpp"
#include "fli_bin.cpp"
#include "fli_main.cpp"

#include "flv_intern.cpp"
#include "flv_token.cpp"
#include "flv_lexer.cpp"
#include "flv_type.cpp"
#include "flv_symbol_table.cpp"
#include "flv_stdlib.cpp"
#include "flv_schema.cpp"
#include "flv_parser.cpp"
#include "flv_type_check.cpp"
#include "flv_bind.cpp"
#include "flv_lower.cpp"
#include "flv_compiler.cpp"
#include "flv_compile.cpp"
#include "flv_kernel.cpp"
#include "flv_run.cpp"

internal void
AsyncThreadEntryPoint(void* Params)
{
    LaneContext LaneCtx = *(LaneContext*) Params;
 
    SetLaneContext(LaneCtx);
    GetTLS()->IS_ASYNC_THREAD = TRUE;
    ThreadSetName("[ASYNC %llu]", LaneIndex());
 
    for (;;) {
        if (!LaneIndex()) {
            LOCK_SCOPE(ASYNC_TICK_BEGIN_MTX) {
                if (!AtomicLoadU32(&ASYNC_LOOP_REPEAT, MEM_ORDER_SEQ_CST)) {
                    CondVarWait(
                        ASYNC_TICK_BEGIN_COND_VAR,
                        ASYNC_TICK_BEGIN_MTX,
                        TimeNow() + MILLISECONDS(100)
                    );
                }
            }

            AtomicExchangeU32(
                &ASYNC_LOOP_REPEAT, 
                0, 
                MEM_ORDER_SEQ_CST
            );
            AtomicExchangeU32(
                &ASYNC_LOOP_REPEAT_HIGH_PRIORITY, 
                0, 
                MEM_ORDER_SEQ_CST
            );
        }
 
        LaneSync();
        OCAsyncTick();
        CAsyncTick();
        FSAsyncTick();
        LogAsyncTick();
        FLIAsyncTick();
        LaneSync();

        if (!LaneIndex())
            AsyncTickEnd();
 
        b32 ShouldQuit = FALSE;
 
        if (!LaneIndex())
            ShouldQuit = AtomicLoadU32(&GLOBAL_ASYNC_EXIT, MEM_ORDER_SEQ_CST);
 
        LaneSyncU64(&ShouldQuit, 0);
 
        if (ShouldQuit)
            break;
    }
}

struct FLVTestView {
    Str8   Path;
    Arena* MemPool;
    u64    Hash;
    b32*   Printed;
    u32    Count;
    b32    HasError;
    u128   RootHash;
    b32    Ready;
};

INTERNAL void
FLVTest(
    FLIKey const* Keys,
    u64 KeysCount,
    char const** ViewPaths,
    u32 ViewCount,
    u64 IdleUSecs
) {
    TempArena Scratch = GetScratch(NULL, 0);
    FLVTestView* Views = ArenaPushArrayZero(
        Scratch.MemPool, 
        FLVTestView, 
        ViewCount
    );
    u64 LastChangeUSecs = TimeNow();
 
    for (u32 Index = 0; Index < ViewCount; ++Index) {
        Views[Index].Path = Str((char*) ViewPaths[Index]);
        Views[Index].MemPool = ArenaAlloc();
    }
 
    for (;;) {
        Access* Acc = AccessOpen();
        b32 Changed = FALSE;
 
        for (u32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex) {
            FLVTestView* View = &Views[ViewIndex];
            u128 RootHash = FSHashFromPathRange(View->Path, Rng((u64) 0, U64_MAX), 0);

            if (RootHash != View->RootHash) {
                PrintOut(
                    "[TEST] %S: root hash %016llx%016llx -> %016llx%016llx (tick %llu)\n",
                    PRINT_STR(StrSkipLastSlash(View->Path)),
                    View->RootHash.U64[0],
                    View->RootHash.U64[1],
                    RootHash.U64[0],
                    RootHash.U64[1],
                    UpdateTickIndex()
                );
                View->RootHash = RootHash;
            }

            FLVUnit* Unit = FLVUnitFromPath(
                Acc, 
                View->Path, 
                Keys, 
                KeysCount, 
                0
            );
 
            if (!!Unit != View->Ready) {
                View->Ready = !!Unit;
                PrintOut(
                    "[TEST] %S: unit %s (tick %llu)\n",
                    PRINT_STR(StrSkipLastSlash(View->Path)),
                    Unit ? "READY" : "NOT READY",
                    UpdateTickIndex()
                );
            }

            if (!Unit)
                continue;
 
            if (Unit->Hash != View->Hash) {
                ArenaClear(View->MemPool);
                View->Hash = Unit->Hash;
                View->Count = Unit->Lowered.ProgramCount * (u32) KeysCount;
                View->Printed = ArenaPushArrayZero(
                    View->MemPool, 
                    b32, 
                    View->Count
                );
                View->HasError = Unit->HasError;
                Changed = TRUE;
 
                PrintOut(
                    "\n=== %S === hash %016llx, %u slots, %u analyses, %u layout nodes, errors: %s\n",
                    PRINT_STR(View->Path),
                    Unit->Hash,
                    Unit->SlotsCount,
                    Unit->Lowered.ProgramCount,
                    Unit->Lowered.LayoutCount,
                    Unit->HasError ? "YES" : "NO"
                );
 
                for (
                    u32 Program = 0; 
                    Program < Unit->Lowered.ProgramCount; 
                    ++Program
                ) {
                    Str8List Lines = FLVProgramPrint(
                        View->MemPool,
                        &Unit->Lowered.Program[Program],
                        Unit->Slots,
                        Unit->SlotNames
                    );
 
                    for (Str8Node* Node = Lines.Head; Node; Node = Node->Next)
                        PrintOut("%S\n", PRINT_STR(Node->String));
                }
            }
 
            if (View->HasError)
                continue;
 
            for (
                u32 Program = 0; 
                Program < Unit->Lowered.ProgramCount; 
                ++Program
            ) {
                for (u64 Flight = 0; Flight < KeysCount; ++Flight) {
                    u32 Slot = Program * (u32) KeysCount + (u32) Flight;
 
                    if (View->Printed[Slot])
                        continue;
 
                    FLVRenderItem* Items = FLVResultFromUnit(
                        Acc,
                        Unit,
                        Keys,
                        KeysCount,
                        Keys[Flight],
                        Program,
                        0
                    );
 
                    if (!Items)
                        continue;
 
                    View->Printed[Slot] = TRUE;
                    Changed = TRUE;
 
                    PrintOut(
                        "[%S] analysis %u on flight %llu:\n",
                        PRINT_STR(StrSkipLastSlash(View->Path)),
                        Program,
                        Flight
                    );
 
                    Str8List Lines = FLVRenderItemsPrint(
                        View->MemPool,
                        &Unit->Lowered.Program[Program],
                        Items
                    );
 
                    for (Str8Node* Node = Lines.Head; Node; Node = Node->Next)
                        PrintOut("%S\n", PRINT_STR(Node->String));
                }
            }
        }
 
        AccessClose(Acc);
 
        if (Changed)
            LastChangeUSecs = TimeNow();
        else if (TimeNow() - LastChangeUSecs > IdleUSecs)
            break;
 
        AsyncWaitTick(MILLISECONDS(50));
    }
 
    for (u32 Index = 0; Index < ViewCount; ++Index)
        ArenaRelease(Views[Index].MemPool);
 
    ReleaseScratch(Scratch);
}

void 
EntryPoint(CommandLine* CLI)
{
#if defined(NC_FLI)
    FLIEntryPoint(CLI);
#else
    TempArena Scratch = GetScratch(NULL, 0);
    u64 LaneBroadcastValue = 0;
    u64 AsyncThreadsCount = GetSystemProperties()->LogicalProcessorCount - 1;
    Str8 AsyncThreadsCountString = CommandLineString(CLI, "threads"_s8);

    if (AsyncThreadsCountString.Size)
        AsyncThreadsCount = U64FromStr(AsyncThreadsCountString);

    AsyncThreadsCount = MAX(1, AsyncThreadsCount);

    Handle Barrier = BarrierAlloc(AsyncThreadsCount);
    LaneContext* LaneCtxs = ArenaPushArrayZero(
        Scratch.MemPool,
        LaneContext,
        AsyncThreadsCount
    );

    ASYNC_THREADS_COUNT = AsyncThreadsCount;

    Handle* AsyncThreads = ArenaPushArrayZero(
        Scratch.MemPool,
        Handle,
        ASYNC_THREADS_COUNT
    );

    for (u64 Index = 0; Index < ASYNC_THREADS_COUNT; ++Index) {
        LaneCtxs[Index].Index = Index;
        LaneCtxs[Index].Count = ASYNC_THREADS_COUNT;
        LaneCtxs[Index].Barrier = Barrier;
        LaneCtxs[Index].BroadcastMemory = &LaneBroadcastValue;
        AsyncThreads[Index] = ThreadLaunch(
            AsyncThreadEntryPoint, 
            &LaneCtxs[Index]
        );
    }

    local char* FLIPaths[] = {
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000010.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000034.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000038.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000047.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000048.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000050.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000058.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000061.BIN",
        // "H:\\rfdesign\\nc_flap\\tools\\logs\\00000064.BIN"
    };
    FLIKey Keys[ARRAY_COUNT(FLIPaths)] = {};

    for (u64 Index = 0; Index < ARRAY_COUNT(FLIPaths); ++Index) {
        Str8 Path = Str(FLIPaths[Index]);

        Keys[Index] = FLIKeyFromPathTimeStamp(Path, 0);
        FLIOpen(Keys[Index]);
    }

    local char const* ViewPaths[] = {
        // "H:\\rfdesign\\nc_flap\\tools\\flv_examples\\power.flv",
        // "H:\\rfdesign\\nc_flap\\tools\\flv_examples\\vibration.flv",
        // "H:\\rfdesign\\nc_flap\\tools\\flv_examples\\ekf.flv",
        // "H:\\rfdesign\\nc_flap\\tools\\flv_examples\\tuning.flv",
        // "H:\\rfdesign\\nc_flap\\tools\\flv_examples\\overview.flv",
        "H:\\rfdesign\\nc_flap\\tools\\flv_examples\\tester.flv"
    };
 
    FLVTest(
        Keys,
        ARRAY_COUNT(FLIPaths),
        ViewPaths,
        ARRAY_COUNT(ViewPaths),
        SECONDS(60)
    );
    AtomicExchangeU32(&GLOBAL_ASYNC_EXIT, TRUE, MEM_ORDER_SEQ_CST);
    AsyncWake(FALSE);

    for (u64 Index = 0; Index < ASYNC_THREADS_COUNT; ++Index)
        ThreadJoin(AsyncThreads[Index], U64_MAX);

    BarrierRelease(Barrier);
    ReleaseScratch(Scratch);
#endif
}
