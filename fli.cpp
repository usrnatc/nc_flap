#include "fli.h"
#include "nc_memory.h"
#include "nc_atomics.h"
#include "nc_proc.h"
#include "nc_math.h"

#include "rad_lzb_simple/rad_lzb_simple.h"
#include "radsort/radsort.h"

FLIParsed            __EMPTY_FLI_PARSED_VALUE;
FLIParsed* const     EMPTY_FLI_PARSED_VALUE           = &__EMPTY_FLI_PARSED_VALUE;
FLIEmptyUnion        __EMPTY_FLI_ELEMENT_UNION_VALUE;
FLIEmptyUnion* const EMPTY_FLI_ELEMENT_UNION_VALUE    = &__EMPTY_FLI_ELEMENT_UNION_VALUE;

FLIShared* FLI_SHARED = NULL;
FLIBakeShared* FLI_BAKE_SHARED = NULL;

u64 FLI_SECTION_ELEMENT_SIZE_TABLE[FLI_SECTION_KIND_COUNT] = {
    sizeof(u8),
    sizeof(FLIFileInfo),
    sizeof(FLIMessageType),
    sizeof(FLIField),
    sizeof(FLIColumn),
    sizeof(FLIParam),
    sizeof(FLIMessage),
    sizeof(FLIMode),
    sizeof(FLIEvt),
    sizeof(FLIError),
    sizeof(u8),
    sizeof(u32),
    sizeof(u32),
    sizeof(FLINameMap),
    sizeof(FLINameMapBucket),
    sizeof(FLINameMapNode)
};

u64 FLI_SCALAR_KIND_SIZE_TABLE[FLI_SCALAR_KIND_COUNT] = {
    1,
    1,
    1,
    2,
    2,
    4,
    4,
    8,
    8,
    4,
    8,
    4,
    16,
    64
};

INTERNAL Str8
FLIStringFromNameMapKind(FLINameMapKind MapKind)
{
    Str8 Result = ""_s8;

    switch (MapKind) {
        default: {} break;

        case FLI_NAME_MAP_KIND_MESSAGE_TYPES: {
            Result = "Message Types"_s8;
        } break;

        case FLI_NAME_MAP_KIND_FIELDS: {
            Result = "Fields"_s8;
        } break;

        case FLI_NAME_MAP_KIND_PARAMS: {
            Result = "Params"_s8;
        } break;

        case FLI_NAME_MAP_KIND_STRING_DATA: {
            Result = "String Data"_s8;
        } break;

        case FLI_NAME_MAP_KIND_STRING_TABLE: {
            Result = "String Table"_s8;
        } break;
    }

    return Result;
}

void 
ListPush(Arena* MemPool, FLIKeyList* List, FLIKey Key)
{
    FLIKeyNode* Node = ArenaPushArrayZero(MemPool, FLIKeyNode, 1);

    Node->V = Key;
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->Count;
}

FLIKeyArray 
FLIKeyArrayFromList(Arena* MemPool, FLIKeyList* List)
{
    FLIKeyArray Result = {};

    Result.Count = List->Count;
    Result.V = ArenaPushArrayZero(MemPool, FLIKey, Result.Count);

    u64 Index = 0;

    for (
        FLIKeyNode* Node = List->Head;
        Node;
        Node = Node->Next
    ) {
        Result.V[Index++] = Node->V;
    }

    return Result;
}

void 
FLIInit(CommandLine* CLI)
{
    Arena* MemPool = ArenaAlloc();

    FLI_SHARED = ArenaPushArrayZero(MemPool, FLIShared, 1);
    FLI_SHARED->MemPool = MemPool;
    FLI_SHARED->Key2PathSlotsCount = 4096;
    FLI_SHARED->Key2PathSlots = ArenaPushArrayZero(
        MemPool, 
        FLIKeySlot, 
        FLI_SHARED->Key2PathSlotsCount
    );
    FLI_SHARED->Key2PathStripes = StripeArrayAlloc(MemPool);
    FLI_SHARED->Path2KeySlotsCount = 4096;
    FLI_SHARED->Path2KeySlots = ArenaPushArrayZero(
        MemPool, 
        FLIKeySlot, 
        FLI_SHARED->Path2KeySlotsCount
    );
    FLI_SHARED->Path2KeyStripes = StripeArrayAlloc(MemPool);
    FLI_SHARED->SlotsCount = 4096;
    FLI_SHARED->Slots = ArenaPushArrayZero(
        MemPool, 
        FLISlot, 
        FLI_SHARED->SlotsCount
    );
    FLI_SHARED->Stripes = StripeArrayAlloc(MemPool);

    for (u64 Index = 0; Index < ARRAY_COUNT(FLI_SHARED->RequestBatches); ++Index) {
        FLI_SHARED->RequestBatches[Index].Mtx = MutexAlloc();
        FLI_SHARED->RequestBatches[Index].MemPool = ArenaAlloc();
    }

    u64 SignalPID = 0;
    Str8 SignalPIDString = CommandLineString(CLI, "signal_pid"_s8);
    b32 HasParent = TRUE;

    if (!U64FromStr(SignalPIDString)) {
        HasParent = FALSE;
        SignalPID = GetProcessProperties()->PID;
    }

    u64 SignalCode = 0;
    Str8 SignalCodeString = CommandLineString(CLI, "signal_code"_s8);

    SignalCode = U64FromStr(SignalCodeString);
    FLI_SHARED->ConversionCompletionCode = SignalCode;
    FLI_SHARED->ConversionCompletionLockSemaphoreName = ArenaPushStrFmt(
        MemPool,
        "conversion_completion_lock_pid_%llu",
        SignalPID
    );
    FLI_SHARED->ConversionCompletionSignalSemaphoreName = ArenaPushStrFmt(
        MemPool,
        "conversion_completion_signal_pid_%llu",
        SignalPID
    );
    FLI_SHARED->ConversionCompletionSharedMemoryName = ArenaPushStrFmt(
        MemPool,
        "conversion_completion_shared_memory_pid_%llu",
        SignalPID
    );

    if (HasParent) {
        FLI_SHARED->ConversionCompletionLockSemaphore = SemaphoreOpen(
            FLI_SHARED->ConversionCompletionLockSemaphoreName
        );
        FLI_SHARED->ConversionCompletionSignalSemaphore = SemaphoreOpen(
            FLI_SHARED->ConversionCompletionSignalSemaphoreName
        );
        FLI_SHARED->ConversionCompletionSharedMemory = SharedMemoryOpen(
            FLI_SHARED->ConversionCompletionSharedMemoryName
        );
    } else {
        FLI_SHARED->ConversionCompletionLockSemaphore = SemaphoreAlloc(
            1,
            1,
            FLI_SHARED->ConversionCompletionLockSemaphoreName
        );
        FLI_SHARED->ConversionCompletionSignalSemaphore = SemaphoreAlloc(
            0,
            U16_MAX + 1,
            FLI_SHARED->ConversionCompletionSignalSemaphoreName
        );
        FLI_SHARED->ConversionCompletionSharedMemory = SharedMemoryAlloc(
            KB(4),
            FLI_SHARED->ConversionCompletionSharedMemoryName
        );
        FLI_SHARED->ConversionCompletionSignalReceiverThread = ThreadLaunch(
            FLIConversionCompletionSignalReceiverThreadEntryPoint,
            NULL
        );
    }

    FLI_SHARED->ConversionCompletionSharedMemoryBase = (u64*) SharedMemoryViewOpen(
        FLI_SHARED->ConversionCompletionSharedMemory,
        Rng(0ULL, KB(4))
    );
    FLI_SHARED->CompletionMtx = MutexAlloc();
    FLI_SHARED->CompletionMemPool = ArenaAlloc();
    FLI_SHARED->EventMtx = MutexAlloc();
    FLI_SHARED->EventMemPool = ArenaAlloc();
}

FLIKey 
FLIKeyFromPathTimeStamp(Str8 Path, u64 MinTimeStamp)
{
    u64 PathHash = Hash(Path);
    u64 SlotIndex = PathHash % FLI_SHARED->Path2KeySlotsCount;
    FLIKeySlot* Slot = &FLI_SHARED->Path2KeySlots[SlotIndex];
    Stripe* Strip = StripeFromSlotIndex(
        &FLI_SHARED->Path2KeyStripes, 
        SlotIndex
    );
    FLIKey Result = {};

    for (b32 WriteMode = FALSE; WriteMode <= TRUE; ++WriteMode) {
        b32 Found = FALSE;

        RWLOCK_SCOPE(Strip->RWMtx, WriteMode) {
            FLIKeyPathNode* Node = NULL;

            for (FLIKeyPathNode* N = Slot->Head; N; N = N->Next) {
                if (
                    StrMatch(N->Path, Path, 0) && 
                    MinTimeStamp <= N->MinTimeStamp
                ) {
                    Found = TRUE;
                    Node = N;
                    Result = Node->Key;
                    break;
                }
            }

            if (!Found && WriteMode) {
                Node = (FLIKeyPathNode*) Strip->Free;

                if (Node)
                    Strip->Free = Node->Next;
                else
                    Node = ArenaPushArrayZero(Strip->MemPool, FLIKeyPathNode, 1);

                Node->Path = ArenaPushStrCpy(Strip->MemPool, Path);
                Node->MinTimeStamp = MinTimeStamp;
                Node->Key = Result;
                DLL_PUSH_BACK(Slot->Head, Slot->Tail, Node);
            }
        }

        if (Found)
            break;

        if (!Found && !WriteMode) {
            u128 FileHash = BigHash(MinTimeStamp, Path);

            MemCpy(&Result, &FileHash, MIN(sizeof(FileHash), sizeof(Result)));

            u64 KeyHash = Hash(Str8Struct(&Result));
            u64 KeySlotIndex = KeyHash % FLI_SHARED->Key2PathSlotsCount;
            FLIKeySlot* KeySlot = &FLI_SHARED->Key2PathSlots[KeySlotIndex];
            Stripe* KeyStripe = StripeFromSlotIndex(
                &FLI_SHARED->Key2PathStripes, 
                KeySlotIndex
            );

            LOCK_SCOPE_W(KeyStripe->RWMtx) {
                FLIKeyPathNode* Node = NULL;

                for (
                    FLIKeyPathNode* N = KeySlot->Head;
                    N;
                    N = N->Next
                ) {
                    if (N->Key == Result) {
                        Node = N;
                        break;
                    }
                }

                if (!Node) {
                    Node = (FLIKeyPathNode*) KeyStripe->Free;

                    if (Node)
                        KeyStripe->Free = Node->Next;
                    else
                        Node = ArenaPushArrayZero(KeyStripe->MemPool, FLIKeyPathNode, 1);

                    DLL_PUSH_BACK(KeySlot->Head, KeySlot->Tail, Node);
                    Node->Path = ArenaPushStrCpy(KeyStripe->MemPool, Path);
                    Node->MinTimeStamp = MinTimeStamp;
                    Node->Key = Result;
                }
            }
        }
    }

    return Result;
}

void 
FLIOpen(FLIKey Key)
{
    u64 KeyHash = Hash(Str8Struct(&Key));
    u64 SlotIndex = KeyHash % FLI_SHARED->SlotsCount;
    FLISlot* Slot = &FLI_SHARED->Slots[SlotIndex];
    Stripe* Strip = StripeFromSlotIndex(&FLI_SHARED->Stripes, SlotIndex);
    b32 NodeIsNew = FALSE;

    LOCK_SCOPE_W(Strip->RWMtx) {
        FLINode* Node = NULL;

        for (FLINode* N = Slot->Head; N; N = N->Next) {
            if (N->Key == Key) {
                Node = N;
                break;
            }
        }

        if (!Node) {
            NodeIsNew = TRUE;
            Node = (FLINode*) Strip->Free;

            if (Node)
                Strip->Free = Node->Next;
            else
                Node = ArenaPushArray(Strip->MemPool, FLINode, 1);

            MemSet(Node, 0, sizeof(*Node));
            DLL_PUSH_BACK(Slot->Head, Slot->Tail, Node);
            Node->Key = Key;
            Node->BatchRequestsCounts[1] = 1;
        }

        ++Node->RefCount;
    }

    if (NodeIsNew) {
        FLIRequestBatch* Batch = &FLI_SHARED->RequestBatches[1];

        LOCK_SCOPE(Batch->Mtx) {
            FLIRequestNode* N = ArenaPushArrayZero(
                Batch->MemPool, 
                FLIRequestNode, 
                1
            );

            SLL_QUEUE_PUSH(Batch->Head, Batch->Tail, N);
            N->V.Key = Key;
            ++Batch->Count;
        }

        AsyncWake(FALSE);
    }
}

void 
FLIClose(FLIKey Key, b32 ForceClosed)
{
    u64 KeyHash = Hash(Str8Struct(&Key));
    u64 SlotIndex = KeyHash % FLI_SHARED->SlotsCount;
    FLISlot* Slot = &FLI_SHARED->Slots[SlotIndex];
    Stripe* Strip = StripeFromSlotIndex(&FLI_SHARED->Stripes, SlotIndex);
    b32 NodeReleased = FALSE;
    Handle File = {};
    Handle FileMap = {};
    FileProperties FileProps = {};
    void* FileBase = NULL;
    Arena* MemPool = NULL;

    LOCK_SCOPE_W(Strip->RWMtx) {
        FLINode* Node = NULL;

        for (FLINode* N = Slot->Head; N; N = N->Next) {
            if (
                N->Key == Key && 
                AtomicLoadU64(&N->CompletionCount, MEM_ORDER_SEQ_CST)
            ) {
                Node = N;
                break;
            }
        }

        if (Node) {
            if (ForceClosed)
                Node->RefCount = 0;
            else
                --Node->RefCount;

            if (!Node->RefCount) {
                for (;;) {
                    if (
                        AccessPointIsExpired(
                            &Node->Acc, 
                            __Params.Time = 0, 
                            __Params.UpdateIndexes = 0
                        )
                    ) {
                        NodeReleased = TRUE;
                        DLL_REMOVE(Slot->Head, Slot->Tail, Node);
                        Node->Next = (FLINode*) Strip->Free;
                        File = Node->File;
                        FileMap = Node->FileMap;
                        FileProps = Node->FileProps;
                        FileBase = Node->FileBase;
                        MemPool = Node->MemPool;
                        break;
                    }

                    CondVarWaitW(Strip->CondVar, Strip->RWMtx, U64_MAX);
                }
            }
        }
    }

    if (NodeReleased) {
        AtomicDecFetchU64(&FLI_SHARED->LoadCount, MEM_ORDER_SEQ_CST);
        AtomicIncFetchU64(&FLI_SHARED->LoadGeneration, MEM_ORDER_SEQ_CST);
        SystemCloseFileMapView(FileMap, FileBase, Rng(0ULL, FileProps.Size));
        SystemCloseFileMap(FileMap);
        SystemCloseFile(File);

        if (MemPool)
            ArenaRelease(MemPool);
    }
}

u64 
FLILoadGeneration(void)
{
    return AtomicLoadU64(&FLI_SHARED->LoadGeneration, MEM_ORDER_SEQ_CST);
}

u64 
FLILoadCount(void)
{
    return AtomicLoadU64(&FLI_SHARED->LoadCount, MEM_ORDER_SEQ_CST);
}

FLIKeyArray 
FLIPushAllLoadedKeys(Arena* MemPool)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    FLIKeyList List = {};

    for (
        u64 SlotIndex = 0; 
        SlotIndex < FLI_SHARED->Key2PathSlotsCount; 
        ++SlotIndex
    ) {
        FLIKeySlot* Slot = &FLI_SHARED->Key2PathSlots[SlotIndex];
        Stripe* Strip = StripeFromSlotIndex(
            &FLI_SHARED->Key2PathStripes, 
            SlotIndex
        );

        LOCK_SCOPE_R(Strip->RWMtx) {
            for (FLIKeyPathNode* N = Slot->Head; N; N = N->Next) {
                FLIKeyNode* DstN = ArenaPushArrayZero(
                    Scratch.MemPool, 
                    FLIKeyNode, 
                    1
                );
                
                SLL_QUEUE_PUSH(List.Head, List.Tail, DstN);
                ++List.Count;
                DstN->V = N->Key;
            }
        }
    }

    FLIKeyArray Result = {};

    Result.Count = List.Count;
    Result.V = ArenaPushArrayZero(MemPool, FLIKey, Result.Count);

    u64 Index = 0;

    for (FLIKeyNode* N = List.Head; N; N = N->Next) {
        Result.V[Index++] = N->V;
    }

    ReleaseScratch(Scratch);

    return Result;
}

FLIParsed* 
FLIParsedFromKey(Access* Acc, FLIKey Key, b32 HighPriority, u64 EndTimeUSecs)
{
    FLIParsed* Result = EMPTY_FLI_PARSED_VALUE;
    u64 KeyHash = Hash(Str8Struct(&Key));
    u64 SlotIndex = KeyHash % FLI_SHARED->SlotsCount;
    FLISlot* Slot = &FLI_SHARED->Slots[SlotIndex];
    Stripe* Strip = StripeFromSlotIndex(&FLI_SHARED->Stripes, SlotIndex);

    LOCK_SCOPE_R(Strip->RWMtx) {
        for (;;) {
            b32 Found = FALSE;
            b32 NeedHiRequest = FALSE;
            b32 Grabbed = FALSE;

            for (FLINode* N = Slot->Head; N; N = N->Next) {
                if (
                    N->Key == Key && 
                    AtomicLoadU64(&N->RefCount, MEM_ORDER_SEQ_CST)
                ) {
                    Found = TRUE;

                    if (
                        HighPriority && 
                        !AtomicCASU64(
                            &N->BatchRequestsCounts[0], 
                            0, 
                            1, 
                            MEM_ORDER_SEQ_CST, 
                            MEM_ORDER_SEQ_CST
                        )
                    ) {
                        NeedHiRequest = TRUE;
                    }

                    if (
                        AtomicLoadU64(
                            &N->CompletionCount, 
                            MEM_ORDER_SEQ_CST
                        )
                    ) {
                        Grabbed = TRUE;
                        Result = &N->FLI;
                        AccessTouch(Acc, &N->Acc, Strip->CondVar);
                    }

                    break;
                }
            }

            if (NeedHiRequest) {
                FLIRequestBatch* Batch = &FLI_SHARED->RequestBatches[0];

                LOCK_SCOPE(Batch->Mtx) {
                    FLIRequestNode* N = ArenaPushArrayZero(
                        Batch->MemPool, 
                        FLIRequestNode, 
                        1
                    );

                    SLL_QUEUE_PUSH(Batch->Head, Batch->Tail, N);
                    N->V.Key = Key;
                    ++Batch->Count;
                }

                AsyncWake(TRUE);
            }

            if (Grabbed || TimeNow() >= EndTimeUSecs)
                break;

            CondVarWaitR(Strip->CondVar, Strip->RWMtx, EndTimeUSecs);
        }
    }

    return Result;
}

FLIEventList 
FLIGetEvents(Arena* MemPool)
{
    FLIEventList Result = {};

    LOCK_SCOPE(FLI_SHARED->EventMtx) {
        for (
            FLIEventNode* SrcN = FLI_SHARED->Events.Head; 
            SrcN; 
            SrcN = SrcN->Next
        ) {
            FLIEventNode* DstN = ArenaPushArrayZero(MemPool, FLIEventNode, 1);

            MemCpy(&DstN->V, &SrcN->V, sizeof(DstN->V));
            DstN->V.String = ArenaPushStrCpy(MemPool, DstN->V.String);
            SLL_QUEUE_PUSH(Result.Head, Result.Tail, DstN);
            ++Result.Count;
        }

        MemSet(&FLI_SHARED->Events, 0, sizeof(FLI_SHARED->Events));
        ArenaClear(FLI_SHARED->EventMemPool);
    }

    return Result;
}

void 
FLIAsyncTick(void)
{
    TempArena Scratch = GetScratch(NULL, 0);

    struct ParseTask {
        FLIKey Key;
        Str8   Path;
    };

    ParseTask* ParseTasks = NULL;
    u64 ParseTasksCount = 0;

    if (!LaneIndex()) {
        struct ParseTaskNode {
            ParseTaskNode* Next;
            ParseTask      V;
        };

        ParseTaskNode* HeadParseTask = NULL;
        ParseTaskNode* TailParseTask = NULL;
        FLIRequestNode* HeadRequest[2] = {};
        FLIRequestNode* TailRequest[2] = {};

        for (
            u64 Index = 0; 
            Index < ARRAY_COUNT(FLI_SHARED->RequestBatches); 
            ++Index
        ) {
            FLIRequestBatch* B = &FLI_SHARED->RequestBatches[Index];

            LOCK_SCOPE(B->Mtx) {
                for (FLIRequestNode* N = B->Head; N; N = N->Next) {
                    FLIRequestNode* NCopy = ArenaPushArrayZero(
                        Scratch.MemPool, 
                        FLIRequestNode, 
                        1
                    );

                    MemCpy(&NCopy->V, &N->V, sizeof(NCopy->V));
                    SLL_QUEUE_PUSH(
                        HeadRequest[Index], 
                        TailRequest[Index], 
                        NCopy
                    );
                }

                ArenaClear(B->MemPool);
                B->Head = NULL;
                B->Tail = NULL;
                B->Count = 0;
            }
        }

        FLILoadCompletion* HeadCompletion = NULL;
        FLILoadCompletion* TailCompletion = NULL;

        LOCK_SCOPE(FLI_SHARED->CompletionMtx) {
            for (
                FLILoadCompletion* C = FLI_SHARED->HeadCompletion; 
                C; 
                C = C->Next
            ) {
                FLILoadCompletion* DstC = ArenaPushArrayZero(
                    Scratch.MemPool, 
                    FLILoadCompletion, 
                    1
                );

                SLL_QUEUE_PUSH(HeadCompletion, TailCompletion, DstC);
                DstC->Code = C->Code;
            }

            ArenaClear(FLI_SHARED->CompletionMemPool);
            FLI_SHARED->HeadCompletion = NULL;
            FLI_SHARED->TailCompletion = NULL;
        }

        for (
            u64 PriorityIndex = 0; 
            PriorityIndex < ARRAY_COUNT(HeadRequest); 
            ++PriorityIndex
        ) {
            for (
                FLIRequestNode* N = HeadRequest[PriorityIndex]; 
                N; 
                N = N->Next
            ) {
                FLIKey Key = N->V.Key;
                b32 RequestIsDuplicate = TRUE;
                u64 KeyHash = Hash(Str8Struct(&Key));
                u64 SlotIndex = KeyHash % FLI_SHARED->SlotsCount;
                FLISlot* Slot = &FLI_SHARED->Slots[SlotIndex];
                Stripe* Strip = StripeFromSlotIndex(
                    &FLI_SHARED->Stripes, 
                    SlotIndex
                );

                LOCK_SCOPE_R(Strip->RWMtx) {
                    for (FLINode* N = Slot->Head; N; N = N->Next) {
                        if (
                            N->Key == Key && 
                            !AtomicLoadU64(
                                &N->CompletionCount, 
                                MEM_ORDER_SEQ_CST
                            )
                        ) {
                            RequestIsDuplicate = !!(
                                AtomicCASU64(
                                    &N->WorkingCount, 
                                    0, 
                                    1, 
                                    MEM_ORDER_SEQ_CST, 
                                    MEM_ORDER_SEQ_CST
                                )
                            );
                            break;
                        }
                    }
                }

                if (!RequestIsDuplicate) {
                    FLILoadTask* T = FLI_SHARED->FreeLoadTask;

                    if (T)
                        SLL_STACK_POP(FLI_SHARED->FreeLoadTask);
                    else
                        T = ArenaPushArray(FLI_SHARED->MemPool, FLILoadTask, 1);

                    MemSet(T, 0, sizeof(*T));
                    DLL_PUSH_BACK(
                        FLI_SHARED->HeadLoadTask[PriorityIndex], 
                        FLI_SHARED->TailLoadTask[PriorityIndex], 
                        T
                    );
                    T->Key = Key;
                }
            }
        }

        for (
            u64 PriorityIndex = 0; 
            PriorityIndex < ARRAY_COUNT(HeadRequest); 
            ++PriorityIndex
        ) {
            for (
                FLILoadTask* T = FLI_SHARED->HeadLoadTask[PriorityIndex], *Next = NULL; 
                T; 
                T = Next
            ) {
                Next = T->Next;

                FLIKey Key = T->Key;
                u64 KeyHash = Hash(Str8Struct(&Key));
                u64 KeySlotIndex = KeyHash % FLI_SHARED->Key2PathSlotsCount;
                FLIKeySlot* KeySlot = &FLI_SHARED->Key2PathSlots[KeySlotIndex];
                Stripe* KeyStrip = StripeFromSlotIndex(
                    &FLI_SHARED->Key2PathStripes, 
                    KeySlotIndex
                );
                Str8 OriginalPath = {};
                u64 OriginalMinTimeStamp = 0;

                LOCK_SCOPE_R(KeyStrip->RWMtx) {
                    for (FLIKeyPathNode* N = KeySlot->Head; N; N = N->Next) {
                        if (N->Key == Key) {
                            OriginalPath = ArenaPushStrCpy(
                                Scratch.MemPool, 
                                N->Path
                            );
                            OriginalMinTimeStamp = N->MinTimeStamp;
                            break;
                        }
                    }
                }

                if (!T->OriginalAnalysed) {
                    T->OriginalAnalysed = TRUE;

                    Handle File = SystemOpenFile(
                        SYS_ACCESS_SHARE_READ | SYS_ACCESS_READ, 
                        OriginalPath
                    );
                    FileProperties FileProps = SystemGetFileProperties(File);

                    T->OriginalSize = FileProps.Size;

                    u64 FLIMagicMaybe = 0;

                    if (
                        SystemReadStructFromFile(
                            File, 
                            0, 
                            &FLIMagicMaybe
                        ) == 8 &&
                        FLIMagicMaybe == FLI_MAGIC_NUMBER
                    ) {
                        T->OriginalIsFLI = TRUE;
                    }

                    SystemCloseFile(File);
                }

                u64 OriginalSize = T->OriginalSize;
                b32 OriginalIsFLI = T->OriginalIsFLI;
                b32 OriginalIsOkay = (OriginalSize > 0);
                Str8 FLIPath = {};

                if (OriginalIsFLI) {
                    FLIPath = OriginalPath;
                } else {
                    FLIPath = ArenaPushStrFmt(
                        Scratch.MemPool, 
                        "%S.fli", 
                        PRINT_STR(StrChopLastDot(OriginalPath))
                    );
                }

                if (!T->FLIAnalysed) {
                    T->FLIAnalysed = TRUE;

                    Handle File = SystemOpenFile(
                        SYS_ACCESS_SHARE_READ | SYS_ACCESS_READ, 
                        FLIPath
                    );
                    FileProperties FileProps = SystemGetFileProperties(File);

                    if (FileProps.Modified < OriginalMinTimeStamp) {
                        T->IsFLIStale = TRUE;
                    } else {
                        T->IsFLIStale = TRUE;

                        FLIHeader Header = {};

                        if (
                            SystemReadStructFromFile(
                                File, 
                                0, 
                                &Header
                            ) == sizeof(Header)
                        ) {
                            T->IsFLIStale = (
                                Header.EncodingVersion != FLI_ENCODING_VERSION
                            );
                        }
                    }

                    SystemCloseFile(File);
                }

                b32 FLIIsStale = T->IsFLIStale;

                if (!OriginalIsFLI && FLIIsStale && !T->ThreadCount) {
                    u64 ThreadCount = 1;
                    u64 MaxThreadCount = GetSystemProperties()->LogicalProcessorCount / 2;

                    if (PriorityIndex)
                        MaxThreadCount = MAX(1, MaxThreadCount / 2);

                    if (OriginalSize <= MB(4))
                        ThreadCount = 1;
                    else if (OriginalSize <= MB(256))
                        ThreadCount = MaxThreadCount / 4;
                    else if (OriginalSize <= MB(512))
                        ThreadCount = MaxThreadCount / 3;
                    else if (OriginalSize <= GB(1))
                        ThreadCount = MaxThreadCount / 2;
                    else
                        ThreadCount = MaxThreadCount;

                    ThreadCount = MAX(1, ThreadCount);
                    T->ThreadCount = ThreadCount;
                }

                b32 ThreadsAvailable = 0;
                u64 MaxThreads = GetSystemProperties()->LogicalProcessorCount / 2;
                u64 CurrentThreads = FLI_SHARED->ConversionThreadCount;
                u64 RequiredThreads = (CurrentThreads + T->ThreadCount);

                ThreadsAvailable = !!(MaxThreads >= RequiredThreads);

                b32 ReadyToLaunchConversion = (
                    ThreadsAvailable &&
                    !OriginalIsFLI &&
                    FLIIsStale &&
                    T->ThreadCount &&
                    T->Status != FLI_LOAD_STATUS_ACTIVE
                );

                if (ReadyToLaunchConversion) {
                    u64 Path2KeyHash = Hash(OriginalPath);
                    u64 Path2KeySlotIndex = Path2KeyHash % FLI_SHARED->Path2KeySlotsCount;
                    FLIKeySlot* Path2KeySlot = &FLI_SHARED->Path2KeySlots[Path2KeySlotIndex];
                    Stripe* Path2KeyStrip = StripeFromSlotIndex(
                        &FLI_SHARED->Path2KeyStripes, 
                        Path2KeySlotIndex
                    );

                    LOCK_SCOPE_R(Path2KeyStrip->RWMtx) {
                        for (
                            FLIKeyPathNode* N = Path2KeySlot->Tail; 
                            N; 
                            N = N->Prev
                        ) {
                            if (
                                StrMatch(
                                    N->Path, 
                                    OriginalPath, 
                                    0
                                ) && 
                                N->Key != Key
                            ) {
                                FLIClose(N->Key, TRUE);
                            }
                        }
                    }
                }

                if (OriginalIsOkay && ReadyToLaunchConversion) {
                    b32 ShouldCompress = FALSE;
                    ProcessLaunchProperties Params = {};
                    Str8 ConverterPath = {};

                #if defined(NC_OS_WIN)
                    ConverterPath = SystemGetFullFilePath(
                        Scratch.MemPool,
                        "fli.exe"_s8
                    );
                #else
                    ConverterPath = SystemGetFullFilePath(
                        Scratch.MemPool,
                        "fli"_s8
                    );
                #endif

                    Params.Path = GetProcessProperties()->InitialPath;
                    Params.InheritEnvironment = TRUE;
                    Params.HideConsole = TRUE;
                    ListPush(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        ConverterPath
                    );
                    ListPush(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--bin"_s8
                    );
                    ListPush(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--quiet"_s8
                    );

                    if (ShouldCompress) {
                        ListPush(
                            Scratch.MemPool, 
                            &Params.CommandLineArgs, 
                            "--compress"_s8
                        );
                    }

                    ListPush(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--fli"_s8
                    );
                    ListPushFmt(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--out:%S", 
                        PRINT_STR(FLIPath)
                    );
                    ListPushFmt(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--thread_count:%llu", 
                        T->ThreadCount
                    );
                    ListPushFmt(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--signal_pid:%llu", 
                        (u64) GetProcessProperties()->PID
                    );
                    ListPushFmt(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "--signal_code:%llu", 
                        INT_FROM_PTR(T)
                    );
                    ListPushFmt(
                        Scratch.MemPool, 
                        &Params.CommandLineArgs, 
                        "%S", 
                        PRINT_STR(OriginalPath)
                    );
                    T->Process = ProcessLaunch(&Params);
                    T->Status = FLI_LOAD_STATUS_ACTIVE;
                    ++FLI_SHARED->ConversionProcessCount;
                    FLI_SHARED->ConversionThreadCount += T->ThreadCount;

                    LOCK_SCOPE(FLI_SHARED->EventMtx) {
                        FLIEventNode* N = ArenaPushArrayZero(
                            FLI_SHARED->EventMemPool, 
                            FLIEventNode, 
                            1
                        );

                        SLL_QUEUE_PUSH(
                            FLI_SHARED->Events.Head, 
                            FLI_SHARED->Events.Tail, 
                            N
                        );
                        ++FLI_SHARED->Events.Count;
                        N->V.Kind = FLI_EVENT_KIND_CONVERSION_STARTED;
                        N->V.String = ArenaPushStrCpy(
                            FLI_SHARED->EventMemPool, 
                            FLIPath
                        );
                    }
                }

                if (T->Status == FLI_LOAD_STATUS_ACTIVE) {
                    b32 TaskIsDone = FALSE;

                    for (
                        FLILoadCompletion* C = HeadCompletion; 
                        C; 
                        C = C->Next
                    ) {
                        if (C->Code == INT_FROM_PTR(T)) {
                            TaskIsDone = TRUE;
                            break;
                        }
                    }

                    if (!TaskIsDone)
                        TaskIsDone = ProcessJoin(T->Process, 0);

                    if (TaskIsDone) {
                        T->Status = FLI_LOAD_STATUS_COMPLETE;
                        --FLI_SHARED->ConversionProcessCount;
                        FLI_SHARED->ConversionThreadCount -= T->ThreadCount;
                    }
                }

                if (!OriginalIsOkay && ReadyToLaunchConversion)
                    T->Status = FLI_LOAD_STATUS_COMPLETE;

                if (!FLIIsStale)
                    T->Status = FLI_LOAD_STATUS_COMPLETE;

                if (FLIIsStale && OriginalIsFLI)
                    T->Status = FLI_LOAD_STATUS_COMPLETE;

                if (T->Status == FLI_LOAD_STATUS_COMPLETE) {
                    if (T->Process != EMPTY_HANDLE_VALUE) {
                        LOCK_SCOPE(FLI_SHARED->EventMtx) {
                            FLIEventNode* N = ArenaPushArrayZero(
                                FLI_SHARED->EventMemPool, 
                                FLIEventNode, 
                                1
                            );

                            SLL_QUEUE_PUSH(
                                FLI_SHARED->Events.Head, 
                                FLI_SHARED->Events.Tail, 
                                N
                            );
                            ++FLI_SHARED->Events.Count;
                            N->V.Kind = FLI_EVENT_KIND_CONVERSION_ENDED;
                            N->V.String = ArenaPushStrCpy(
                                FLI_SHARED->EventMemPool, 
                                FLIPath
                            );
                        }
                    }

                    DLL_REMOVE(
                        FLI_SHARED->HeadLoadTask[PriorityIndex], 
                        FLI_SHARED->TailLoadTask[PriorityIndex], 
                        T
                    );
                    SLL_STACK_PUSH(FLI_SHARED->FreeLoadTask, T);

                    ParseTaskNode* N = ArenaPushArrayZero(
                        Scratch.MemPool, 
                        ParseTaskNode, 
                        1
                    );

                    N->V.Key = Key;
                    N->V.Path = FLIPath;
                    SLL_QUEUE_PUSH(HeadParseTask, TailParseTask, N);
                    ++ParseTasksCount;
                }
            }
        }

        ParseTasks = ArenaPushArrayZero(
            Scratch.MemPool, 
            ParseTask, 
            ParseTasksCount
        );

        u64 Index = 0;

        for (ParseTaskNode* N = HeadParseTask; N; N = N->Next)
            ParseTasks[Index++] = N->V;
    }

    LaneSyncU64(&ParseTasks, 0);
    LaneSyncU64(&ParseTasksCount, 0);
    LaneSync();

    u64 ParseTaskTakeCounter = 0;
    u64* ParseTaskTakeCounterPtr = NULL;

    if (!LaneIndex()) {
        ParseTaskTakeCounterPtr = &ParseTaskTakeCounter;
    }

    LaneSyncU64(&ParseTaskTakeCounterPtr, 0);

    for (;;) {
        u64 ParseTaskIndex = AtomicIncFetchU64(
            ParseTaskTakeCounterPtr, 
            MEM_ORDER_SEQ_CST
        ) - 1;

        if (ParseTaskIndex >= ParseTasksCount)
            break;

        FLIKey Key = ParseTasks[ParseTaskIndex].Key;
        Str8 FLIPath = ParseTasks[ParseTaskIndex].Path;

        Handle File = {};
        Handle FileMap = {};
        FileProperties FileProps = {};
        void* FileBase = NULL;

        File = SystemOpenFile(
            SYS_ACCESS_READ | SYS_ACCESS_SHARE_READ | SYS_ACCESS_SHARE_WRITE, 
            FLIPath
        );
        FileMap = SystemOpenFileMap(SYS_ACCESS_READ, File);
        FileProps = SystemGetFileProperties(File);
        FileBase = SystemOpenFileMapView(
            FileMap, 
            SYS_ACCESS_READ, 
            Rng(0ULL, FileProps.Size)
        );

        FLIParsed FLIParsedMaybeCompressed = *EMPTY_FLI_PARSED_VALUE;
        FLIParseStatus ParseStatus = FLIParse(
            (u8*) FileBase,
            FileProps.Size,
            &FLIParsedMaybeCompressed
        );

        UNUSED(ParseStatus);

        Arena* FLIParsedMemPool = NULL;
        FLIParsed Parsed = FLIParsedMaybeCompressed;
        u64 DecompressedSize = FLIDecompressedSizeFromParsed(
            &FLIParsedMaybeCompressed
        );

        if (DecompressedSize > FileProps.Size) {
            FLIParsedMemPool = ArenaAlloc();

            u8* DecompressedData = ArenaPushArray(
                FLIParsedMemPool, 
                u8, 
                DecompressedSize
            );

            FLIDecompressParsed(
                DecompressedData, 
                DecompressedSize, 
                &FLIParsedMaybeCompressed
            );
            ParseStatus = FLIParse(
                DecompressedData, 
                DecompressedSize, 
                &Parsed
            );

            UNUSED(ParseStatus);
        }

        u64 KeyHash = Hash(Str8Struct(&Key));
        u64 SlotIndex = KeyHash % FLI_SHARED->SlotsCount;
        FLISlot* Slot = &FLI_SHARED->Slots[SlotIndex];
        Stripe* Strip = StripeFromSlotIndex(&FLI_SHARED->Stripes, SlotIndex);

        LOCK_SCOPE_W(Strip->RWMtx) {
            FLINode* Node = NULL;

            for (FLINode* N = Slot->Head; N; N = N->Next) {
                if (N->Key == Key) {
                    Node = N;
                    break;
                }
            }

            if (Node) {
                Node->File = File;
                Node->FileMap = FileMap;
                Node->FileProps = FileProps;
                Node->FileBase = FileBase;
                Node->MemPool = FLIParsedMemPool;
                MemCpy(&Node->FLI, &Parsed, sizeof(Node->FLI));
                ++Node->CompletionCount;
                --Node->WorkingCount;

                if (Node->FLI.RawDataSize) {
                    AtomicIncFetchU64(
                        &FLI_SHARED->LoadGeneration, 
                        MEM_ORDER_SEQ_CST
                    );
                }

                AtomicIncFetchU64(&FLI_SHARED->LoadCount, MEM_ORDER_SEQ_CST);
            } else {
                if (FLIParsedMemPool)
                    ArenaRelease(FLIParsedMemPool);

                SystemCloseFileMapView(
                    FileMap, 
                    FileBase, 
                    Rng(0ULL, FileProps.Size)
                );
                SystemCloseFileMap(FileMap);
                SystemCloseFile(File);
            }
        }

        CondVarBroadcast(Strip->CondVar);
    }

    LaneSync();
    ReleaseScratch(Scratch);
}

void 
FLISignalCompletion(void)
{
    SemaphoreLock(
        FLI_SHARED->ConversionCompletionLockSemaphore, 
        U64_MAX
    );
    FLI_SHARED->ConversionCompletionSharedMemoryBase[0] = FLI_SHARED->ConversionCompletionCode;
    SemaphoreUnlock(FLI_SHARED->ConversionCompletionLockSemaphore);
    SemaphoreUnlock(FLI_SHARED->ConversionCompletionSignalSemaphore);
}

void 
FLIConversionCompletionSignalReceiverThreadEntryPoint(void* Params)
{
    ThreadSetName("[FLI_CONV_COMP_SIG_RECV]"_s8);

    for (;;) {
        if (
            SemaphoreLock(
                FLI_SHARED->ConversionCompletionSignalSemaphore, 
                U64_MAX
            )
        ) {
            u64 RetiredCode = 0;

            SemaphoreLock(
                FLI_SHARED->ConversionCompletionLockSemaphore, 
                U64_MAX
            );
            RetiredCode = FLI_SHARED->ConversionCompletionSharedMemoryBase[0];
            SemaphoreUnlock(FLI_SHARED->ConversionCompletionLockSemaphore);

            LOCK_SCOPE(FLI_SHARED->CompletionMtx) {
                FLILoadCompletion* C = ArenaPushArrayZero(
                    FLI_SHARED->CompletionMemPool, 
                    FLILoadCompletion, 
                    1
                );

                SLL_QUEUE_PUSH(
                    FLI_SHARED->HeadCompletion, 
                    FLI_SHARED->TailCompletion, 
                    C
                );
                C->Code = RetiredCode;
            }

            AsyncWake(TRUE);
        }
    }
}

OCObject 
FLISearchObjectCreate(
    Str8 KeyString, 
    b32* CancelSignal, 
    OUT b32* Retry, 
    OUT u64* Generation
) {
    Access* Acc = AccessOpen();
    TempArena Scratch= GetScratch(NULL, 0);
    OCObject Object = {};
    FLISectionKind Section = FLI_SECTION_KIND_NULL;
    Str8 Query = {};
    u64 KeyReadOffset = 0;

    KeyReadOffset += SerialPopStruct(KeyString, KeyReadOffset, &Section);
    KeyReadOffset += SerialPopStruct(KeyString, KeyReadOffset, &Query.Size);
    Query.Str = ArenaPushArrayZero(Scratch.MemPool, u8, Query.Size);
    KeyReadOffset += SerialPop(
        KeyString, 
        KeyReadOffset, 
        Query.Str, 
        Query.Size, 
        1
    );

    FLIKeyArray Keys = {};

    if (!LaneIndex())
        Keys = FLIPushAllLoadedKeys(Scratch.MemPool);

    LaneSyncU64(&Keys.V, 0);
    LaneSyncU64(&Keys.Count, 0);

    FLIParsed** FLIs = NULL;

    if (!LaneIndex())
        FLIs = ArenaPushArrayZero(Scratch.MemPool, FLIParsed*, Keys.Count);

    LaneSyncU64(&FLIs, 0);

    r1u64 Range = LaneRange(Keys.Count);

    for (u64 Index = Range.Min; Index < Range.Max; ++Index)
        FLIs[Index] = FLIParsedFromKey(Acc, Keys.V[Index], FALSE, 0);

    LaneSync();

    Arena* MemPool = ArenaAlloc();
    Arena** MemPools = NULL;
    u64 MemPoolsCount = LaneCount();

    if (!LaneIndex())
        MemPools = ArenaPushArrayZero(MemPool, Arena*, MemPoolsCount);

    LaneSyncU64(&MemPools, 0);
    MemPools[LaneIndex()] = MemPool;

    FLISearchItemChunkList* LanesItems = NULL;

    if (!LaneIndex()) {
        LanesItems = ArenaPushArrayZero(
            Scratch.MemPool, 
            FLISearchItemChunkList, 
            LaneCount()
        );
    }

    LaneSyncU64(&LanesItems, 0);

    FLISearchItemChunkList* LaneItems = &LanesItems[LaneIndex()];

    for (u64 FLIIndex = 0; FLIIndex < Keys.Count; ++FLIIndex) {
        FLIKey Key = Keys.V[FLIIndex];
        FLIParsed* FLI = FLIs[FLIIndex];
        u64 ElementCount = 0;
        void* TableBase = FLISectionRawTableFromKind(
            FLI, 
            Section, 
            &ElementCount
        );
        u64 ElementSize = FLI_SECTION_ELEMENT_SIZE_TABLE[Section];
        u64 ElementNameIndexOffset = 0;

        switch (Section) {
            default: {} break;

            case FLI_SECTION_KIND_MESSAGE_TYPES: {
                ElementNameIndexOffset = OFFSETOF(
                    FLIMessageType, 
                    NameStringIndex
                );
            } break;

            case FLI_SECTION_KIND_FIELDS: {
                ElementNameIndexOffset = OFFSETOF(FLIField, NameStringIndex);
            } break;

            case FLI_SECTION_KIND_PARAMS: {
                ElementNameIndexOffset = OFFSETOF(FLIParam, NameStringIndex);
            } break;

            case FLI_SECTION_KIND_MESSAGES: {
                ElementNameIndexOffset = OFFSETOF(FLIMessage, TextStringIndex);
            } break;
        }

        r1u64 Range = LaneRange(ElementCount);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            if (
                !(Index % 10000) && 
                !!AtomicLoadU32(CancelSignal, MEM_ORDER_SEQ_CST)
            ) {
                break;
            }

            void* Element = (u8*) TableBase + ElementSize * Index;
            u32 NameIndex = *(u32*) ((u8*) Element + ElementNameIndexOffset);
            u64 NameSize = 0;
            u8* NameBase = FLIStrFromIndex(FLI, NameIndex, &NameSize);
            Str8 Name = Str(NameBase, NameSize);

            if (!Name.Size) {
                continue;
            }

            FMRangeList Matches = FuzzyFind(MemPool, Query, Name);

            if (Matches.Count == Matches.StrPartCount) {
                FLISearchItemChunk* Chunk = LaneItems->Tail;

                if (!Chunk || Chunk->Count >= Chunk->Capacity) {
                    Chunk = ArenaPushArrayZero(
                        Scratch.MemPool, 
                        FLISearchItemChunk, 
                        1
                    );
                    Chunk->BaseIndex = LaneItems->TotalCount;
                    Chunk->Capacity = 1024;
                    Chunk->Count = 0;
                    Chunk->V = ArenaPushArray(
                        Scratch.MemPool, 
                        FLISearchItem, 
                        Chunk->Capacity
                    );
                    SLL_QUEUE_PUSH(LaneItems->Head, LaneItems->Tail, Chunk);
                    ++LaneItems->ChunkCount;
                }

                Chunk->V[Chunk->Count].Index = Index;
                Chunk->V[Chunk->Count].Key = Key;
                Chunk->V[Chunk->Count].MatchRanges = Matches;
                Chunk->V[Chunk->Count].MissedSize = (Name.Size > Matches.TotalDimensions)
                    ? (Name.Size - Matches.TotalDimensions)
                    : 0;
                ++Chunk->Count;
                ++LaneItems->TotalCount;
            }
        }
    }

    LaneSync();

    FLISearchItemChunkList* AllItems = &LanesItems[0];

    if (!LaneIndex()) {
        for (u64 Index = 1; Index < LaneCount(); ++Index) {
            FLISearchItemChunkList* Dst = AllItems;
            FLISearchItemChunkList* Items = &LanesItems[Index];

            for (FLISearchItemChunk* N = Items->Head; N; N = N->Next)
                N->BaseIndex += Dst->TotalCount;

            if (Dst->Head && Items->Head) {
                Dst->Tail->Next = Items->Head;
                Dst->Tail = Items->Tail;
                Dst->ChunkCount += Items->ChunkCount;
                Dst->TotalCount += Items->TotalCount;
            } else if (!Dst->Head) {
                MemCpy(Dst, Items, sizeof(*Dst));
            }

            MemSet(Items, 0, sizeof(*Items));
        }
    }

    LaneSync();

    b32 Cancelled = FALSE;

    if (!LaneIndex() && !!AtomicLoadU32(CancelSignal, MEM_ORDER_SEQ_CST))
        Cancelled = TRUE;

    LaneSyncU64(&Cancelled, 0);

    struct SortRecord {
        u64            Key;
        FLISearchItem* Item;
    };

    u64 SortRecordsCount = AllItems->TotalCount;
    SortRecord* SortRecords = NULL;
    SortRecord* __SortRecords = NULL;

    if (!Cancelled) {
        if (!LaneIndex())
            SortRecords = ArenaPushArray(Scratch.MemPool, SortRecord, SortRecordsCount);

        if (LaneIndex() == LaneFromTaskIndex(1))
            __SortRecords = ArenaPushArray(Scratch.MemPool, SortRecord, SortRecordsCount);

        LaneSyncU64(&SortRecords, 0);
        LaneSyncU64(&__SortRecords, LaneFromTaskIndex(1));

        for (FLISearchItemChunk* N = AllItems->Head; N; N = N->Next) {
            r1u64 Range = LaneRange(N->Count);
            u64 DstIndex = N->BaseIndex + Range.Min;

            for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
                FLISearchItem* Item = &N->V[Index];

                SortRecords[DstIndex].Item = Item;
                SortRecords[DstIndex].Key = (
                    ((Item->MissedSize & U32_MAX) << 32) | 
                    (Hash(Item->Index, Str8Struct(&KeyString)) & U32_MAX)
                );
            }
        }
    }

    LaneSync();

    if (!Cancelled) {
        u64 BitsPerDigit = 8;
        u64 DigitsCount = 64 / BitsPerDigit;
        u64 NumPossibleValuesPerDigit = 1 << BitsPerDigit;
        u32** LanesDigitCounts = NULL;
        u32** LanesDigitOffsets = NULL;

        if (!LaneIndex()) {
            LanesDigitCounts = ArenaPushArrayZero(
                Scratch.MemPool, 
                u32*, 
                LaneCount()
            );
            LanesDigitOffsets = ArenaPushArrayZero(
                Scratch.MemPool, 
                u32*, 
                LaneCount()
            );
        }

        LaneSyncU64(&LanesDigitCounts, 0);
        LaneSyncU64(&LanesDigitOffsets, 0);
        LanesDigitCounts[LaneIndex()] = ArenaPushArrayZero(
            Scratch.MemPool, 
            u32, 
            NumPossibleValuesPerDigit
        );
        LanesDigitOffsets[LaneIndex()] = ArenaPushArrayZero(
            Scratch.MemPool, 
            u32, 
            NumPossibleValuesPerDigit
        );

        SortRecord* Src = SortRecords;
        SortRecord* Dst = __SortRecords;
        u64 Count = SortRecordsCount;

        for (u64 DigitIndex = 0; DigitIndex < DigitsCount; ++DigitIndex) {
            u32* DigitCounts = LanesDigitCounts[LaneIndex()];

            MemSet(
                DigitCounts, 
                0, 
                NumPossibleValuesPerDigit * sizeof(*DigitCounts)
            );

            r1u64 Range = LaneRange(Count);

            for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
                SortRecord* R = &Src[Index];
                u16 DigitValue = (u16) (u8) (R->Key >> (DigitIndex * BitsPerDigit));

                ++DigitCounts[DigitValue];
            }

            LaneSync();
            Range = LaneRange(NumPossibleValuesPerDigit);

            for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
                u64 LayoutOffset = 0;

                for (u64 LIndex = 0; LIndex < LaneCount(); ++LIndex) {
                    LanesDigitOffsets[LIndex][Index] = LayoutOffset;
                    LayoutOffset += LanesDigitCounts[LIndex][Index];
                }
            }

            LaneSync();

            if (!LaneIndex()) {
                u64 LastOffset = 0;

                for (
                    u64 ValueIndex = 0; 
                    ValueIndex < NumPossibleValuesPerDigit; 
                    ++ValueIndex
                ) {
                    for (u64 LIndex = 0; LIndex < LaneCount(); ++LIndex) {
                        LanesDigitOffsets[LIndex][ValueIndex] += LastOffset;
                    }

                    LastOffset = LanesDigitOffsets[LaneCount() - 1][ValueIndex] + LanesDigitCounts[LaneCount() - 1][ValueIndex];
                }
            }

            LaneSync();

            u32* LaneDigitOffsets = LanesDigitOffsets[LaneIndex()];

            Range = LaneRange(Count);

            for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
                SortRecord* SrcR = &Src[Index];
                u16 DigitValue = (u16) (u8) (SrcR->Key >> (DigitIndex * BitsPerDigit));
                u64 DstOffset = LaneDigitOffsets[DigitValue];

                ++LaneDigitOffsets[DigitValue];
                MemCpy(&Dst[DstOffset], SrcR, sizeof(Dst[DstOffset]));
            }

            LaneSync();
            SWAP(SortRecord*, Src, Dst);
        }
    }

    LaneSync();

    FLISearchItemArray Items = {};

    if (!Cancelled) {
        if (!LaneIndex()) {
            Items.Count = AllItems->TotalCount;
            Items.V = ArenaPushArray(MemPool, FLISearchItem, Items.Count);
        }

        LaneSyncU64(&Items.Count, 0);
        LaneSyncU64(&Items.V, 0);

        r1u64 Range = LaneRange(SortRecordsCount);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            SortRecord* Record = &SortRecords[Index];
            FLISearchItem* DstItem = &Items.V[Index];

            MemCpy(DstItem, Record->Item, sizeof(*DstItem));
        }
    }

    LaneSync();

    if (!Cancelled) {
        Object.V[0] = INT_FROM_PTR(MemPools);
        Object.V[1] = MemPoolsCount;
        Object.V[2] = INT_FROM_PTR(Items.V);
        Object.V[3] = Items.Count;
    } else {
        ArenaRelease(MemPool);
    }

    ReleaseScratch(Scratch);
    AccessClose(Acc);

    return Object;
}

void 
FLISearchObjectDestroy(OCObject Object)
{
    TempArena Scratch = GetScratch(NULL, 0);
    Arena** MemPools = (Arena**) Object.V[0];
    u64 MemPoolsCount = Object.V[1];
    Arena** MemPoolsCopy = ArenaPushArrayZero(
        Scratch.MemPool, 
        Arena*, 
        MemPoolsCount
    );

    MemCpy(MemPoolsCopy, MemPools, sizeof(Arena*) * MemPoolsCount);

    for (u64 Index = 0; Index < MemPoolsCount; ++Index)
        if (MemPoolsCopy[Index])
            ArenaRelease(MemPoolsCopy[Index]);

    ReleaseScratch(Scratch);
}

FLISearchItemArray 
FLISearchItemArrayFromTargetQuery(
    Access* Acc, 
    FLISectionKind Section, 
    Str8 Query, 
    u64 EndTimeUSecs, 
    OUT b32* Stale
) {
    FLISearchItemArray Result = {};
    TempArena Scratch = GetScratch(NULL, 0);
    Str8List KeyParts = {};

    ListPush(Scratch.MemPool, &KeyParts, Str8Struct(&Section));
    ListPush(Scratch.MemPool, &KeyParts, Str8Struct(&Query.Size));
    ListPush(Scratch.MemPool, &KeyParts, Query);

    Str8 Key = StrListJoin(Scratch.MemPool, &KeyParts, NULL);
    OCObject Object = OCObjectFromKey(
        Acc, 
        Key, 
        FLISearchObjectCreate, 
        FLISearchObjectDestroy,
        EndTimeUSecs,
        __Params.Generation = FLILoadGeneration(),
        __Params.Flags = OC_WIDE,
        __Params.EvictionThresholdUSecs = MILLISECONDS(100),
        __Params.Stale = Stale
    );

    Result.V = (FLISearchItem*) Object.V[2];
    Result.Count = Object.V[3];
    ReleaseScratch(Scratch);

    return Result;
}

OCObject 
FLIMatchObjectCreate(
    Str8 Key, 
    b32* CancelSignal, 
    OUT b32* Retry, 
    OUT u64* Generation
) {
    TempArena Scratch = GetScratch(NULL, 0);
    u64 Index = 0;
    Str8 Name = {};
    FLIKey PreferredFLIKey = {};
    u64 KeyReadOffset = 0;

    KeyReadOffset += SerialPopStruct(Key, KeyReadOffset, &Index);
    KeyReadOffset += SerialPopStruct(Key, KeyReadOffset, &PreferredFLIKey);
    KeyReadOffset += SerialPopStruct(Key, KeyReadOffset, &Name.Size);
    Name.Str = ArenaPushArray(Scratch.MemPool, u8, Name.Size);
    KeyReadOffset += SerialPop(Key, KeyReadOffset, Name.Str, Name.Size, 1);

    FLIKeyArray FLIKeys = FLIPushAllLoadedKeys(Scratch.MemPool);
    b32 Cancelled = FALSE;

    if (!LaneIndex())
        Cancelled = AtomicLoadU32(CancelSignal, MEM_ORDER_SEQ_CST);

    LaneSyncU64(&Cancelled, 0);

    FLIMatch* LaneMatches = NULL;

    if (!Cancelled) {
        if (!LaneIndex()) {
            LaneMatches = ArenaPushArrayZero(
                Scratch.MemPool, 
                FLIMatch, 
                LaneCount()
            );
        }

        LaneSyncU64(&LaneMatches, 0);

        readonly local FLINameMapKind NameMapKinds[] = {
            FLI_NAME_MAP_KIND_MESSAGE_TYPES,
            FLI_NAME_MAP_KIND_FIELDS,
            FLI_NAME_MAP_KIND_PARAMS,
            FLI_NAME_MAP_KIND_STRING_DATA,
            FLI_NAME_MAP_KIND_STRING_TABLE
        };
        readonly local FLISectionKind NameMapSectionKinds[] = {
            FLI_SECTION_KIND_MESSAGE_TYPES,
            FLI_SECTION_KIND_FIELDS,
            FLI_SECTION_KIND_PARAMS,
            FLI_SECTION_KIND_STRING_DATA,
            FLI_SECTION_KIND_STRING_TABLE
        };
        r1u64 Range = LaneRange(FLIKeys.Count);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            Access* Acc = AccessOpen();
            FLIKey FLIIKey = FLIKeys.V[Index];
            FLIParsed* FLI = FLIParsedFromKey(Acc, FLIIKey, FALSE, 0);

            for (
                u64 MapKindIndex = 0; 
                MapKindIndex < ARRAY_COUNT(NameMapKinds); 
                ++MapKindIndex
            ) {
                FLINameMap* NameMap = FLIElementFromNameIndex(
                    FLI, 
                    NameMaps,
                    NAME_MAPS,
                    NameMapKinds[MapKindIndex]
                );
                FLIParsedNameMap ParsedNameMap = {};

                FLIParsedFromNameMap(FLI, NameMap, &ParsedNameMap);

                FLINameMapNode* MapNode = FLINameMapLookup(
                    FLI,
                    &ParsedNameMap,
                    Name.Str,
                    Name.Size
                );
                u32 Num = 0;
                u32* Run = FLIMatchesFromMapNode(FLI, MapNode, &Num);

                if (Num) {
                    LaneMatches[LaneIndex()].Key = FLIIKey;
                    LaneMatches[LaneIndex()].Kind = NameMapSectionKinds[MapKindIndex];
                    LaneMatches[LaneIndex()].Index = Run[Num - 1];
                }
            }

            AccessClose(Acc);
        }
    }

    LaneSync();

    FLIMatch Match = {};

    if (LaneMatches) {
        for (u64 Index = 0; Index < LaneCount(); ++Index) {
            if (LaneMatches[Index].Index) {
                Match = LaneMatches[Index];

                if (
                    PreferredFLIKey == FLIKey{} || 
                    Match.Key == PreferredFLIKey
                ) {
                    break;
                }
            }
        }
    }

    OCObject Result = {};

    STATIC_ASSERT(ARRAY_COUNT(Result.V) >= 4, ObjectSizeCheck);

    Result.V[0] = Match.Key.U64[0];
    Result.V[1] = Match.Key.U64[1];
    Result.V[2] = Match.Kind;
    Result.V[3] = Match.Index;
    LaneSync();
    ReleaseScratch(Scratch);

    return Result;
}

FLIMatch 
FLIMatchFromStr(
    Str8 String, 
    u64 Index, 
    FLIKey PreferredFLIKey, 
    u64 EndTimeUSecs
) {
    FLIMatch Result = {};
    Access* Acc = AccessOpen();
    TempArena Scratch = GetScratch(NULL, 0);
    Str8List KeyParts = {};

    ListPush(Scratch.MemPool, &KeyParts, Str8Struct(&Index));
    ListPush(Scratch.MemPool, &KeyParts, Str8Struct(&PreferredFLIKey));
    ListPush(Scratch.MemPool, &KeyParts, Str8Struct(&String.Size));
    ListPush(Scratch.MemPool, &KeyParts, String);

    Str8 Key = StrListJoin(Scratch.MemPool, &KeyParts, NULL);
    u64 FLICount = FLILoadCount();
    b32 IsWide = (FLICount > 256);
    OCObject Object = OCObjectFromKey(
        Acc, 
        Key, 
        FLIMatchObjectCreate, 
        0, 
        EndTimeUSecs,
        __Params.Flags = IsWide 
            ? OC_WIDE 
            : 0,
        __Params.Generation = FLILoadGeneration(),
        __Params.EvictionThresholdUSecs = IsWide 
            ? SECONDS(20) 
            : SECONDS(10)
    );

    Result.Key.U64[0] = Object.V[0];
    Result.Key.U64[1] = Object.V[1];
    Result.Kind = (FLISectionKind) Object.V[2];
    Result.Index = Object.V[3];
    ReleaseScratch(Scratch);
    AccessClose(Acc);

    return Result;
}

u64 
Hash(u8* Ptr, u64 Size)
{
    u64 Result = 0;

    if (Size) {
        Result = 5381;

        u8* EndPtr = Ptr + Size;

        for ( ; Ptr < EndPtr; ++Ptr)
            Result = ((Result << 5) + Result) + *Ptr;
    }

    return Result;
}

FLIParseStatus 
FLIParse(u8* Data, u64 Size, OUT FLIParsed* Parsed)
{
    FLIParseStatus Result = FLI_PARSE_STATUS_SUCCESS;
    FLIHeader* Header = NULL;

    if (Result == FLI_PARSE_STATUS_SUCCESS) {
        if (sizeof(*Header) <= Size)
            Header = (FLIHeader*) Data;

        if (!Header || Header->Magic != FLI_MAGIC_NUMBER) {
            Header = NULL;
            Result = FLI_PARSE_STATUS_HEADER_MISMATCH;
        }

        if (Header && Header->EncodingVersion != FLI_ENCODING_VERSION) {
            Header = NULL;
            Result = FLI_PARSE_STATUS_VERSION_MISMATCH;
        }
    }

    FLISection* DataSections = NULL;
    u32 DataSectionsCount = 0;

    if (Result == FLI_PARSE_STATUS_SUCCESS) {
        u64 End = (u64) Header->DataSectionOffset + (u64) Header->DataSectionCount * sizeof(*DataSections);

        if (End <= Size) {
            DataSections = (FLISection*) (Data + Header->DataSectionOffset);
            DataSectionsCount = Header->DataSectionCount;
        }

        if (!DataSections)
            Result = FLI_PARSE_STATUS_DATA_SECTION_LAYOUT_MISMATCH;
    }

    if (Result == FLI_PARSE_STATUS_SUCCESS) {
        Parsed->RawData = Data;
        Parsed->RawDataSize = Size;
        Parsed->Sections = DataSections;
        Parsed->SectionsCount = DataSectionsCount;
    }

    return Result;
}

u64 
FLIDecompressedSizeFromParsed(FLIParsed* Parsed)
{
    u64 DecompressedSize = Parsed->RawDataSize;

    for (
        u64 SectionIndex = 0; 
        SectionIndex < Parsed->SectionsCount; 
        ++SectionIndex
    ) {
        DecompressedSize += (
            Parsed->Sections[SectionIndex].UnpackedSize - 
            Parsed->Sections[SectionIndex].EncodedSize
        );
    }

    return DecompressedSize;
}

void 
FLIDecompressParsed(
    u8* DecompressedData, 
    u64 DecompressedSize, 
    FLIParsed* Parsed
) {
    FLIHeader* SrcHeader = (FLIHeader*) Parsed->RawData;
    FLIHeader* DstHeader = (FLIHeader*) DecompressedData;

    MemCpy(DstHeader, SrcHeader, sizeof(*DstHeader));

    if (Parsed->SectionsCount) {
        FLISection* DataSectionBase = (FLISection*) (
            DecompressedData + DstHeader->DataSectionOffset
        );

        MemCpy(
            DataSectionBase, 
            (u8*) Parsed->RawData + SrcHeader->DataSectionOffset, 
            sizeof(FLISection) * Parsed->SectionsCount
        );

        u64 Offset = DstHeader->DataSectionOffset + sizeof(FLISection) * Parsed->SectionsCount;

        Offset += 7;
        Offset -= Offset % 8;

        for (u64 Index = 0; Index < Parsed->SectionsCount; ++Index) {
            DataSectionBase[Index].Encoding = FLI_SECTION_ENCODING_UNPACKED;
            DataSectionBase[Index].Offset = Offset;
            DataSectionBase[Index].EncodedSize = DataSectionBase[Index].UnpackedSize;
            Offset += DataSectionBase[Index].UnpackedSize;
            Offset += 7;
            Offset -= Offset % 8;
        }
    }

    if (Parsed->SectionsCount) {
        FLISection* SrcStart = Parsed->Sections;
        FLISection* DstStart = (FLISection*) (
            DecompressedData + 
            DstHeader->DataSectionOffset
        );
        FLISection* SrcEnd = SrcStart + Parsed->SectionsCount;
        FLISection* DstEnd = DstStart + Parsed->SectionsCount;

        for (
            FLISection* Src = SrcStart, *Dst = DstStart;
            Src < SrcEnd && Dst < DstEnd;
            ++Src, ++Dst
        ) {
            if (Src->Encoding == FLI_SECTION_ENCODING_LZB) {
                rr_lzb_simple_decode(
                    (u8*) Parsed->RawData + Src->Offset,
                    Src->EncodedSize,
                    DecompressedData + Dst->Offset,
                    Dst->UnpackedSize
                );
            } else {
                MemCpy(
                    DecompressedData + Dst->Offset,
                    (u8*) Parsed->RawData + Src->Offset,
                    Src->EncodedSize
                );
            }
        }
    }
}

INTERNAL u64
FLISectionElementSizeFromKind(FLIParsed* FLI, FLISectionKind Kind)
{
    u64 Result = sizeof(u8);

    if (Kind < FLI_SECTION_KIND_COUNT) {
        Result = FLI_SECTION_ELEMENT_SIZE_TABLE[Kind];
    } else {
        u64 ColumnsCount = 0;
        FLIColumn* Columns = FLITableFromName(
            FLI, 
            Columns, 
            COLUMNS, 
            &ColumnsCount
        );
        u64 ColumnIndex = (u64) Kind - FLI_SECTION_KIND_COUNT;

        if (ColumnIndex < ColumnsCount)
            Result = FLI_SCALAR_KIND_SIZE_TABLE[Columns[ColumnIndex].Kind];
    }

    return Result;
}

void* 
FLISectionRawElementFromKindIndex(
    FLIParsed* FLI, 
    FLISectionKind Kind, 
    u64 Index
) {
    u64 Count = 0;
    void* Table = FLISectionRawTableFromKind(FLI, Kind, &Count);
    void* Result = Table;

    if (Index < Count) {
        u64 ElementSize = FLISectionElementSizeFromKind(FLI, Kind);

        Result = (u8*) Table + ElementSize * Index;
    }

    return Result;
}

void*
FLISectionRawDataFromKind(
    FLIParsed* FLI,
    FLISectionKind Kind,
    OUT FLISectionEncoding* Encoding,
    OUT u64* Size
) {
    void* Result = NULL;

#if !defined(FLI_DISABLE_NULLS)
    Result = EMPTY_FLI_ELEMENT_UNION_VALUE;
    *Size = FLISectionElementSizeFromKind(FLI, Kind);
#endif

    if (
        Kind > 0 && 
        Kind < FLI->SectionsCount &&
        FLI->Sections[Kind].Offset < FLI->RawDataSize
    ) {
        Result = FLI->RawData + FLI->Sections[Kind].Offset;
        *Size = FLI->Sections[Kind].EncodedSize;
        *Encoding = FLI->Sections[Kind].Encoding;
    }

    return Result;
}

void* 
FLISectionRawTableFromKind(
    FLIParsed* FLI, 
    FLISectionKind Kind, 
    OUT u64* ElementCount
) {
    void* Result = NULL;
    u64 AllElementsSize = 0;
    FLISectionEncoding AllElementsEncoding = 0;
    void* AllElements = FLISectionRawDataFromKind(
        FLI,
        Kind,
        &AllElementsEncoding,
        &AllElementsSize
    );

    if (AllElementsEncoding == FLI_SECTION_ENCODING_UNPACKED) {
        u64 ElementSize = FLISectionElementSizeFromKind(FLI, Kind);
        u64 AllElementsCount = AllElementsSize / ElementSize;

        Result = AllElements;
        *ElementCount = AllElementsCount;
    }

    return Result;
}

void*
FLIColumnRawDataFromIndex(
    FLIParsed* FLI, 
    u32 ColumnIndex, 
    OUT u64* ElementCount
) {
    FLIColumn* Column = FLIElementFromNameIndex(
        FLI, 
        Columns, 
        COLUMNS, 
        ColumnIndex
    );
    void* Result = FLISectionRawTableFromKind(
        FLI,
        (FLISectionKind) Column->SectionIndex,
        ElementCount
    );

    return Result;
}

u32
FLIColumnIndexFromNames(FLIParsed* FLI, Str8 MessageTypeName, Str8 FieldName)
{
    u32 Result = 0;
    u32 MessageTypeIndex = 0;

    {
        FLINameMap* NameMap = FLIElementFromNameIndex(
            FLI, 
            NameMaps, 
            NAME_MAPS, 
            FLI_NAME_MAP_KIND_MESSAGE_TYPES
        );
        FLIParsedNameMap ParsedNameMap = {};

        FLIParsedFromNameMap(FLI, NameMap, &ParsedNameMap);

        FLINameMapNode* MapNode = FLINameMapLookup(
            FLI, 
            &ParsedNameMap, 
            MessageTypeName.Str, 
            MessageTypeName.Size
        );
        u32 Num = 0;
        u32* Run = FLIMatchesFromMapNode(FLI, MapNode, &Num);

        if (Num)
            MessageTypeIndex = Run[0];
    }

    if (MessageTypeIndex) {
        FLIMessageType* MessageType = FLIElementFromNameIndex(
            FLI, 
            MessageTypes, 
            MESSAGE_TYPES, 
            MessageTypeIndex
        );
        FLINameMap* NameMap = FLIElementFromNameIndex(
            FLI, 
            NameMaps, 
            NAME_MAPS, 
            FLI_NAME_MAP_KIND_FIELDS
        );
        FLIParsedNameMap ParsedNameMap = {};

        FLIParsedFromNameMap(FLI, NameMap, &ParsedNameMap);

        FLINameMapNode* MapNode = FLINameMapLookup(
            FLI, 
            &ParsedNameMap, 
            FieldName.Str, 
            FieldName.Size
        );
        u32 Num = 0;
        u32* Run = FLIMatchesFromMapNode(FLI, MapNode, &Num);
        u32 FieldFirst = MessageType->HeadField;
        u32 FieldEnd = FieldFirst + MessageType->FieldCount;

        for (u32 Index = 0; Index < Num; ++Index) {
            if (FieldFirst <= Run[Index] && Run[Index] < FieldEnd) {
                FLIField* Field = FLIElementFromNameIndex(
                    FLI, 
                    Fields, 
                    FIELDS, 
                    Run[Index]
                );

                Result = Field->ColumnIndex;
                break;
            }
        }
    }

    return Result;
}

void 
FLIParsedFromNameMap(
    FLIParsed* FLI, 
    FLINameMap* MapPtr, 
    OUT FLIParsedNameMap* ParsedMap
) {
    ParsedMap->Buckets = NULL;
    ParsedMap->BucketCount = 0;

    if (MapPtr) {
        u64 AllBucketsCount = 0;
        FLINameMapBucket* AllBuckets = FLITableFromName(
            FLI,
            NameMapBuckets,
            NAME_MAP_BUCKETS,
            &AllBucketsCount
        );
        u64 AllNodesCount = 0;
        FLINameMapNode* AllNodes = FLITableFromName(
            FLI,
            NameMapNodes,
            NAME_MAP_NODES,
            &AllNodesCount
        );

        ParsedMap->Buckets = AllBuckets + MapPtr->BucketBaseIndex;
        ParsedMap->Nodes = AllNodes + MapPtr->NodeBaseIndex;
        ParsedMap->BucketCount = MapPtr->BucketCount;
        ParsedMap->NodeCount = MapPtr->NodeCount;

        if (MapPtr->BucketBaseIndex > AllBucketsCount) {
            ParsedMap->Buckets = NULL;
            ParsedMap->BucketCount = 0;
        }

        if (MapPtr->NodeBaseIndex > AllNodesCount) {
            ParsedMap->Nodes = NULL;
            ParsedMap->NodeCount = 0;
        }
    }
}
 
u8*
FLIStrFromIndex(
    FLIParsed* FLI,
    u32 Index,
    OUT u64* StringLength
) {
    u8* ResultBase = NULL;
    u64 ResultSize = 0;
    u64 StringOffsetsCount = 0;
    u32* StringOffsets = (u32*) FLITableFromName(
        FLI,
        StringTable,
        STRING_TABLE,
        &StringOffsetsCount
    );

    if (Index < StringOffsetsCount) {
        u64 StringDataSize = 0;
        u8* StringData = FLITableFromName(
            FLI,
            StringData,
            STRING_DATA,
            &StringDataSize
        );
        u32 OffsetRaw = StringOffsets[Index];
        u32 EndRaw = StringOffsets[Index + 1];
        u32 End = MIN(EndRaw, StringDataSize);
        u32 Offset = MIN(OffsetRaw, End);

        ResultBase = StringData + Offset;
        ResultSize = End - Offset;
    }

    *StringLength = ResultSize;

    return ResultBase;
}

FLINameMapNode* 
FLINameMapLookup(
    FLIParsed* FLI, 
    FLIParsedNameMap* Map, 
    u8* NameString, 
    u64 NameLength
) {
    FLINameMapNode* Result = NULL;

    if (Map->BucketCount) {
        u64 BucketCount = Map->BucketCount;
        u64 NameHash = Hash(NameString, NameLength);
        u64 BucketIndex = NameHash % BucketCount;
        FLINameMapBucket* Bucket = Map->Buckets + BucketIndex;
        FLINameMapNode* Node = Map->Nodes + Bucket->FirstNode;
        FLINameMapNode* NodeEnd = Node + Bucket->NodeCount;

        for ( ; Node < NodeEnd; ++Node) {
            u64 NLength = 0;
            u8* NString = FLIStrFromIndex(FLI, Node->StringIndex, &NLength);
            i32 Match = 0;

            if (NLength == NameLength) {
                u8* A = NameString;
                u8* AEnd = NameString + NameLength;
                u8* B = NString;

                for ( ; A < AEnd && *A == *B; ++A, ++B);

                Match = (A == AEnd);
            }

            if (Match) {
                Result = Node;
                break;
            }
        }
    }

    return Result;
}

INTERNAL u32*
FLIIndexRunFromFirstCount(
    FLIParsed* FLI,
    u32 RawFirst,
    u32 RawCount,
    OUT u32* Count
) {
    u64 IndexRunCount = 0;
    u32* IndexRunData = FLITableFromName(
        FLI,
        IndexRuns,
        INDEX_RUNS,
        &IndexRunCount
    );
    u32 RawEnd = RawFirst + RawCount;
    u32 End = MIN(RawEnd, IndexRunCount);
    u32 First = MIN(RawFirst, End);
    u32* Result = NULL;

    if (First < IndexRunCount)
        Result = IndexRunData + First;

    *Count = End - First;

    return Result;
}

u32* 
FLIMatchesFromMapNode(FLIParsed* FLI, FLINameMapNode* Node, OUT u32* Count)
{
    u32* Result = NULL;

    *Count = 0;

    if (Node) {
        if (Node->MatchCount == 1) {
            Result = &Node->MatchIndexOrIndexRunFirst;
            *Count = 1;
        } else {
            Result = FLIIndexRunFromFirstCount(
                FLI,
                Node->MatchIndexOrIndexRunFirst,
                Node->MatchCount,
                Count
            );
        }
    }

    return Result;
}

INTERNAL b32
SortKeyIsBefore(void* _KeyA, void* _KeyB)
{
    return (((FLISortKey*) _KeyA)->Key < ((FLISortKey*) _KeyB)->Key);
}

FLISortKey* 
FLISortKeyArray(Arena* MemPool, FLISortKey* Keys, u64 Count)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    FLISortKey* Result = NULL;

    if (Count <= 1) {
        Result = Keys;
    } else {
        FLIOrderedRange* HeadRanges = NULL;
        FLIOrderedRange* TailRanges = NULL;
        u64 RangeCount = 0;
        u64 Position = 0;

        for ( ; Position < Count; ) {
            u64 Start = Position;
            u64 End = Position + 1;

            for (
                ; 
                End < Count && Keys[End - 1].Key <= Keys[End].Key; 
                ++End
            ) {
                FLIOrderedRange* NewRange = ArenaPushArrayZero(
                    Scratch.MemPool,
                    FLIOrderedRange,
                    1
                );

                SLL_QUEUE_PUSH(HeadRanges, TailRanges, NewRange);
                ++RangeCount;
                NewRange->Start = Start;
                NewRange->End = End;
                Position = End;
            }
        }

        if (RangeCount == 1) {
            Result = Keys;
        } else {
            FLISortKey* KeysSwap = ArenaPushArray(
                Scratch.MemPool, 
                FLISortKey, 
                Count
            );
            FLISortKey* Src = Keys;
            FLISortKey* Dst = KeysSwap;
            FLIOrderedRange* SrcRanges = HeadRanges;
            FLIOrderedRange* DstRanges = NULL;
            FLIOrderedRange* TailDstRanges = NULL;

            for (;;) {
                for (;;) {
                    if (!SrcRanges)
                        break;

                    FLIOrderedRange* Range1 = SrcRanges;

                    SLL_STACK_POP(SrcRanges);

                    if (!Range1->Start && Range1->End == Count) {
                        Result = Src;
                        goto SORT_DONE;
                    }

                    if (!SrcRanges) {
                        u64 Start = Range1->Start;

                        MemCpy(
                            Dst + Start, 
                            Src + Start, 
                            sizeof(*Src) * (Range1->End - Start)
                        );
                        SLL_QUEUE_PUSH(DstRanges, TailDstRanges, Range1);
                        break;
                    }

                    FLIOrderedRange* Range2 = SrcRanges;

                    SLL_STACK_POP(SrcRanges);

                    u64 JD = Range1->Start;
                    u64 J1 = Range1->Start;
                    u64 J1End =- Range1->End;
                    u64 J2 = Range2->Start;
                    u64 J2End = Range2->End;

                    for (;;) {
                        if (Src[J1].Key <= Src[J2].Key) {
                            MemCpy(Dst + JD, Src + J1, sizeof(*Src));
                            ++J1;
                            ++JD;

                            if (J1 >= J1End)
                                break;
                        } else {
                            MemCpy(Dst + JD, Src + J2, sizeof(*Src));
                            ++J2;
                            ++JD;

                            if (J2 >= J2End)
                                break;
                        }
                    }

                    if (J1 < J1End)
                        MemCpy(Dst + JD, Src + J1, sizeof(*Src) * (J1End - J1));
                    else
                        MemCpy(Dst + JD, Src + J2, sizeof(*Src) * (J2End - J2));

                    Range1->End = Range2->End;
                    SLL_QUEUE_PUSH(DstRanges, TailDstRanges, Range1);
                }

                SWAP(FLISortKey*, Src, Dst);
                SrcRanges = DstRanges;
                DstRanges = NULL;
                TailDstRanges = NULL;
            }
        }
    }

SORT_DONE:

    ReleaseScratch(Scratch);

    return Result;
}

FLIBakeMessageType* 
FLIBakeMessageTypeChunkListPush(
    Arena* MemPool, 
    FLIBakeMessageTypeChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeMessageTypeChunkNode, 
        FLIBakeMessageType, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeMessageType(FLIBakeMessageType* MessageType)
{
    FLIBakeIndexedChunkListElementGetIndex(
        MessageType, 
        Index
    );

    return Index;
}

void 
FLIBakeMessageTypeChunkListCat(
    FLIBakeMessageTypeChunkList* Dst, 
    FLIBakeMessageTypeChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeMessageTypeChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeField* 
FLIBakeFieldChunkListPush(
    Arena* MemPool, 
    FLIBakeFieldChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeFieldChunkNode, 
        FLIBakeField, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeField(FLIBakeField* Field)
{
    FLIBakeIndexedChunkListElementGetIndex(Field, Index);

    return Index;
}

void 
FLIBakeFieldChunkListCat(
    FLIBakeFieldChunkList* Dst, 
    FLIBakeFieldChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeFieldChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeColumn* 
FLIBakeColumnChunkListPush(
    Arena* MemPool, 
    FLIBakeColumnChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeColumnChunkNode, 
        FLIBakeColumn, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeColumn(FLIBakeColumn* Column)
{
    FLIBakeIndexedChunkListElementGetIndex(Column, Index);

    return Index;
}

void 
FLIBakeColumnChunkListCat(
    FLIBakeColumnChunkList* Dst, 
    FLIBakeColumnChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeColumnChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeParam* 
FLIBakeParamChunkListPush(
    Arena* MemPool, 
    FLIBakeParamChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeParamChunkNode, 
        FLIBakeParam, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeParam(FLIBakeParam* Param)
{
    FLIBakeIndexedChunkListElementGetIndex(Param, Index);

    return Index;
}

void 
FLIBakeParamChunkListCat(
    FLIBakeParamChunkList* Dst, 
    FLIBakeParamChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeParamChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeMessage* 
FLIBakeMessageChunkListPush(
    Arena* MemPool, 
    FLIBakeMessageChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeMessageChunkNode, 
        FLIBakeMessage, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeMessage(FLIBakeMessage* Message)
{
    FLIBakeIndexedChunkListElementGetIndex(Message, Index);

    return Index;
}

void 
FLIBakeMessageChunkListCat(
    FLIBakeMessageChunkList* Dst, 
    FLIBakeMessageChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeMessageChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeMode* 
FLIBakeModeChunkListPush(
    Arena* MemPool, 
    FLIBakeModeChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeModeChunkNode, 
        FLIBakeMode, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeMode(FLIBakeMode* Mode)
{
    FLIBakeIndexedChunkListElementGetIndex(Mode, Index);

    return Index;
}

void 
FLIBakeModeChunkListCat(
    FLIBakeModeChunkList* Dst, 
    FLIBakeModeChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeModeChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeEvent* 
FLIBakeEventChunkListPush(
    Arena* MemPool, 
    FLIBakeEventChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeEventChunkNode, 
        FLIBakeEvent, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeEvent(FLIBakeEvent* Event)
{
    FLIBakeIndexedChunkListElementGetIndex(Event, Index);

    return Index;
}

void 
FLIBakeEventChunkListCat(
    FLIBakeEventChunkList* Dst, 
    FLIBakeEventChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeEventChunkNode, 
        Dst, 
        ToPush
    );
}

FLIBakeError* 
FLIBakeErrorChunkListPush(
    Arena* MemPool, 
    FLIBakeErrorChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexedChunkListPush(
        MemPool, 
        List, 
        FLIBakeErrorChunkNode, 
        FLIBakeError, 
        Capacity, 
        Result
    );

    return Result;
}

u64 
FLIIndexFromBakeError(FLIBakeError* Error)
{
    FLIBakeIndexedChunkListElementGetIndex(Error, Index);

    return Index;
}

void 
FLIBakeErrorChunkListCat(
    FLIBakeErrorChunkList* Dst, 
    FLIBakeErrorChunkList* ToPush
) {
    FLIBakeIndexedChunkListCat(
        FLIBakeErrorChunkNode, 
        Dst, 
        ToPush
    );
}

void 
FLIBakeParamsCat(FLIBakeParams* Dst, FLIBakeParams* Src)
{
    FLIBakeMessageTypeChunkListCat(&Dst->MessageTypes, &Src->MessageTypes);
    FLIBakeFieldChunkListCat(&Dst->Fields, &Src->Fields);
    FLIBakeColumnChunkListCat(&Dst->Columns, &Src->Columns);
    FLIBakeParamChunkListCat(&Dst->Params, &Src->Params);
    FLIBakeMessageChunkListCat(&Dst->Messages, &Src->Messages);
    FLIBakeModeChunkListCat(&Dst->Modes, &Src->Modes);
    FLIBakeEventChunkListCat(&Dst->Events, &Src->Events);
    FLIBakeErrorChunkListCat(&Dst->Errors, &Src->Errors);
}

FLIBakeStr* 
FLIBakeStrChunkListPush(
    Arena* MemPool, 
    FLIBakeStrChunkList* List, 
    u64 Capacity
) {
    FLIBakeStrChunkNode* N = List->Tail;

    if (!N || N->Count >= N->Capacity) {
        N = ArenaPushArrayZero(MemPool, FLIBakeStrChunkNode, 1);
        N->Capacity = Capacity;
        N->V = ArenaPushArrayZero(MemPool, FLIBakeStr, N->Capacity);
        SLL_QUEUE_PUSH(List->Head, List->Tail, N);
        ++List->ChunkCount;
    }

    FLIBakeStr* Result = &N->V[N->Count];

    ++N->Count;
    ++List->TotalCount;

    return Result;
}

void 
FLIBakeStrChunkListCat(
    FLIBakeStrChunkList* Dst, 
    FLIBakeStrChunkList* ToPush
) {
    for (FLIBakeStrChunkNode* N = ToPush->Head; N; N = N->Next)
        N->BaseIndex += Dst->TotalCount;

    if (Dst->Tail && ToPush->Head) {
        Dst->Tail->Next = ToPush->Head;
        Dst->Tail = ToPush->Tail;
        Dst->ChunkCount += ToPush->ChunkCount;
        Dst->TotalCount += ToPush->TotalCount;
    } else if (!Dst->Head) {
        MemCpy(Dst, ToPush, sizeof(*Dst));
    }

    MemSet(ToPush, 0, sizeof(*ToPush));
}

INTERNAL int
FLIBakeStrIsBefore(void* _BakeStrA, void* _BakeStrB)
{
    return (
        ((FLIBakeStr*) _BakeStrA)->String < ((FLIBakeStr*) _BakeStrB)->String
    );
}

FLIBakeStrChunkList 
FLIBakeStrChunkListSortedFromUnsorted(
    Arena* MemPool, 
    FLIBakeStrChunkList* Src
) {
    FLIBakeStrChunkList Dst = {};

    for (FLIBakeStrChunkNode* Node = Src->Head; Node; Node = Node->Next) {
        for (u64 Index = 0; Index < Node->Count; ++Index) {
            FLIBakeStr* SrcString = &Node->V[Index];
            FLIBakeStr* DstString = FLIBakeStrChunkListPush(
                MemPool,
                &Dst,
                Src->TotalCount
            );

            MemCpy(DstString, SrcString, sizeof(*DstString));
        }
    }

    if (Dst.Head)
        radsort(Dst.Head->V, Dst.Head->Count, FLIBakeStrIsBefore);

    u64 NumDuplicates = 0;

    if (Dst.Head) {
        u64 LastIndex = 0;

        for (u64 Index = 1; Index < Dst.Head->Count; ++Index) {
            if (
                StrMatch(
                    Dst.Head->V[LastIndex].String,
                    Dst.Head->V[Index].String,
                    0
                )
            ) {
                MemSet(&Dst.Head->V[Index], 0, sizeof(Dst.Head->V[Index]));
                ++NumDuplicates;
            } else {
                LastIndex = Index;
            }
        }
    }

    if (NumDuplicates) {
        u64 LastIndex = 0;

        for (u64 Index = 1; Index < Dst.Head->Count; ++Index) {
            if (
                !LastIndex &&
                !Dst.Head->V[Index].String.Size &&
                !Dst.Head->V[Index].StringHash
            ) {
                LastIndex = Index;
            }

            if (
                LastIndex &&
                Dst.Head->V[Index].String.Size
            ) {
                MemCpy(
                    &Dst.Head->V[LastIndex], 
                    &Dst.Head->V[Index], 
                    sizeof(Dst.Head->V[LastIndex])
                );
                MemSet(&Dst.Head->V[Index], 0, sizeof(Dst.Head->V[Index]));
                ++LastIndex;
            }
        }

        if (NumDuplicates) {
            u64 MemPoolPositionPrePop = ArenaGetPosition(MemPool);

            ArenaPopTo(
                MemPool, 
                MemPoolPositionPrePop - NumDuplicates * sizeof(Dst.Head->V[0])
            );
            Dst.Head->Count -= NumDuplicates;
            Dst.Head->Capacity -= NumDuplicates;
            Dst.TotalCount -= NumDuplicates;
        }
    }

    return Dst;
}

FLIBakeStrMapLoose* 
FLIBakeStrMapLooseAlloc(Arena* MemPool, FLIBakeStrMapTopology* Top)
{
    FLIBakeStrMapLoose* Result = ArenaPushArrayZero(
        MemPool, 
        FLIBakeStrMapLoose, 
        1
    );

    Result->Slots = ArenaPushArrayZero(
        MemPool,
        FLIBakeStrChunkList*,
        Top->SlotsCount
    );

    return Result;
}

void 
FLIBakeStrMapLooseInsert(
    Arena* MemPool, 
    FLIBakeStrMapTopology* MapTopology, 
    FLIBakeStrMapLoose* Map, 
    u64 ChunkCapacity, 
    Str8 String
) {
    if (String.Size) {
        u64 StringHash = Hash(String.Str, String.Size);
        u64 SlotIndex = StringHash % MapTopology->SlotsCount;
        FLIBakeStrChunkList* Slot = Map->Slots[SlotIndex];

        if (!Slot) {
            Map->Slots[SlotIndex] = ArenaPushArrayZero(
                MemPool,
                FLIBakeStrChunkList,
                1
            );
            Slot = Map->Slots[SlotIndex];
        }

        b32 IsDuplicate = FALSE;

        for (FLIBakeStrChunkNode* Node = Slot->Head; Node; Node = Node->Next) {
            for (u64 Index = 0; Index < Node->Count; ++Index) {
                if (StrMatch(Node->V[Index].String, String, 0)) {
                    IsDuplicate = TRUE;
                    goto BREAK_ALL;
                }
            }
        }

BREAK_ALL:
        if (!IsDuplicate) {
            FLIBakeStr* BakeString = FLIBakeStrChunkListPush(
                MemPool, 
                Slot, 
                ChunkCapacity
            );

            BakeString->String = String;
            BakeString->StringHash = StringHash;
        }
    }
}

FLIBakeStrMapBaseIndices 
FLIBakeStrMapBaseIndicesFromMapLoose(
    Arena* MemPool, 
    FLIBakeStrMapTopology* MapTopology, 
    FLIBakeStrMapLoose* Map
) {
    FLIBakeStrMapBaseIndices Result = {};

    Result.SlotsBaseIndices = ArenaPushArrayZero(
        MemPool,
        u64,
        MapTopology->SlotsCount + 1
    );

    u64 TotalCount = 0;

    for (u64 Index = 0; Index < MapTopology->SlotsCount; ++Index) {
        Result.SlotsBaseIndices[Index] += TotalCount;

        if (Map->Slots[Index])
            TotalCount += Map->Slots[Index]->TotalCount;
    }

    Result.SlotsBaseIndices[MapTopology->SlotsCount] = TotalCount;

    return Result;
}

u32 
FLIBakeIndexFromStr(FLIBakeStrMapTight* Map, Str8 String)
{
    u32 Index = 0;

    if (String.Size) {
        u64 StringHash = Hash(String.Str, String.Size);
        u64 SlotIndex = StringHash % Map->SlotsCount;

        for (
            FLIBakeStrChunkNode* Node = Map->Slots[SlotIndex].Head; 
            Node; 
            Node = Node->Next
        ) {
            for (u64 ChunkIndex = 0; ChunkIndex < Node->Count; ++ChunkIndex) {
                if (
                    Node->V[ChunkIndex].StringHash == StringHash && 
                    StrMatch(Node->V[ChunkIndex].String, String, 0)
                ) {
                    Index = Map->SlotsBaseIndices[SlotIndex] + Node->BaseIndex + ChunkIndex + 1;
                    break;
                }
            }
        }
    }

    return Index;
}

u64 
FLIHashFromIndexRun(u32* IndexRun, u32 Count)
{
    u64 Result = 5381;
    u32* Ptr = IndexRun;
    u32* End = IndexRun + Count;

    for ( ; Ptr < End; ++Ptr)
        Result = ((Result << 5) + Result) + (*Ptr);

    return Result;
}

FLIBakeIndexRun* 
FLIBakeIndexRunChunkListPush(
    Arena* MemPool, 
    FLIBakeIndexRunChunkList* List, 
    u64 Capacity
) {
    FLIBakeIndexRunChunkNode* Node = List->Tail;

    if (!Node || Node->Count >= Node->Capacity) {
        Node = ArenaPushArrayZero(MemPool, FLIBakeIndexRunChunkNode, 1);
        Node->Capacity = Capacity;
        Node->V = ArenaPushArrayZero(MemPool, FLIBakeIndexRun, Node->Capacity);
        SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
        ++List->ChunkCount;
    }

    FLIBakeIndexRun* Result = &Node->V[Node->Count];

    ++Node->Count;
    ++List->TotalCount;

    return Result;
}

void 
FLIBakeIndexRunChunkListCat(
    FLIBakeIndexRunChunkList* Dst, 
    FLIBakeIndexRunChunkList* ToPush
) {
    if (Dst->Tail && ToPush->Head) {
        Dst->Tail->Next = ToPush->Head;
        Dst->Tail = ToPush->Tail;
        Dst->ChunkCount += ToPush->ChunkCount;
        Dst->TotalCount += ToPush->TotalCount;
    } else if (!Dst->Head) {
        MemCpy(Dst, ToPush, sizeof(*Dst));
    }

    MemSet(ToPush, 0, sizeof(*ToPush));
}

INTERNAL int
FLIBakeIndexRunIsBefore(void* __BakeIndexA, void* __BakeIndexB)
{
    b32 Result = FALSE;
    FLIBakeIndexRun* BakeIndexA = (FLIBakeIndexRun*) __BakeIndexA;
    FLIBakeIndexRun* BakeIndexB = (FLIBakeIndexRun*) __BakeIndexB;
    u64 CommonCount = MIN(BakeIndexA->Count, BakeIndexB->Count);

    for (u64 Offset = 0; Offset < CommonCount; ++Offset) {
        if (BakeIndexA->Indices[Offset] < BakeIndexB->Indices[Offset]) {
            Result = TRUE;
            break;
        } else if (
            BakeIndexA->Indices[Offset] > BakeIndexB->Indices[Offset]
        ) {
            Result = FALSE;
            break;
        } else if (Offset + 1 == CommonCount) {
            Result = (BakeIndexA->Count < BakeIndexB->Count);
        }
    }

    return Result;
}

FLIBakeIndexRunChunkList 
FLIBakeIndexRunChunkListSortedFromUnsorted(
    Arena* MemPool, 
    FLIBakeIndexRunChunkList* Src
) {
    FLIBakeIndexRunChunkList Result = {};

    for (
        FLIBakeIndexRunChunkNode* Node = Src->Head;
        Node;
        Node = Node->Next
    ) {
        for (u64 Index = 0; Index < Node->Count; ++Index) {
            FLIBakeIndexRun* SrcString = &Node->V[Index];
            FLIBakeIndexRun* DstString = FLIBakeIndexRunChunkListPush(
                MemPool,
                &Result,
                Src->TotalCount
            );

            MemCpy(DstString, SrcString, sizeof(*DstString));
        }
    }

    if (Result.Head)
        radsort(Result.Head->V, Result.Head->Count, FLIBakeIndexRunIsBefore);

    u64 NumDuplicates = 0;

    if (Result.Head) {
        u64 LastIndex = 0;

        for (u64 Index = 1; Index < Result.Head->Count; ++Index) {
            if (
                Result.Head->V[LastIndex].Count == Result.Head->V[Index].Count && 
                MemCmp(
                    Result.Head->V[LastIndex].Indices,
                    Result.Head->V[Index].Indices,
                    sizeof(
                        Result.Head->V[Index].Indices[0]
                    ) * Result.Head->V[Index].Count
                )
            ) {
                MemSet(
                    &Result.Head->V[Index], 
                    0, 
                    sizeof(Result.Head->V[Index])
                );
                ++NumDuplicates;
            } else {
                LastIndex = Index;
            }
        }
    }

    if (NumDuplicates) {
        u64 LastIndex = 0;

        for (u64 Index = 1; Index < Result.Head->Count; ++Index) {
            if (!LastIndex && !Result.Head->V[Index].RunHash)
                LastIndex = Index;

            if (LastIndex && Result.Head->V[Index].RunHash) {
                MemCpy(
                    &Result.Head->V[LastIndex], 
                    &Result.Head->V[Index], 
                    sizeof(Result.Head->V[LastIndex])
                );
                MemSet(
                    &Result.Head->V[Index], 
                    0, 
                    sizeof(Result.Head->V[Index])
                );
                ++LastIndex;
            }
        }

        if (NumDuplicates) {
            u64 MemPoolPositionPrePop = ArenaGetPosition(MemPool);

            ArenaPopTo(
                MemPool,
                MemPoolPositionPrePop - NumDuplicates * sizeof(Result.Head->V[0])
            );
            Result.Head->Count -= NumDuplicates;
            Result.Head->Capacity -= NumDuplicates;
            Result.TotalCount -= NumDuplicates;
        }
    }

    return Result;
}

FLIBakeIndexRunMapLoose* 
FLIBakeIndexRunMapLooseAlloc(Arena* MemPool, FLIBakeIndexRunMapTopology* Top)
{
    FLIBakeIndexRunMapLoose* Result = ArenaPushArrayZero(
        MemPool,
        FLIBakeIndexRunMapLoose,
        1
    );

    Result->Slots = ArenaPushArrayZero(
        MemPool,
        FLIBakeIndexRunChunkList*,
        Top->SlotsCount
    );
    Result->SlotsIndicesCount = ArenaPushArrayZero(
        MemPool,
        u64,
        Top->SlotsCount
    );

    return Result;
}

void 
FLIBakeIndexRunMapLooseInsert(
    Arena* MemPool, 
    FLIBakeIndexRunMapTopology* MapTopology, 
    FLIBakeIndexRunMapLoose* Map, 
    u64 ChunkCapacity, 
    u32* Indices, 
    u32 Count
) {
    if (Count) {
        u64 RunHash = FLIHashFromIndexRun(Indices, Count);
        u64 SlotIndex = RunHash % MapTopology->SlotsCount;
        FLIBakeIndexRunChunkList* Slot = Map->Slots[SlotIndex];

        if (!Slot) {
            Map->Slots[SlotIndex] = ArenaPushArrayZero(
                MemPool,
                FLIBakeIndexRunChunkList,
                1
            );
            Slot = Map->Slots[SlotIndex];
        }

        b32 IsDuplicate = FALSE;

        for (
            FLIBakeIndexRunChunkNode* Node = Slot->Head; 
            Node; 
            Node = Node->Next
        ) {
            for (u64 Index = 0; Index < Node->Count; ++Index) {
                if (
                    Node->V[Index].RunHash == RunHash &&
                    Node->V[Index].Count == Count &&
                    MemCmp(
                        Node->V[Index].Indices, 
                        Indices, 
                        sizeof(Indices[0]) * Count
                    )
                ) {
                    IsDuplicate = TRUE;
                    goto BREAK_ALL;
                }
            }
        }

BREAK_ALL:

        if (!IsDuplicate) {
            FLIBakeIndexRun* BakeRun = FLIBakeIndexRunChunkListPush(
                MemPool,
                Slot,
                ChunkCapacity
            );

            BakeRun->RunHash = RunHash;
            BakeRun->Count = Count;
            BakeRun->Indices = Indices;
            Map->SlotsIndicesCount[SlotIndex] += Count;
        }
    }
}

u32 
FLIBakeIndexFromIndexRun(FLIBakeIndexRunMap* Map, u32* Indices, u32 Count)
{
    u32 Index = 0;

    if (Count) {
        u64 RunHash = FLIHashFromIndexRun(Indices, Count);
        u64 SlotIndex = RunHash % Map->SlotsCount;
        u64 Offset = 0;

        for (
            FLIBakeIndexRunChunkNode* Node = Map->Slots[SlotIndex].Head; 
            Node; 
            Node = Node->Next
        ) {
            for (
                u64 ChunkIndex = 0; 
                ChunkIndex < Node->Count; 
                ++ChunkIndex
            ) {
                if (
                    Node->V[ChunkIndex].RunHash == RunHash &&
                    Node->V[ChunkIndex].Count == Count &&
                    MemCmp(
                        Node->V[ChunkIndex].Indices, 
                        Indices, 
                        sizeof(Indices[0]) * Count
                    )
                ) {
                    Index = SafeTruncateU64(
                        Map->SlotsBaseIndices[SlotIndex] + Offset
                    );
                    goto END_LOOKUP;
                }

                Offset += Node->V[ChunkIndex].Count;
            }
        }

END_LOOKUP:;
    }

    return Index;
}

FLIBakeName* 
FLIBakeNameChunkListPush(
    Arena* MemPool, 
    FLIBakeNameChunkList* List, 
    u64 Capacity
) {
    FLIBakeNameChunkNode* Node = List->Tail;

    if (!Node || Node->Count >= Node->Capacity) {
        Node = ArenaPushArrayZero(MemPool, FLIBakeNameChunkNode, 1);
        Node->Capacity = Capacity;
        Node->BaseIndex = List->TotalCount;
        Node->V = ArenaPushArrayZero(MemPool, FLIBakeName, Node->Capacity);
        SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
        ++List->ChunkCount;
    }

    FLIBakeName* Result =&Node->V[Node->Count];

    ++Node->Count;
    ++List->TotalCount;

    return Result;
}

void 
FLIBakeNameChunkListCat(
    FLIBakeNameChunkList* Dst, 
    FLIBakeNameChunkList* ToPush
) {
    for (FLIBakeNameChunkNode* Node = ToPush->Head; Node; Node = Node->Next)
        Node->BaseIndex += Dst->TotalCount;

    if (Dst->Tail && ToPush->Head) {
        Dst->Tail->Next = ToPush->Head;
        Dst->Tail = ToPush->Tail;
        Dst->ChunkCount += ToPush->ChunkCount;
        Dst->TotalCount += ToPush->TotalCount;
    } else if (!Dst->Head) {
        MemCpy(Dst, ToPush, sizeof(*Dst));
    }

    MemSet(ToPush, 0, sizeof(*ToPush));
}

INTERNAL int
FLIBakeNameIsBefore(void* __BakeNameA, void* __BakeNameB)
{
    FLIBakeName* BakeNameA = (FLIBakeName*) __BakeNameA;
    FLIBakeName* BakeNameB = (FLIBakeName*) __BakeNameB;
    b32 BakeNameAIsLess = BakeNameA->String < BakeNameB->String;
    b32 Result = BakeNameAIsLess;

    if (!BakeNameAIsLess) {
        b32 BakeNameBIsLess = BakeNameB->String < BakeNameA->String;

        if (!BakeNameBIsLess)
            Result = BakeNameA->Index < BakeNameB->Index;
    }

    return Result;
}

FLIBakeNameChunkList 
FLIBakeNameChunkListSortedFromUnsorted(
    Arena* MemPool, 
    FLIBakeNameChunkList* Src
) {
    FLIBakeNameChunkList Result = {};

    for (FLIBakeNameChunkNode* Node = Src->Head; Node; Node = Node->Next) {
        for (u64 Index = 0; Index < Node->Count; ++Index) {
            FLIBakeName* SrcString = &Node->V[Index];
            FLIBakeName* ResultString = FLIBakeNameChunkListPush(
                MemPool,
                &Result,
                Src->TotalCount
            );

            MemCpy(ResultString, SrcString, sizeof(*ResultString));
        }
    }

    if (Result.Head)
        radsort(Result.Head->V, Result.Head->Count, FLIBakeNameIsBefore);

    u64 NumDuplicates = 0;

    if (Result.Head) {
        u64 LastIndex = 0;

        for (u64 Index = 1; Index < Result.Head->Count; ++Index) {
            if (
                StrMatch(
                    Result.Head->V[LastIndex].String, 
                    Result.Head->V[Index].String, 
                    0
                ) &&
                Result.Head->V[LastIndex].Index == Result.Head->V[Index].Index
            ) {
                MemSet(
                    &Result.Head->V[Index], 
                    0, 
                    sizeof(Result.Head->V[Index])
                );
                ++NumDuplicates;
            } else {
                LastIndex = Index;
            }
        }
    }

    if (NumDuplicates) {
        u64 LastIndex = 0;

        for (u64 Index = 1; Index < Result.Head->Count; ++Index) {
            if (
                !LastIndex &&
                !Result.Head->V[Index].String.Size &&
                !Result.Head->V[Index].NameHash
            ) {
                LastIndex = Index;
            }

            if (LastIndex && Result.Head->V[Index].String.Size) {
                MemCpy(
                    &Result.Head->V[LastIndex], 
                    &Result.Head->V[Index], 
                    sizeof(Result.Head->V[LastIndex])
                );
                MemSet(
                    &Result.Head->V[Index], 
                    0, 
                    sizeof(Result.Head->V[Index])
                );
                ++LastIndex;
            }
        }

        if (NumDuplicates) {
            u64 MemPoolPositionPrePop = ArenaGetPosition(MemPool);

            ArenaPopTo(
                MemPool,
                MemPoolPositionPrePop - NumDuplicates * sizeof(Result.Head->V[0])
            );
            Result.Head->Count -= NumDuplicates;
            Result.Head->Capacity -= NumDuplicates;
            Result.TotalCount -= NumDuplicates;
        }
    }

    return Result;
}

FLIBakeNameMap* 
FLIBakeNameMapAlloc(Arena* MemPool, FLIBakeNameMapTopology* Top)
{
    FLIBakeNameMap* Result = ArenaPushArrayZero(MemPool, FLIBakeNameMap, 1);

    Result->Slots = ArenaPushArrayZero(
        MemPool,
        FLIBakeNameChunkList*,
        Top->SlotsCount
    );

    return Result;
}

void 
FLIBakeNameMapInsert(
    Arena* MemPool, 
    FLIBakeNameMapTopology* MapTopology, 
    FLIBakeNameMap* Map, 
    u64 ChunkCapacity, 
    Str8 String, 
    u64 Index
) {
    if (String.Size) {
        u64 NameHash = Hash(String.Str, String.Size);
        u64 SlotIndex = NameHash % MapTopology->SlotsCount;
        FLIBakeNameChunkList* Slot = Map->Slots[SlotIndex];

        if (!Slot) {
            Map->Slots[SlotIndex] = ArenaPushArrayZero(
                MemPool,
                FLIBakeNameChunkList,
                1
            );
            Slot = Map->Slots[SlotIndex];
        }

        b32 IsDuplicate = FALSE;

        for (
            FLIBakeNameChunkNode* Node = Slot->Head; 
            Node; 
            Node = Node->Next
        ) {
            for (u64 I = 0; I < Node->Count; ++I) {
                if (
                    StrMatch(Node->V[I].String, String, 0) && 
                    Node->V[I].Index == I
                ) {
                    IsDuplicate = TRUE;
                    goto BREAK_ALL;
                }
            }
        }

BREAK_ALL:

        if (!IsDuplicate) {
            FLIBakeName* BakeString = FLIBakeNameChunkListPush(
                MemPool,
                Slot,
                ChunkCapacity
            );

            BakeString->String = String;
            BakeString->Index = Index;
            BakeString->NameHash = NameHash;
        }
    }
}

FLIBakeSection* 
FLIBakeSectionListPush(Arena* MemPool, FLIBakeSectionList* List)
{
    FLIBakeSectionNode* N = ArenaPushArrayZero(
        MemPool, 
        FLIBakeSectionNode, 
        1
    );

    SLL_QUEUE_PUSH(List->Head, List->Tail, N);
    ++List->Count;

    return &N->V;
}

FLIBakeSection* 
FLIBakeSectionListPushNewUnpacked(
    Arena* MemPool, 
    FLIBakeSectionList* List, 
    void* Data, 
    u64 Size, 
    FLISectionKind Tag, 
    u64 TagIndex
) {
    FLIBakeSection* Section = FLIBakeSectionListPush(MemPool, List);

    Section->Data = Data;
    Section->Encoding = FLI_SECTION_ENCODING_UNPACKED;
    Section->EncodingSize = Size;
    Section->UnpackedSize = Size;
    Section->Tag = Tag;
    Section->TagIndex = TagIndex;

    return Section;
}

void 
FLIBakeSectionListCat(
    FLIBakeSectionList* Dst, 
    FLIBakeSectionList* ToPush
) {
    if (Dst->Tail && ToPush->Head) {
        Dst->Tail->Next = ToPush->Head;
        Dst->Tail = ToPush->Tail;
        Dst->Count += ToPush->Count;
    } else if (!ToPush->Head) {
        MemCpy(Dst, ToPush, sizeof(*Dst));
    }

    MemSet(ToPush, 0, sizeof(*ToPush));
}

FLISerialisedSection 
FLISerialisedSectionMakeUnpacked(void* Data, u64 Size)
{
    FLISerialisedSection Result = {};

    Result.Data = Data;
    Result.EncodedSize = Size;
    Result.UnpackedSize = Size;
    Result.Encoding = FLI_SECTION_ENCODING_UNPACKED;

    return Result;
}

FLISerialisedSectionBundle 
FLISerialisedSectionBundleFromBakeResults(
    Arena* MemPool, 
    FLIBakeResults* Results
) {
    FLISerialisedSectionBundle Bundle = {};

    UNUSED(MemPool);

    Bundle.Sections[FLI_SECTION_KIND_FILE_INFO] = FLISerialisedSectionMakeUnpackedArray(
        Results->FileInfo.FileInfo,
        Results->FileInfo.FileInfoCount
    );
    Bundle.Sections[FLI_SECTION_KIND_MESSAGE_TYPES] = FLISerialisedSectionMakeUnpackedArray(
        Results->MessageTypes.MessageTypes,
        Results->MessageTypes.MessageTypesCount
    );
    Bundle.Sections[FLI_SECTION_KIND_FIELDS] = FLISerialisedSectionMakeUnpackedArray(
        Results->Fields.Fields,
        Results->Fields.FieldsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_COLUMNS] = FLISerialisedSectionMakeUnpackedArray(
        Results->Columns.Columns,
        Results->Columns.ColumnsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_PARAMS] = FLISerialisedSectionMakeUnpackedArray(
        Results->Params.Params,
        Results->Params.ParamsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_MESSAGES] = FLISerialisedSectionMakeUnpackedArray(
        Results->Messages.Messages,
        Results->Messages.MessagesCount
    );
    Bundle.Sections[FLI_SECTION_KIND_MODES] = FLISerialisedSectionMakeUnpackedArray(
        Results->Modes.Modes,
        Results->Modes.ModesCount
    );
    Bundle.Sections[FLI_SECTION_KIND_EVENTS] = FLISerialisedSectionMakeUnpackedArray(
        Results->Events.Events,
        Results->Events.EventsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_ERRORS] = FLISerialisedSectionMakeUnpackedArray(
        Results->Errors.Errors,
        Results->Errors.ErrorsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_STRING_DATA] = FLISerialisedSectionMakeUnpacked(
        Results->Strings.StringData, 
        Results->Strings.StringDataSize
    );
    Bundle.Sections[FLI_SECTION_KIND_STRING_TABLE] = FLISerialisedSectionMakeUnpackedArray(
        Results->Strings.StringOffsets, 
        Results->Strings.StringOffsetsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_INDEX_RUNS] = FLISerialisedSectionMakeUnpackedArray(
        Results->IndexRuns.IndexRuns, 
        Results->IndexRuns.IndexRunsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_NAME_MAPS] = FLISerialisedSectionMakeUnpackedArray(
        Results->NameMaps.NameMaps, 
        Results->NameMaps.NameMapsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_NAME_MAP_BUCKETS] = FLISerialisedSectionMakeUnpackedArray(
        Results->NameMaps.Buckets, 
        Results->NameMaps.BucketsCount
    );
    Bundle.Sections[FLI_SECTION_KIND_NAME_MAP_NODES] = FLISerialisedSectionMakeUnpackedArray(
        Results->NameMaps.Nodes, 
        Results->NameMaps.NodesCount
    );
    Bundle.ColumnSections = Results->Columns.ColumnData;
    Bundle.ColumnSectionsCount = Results->Columns.ColumnsCount;

    return Bundle;
}

Str8List 
FLIFileBlobsFromSectionBundle(
    Arena* MemPool, 
    FLISerialisedSectionBundle* Bundle
) {
    Str8List Strings = {};
    TempArena Scratch = GetScratch(&MemPool, 1);
    u64 SectionsCount = FLI_SECTION_KIND_COUNT + Bundle->ColumnSectionsCount;
    FLIHeader* Header = ArenaPushArrayZero(MemPool, FLIHeader, 1);
    FLISection* Sections = ArenaPushArrayZero(
        MemPool, 
        FLISection, 
        SectionsCount
    );

    ListPush(MemPool, &Strings, Str8Struct(Header));
    ListPushAlign(MemPool, &Strings, 8);

    u32 DataSectionOff = (u32) Strings.TotalSize;

    ListPush(
        MemPool, 
        &Strings, 
        Str(
            (u8*) Sections, 
            sizeof(FLISection) * SectionsCount
        )
    );
    Header->Magic = FLI_MAGIC_NUMBER;
    Header->EncodingVersion = FLI_ENCODING_VERSION;
    Header->DataSectionOffset = DataSectionOff;
    Header->DataSectionCount = SafeTruncateU64(SectionsCount);

    for (u64 K = FLI_SECTION_KIND_NULL; K < SectionsCount; ++K) {
        FLISerialisedSection* Src = (K < FLI_SECTION_KIND_COUNT)
            ? &Bundle->Sections[K]
            : &Bundle->ColumnSections[K - FLI_SECTION_KIND_COUNT];
        FLISection* Dst = Sections + K;
        u64 SectionDataOff = 0;

        if (Src->EncodedSize != 0) {
            ListPushAlign(MemPool, &Strings, 8);
            SectionDataOff = Strings.TotalSize;
            ListPush(
                MemPool, 
                &Strings, 
                Str(
                    (u8*) Src->Data, 
                    Src->EncodedSize
                )
            );
        }

        Dst->Encoding = Src->Encoding;
        Dst->Offset = SectionDataOff;
        Dst->EncodedSize = Src->EncodedSize;
        Dst->UnpackedSize = Src->UnpackedSize;
    }

    ReleaseScratch(Scratch);

    return Strings;
}

FLIBakeResults
FLIBake(Arena* MemPool, FLIBakeParams* Params)
{
    if (!LaneIndex())
        FLI_BAKE_SHARED = ArenaPushArrayZero(MemPool, FLIBakeShared, 1);

    LaneSync();

    TempArena Scratch = GetScratch(&MemPool, 1);
    FLIBakeStrMapTopology* StringMapTopology = NULL;
    FLIBakeStrMapLoose** LaneMapsLoose = NULL;
    FLIBakeStrMapLoose* MasterLooseMap = NULL;

    if (!LaneIndex()) {
        StringMapTopology = ArenaPushArrayZero(
            Scratch.MemPool,
            FLIBakeStrMapTopology,
            1
        );
        StringMapTopology->SlotsCount = (
            64 +
            Params->MessageTypes.TotalCount * 2 +
            Params->Fields.TotalCount * 2 +
            Params->Params.TotalCount +
            Params->Messages.TotalCount
        );
        LaneMapsLoose = ArenaPushArrayZero(
            Scratch.MemPool,
            FLIBakeStrMapLoose*,
            LaneCount()
        );
        MasterLooseMap = FLIBakeStrMapLooseAlloc(
            Scratch.MemPool,
            StringMapTopology
        );
    }

    LaneSyncU64(&StringMapTopology, 0);
    LaneSyncU64(&LaneMapsLoose, 0);
    LaneSyncU64(&MasterLooseMap, 0);
    LaneMapsLoose[LaneIndex()] = FLIBakeStrMapLooseAlloc(
        Scratch.MemPool,
        StringMapTopology
    );

    FLIBakeStrMapLoose* LaneMap = LaneMapsLoose[LaneIndex()];

    if (!LaneIndex()) {
        FLIBakeStrMapLooseInsert(
            MemPool, 
            StringMapTopology, 
            LaneMap, 
            1, 
            "[NULL]"_s8
        );
        FLIBakeStrMapLooseInsert(
            MemPool, 
            StringMapTopology, 
            LaneMap, 
            1, 
            Params->FileInfo.SourcePath
        );
        FLIBakeStrMapLooseInsert(
            MemPool, 
            StringMapTopology, 
            LaneMap, 
            1, 
            Params->FileInfo.Vehicle
        );
        FLIBakeStrMapLooseInsert(
            MemPool, 
            StringMapTopology, 
            LaneMap, 
            1, 
            Params->FileInfo.Firmware
        );
    }

    for (
        FLIBakeMessageTypeChunkNode* Node = Params->MessageTypes.Head;
        Node;
        Node = Node->Next
    ) {
        r1u64 Range = LaneRange(Node->Count);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            FLIBakeMessageType* MessageType = &Node->V[Index];

            FLIBakeStrMapLooseInsert(
                MemPool, 
                StringMapTopology, 
                LaneMap, 
                2, 
                MessageType->Name
            );
            FLIBakeStrMapLooseInsert(
                MemPool, 
                StringMapTopology, 
                LaneMap, 
                2, 
                MessageType->Format
            );
        }
    }

    for (
        FLIBakeFieldChunkNode* Node = Params->Fields.Head;
        Node;
        Node = Node->Next
    ) {
        r1u64 Range = LaneRange(Node->Count);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            FLIBakeField* Field = &Node->V[Index];

            FLIBakeStrMapLooseInsert(
                MemPool, 
                StringMapTopology, 
                LaneMap, 
                4, 
                Field->Name
            );
            FLIBakeStrMapLooseInsert(
                MemPool, 
                StringMapTopology, 
                LaneMap, 
                4, 
                Field->Unit
            );
        }
    }

    for (
        FLIBakeParamChunkNode* Node = Params->Params.Head;
        Node;
        Node = Node->Next
    ) {
        r1u64 Range = LaneRange(Node->Count);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            FLIBakeParam* Param = &Node->V[Index];

            FLIBakeStrMapLooseInsert(
                MemPool, 
                StringMapTopology, 
                LaneMap, 
                4, 
                Param->Name
            );
        }
    }

    for (
        FLIBakeMessageChunkNode* Node = Params->Messages.Head;
        Node;
        Node = Node->Next
    ) {
        r1u64 Range = LaneRange(Node->Count);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            FLIBakeMessage* Message = &Node->V[Index];

            FLIBakeStrMapLooseInsert(
                MemPool, 
                StringMapTopology, 
                LaneMap, 
                2, 
                Message->Text
            );
        }
    }

    LaneSync();

    {
        r1u64 SlotRange = LaneRange(StringMapTopology->SlotsCount);

        for (
            u64 SlotIndex = SlotRange.Min; 
            SlotIndex < SlotRange.Max; 
            ++SlotIndex
        ) {
            for (u32 LIndex = 0; LIndex < LaneCount(); ++LIndex) {
                FLIBakeStrChunkList* Src = (LaneMapsLoose[LIndex])
                    ? LaneMapsLoose[LIndex]->Slots[SlotIndex]
                    : NULL;
                FLIBakeStrChunkList* Dst = MasterLooseMap->Slots[SlotIndex];

                if (!Dst && Src)
                    MasterLooseMap->Slots[SlotIndex] = Src;
                else if (Dst && Src)
                    FLIBakeStrChunkListCat(Dst, Src);
            }
        }
    }

    LaneSync();

    {
        r1u64 SlotRange = LaneRange(StringMapTopology->SlotsCount);

        for (
            u64 SlotIndex = SlotRange.Min; 
            SlotIndex < SlotRange.Max; 
            ++SlotIndex
        ) {
            if (MasterLooseMap->Slots[SlotIndex]) {
                *MasterLooseMap->Slots[SlotIndex] = FLIBakeStrChunkListSortedFromUnsorted(
                    MemPool,
                    MasterLooseMap->Slots[SlotIndex]
                );
            }
        }
    }

    LaneSync();

    if (!LaneIndex()) {
        FLIBakeStrMapBaseIndices BakeBase = FLIBakeStrMapBaseIndicesFromMapLoose(
            MemPool,
            StringMapTopology,
            MasterLooseMap
        );

        FLI_BAKE_SHARED->BakeStrings.SlotsCount = StringMapTopology->SlotsCount;
        FLI_BAKE_SHARED->BakeStrings.Slots = ArenaPushArrayZero(
            MemPool,
            FLIBakeStrChunkList,
            FLI_BAKE_SHARED->BakeStrings.SlotsCount
        );
        FLI_BAKE_SHARED->BakeStrings.SlotsBaseIndices = BakeBase.SlotsBaseIndices;
        FLI_BAKE_SHARED->BakeStrings.TotalCount = BakeBase.SlotsBaseIndices[FLI_BAKE_SHARED->BakeStrings.SlotsCount];
    }

    LaneSync();

    {
        r1u64 SlotRange = LaneRange(FLI_BAKE_SHARED->BakeStrings.SlotsCount);

        for (
            u64 SlotIndex = SlotRange.Min; 
            SlotIndex < SlotRange.Max; 
            ++SlotIndex
        ) {
            if (MasterLooseMap->Slots[SlotIndex]) {
                MemCpy(
                    &FLI_BAKE_SHARED->BakeStrings.Slots[SlotIndex],
                    MasterLooseMap->Slots[SlotIndex],
                    sizeof(FLIBakeStrChunkList)
                );
            }
        }
    }

    LaneSync();

    FLIBakeStrMapTight* BakeStrings = &FLI_BAKE_SHARED->BakeStrings;
    b32 NameMapsNeedBuild[FLI_NAME_MAP_KIND_COUNT] = {};

    NameMapsNeedBuild[FLI_NAME_MAP_KIND_MESSAGE_TYPES] = !!(Params->MessageTypes.TotalCount);
    NameMapsNeedBuild[FLI_NAME_MAP_KIND_FIELDS] = !!(Params->Fields.TotalCount);
    NameMapsNeedBuild[FLI_NAME_MAP_KIND_PARAMS] = !!(Params->Params.TotalCount);

    FLIBakeNameMapTopology* BakeNameMapsTopologies = NULL;
    FLIBakeNameMap** BakeNameMaps = NULL;

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        FLIBakeNameMap*** LaneMaps = NULL;

        if (!LaneIndex()) {
            BakeNameMapsTopologies = ArenaPushArrayZero(
                MemPool,
                FLIBakeNameMapTopology,
                FLI_NAME_MAP_KIND_COUNT
            );
            BakeNameMaps = ArenaPushArrayZero(
                MemPool,
                FLIBakeNameMap*,
                FLI_NAME_MAP_KIND_COUNT
            );
            LaneMaps = ArenaPushArrayZero(
                MemPool,
                FLIBakeNameMap**,
                FLI_NAME_MAP_KIND_COUNT
            );

            for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
                u64 SlotCount = 0;

                switch (K) {
                    case FLI_NAME_MAP_KIND_NULL:
                    case FLI_NAME_MAP_KIND_COUNT: {} break;

                #define CASE(X, Y) case FLI_NAME_MAP_KIND_##X: { SlotCount = ((Y) + (Y) / 4); } break;

                    CASE(MESSAGE_TYPES, Params->MessageTypes.TotalCount)

                    CASE(FIELDS, Params->Fields.TotalCount)

                    CASE(PARAMS, Params->Params.TotalCount)

                #undef CASE
                }

                LaneMaps[K] = ArenaPushArrayZero(
                    MemPool,
                    FLIBakeNameMap*,
                    LaneCount()
                );
                BakeNameMapsTopologies[K].SlotsCount = SlotCount;
            }
        }

        LaneSyncU64(&BakeNameMapsTopologies, 0);
        LaneSyncU64(&BakeNameMaps, 0);
        LaneSyncU64(&LaneMaps, 0);

        for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
            if (!NameMapsNeedBuild[K])
                continue;

            FLIBakeNameMapTopology* Top = &BakeNameMapsTopologies[K];

            LaneMaps[K][LaneIndex()] = FLIBakeNameMapAlloc(Scratch.MemPool, Top);

            FLIBakeNameMap* Map = LaneMaps[K][LaneIndex()];

            switch (K) {
                case FLI_NAME_MAP_KIND_NULL:
                case FLI_NAME_MAP_KIND_COUNT: {} break;

                case FLI_NAME_MAP_KIND_MESSAGE_TYPES: {
                    for (
                        FLIBakeMessageTypeChunkNode* Node = Params->MessageTypes.Head; 
                        Node; 
                        Node = Node->Next
                    ) {
                        r1u64 Range = LaneRange(Node->Count);

                        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
                            FLIBakeMessageType* MessageType = &Node->V[Index];

                            FLIBakeNameMapInsert(
                                Scratch.MemPool,
                                Top,
                                Map,
                                4,
                                MessageType->Name,
                                FLIIndexFromBakeMessageType(MessageType)
                            );
                        }
                    }
                } break;

                case FLI_NAME_MAP_KIND_FIELDS: {
                    for (
                        FLIBakeFieldChunkNode* Node = Params->Fields.Head;
                        Node;
                        Node = Node->Next
                    ) {
                        r1u64 Range = LaneRange(Node->Count);

                        for (
                            u64 Index = Range.Min; 
                            Index < Range.Max; 
                            ++Index
                        ) {
                            FLIBakeField* Field = &Node->V[Index];

                            FLIBakeNameMapInsert(
                                Scratch.MemPool,
                                Top,
                                Map,
                                4,
                                Field->Name,
                                FLIIndexFromBakeField(Field)
                            );
                        }
                    }
                } break;

                case FLI_NAME_MAP_KIND_PARAMS: {
                    for (
                        FLIBakeParamChunkNode* Node = Params->Params.Head;
                        Node;
                        Node = Node->Next
                    ) {
                        r1u64 Range = LaneRange(Node->Count);

                        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
                            FLIBakeParam* Param = &Node->V[Index];

                            FLIBakeNameMapInsert(
                                Scratch.MemPool,
                                Top,
                                Map,
                                4,
                                Param->Name,
                                FLIIndexFromBakeParam(Param)
                            );
                        }
                    }
                } break;
            }
        }

        LaneSync();

        if (!LaneIndex()) {
            for (u32 K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
                if (!NameMapsNeedBuild[K])
                    continue;

                BakeNameMaps[K] = FLIBakeNameMapAlloc(
                    MemPool, 
                    &BakeNameMapsTopologies[K]
                );
            }
        }

        LaneSync();

        for (u32 K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
            if (!NameMapsNeedBuild[K])
                continue;

            FLIBakeNameMapTopology* Top = &BakeNameMapsTopologies[K];
            FLIBakeNameMap* Map = BakeNameMaps[K];
            r1u64 SlotRange = LaneRange(Top->SlotsCount);

            for (
                u64 SlotIndex = SlotRange.Min;
                SlotIndex < SlotRange.Max;
                ++SlotIndex
            ) {
                for (u64 LIndex = 0; LIndex < LaneCount(); ++LIndex) {
                    FLIBakeNameMap* SrcMap = LaneMaps[K][LIndex];
                    FLIBakeNameMap* DstMap = Map;

                    if (
                        !DstMap->Slots[SlotIndex] && 
                        SrcMap->Slots[SlotIndex]
                    ) {
                        DstMap->Slots[SlotIndex] = SrcMap->Slots[SlotIndex];
                    } else if (
                        DstMap->Slots[SlotIndex] && 
                        SrcMap->Slots[SlotIndex]
                    ) {
                        FLIBakeNameChunkListCat(
                            DstMap->Slots[SlotIndex], 
                            SrcMap->Slots[SlotIndex]
                        );
                    }
                }
            }

            SlotRange = LaneRange(Top->SlotsCount);

            for (
                u64 SlotIndex = SlotRange.Min;
                SlotIndex < SlotRange.Max;
                ++SlotIndex
            ) {
                if (Map->Slots[SlotIndex]) {
                    FLIBakeNameChunkList* ToBeSorted = Map->Slots[SlotIndex];

                    Map->Slots[SlotIndex] = ArenaPushArrayZero(
                        MemPool,
                        FLIBakeNameChunkList,
                        1
                    );
                    *Map->Slots[SlotIndex] = FLIBakeNameChunkListSortedFromUnsorted(
                        MemPool,
                        ToBeSorted
                    );
                }
            }
        }
    }

    LaneSync();
    ReleaseScratch(Scratch);
    LaneSync();

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        FLIBakeIndexRunMapTopology* Top = NULL;
        FLIBakeIndexRunMapLoose** LaneMapsLoose = NULL;
        FLIBakeIndexRunMapLoose* MapLoose = NULL;

        if (!LaneIndex()) {
            Top = ArenaPushArrayZero(
                Scratch.MemPool,
                FLIBakeIndexRunMapTopology,
                1
            );

            u64 EstimatedCount = (
                Params->MessageTypes.TotalCount +
                Params->Fields.TotalCount +
                Params->Params.TotalCount
            );

            Top->SlotsCount = 64 + (EstimatedCount * 3) / 4;
            LaneMapsLoose = ArenaPushArrayZero(
                Scratch.MemPool,
                FLIBakeIndexRunMapLoose*,
                LaneCount()
            );
            MapLoose = FLIBakeIndexRunMapLooseAlloc(
                Scratch.MemPool,
                Top
            );
        }

        LaneSyncU64(&Top, 0);
        LaneSyncU64(&LaneMapsLoose, 0);
        LaneSyncU64(&MapLoose, 0);
        LaneMapsLoose[LaneIndex()] = FLIBakeIndexRunMapLooseAlloc(
            Scratch.MemPool,
            Top
        );

        FLIBakeIndexRunMapLoose* LaneMap = LaneMapsLoose[LaneIndex()];

        for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
            if (!NameMapsNeedBuild[K])
                continue;

            FLIBakeNameMapTopology* NameMapTop = &BakeNameMapsTopologies[K];
            FLIBakeNameMap* NameMap = BakeNameMaps[K];
            r1u64 SlotRange = LaneRange(NameMapTop->SlotsCount);

            for (u64 Index = SlotRange.Min; Index < SlotRange.Max; ++Index) {
                FLIBakeNameChunkList* Slot = NameMap->Slots[Index];

                if (Slot) {
                    struct IndexRunNode {
                        IndexRunNode* Next;
                        u64           Index;
                    };

                    IndexRunNode* HeadIndexRunNode = NULL;
                    IndexRunNode* TailIndexRunNode = NULL;
                    u64 ActiveIndexCount = 0;
                    Str8 ActiveString = {};
                    FLIBakeNameChunkNode* Node = Slot->Head;
                    u64 NodeIndex = 0;

                    for (;;) {
                        if (Node && NodeIndex >= Node->Count) {
                            Node = Node->Next;
                            NodeIndex = 0;
                        }

                        Str8 String = {};
                        u64 I = 0;

                        if (Node) {
                            String = Node->V[NodeIndex].String;
                            I = Node->V[NodeIndex].Index;
                        }

                        if (!StrMatch(String, ActiveString, 0)) {
                            if (ActiveIndexCount > 1) {
                                u64 IndicesCount = ActiveIndexCount;
                                u32* Indices = ArenaPushArrayZero(
                                    MemPool,
                                    u32,
                                    IndicesCount
                                );
                                u64 WriteIndex = 0;

                                for (
                                    IndexRunNode* RunNode = HeadIndexRunNode; 
                                    RunNode; 
                                    RunNode = RunNode->Next
                                ) {
                                    Indices[WriteIndex] = SafeTruncateU64(
                                        RunNode->Index
                                    );
                                    ++WriteIndex;
                                }

                                FLIBakeIndexRunMapLooseInsert(
                                    Scratch.MemPool,
                                    Top,
                                    LaneMap,
                                    4,
                                    Indices,
                                    IndicesCount
                                );
                            }

                            ActiveString = String;
                            HeadIndexRunNode = NULL;
                            TailIndexRunNode = NULL;
                            ActiveIndexCount = 0;
                        }

                        if (
                            ActiveString.Size && 
                            StrMatch(String, ActiveString, 0)
                        ) {
                            IndexRunNode* RunNode = ArenaPushArrayZero(
                                Scratch.MemPool,
                                IndexRunNode,
                                1
                            );

                            RunNode->Index = I;
                            SLL_QUEUE_PUSH(
                                HeadIndexRunNode, 
                                TailIndexRunNode, 
                                RunNode
                            );
                            ++ActiveIndexCount;
                        }

                        ++NodeIndex;

                        if (!Node)
                            break;
                    }
                }
            }
        }

        LaneSync();

        {
            r1u64 SlotRange = LaneRange(Top->SlotsCount);

            for (
                u64 SlotIndex = SlotRange.Min; 
                SlotIndex < SlotRange.Max; 
                ++SlotIndex
            ) {
                for (u64 LIndex = 0; LIndex < LaneCount(); ++LIndex) {
                    FLIBakeIndexRunMapLoose* SrcMap = LaneMapsLoose[LIndex];
                    FLIBakeIndexRunMapLoose* DstMap = MapLoose;

                    DstMap->SlotsIndicesCount[SlotIndex] += SrcMap->SlotsIndicesCount[SlotIndex];

                    if (
                        !DstMap->Slots[SlotIndex] && 
                        SrcMap->Slots[SlotIndex]
                    ) {
                        DstMap->Slots[SlotIndex] = SrcMap->Slots[SlotIndex];
                    } else if (
                        DstMap->Slots[SlotIndex] &&
                        SrcMap->Slots[SlotIndex]
                    ) {
                        FLIBakeIndexRunChunkListCat(
                            DstMap->Slots[SlotIndex],
                            SrcMap->Slots[SlotIndex]
                        );
                    }
                }
            }
        }

        LaneSync();

        {
            FLIBakeIndexRunMapLoose* Map = MapLoose;
            r1u64 SlotRange = LaneRange(Top->SlotsCount);

            for (
                u64 SlotIndex = SlotRange.Min; 
                SlotIndex < SlotRange.Max; 
                ++SlotIndex
            ) {
                if (Map->Slots[SlotIndex]) {
                    *Map->Slots[SlotIndex] = FLIBakeIndexRunChunkListSortedFromUnsorted(
                        MemPool,
                        Map->Slots[SlotIndex]
                    );
                    Map->SlotsIndicesCount[SlotIndex] = 0;

                    for (
                        FLIBakeIndexRunChunkNode* N = Map->Slots[SlotIndex]->Head;
                        N;
                        N = N->Next
                    ) {
                        for (u64 I = 0; I < N->Count; ++I) {
                            Map->SlotsIndicesCount[SlotIndex] += N->V[I].Count;
                        }
                    }
                }
            }
        }

        LaneSync();

        {
            FLIBakeIndexRunMapLoose* Map = MapLoose;

            if (!LaneIndex()) {
                FLI_BAKE_SHARED->BakeIndexRuns.SlotsCount = Top->SlotsCount;
                FLI_BAKE_SHARED->BakeIndexRuns.Slots = ArenaPushArrayZero(
                    MemPool,
                    FLIBakeIndexRunChunkList,
                    FLI_BAKE_SHARED->BakeIndexRuns.SlotsCount
                );
                FLI_BAKE_SHARED->BakeIndexRuns.SlotsBaseIndices = ArenaPushArrayZero(
                    MemPool,
                    u64,
                    FLI_BAKE_SHARED->BakeIndexRuns.SlotsCount + 1
                );

                u64 EncodingIndexOffset = 0;

                for (u64 SlotIndex = 0; SlotIndex < Top->SlotsCount; ++SlotIndex) {
                    FLI_BAKE_SHARED->BakeIndexRuns.SlotsBaseIndices[SlotIndex] = EncodingIndexOffset;

                    if (Map->Slots[SlotIndex])
                        EncodingIndexOffset += Map->SlotsIndicesCount[SlotIndex];
                }

                FLI_BAKE_SHARED->BakeIndexRuns.SlotsBaseIndices[Top->SlotsCount] = EncodingIndexOffset;
            }

            LaneSync();

            r1u64 SlotRange = LaneRange(FLI_BAKE_SHARED->BakeIndexRuns.SlotsCount);

            for (u64 Index = SlotRange.Min; Index < SlotRange.Max; ++Index) {
                if (Map->Slots[Index]) {
                    MemCpy(
                        &FLI_BAKE_SHARED->BakeIndexRuns.Slots[Index],
                        Map->Slots[Index],
                        sizeof(FLI_BAKE_SHARED->BakeIndexRuns.Slots[Index])
                    );
                }
            }

            LaneSync();
            ReleaseScratch(Scratch);
        }
    }

    LaneSync();

    FLIBakeIndexRunMap* BakeIndexRuns = &FLI_BAKE_SHARED->BakeIndexRuns;

    {
        if (!LaneIndex()) {
            FLI_BAKE_SHARED->BakedStrings.StringOffsetsCount = BakeStrings->TotalCount + 2;
            FLI_BAKE_SHARED->BakedStrings.StringOffsets = ArenaPushArrayZero(
                MemPool,
                u32,
                FLI_BAKE_SHARED->BakedStrings.StringOffsetsCount
            );

            u64 OffsetCursor = 0;

            for (u64 SlotIndex = 0; SlotIndex < BakeStrings->SlotsCount; ++SlotIndex) {
                for (
                    FLIBakeStrChunkNode* Node = BakeStrings->Slots[SlotIndex].Head;
                    Node;
                    Node = Node->Next
                ) {
                    for (u64 NodeIndex = 0; NodeIndex < Node->Count; ++NodeIndex) {
                        FLIBakeStr* Src = &Node->V[NodeIndex];
                        u64 DstIndex = BakeStrings->SlotsBaseIndices[SlotIndex] + Node->BaseIndex + NodeIndex + 1;

                        ASSERT(OffsetCursor < U32_MAX);

                        FLI_BAKE_SHARED->BakedStrings.StringOffsets[DstIndex] = OffsetCursor;
                        OffsetCursor += Src->String.Size;
                    }
                }
            }

            FLI_BAKE_SHARED->BakedStrings.StringOffsets[BakeStrings->TotalCount + 1] = SafeTruncateU64(OffsetCursor);
            FLI_BAKE_SHARED->BakedStrings.StringDataSize = OffsetCursor;
            FLI_BAKE_SHARED->BakedStrings.StringData = ArenaPushArrayZero(
                MemPool,
                u8,
                FLI_BAKE_SHARED->BakedStrings.StringDataSize
            );
        }

        LaneSync();

        r1u64 SlotIndexRange = LaneRange(BakeStrings->SlotsCount);

        for (
            u64 SlotIndex = SlotIndexRange.Min; 
            SlotIndex < SlotIndexRange.Max; 
            ++SlotIndex
        ) {
            for (
                FLIBakeStrChunkNode* Node = BakeStrings->Slots[SlotIndex].Head;
                Node;
                Node = Node->Next
            ) {
                for (u64 NodeIndex = 0; NodeIndex < Node->Count; ++NodeIndex) {
                    FLIBakeStr* Src = &Node->V[NodeIndex];
                    u64 DstIndex = BakeStrings->SlotsBaseIndices[SlotIndex] + Node->BaseIndex + NodeIndex + 1;
                    u64 DstOffset = FLI_BAKE_SHARED->BakedStrings.StringOffsets[DstIndex];

                    MemCpy(
                        FLI_BAKE_SHARED->BakedStrings.StringData + DstOffset,
                        Src->String.Str,
                        Src->String.Size
                    );
                }
            }
        }
    }

    LaneSync();

    {
        if (!LaneIndex()) {
            FLI_BAKE_SHARED->BakedIndexRuns.IndexRunsCount = BakeIndexRuns->SlotsBaseIndices[BakeIndexRuns->SlotsCount];
            FLI_BAKE_SHARED->BakedIndexRuns.IndexRuns = ArenaPushArrayZero(
                MemPool,
                u32,
                FLI_BAKE_SHARED->BakedIndexRuns.IndexRunsCount
            );
        }

        LaneSync();

        {
            r1u64 Range = LaneRange(BakeIndexRuns->SlotsCount);

            for (
                u64 SlotIndex = Range.Min; 
                SlotIndex < Range.Max; 
                ++SlotIndex
            ) {
                u64 Offset = BakeIndexRuns->SlotsBaseIndices[SlotIndex];

                for (
                    FLIBakeIndexRunChunkNode* Node = BakeIndexRuns->Slots[SlotIndex].Head;
                    Node;
                    Node = Node->Next
                ) {
                    STATIC_ASSERT(
                        sizeof(FLI_BAKE_SHARED->BakedIndexRuns.IndexRuns[0]) == sizeof(Node->V[0].Indices[0]), 
                        IndexRunSizeCheck
                    );

                    for (
                        u64 NodeIndex = 0; 
                        NodeIndex < Node->Count; 
                        ++NodeIndex
                    ) {
                        MemCpy(
                            FLI_BAKE_SHARED->BakedIndexRuns.IndexRuns + Offset,
                            Node->V[NodeIndex].Indices,
                            sizeof(Node->V[NodeIndex].Indices[0]) * Node->V[NodeIndex].Count
                        );
                        Offset += Node->V[NodeIndex].Count;
                    }
                }
            }
        }
    }

    LaneSync();

    if (!LaneIndex()) {
        for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
            FLI_BAKE_SHARED->LaneNameMapNodeCounts[K] = ArenaPushArrayZero(
                MemPool,
                u64,
                LaneCount()
            );
            FLI_BAKE_SHARED->LaneNameMapNodeOffs[K] = ArenaPushArrayZero(
                MemPool,
                u64,
                LaneCount()
            );
        }
    }

    LaneSync();

    for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
        if (!NameMapsNeedBuild[K])
            continue;

        FLIBakeNameMapTopology* Top = &BakeNameMapsTopologies[K];
        FLIBakeNameMap* Map = BakeNameMaps[K];
        r1u64 Range = LaneRange(Top->SlotsCount);

        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            if (Map->Slots[Index]) {
                u64 TotalUniqueNameCount = 0;
                u64 LastHash = 0;

                for (
                    FLIBakeNameChunkNode* Node = Map->Slots[Index]->Head;
                    Node;
                    Node = Node->Next
                ) {
                    for (u64 NodeIndex = 0; NodeIndex < Node->Count; ++NodeIndex) {
                        if (Node->V[NodeIndex].NameHash != LastHash) {
                            ++TotalUniqueNameCount;
                            LastHash = Node->V[NodeIndex].NameHash;
                        }
                    }
                }

                FLI_BAKE_SHARED->LaneNameMapNodeCounts[K][LaneIndex()] += TotalUniqueNameCount;
            }
        }
    }

    LaneSync();

    if (!LaneIndex()) {
        for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
            u64 NodeOffset = 0;

            for (u64 LIndex = 0; LIndex < LaneCount(); ++LIndex) {
                FLI_BAKE_SHARED->NameMapNodeCounts[K] += FLI_BAKE_SHARED->LaneNameMapNodeCounts[K][LIndex];
                FLI_BAKE_SHARED->LaneNameMapNodeOffs[K][LIndex] = NodeOffset;
                NodeOffset += FLI_BAKE_SHARED->LaneNameMapNodeCounts[K][LIndex];
            }

            FLI_BAKE_SHARED->TotalNameMapNodeCount += FLI_BAKE_SHARED->NameMapNodeCounts[K];
        }
    }

    LaneSync();

    if (LaneIndex() == LaneFromTaskIndex(0)) {
        FLI_BAKE_SHARED->BakedNameMaps.NameMapsCount = FLI_NAME_MAP_KIND_COUNT;
        FLI_BAKE_SHARED->BakedNameMaps.NameMaps = ArenaPushArrayZero(
            MemPool,
            FLINameMap,
            FLI_BAKE_SHARED->BakedNameMaps.NameMapsCount
        );

        u32 BucketOffset = 0;
        u32 NodeOffset = 0;

        for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
            FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K].BucketBaseIndex = BucketOffset;
            FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K].NodeBaseIndex = NodeOffset;
            FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K].BucketCount = SafeTruncateU64(
                BakeNameMapsTopologies[K].SlotsCount
            );
            FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K].NodeCount = SafeTruncateU64(
                FLI_BAKE_SHARED->NameMapNodeCounts[K]
            );
            BucketOffset += FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K].BucketCount;
            NodeOffset += FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K].NodeCount;
        }

        FLI_BAKE_SHARED->BakedNameMaps.BucketsCount = BucketOffset;
        FLI_BAKE_SHARED->BakedNameMaps.Buckets = ArenaPushArrayZero(
            MemPool,
            FLINameMapBucket,
            FLI_BAKE_SHARED->BakedNameMaps.BucketsCount
        );
    }

    if (LaneIndex() == LaneFromTaskIndex(1)) {
        FLI_BAKE_SHARED->BakedNameMaps.NodesCount = FLI_BAKE_SHARED->TotalNameMapNodeCount;
        FLI_BAKE_SHARED->BakedNameMaps.Nodes = ArenaPushArrayZero(
            MemPool,
            FLINameMapNode,
            FLI_BAKE_SHARED->BakedNameMaps.NodesCount
        );
    }

    LaneSync();

    for (FLINameMapKind K = 0; K < FLI_NAME_MAP_KIND_COUNT; ++K) {
        if (!NameMapsNeedBuild[K])
            continue;

        u64 WriteNodeOffset = FLI_BAKE_SHARED->LaneNameMapNodeOffs[K][LaneIndex()];
        FLIBakeNameMapTopology* Top = &BakeNameMapsTopologies[K];
        u64 SlotsCount = Top->SlotsCount;
        FLIBakeNameMap* SrcMap = BakeNameMaps[K];
        FLINameMap* DstMap = &FLI_BAKE_SHARED->BakedNameMaps.NameMaps[K];
        FLINameMapBucket* DstBuckets = FLI_BAKE_SHARED->BakedNameMaps.Buckets + DstMap->BucketBaseIndex;
        FLINameMapNode* DstNodes = FLI_BAKE_SHARED->BakedNameMaps.Nodes + DstMap->NodeBaseIndex;
        r1u64 SlotRange = LaneRange(SlotsCount);

        for (
            u64 SlotIndex = SlotRange.Min; 
            SlotIndex < SlotRange.Max; 
            ++SlotIndex
        ) {
            FLIBakeNameChunkList* SrcSlot = SrcMap->Slots[SlotIndex];

            if (!SrcSlot)
                continue;

            FLINameMapBucket* DstBucket = &DstBuckets[SlotIndex];

            DstBucket->FirstNode = WriteNodeOffset;

            {
                TempArena Scratch = GetScratch(&MemPool, 1);

                struct IndexRunNode {
                    IndexRunNode* Next;
                    u64           Index;
                };

                IndexRunNode* HeadIndexRunNode = NULL;
                IndexRunNode* TailIndexRunNode = NULL;
                u64 ActiveIndexCount = 0;
                Str8 ActiveString = {};
                FLIBakeNameChunkNode* Node = SrcSlot->Head;
                u64 NodeIndex = 0;

                for (;;) {
                    if (Node && NodeIndex >= Node->Count) {
                        Node = Node->Next;
                        NodeIndex = 0;
                    }

                    u64 I = 0;
                    Str8 String = {};

                    if (Node) {
                        I = Node->V[NodeIndex].Index;
                        String = Node->V[NodeIndex].String;
                    }

                    if (!StrMatch(ActiveString, String, 0)) {
                        if (ActiveString.Size) {
                            u64 IndicesCount = ActiveIndexCount;
                            u32* Indices = ArenaPushArrayZero(
                                Scratch.MemPool,
                                u32,
                                IndicesCount
                            );
                            u64 WriteIndex = 0;

                            for (
                                IndexRunNode* RunNode = HeadIndexRunNode;
                                RunNode;
                                RunNode = RunNode->Next
                            ) {
                                Indices[WriteIndex] = SafeTruncateU64(
                                    RunNode->Index
                                );
                                ++WriteIndex;
                            }

                            FLINameMapNode* DstNode = &DstNodes[WriteNodeOffset];

                            DstNode->StringIndex = FLIBakeIndexFromStr(BakeStrings, ActiveString);
                            DstNode->MatchCount = IndicesCount;

                            if (DstNode->MatchCount == 1) {
                                DstNode->MatchIndexOrIndexRunFirst = Indices[0];
                            } else if (DstNode->MatchCount > 1) {
                                DstNode->MatchIndexOrIndexRunFirst = FLIBakeIndexFromIndexRun(
                                    BakeIndexRuns,
                                    Indices,
                                    IndicesCount
                                );
                            }

                            ++DstBucket->NodeCount;
                            ++WriteNodeOffset;
                        }

                        ActiveString = String;
                        HeadIndexRunNode = NULL;
                        TailIndexRunNode = NULL;
                        ActiveIndexCount = 0;
                        ArenaEndTemp(Scratch);
                    }

                    if (ActiveString.Size && StrMatch(ActiveString, String, 0)) {
                        IndexRunNode* RunNode = ArenaPushArrayZero(
                            Scratch.MemPool,
                            IndexRunNode,
                            1
                        );

                        RunNode->Index = I;
                        SLL_QUEUE_PUSH(HeadIndexRunNode, TailIndexRunNode, RunNode);
                        ++ActiveIndexCount;
                    }

                    ++NodeIndex;

                    if (!Node)
                        break;
                }

                ReleaseScratch(Scratch);
            }
        }
    }

    LaneSync();

    {
        if (!LaneIndex()) {
            FLIBakeFileInfo* Src = &Params->FileInfo;
            FLIFileInfo* Dst = ArenaPushArrayZero(MemPool, FLIFileInfo, 1);

            Dst->SourcePathStringIndex = FLIBakeIndexFromStr(
                BakeStrings, 
                Src->SourcePath
            );
            Dst->VehicleStringIndex = FLIBakeIndexFromStr(
                BakeStrings, 
                Src->Vehicle
            );
            Dst->FirmwareStringIndex = FLIBakeIndexFromStr(
                BakeStrings, 
                Src->Firmware
            );
            Dst->SourceMD5 = Src->SourceMD5;
            Dst->StartTimeUS = Src->StartTimeUS;
            Dst->EndTimeUS = Src->EndTimeUS;
            Dst->RecordCount = Src->RecordCount;
            Dst->SkippedRecordCount = Src->SkippedRecordCount;
            FLI_BAKE_SHARED->BakedFileInfo.FileInfo = Dst;
            FLI_BAKE_SHARED->BakedFileInfo.FileInfoCount = 1;
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalMessageTypes = Params->MessageTypes.TotalCount + 1;

            FLI_BAKE_SHARED->BakedMessageTypes.MessageTypesCount = TotalMessageTypes;
            FLI_BAKE_SHARED->BakedMessageTypes.MessageTypes = ArenaPushArrayZero(
                MemPool,
                FLIMessageType,
                TotalMessageTypes
            );
        }

        LaneSync();

        for (
            FLIBakeMessageTypeChunkNode* Node = Params->MessageTypes.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeMessageType* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIMessageType* Dst = &FLI_BAKE_SHARED->BakedMessageTypes.MessageTypes[DstIndex];

                Dst->ID = Src->ID;
                Dst->Length = Src->Length;
                Dst->NameStringIndex = FLIBakeIndexFromStr(
                    BakeStrings, 
                    Src->Name
                );
                Dst->FormatStringIndex = FLIBakeIndexFromStr(
                    BakeStrings, 
                    Src->Format
                );
                Dst->HeadField = SafeTruncateU64(
                    FLIIndexFromBakeField(
                        Src->HeadField
                    )
                );
                Dst->FieldCount = Src->FieldCount;
                Dst->InstanceField = Src->InstanceField;
                Dst->TimeColumn = SafeTruncateU64(
                    FLIIndexFromBakeColumn(
                        Src->TimeColumn
                    )
                );
                Dst->HeadColumn = SafeTruncateU64(
                    FLIIndexFromBakeColumn(
                        Src->HeadColumn
                    )
                );
                Dst->ColumnCount = Src->ColumnCount;
                Dst->RecordCount = Src->RecordCount;
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalFields = Params->Fields.TotalCount + 1;

            FLI_BAKE_SHARED->BakedFields.FieldsCount = TotalFields;
            FLI_BAKE_SHARED->BakedFields.Fields = ArenaPushArrayZero(
                MemPool,
                FLIField,
                TotalFields
            );
        }

        LaneSync();

        for (
            FLIBakeFieldChunkNode* Node = Params->Fields.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeField* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIField* Dst = &FLI_BAKE_SHARED->BakedFields.Fields[DstIndex];

                Dst->NameStringIndex = FLIBakeIndexFromStr(
                    BakeStrings, 
                    Src->Name
                );
                Dst->UnitStringIndex = FLIBakeIndexFromStr(
                    BakeStrings, 
                    Src->Unit
                );
                Dst->FormatChar = Src->FormatChar;
                Dst->LoggedKind = Src->LoggedKind;
                Dst->StoredKind = Src->StoredKind;
                Dst->LoggedSize = Src->LoggedSize;
                Dst->ColumnIndex = SafeTruncateU64(
                    FLIIndexFromBakeColumn(
                        Src->Column
                    )
                );
                Dst->Multiplier = Src->Multiplier;
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalColumns = Params->Columns.TotalCount + 1;

            FLI_BAKE_SHARED->BakedColumns.ColumnsCount = TotalColumns;
            FLI_BAKE_SHARED->BakedColumns.Columns = ArenaPushArrayZero(
                MemPool,
                FLIColumn,
                TotalColumns
            );
            FLI_BAKE_SHARED->BakedColumns.ColumnData = ArenaPushArrayZero(
                MemPool,
                FLISerialisedSection,
                TotalColumns
            );
        }

        LaneSync();

        for (
            FLIBakeColumnChunkNode* Node = Params->Columns.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeColumn* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIColumn* Dst = &FLI_BAKE_SHARED->BakedColumns.Columns[DstIndex];
                FLISerialisedSection* DstData = &FLI_BAKE_SHARED->BakedColumns.ColumnData[DstIndex];

                Dst->MessageTypeIndex = SafeTruncateU64(
                    FLIIndexFromBakeMessageType(
                        Src->MessageType
                    )
                );
                Dst->FieldIndex = SafeTruncateU64(
                    FLIIndexFromBakeField(
                        Src->Field
                    )
                );
                Dst->SectionIndex = SafeTruncateU64(
                    FLI_SECTION_KIND_COUNT + DstIndex
                );
                Dst->Kind = Src->Kind;
                Dst->Count = Src->Count;
                Dst->Min = Src->Min;
                Dst->Max = Src->Max;
                Dst->Mean = Src->Mean;
                *DstData = FLISerialisedSectionMakeUnpacked(
                    Src->Data,
                    Src->Count * FLI_SCALAR_KIND_SIZE_TABLE[Src->Kind]
                );
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalParams = Params->Params.TotalCount + 1;

            FLI_BAKE_SHARED->BakedParams.ParamsCount = TotalParams;
            FLI_BAKE_SHARED->BakedParams.Params = ArenaPushArrayZero(
                MemPool,
                FLIParam,
                TotalParams
            );
        }

        LaneSync();

        for (
            FLIBakeParamChunkNode* Node = Params->Params.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeParam* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIParam* Dst = &FLI_BAKE_SHARED->BakedParams.Params[DstIndex];

                Dst->NameStringIndex = FLIBakeIndexFromStr(
                    BakeStrings, 
                    Src->Name
                );
                Dst->Value = Src->Value;
                Dst->Default = Src->Default;
                Dst->TimeUS = Src->TimeUS;
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalMessages = Params->Messages.TotalCount + 1;

            FLI_BAKE_SHARED->BakedMessages.MessagesCount = TotalMessages;
            FLI_BAKE_SHARED->BakedMessages.Messages = ArenaPushArrayZero(
                MemPool,
                FLIMessage,
                TotalMessages
            );
        }

        LaneSync();

        for (
            FLIBakeMessageChunkNode* Node = Params->Messages.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeMessage* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIMessage* Dst = &FLI_BAKE_SHARED->BakedMessages.Messages[DstIndex];

                Dst->TimeUS = Src->TimeUS;
                Dst->TextStringIndex = FLIBakeIndexFromStr(
                    BakeStrings, 
                    Src->Text
                );
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalModes = Params->Modes.TotalCount + 1;

            FLI_BAKE_SHARED->BakedModes.ModesCount = TotalModes;
            FLI_BAKE_SHARED->BakedModes.Modes = ArenaPushArrayZero(
                MemPool,
                FLIMode,
                TotalModes
            );
        }

        LaneSync();

        for (
            FLIBakeModeChunkNode* Node = Params->Modes.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeMode* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIMode* Dst = &FLI_BAKE_SHARED->BakedModes.Modes[DstIndex];

                Dst->TimeUS = Src->TimeUS;
                Dst->Mode = Src->Mode;
                Dst->ModeNum = Src->ModeNum;
                Dst->Reason = Src->Reason;
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalEvents = Params->Events.TotalCount + 1;

            FLI_BAKE_SHARED->BakedEvents.EventsCount = TotalEvents;
            FLI_BAKE_SHARED->BakedEvents.Events = ArenaPushArrayZero(
                MemPool,
                FLIEvt,
                TotalEvents
            );
        }

        LaneSync();

        for (
            FLIBakeEventChunkNode* Node = Params->Events.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeEvent* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIEvt* Dst = &FLI_BAKE_SHARED->BakedEvents.Events[DstIndex];

                Dst->TimeUS = Src->TimeUS;
                Dst->ID = Src->ID;
            }
        }

        LaneSync();
    }

    {
        if (!LaneIndex()) {
            u64 TotalErrors = Params->Errors.TotalCount + 1;

            FLI_BAKE_SHARED->BakedErrors.ErrorsCount = TotalErrors;
            FLI_BAKE_SHARED->BakedErrors.Errors = ArenaPushArrayZero(
                MemPool,
                FLIError,
                TotalErrors
            );
        }

        LaneSync();

        for (
            FLIBakeErrorChunkNode* Node = Params->Errors.Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 Range = LaneRange(Node->Count);

            for (u64 I = Range.Min; I < Range.Max; ++I) {
                FLIBakeError* Src = &Node->V[I];
                u64 DstIndex = Node->BaseIndex + I + 1;
                FLIError* Dst = &FLI_BAKE_SHARED->BakedErrors.Errors[DstIndex];

                Dst->TimeUS = Src->TimeUS;
                Dst->Subsystem = Src->Subsystem;
                Dst->Code = Src->Code;
            }
        }

        LaneSync();
    }

    FLIBakeResults Result = {};

    Result.FileInfo = FLI_BAKE_SHARED->BakedFileInfo;
    Result.Strings = FLI_BAKE_SHARED->BakedStrings;
    Result.MessageTypes = FLI_BAKE_SHARED->BakedMessageTypes;
    Result.Fields = FLI_BAKE_SHARED->BakedFields;
    Result.Columns = FLI_BAKE_SHARED->BakedColumns;
    Result.Params = FLI_BAKE_SHARED->BakedParams;
    Result.Messages = FLI_BAKE_SHARED->BakedMessages;
    Result.Modes = FLI_BAKE_SHARED->BakedModes;
    Result.Events = FLI_BAKE_SHARED->BakedEvents;
    Result.Errors = FLI_BAKE_SHARED->BakedErrors;
    Result.NameMaps = FLI_BAKE_SHARED->BakedNameMaps;
    Result.IndexRuns = FLI_BAKE_SHARED->BakedIndexRuns;

    return Result;
}

FLISerialisedSectionBundle 
FLICompress(Arena* MemPool, IN FLISerialisedSectionBundle* Bundle)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    FLISerialisedSectionBundle __Result = {};
    FLISerialisedSectionBundle* Result = &__Result;

    LaneSyncU64(&Bundle, 0);
    LaneSyncU64(&Result, 0);

    if (!LaneIndex()) {
        Result->ColumnSectionsCount = Bundle->ColumnSectionsCount;
        Result->ColumnSections = ArenaPushArrayZero(
            MemPool,
            FLISerialisedSection,
            Result->ColumnSectionsCount
        );
    }

    LaneSync();

    rr_lzb_simple_context Ctx = {};

    Ctx.m_tableSizeBits = 14;
    Ctx.m_hashTable = ArenaPushArrayZero(
        Scratch.MemPool,
        u16,
        1 << Ctx.m_tableSizeBits
    );

    u64 SectionsCount = FLI_SECTION_KIND_COUNT + Bundle->ColumnSectionsCount;
    r1u64 Range = LaneRange(SectionsCount);

    for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
        FLISerialisedSection* Src = (Index < FLI_SECTION_KIND_COUNT)
            ? &Bundle->Sections[Index]
            : &Bundle->ColumnSections[Index - FLI_SECTION_KIND_COUNT];
        FLISerialisedSection* Dst = (Index < FLI_SECTION_KIND_COUNT)
            ? &Result->Sections[Index]
            : &Result->ColumnSections[Index - FLI_SECTION_KIND_COUNT];

        MemCpy(Dst, Src, sizeof(*Dst));

        if (Src->EncodedSize) {
            MemSet(Ctx.m_hashTable, 0, sizeof(u16) * 1 << Ctx.m_tableSizeBits);
            Dst->Data = ArenaPushArray(MemPool, u8, Src->EncodedSize);
            Dst->EncodedSize = rr_lzb_simple_encode_veryfast(
                &Ctx, 
                Src->Data, 
                Src->EncodedSize, 
                Dst->Data
            );
            Dst->UnpackedSize = Src->EncodedSize;
            Dst->Encoding = FLI_SECTION_ENCODING_LZB;
        }
    }

    LaneSync();
    ReleaseScratch(Scratch);

    return *Result;
}

#if defined(NC_LANG_CPP)
    b32 
    operator==(FLIKey KeyA, FLIKey KeyB)
    {
        return MemCmpStruct(&KeyA, &KeyB);
    }

    b32 
    operator!=(FLIKey KeyA, FLIKey KeyB)
    {
        return !(KeyA == KeyB);
    }
#endif
