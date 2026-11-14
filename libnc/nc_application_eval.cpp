#include "nc_application_eval.h"
#include "nc_application.h"
#include "nc_file.h"

EVAL_TYPE_ACCESS_FUNC_DEF(Commands)
{
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };

    if (Expr->Kind == EVAL_EXPR_KIND_MEMBER_ACCESS) {
        Str8 CommandName = Expr->Head->Next->String;
        AppCommandKindInfo* CommandInfo = AppCommandKindInfoFromStr(
            CommandName
        );
        EvalTypeKey CommandType = EvalTypeKeyConstruct(
            __Params.Kind = EVAL_TYPE_KIND_U64,
            __Params.Name = "command"_s8
        );

        CommandType = EvalTypeKeyConstructMetaDescription(
            CommandType,
            CommandInfo->Description
        );
        Result.TypeKey = CommandType;
        Result.Mode = EVAL_MODE_VALUE;
        Result.Root = EvalIRTreeSetSpace(
            MemPool,
            EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_CMD),
            EvalIRTreeConstU(MemPool, EvalIDFromStr(CommandName))
        );
    }

    return Result;
}

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Commands)
{
    EvalTypeExpandInfo Result = {};

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        Str8List CommandNames = {};
        EvalType* __Type = EvalTypeFromKey(Eval.IRTree.TypeKey);

        for EACH_ENUM(AppCommandKind, K, APP_COMMAND_KIND_COUNT) {
            AppCommandKindInfo* Info = &APP_COMMAND_KIND_INFO_TABLE[K];

            if (
                Info->Flags & APP_COMMAND_KIND_FLAG_LIST_IN_UI &&
                (
                    !StrMatch(__Type->Name, "tab_commands"_s8, 0) ||
                    K >= APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD
                )
            ) {
                Str8 CodeName = Info->String;
                Str8 Description = Info->Description;
                Str8 SearchTags = Info->SearchTags;
                Str8 DisplayName = AppDisplayFromCodeName(CodeName);
                FMRangeList DescMatches = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    Description
                );
                FMRangeList NameMatches = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    DisplayName
                );
                FMRangeList TagMatches = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    SearchTags
                );
                b32 BindingMatchesOkay = FALSE;
                ConfigInputMapNodePtrList Bindings = ConfigInputMapNodePtrListFromName(
                    Scratch.MemPool,
                    APP_STATE->KeyMap,
                    CodeName
                );

                for (
                    ConfigInputMapNodePtr* Node = Bindings.Head;
                    Node;
                    Node = Node->Next
                ) {
                    Str8 BindingText = StrFromInputModifierInput(
                        Scratch.MemPool,
                        Node->V->Binding.ModKind,
                        Node->V->Binding.Input
                    );
                    FMRangeList Matches = FuzzyFind(
                        Scratch.MemPool,
                        Filter,
                        BindingText
                    );

                    if (Matches.Count == Matches.StrPartCount) {
                        BindingMatchesOkay = TRUE;
                        break;
                    }
                }

                if (
                    NameMatches.Count == NameMatches.StrPartCount ||
                    DescMatches.Count == DescMatches.StrPartCount ||
                    TagMatches.Count == TagMatches.StrPartCount ||
                    BindingMatchesOkay
                ) {
                    ListPush(Scratch.MemPool, &CommandNames, CodeName);
                }
            }
        }

        Str8Array* Accel = ArenaPushArrayZero(
            MemPool,
            Str8Array,
            1
        );

        *Accel = StrArrayFromList(MemPool, &CommandNames);
        Result.UserData = Accel;
        Result.ExprCount = Accel->Count;
        ReleaseScratch(Scratch);
    }

    return Result;
}

EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Commands)
{
    u64 I = 0;
    Str8Array* Accel = (Str8Array*) UserData;

    for (
        u64 Index = IndexRange.Min;
        Index < IndexRange.Max;
        ++Index, ++I
    ) {
        Str8 CommandName = Accel->Data[Index];
        EvalEval CommandEval = EvalEvalFromStrFmt(
            "query:commands.%S",
            PRINT_STR(CommandName)
        );

        Evals[I] = CommandEval;
    }
}

EVAL_TYPE_ACCESS_FUNC_DEF(Themes)
{
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };

    if (
        Expr->Kind == EVAL_EXPR_KIND_ARRAY_INDEX &&
        Expr->Head->Next->Kind == EVAL_EXPR_KIND_LEAF_STR_LIT
    ) {
        Str8 ThemeName = Expr->Head->Next->String;
        EvalTypeKey ThemeType = EvalTypeKeyConstruct(
            __Params.Kind = EVAL_TYPE_KIND_U64,
            __Params.Name = "theme"_s8
        );

        Result.TypeKey = ThemeType;
        Result.Mode = EVAL_MODE_VALUE;
        Result.Root = EvalIRTreeSetSpace(
            MemPool,
            EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_THEME),
            EvalIRTreeConstU(MemPool, EvalIDFromStr(ThemeName))
        );
    }

    return Result;
}

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Themes)
{
    EvalTypeExpandInfo Result = {};

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        Str8List Names = {};

        for EACH_ENUM(AppThemePreset, P, APP_THEME_PRESET_COUNT) {
            Str8 Name = APP_THEME_PRESET_DISPLAY_STR_TABLE[P];
            FMRangeList NameMatches = FuzzyFind(
                Scratch.MemPool,
                Filter,
                Name
            );

            if (NameMatches.Count == NameMatches.StrPartCount)
                ListPush(Scratch.MemPool, &Names, Name);
        }

        {
            Str8 ThemeFolder = ArenaPushStrFmt(
                Scratch.MemPool,
                "%S/%s/themes",
                PRINT_STR(GetProcessProperties()->UserProgramDataPath),
                NC_DATA_DIR
            );
            FileIter* Iter = SystemFileIterBegin(
                Scratch.MemPool,
                ThemeFolder,
                SYS_FILE_ITER_SKIP_DIRS
            );

            for (
                FileInfo Info = {};
                SystemFileIterNext(Scratch.MemPool, Iter, &Info);
            ) {
                Str8 Name = Info.Name;
                FMRangeList NameMatches = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    Name
                );

                if (NameMatches.Count == NameMatches.StrPartCount) {
                    ListPush(
                        Scratch.MemPool,
                        &Names,
                        ArenaPushStrCpy(MemPool, Name)
                    );
                }
            }

            SystemFileIterEnd(Iter);
        }

        Str8Array* Accel = ArenaPushArrayZero(
            MemPool,
            Str8Array,
            1
        );

        *Accel = StrArrayFromList(MemPool, &Names);
        Result.UserData = Accel;
        Result.ExprCount = Accel->Count;
        ReleaseScratch(Scratch);
    }

    return Result;
}

EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Themes)
{
    u64 I = 0;
    Str8Array* Accel = (Str8Array*) UserData;

    for (
        u64 Index = IndexRange.Min;
        Index < IndexRange.Max;
        ++Index, ++I
    ) {
        Str8 Name = Accel->Data[Index];

        Evals[I] = EvalEvalWrapFmt(
            Eval,
            "$[\"%S\"]",
            PRINT_STR(Name)
        );
    }
}

EVAL_TYPE_ACCESS_FUNC_DEF(Views)
{
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };

    if (
        Expr->Kind == EVAL_EXPR_KIND_ARRAY_INDEX &&
        Expr->Head->Next->Kind == EVAL_EXPR_KIND_LEAF_STR_LIT
    ) {
        TempArena Scratch = GetScratch(&MemPool, 1);
        Str8 ViewName = Expr->Head->Next->String;
        EvalTypeKey ViewType = EvalTypeKeyConstruct(
            __Params.Kind = EVAL_TYPE_KIND_U64,
            __Params.Name = "view"_s8
        );

        Result.TypeKey = ViewType;
        Result.Mode = EVAL_MODE_NULL;
        Result.Root = EvalIRTreeSetSpace(
            MemPool,
            EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_VIEW),
            EvalIRTreeConstU(MemPool, EvalIDFromStr(ViewName))
        );
        ReleaseScratch(Scratch);
    }

    return Result;
}

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Views)
{
    EvalTypeExpandInfo Result = {};

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        Str8List Names = {};

        for (
            u64 Index = 0;
            Index < ARRAY_COUNT(APP_NAME_SCHEMA_INFO_TABLE);
            ++Index
        ) {
            if (APP_NAME_SCHEMA_INFO_TABLE[Index].IsView) {
                Str8 Name = APP_NAME_SCHEMA_INFO_TABLE[Index].Name;
                FMRangeList NameMatches = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    Name
                );

                if (NameMatches.Count == NameMatches.StrPartCount)
                    ListPush(Scratch.MemPool, &Names, Name);
            }
        }

        Str8Array* Accel = ArenaPushArrayZero(
            MemPool,
            Str8Array,
            1
        );

        *Accel = StrArrayFromList(MemPool, &Names);
        Result.UserData = Accel;
        Result.ExprCount = Accel->Count;
        ReleaseScratch(Scratch);
    }

    return Result;
}

EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Views)
{
    u64 I = 0;
    Str8Array* Accel = (Str8Array*) UserData;

    for (
        u64 Index = IndexRange.Min;
        Index < IndexRange.Max;
        ++Index, ++I
    ) {
        Str8 Name = Accel->Data[Index];

        Evals[I] = EvalEvalFromStr(Name);
    }
}

EVAL_TYPE_IR_EXT_FUNC_DEF(Schema)
{
    AppSchemaIRExt* Ext = ArenaPushArrayZero(
        MemPool,
        AppSchemaIRExt,
        1
    );

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        EvalOpList OpList = EvalOpListFromIRTree(
            Scratch.MemPool, 
            IRTree->Root
        );
        Str8 ByteCode = EvalByteCodeFromOpList(Scratch.MemPool, &OpList);
        EvalInterp Interp = EvalInterpret(ByteCode);
        EvalTypeKey TypeKey = IRTree->TypeKey;
        EvalType* __Type = EvalTypeFromKey(TypeKey);

        Ext->Config = AppConfigFromEvalSpace(Interp.Space);
        Ext->Schemas = ConfigSchemasFromName(
            MemPool, 
            APP_STATE->CfgSchemaTable, 
            __Type->Name
        );
        ReleaseScratch(Scratch);
    }

    EvalIRExt Result = {
        Ext
    };

    return Result;
}

EVAL_TYPE_ACCESS_FUNC_DEF(Schema)
{
    AppSchemaIRExt* Ext = (AppSchemaIRExt*) LHSIRTree->UserData;
    EvalIRTreeAndType IRTree = {
        EMPTY_EVAL_IR_NODE_VALUE
    };

    if (Expr->Kind == EVAL_EXPR_KIND_MEMBER_ACCESS) {
        MDNode* ChildSchema = EMPTY_MD_NODE_VALUE;

        for (
            MDNodePtrNode* N = Ext->Schemas.Head;
            N;
            N = N->Next
        ) {
            for EACH_MDNODE(Child, N->V->Head) {
                if (StrMatch(Child->String, Expr->Head->Next->String, 0)) {
                    ChildSchema = Child;
                    break;
                }
            }
        }

        if (ChildSchema != EMPTY_MD_NODE_VALUE) {
            ConfigNode* Cfg = Ext->Config;
            ConfigNode* Child = ConfigNodeChildFromStr(
                Cfg, 
                ChildSchema->String
            );
            EvalTypeKey ChildTypeKey = {};
            b32 WrapChildWithMetaExpr = FALSE;
            b32 IsQueryChild = MDNodeHasTag(ChildSchema, "query"_s8, 0);
            EvalTypeFlag TypeFlags = (
                !!IsQueryChild * EVAL_TYPE_FLAG_IS_NOT_EDITABLE
            );
            b32 IsMetaEval = TRUE;
            EvalMode ChildMode = EVAL_MODE_OFFSET;
            Str8 TextValue = Child->Head->String;

            if (!TextValue.Size) {
                TextValue = MDTagFromStr(
                    ChildSchema, 
                    "default"_s8, 
                    0
                )->Head->String;
            }

            if (
                StrMatch(ChildSchema->Head->String, "code_string"_s8, 0) ||
                StrMatch(ChildSchema->Head->String, "expr_string"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyConstructArray(
                    EvalTypeKeyBasic(EVAL_TYPE_KIND_U8),
                    TextValue.Size,
                    TypeFlags
                );
            } else if (
                StrMatch(ChildSchema->Head->String, "path"_s8, 0) ||
                StrMatch(ChildSchema->Head->String, "path_pt"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyConstructArray(
                    EvalTypeKeyBasic(EVAL_TYPE_KIND_U8),
                    TextValue.Size,
                    TypeFlags | EVAL_TYPE_FLAG_IS_PATH_TEXT
                );
            } else if (
                StrMatch(ChildSchema->Head->String, "string"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyConstructArray(
                    EvalTypeKeyBasic(EVAL_TYPE_KIND_U8),
                    TextValue.Size,
                    TypeFlags | EVAL_TYPE_FLAG_IS_PLAIN_TEXT
                );
            } else if (
                StrMatch(ChildSchema->Head->String, "u64"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_U64);
                WrapChildWithMetaExpr = TRUE;
            } else if (
                StrMatch(ChildSchema->Head->String, "u32"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_U32);
                WrapChildWithMetaExpr = TRUE;
            } else if (
                StrMatch(ChildSchema->Head->String, "f32"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_F32);
                WrapChildWithMetaExpr = TRUE;
            } else if (
                StrMatch(ChildSchema->Head->String, "bool"_s8, 0)
            ) {
                ChildTypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_BOOL);
                WrapChildWithMetaExpr = TRUE;
            } else if (
                StrMatch(ChildSchema->Head->String, "set"_s8, 0)
            ) {
                ChildTypeKey = EvalStr2TypeKeyMapLookup(
                    APP_STATE->MetaName2TypeMap,
                    ChildSchema->String
                );
            }

            if (WrapChildWithMetaExpr) {
                TempArena Scratch = GetScratch(&MemPool, 1);
                EvalExpr* ChildExpr = EvalParseFromStr(
                    Child->Head->String
                ).Expr;
                b32 ExprIsSimple = FALSE;

                if (
                    ChildExpr->Kind == EVAL_EXPR_KIND_LEAF_U64 ||
                    ChildExpr->Kind == EVAL_EXPR_KIND_LEAF_F64 ||
                    ChildExpr->Kind == EVAL_EXPR_KIND_LEAF_F32
                ) {
                    ExprIsSimple = TRUE;
                }

                if (
                    (
                        ChildExpr->Kind == EVAL_EXPR_KIND_POS || 
                        ChildExpr->Kind == EVAL_EXPR_KIND_NEG
                    ) &&
                    ChildExpr->Head == ChildExpr->Tail &&
                    (
                        ChildExpr->Head->Kind == EVAL_EXPR_KIND_LEAF_U64 ||
                        ChildExpr->Head->Kind == EVAL_EXPR_KIND_LEAF_F64 ||
                        ChildExpr->Head->Kind == EVAL_EXPR_KIND_LEAF_F32
                    )
                ) {
                    ExprIsSimple = TRUE;
                }

                if (
                    ChildExpr->Kind == EVAL_EXPR_KIND_LEAF_IDENT &&
                    (
                        StrMatch(ChildExpr->String, "true"_s8, 0) ||
                        StrMatch(ChildExpr->String, "false"_s8, 0)
                    )
                ) {
                    ExprIsSimple = TRUE;
                }

                if (!ExprIsSimple && ChildExpr != EMPTY_EVAL_EXPR_VALUE) {
                    ChildTypeKey = EvalTypeKeyConstructMetaExpr(
                        ChildTypeKey,
                        Child->Head->String
                    );
                }

                ReleaseScratch(Scratch);
            }

            {
                MDNode* DisplayName = MDTagFromStr(
                    ChildSchema, 
                    "display_name"_s8, 
                    0
                );
                MDNode* Description = MDTagFromStr(
                    ChildSchema, 
                    "description"_s8, 
                    0
                );

                if (!MDNodeIsEmpty(DisplayName)) {
                    ChildTypeKey = EvalTypeKeyConstructMetaDisplayName(
                        ChildTypeKey,
                        DisplayName->Head->String
                    );
                }

                if (!MDNodeIsEmpty(Description)) {
                    ChildTypeKey = EvalTypeKeyConstructMetaDescription(
                        ChildTypeKey,
                        Description->Head->String
                    );
                }
            }

            {
                MDNode* Hex = MDTagFromStr(ChildSchema->Head, "hex"_s8, 0);

                if (!MDNodeIsEmpty(Hex)) {
                    ChildTypeKey = EvalTypeKeyConstruct(
                        __Params.Kind = EVAL_TYPE_KIND_LENS,
                        __Params.Name = "hex"_s8,
                        __Params.DirectKey = ChildTypeKey
                    );
                }
            }

            {
                MDNode* Colour = MDTagFromStr(
                    ChildSchema->Head, 
                    "colour"_s8, 
                    0
                );

                if (!MDNodeIsEmpty(Colour)) {
                    ChildTypeKey = EvalTypeKeyConstruct(
                        __Params.Kind = EVAL_TYPE_KIND_LENS,
                        __Params.Name = "colour"_s8,
                        __Params.DirectKey = ChildTypeKey
                    );
                }
            }

            {
                MDNode* Range = MDTagFromStr(
                    ChildSchema->Head, 
                    "range"_s8, 
                    0
                );

                if (!MDNodeIsEmpty(Range)) {
                    EvalExpr* MinBound = EvalParseFromStr(
                        Range->Head->String
                    ).Expr;
                    EvalExpr* MaxBound = EvalParseFromStr(
                        Range->Head->Next->String
                    ).Expr;
                    EvalExpr* Args[] = {
                        MinBound,
                        MaxBound
                    };

                    ChildTypeKey = EvalTypeKeyConstruct(
                        __Params.Kind = EVAL_TYPE_KIND_LENS,
                        __Params.Name = "range1"_s8,
                        __Params.DirectKey = ChildTypeKey,
                        __Params.Count = 2,
                        __Params.Args = Args
                    );
                }
            }

            EvalSpace ChildEvalSpace = {};

            if (Cfg != EMPTY_CFG_NODE_VALUE) {
                ChildEvalSpace = EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_CFG);
                ChildEvalSpace.U64s[0] = Cfg->ID;
                ChildEvalSpace.U64s[1]  = EvalIDFromStr(ChildSchema->String);
            }

            IRTree.Root = EvalIRTreeSetSpace(
                MemPool, 
                ChildEvalSpace, 
                EvalPushIRNode(
                    MemPool, 
                    EVAL_EVAL_OP_CONST_U64
                )
            );
            IRTree.TypeKey = ChildTypeKey;
            IRTree.Mode = ChildMode;
        }
    }

    return IRTree;
}


EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Schema)
{
    EvalTypeExpandInfo Result = {};

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        AppSchemaIRExt* Ext = (AppSchemaIRExt*) Eval.IRTree.UserData;
        Str8Array Commands = {};

        {
            Str8List CommandsList = {};

            for (
                MDNodePtrNode* N = Ext->Schemas.Head;
                N;
                N = N->Next
            ) {
                MDNode* Schema = N->V;
                MDNode* Tag = MDTagFromStr(Schema, "expand_commands"_s8, 0);

                for EACH_MDNODE(Arg, Tag->Head) {
                    b32 Filtered = FALSE;

                    if (MDNodeHasTag(Arg, "output"_s8, 0)) {
                        Str8 Expr = AppExpressionFromConfig(Ext->Config);

                        Filtered = !StrMatch(Expr, "query:output"_s8, 0);
                    }

                    if (!Filtered) {
                        AppCommandKindInfo* CommandKindInfo = AppCommandKindInfoFromStr(
                            Arg->String
                        );
                        FMRangeList NameMatches = FuzzyFind(
                            Scratch.MemPool, 
                            Filter, 
                            AppDisplayFromCodeName(CommandKindInfo->String)
                        );
                        FMRangeList DescMatches = FuzzyFind(
                            Scratch.MemPool, 
                            Filter, 
                            CommandKindInfo->Description
                        );
                        FMRangeList TagsMatches = FuzzyFind(
                            Scratch.MemPool, 
                            Filter, 
                            CommandKindInfo->SearchTags
                        );

                        if (
                            NameMatches.Count == NameMatches.StrPartCount ||
                            DescMatches.Count == DescMatches.StrPartCount ||
                            TagsMatches.Count == TagsMatches.StrPartCount
                        ) {
                            ListPush(
                                Scratch.MemPool, 
                                &CommandsList, 
                                Arg->String
                            );
                        }
                    }
                }
            }

            Commands = StrArrayFromList(MemPool, &CommandsList);
        }

        struct ExpandChildNode {
            ExpandChildNode* Next;
            MDNode*          N;
        };

        ExpandChildNode* HeadChildNode = NULL;
        ExpandChildNode* TailChildNode = NULL;
        u64 ChildCount = 0;

        for (
            MDNodePtrNode* N = Ext->Schemas.Head;
            N;
            N = N->Next
        ) {
            MDNode* Schema = N->V;

            for EACH_MDNODE(Child, Schema->Head) {
                if (!MDNodeHasTag(Child, "no_expand"_s8, 0)) {
                    MDNode* ExpandCheck = MDTagFromStr(
                        Child, 
                        "expand_if"_s8, 
                        0
                    );
                    b32 ExpandThisChild = TRUE;

                    if (!MDNodeIsEmpty(ExpandCheck)) {
                        EvalParentKey(Eval.Key) {
                            ExpandThisChild = !!EvalValueFromStr(
                                ExpandCheck->Head->String
                            ).U64;
                        }
                    }

                    if (ExpandThisChild) {
                        Str8 DisplayName = MDTagFromStr(
                            Child, 
                            "display_name"_s8, 
                            0
                        )->Head->String;

                        if (!DisplayName.Size) {
                            DisplayName = AppDisplayFromCodeName(
                                Child->String
                            );
                        }

                        Str8 Desc = MDTagFromStr(
                            Child, 
                            "description"_s8, 
                            0
                        )->Head->String;
                        FMRangeList NameMatches = FuzzyFind(
                            Scratch.MemPool, 
                            Filter, 
                            Child->String
                        );
                        FMRangeList DisplayNameMatches = FuzzyFind(
                            Scratch.MemPool, 
                            Filter, 
                            DisplayName
                        );
                        FMRangeList DescMatches = FuzzyFind(
                            Scratch.MemPool, 
                            Filter, 
                            Desc
                        );

                        if (
                            NameMatches.Count == NameMatches.StrPartCount ||
                            DisplayNameMatches.Count == DisplayNameMatches.StrPartCount ||
                            DescMatches.Count == DescMatches.StrPartCount
                        ) {
                            ExpandChildNode* N = ArenaPushArrayZero(
                                Scratch.MemPool,
                                ExpandChildNode,
                                1
                            );

                            N->N = Child;
                            SLL_QUEUE_PUSH(HeadChildNode, TailChildNode, N);
                            ++ChildCount;
                        }
                    }
                }
            }
        }

        MDNode** Children = ArenaPushArray(MemPool, MDNode*, ChildCount);

        {
            u64 Index = 0;

            for (
                ExpandChildNode* N = HeadChildNode;
                N;
                N = N->Next, ++Index
            ) {
                Children[Index] = N->N;
            }
        }

        AppSchemaExpandAccel* Accel = ArenaPushArrayZero(
            MemPool,
            AppSchemaExpandAccel,
            1
        );

        Accel->Commands = Commands;
        Accel->Children = Children;
        Accel->ChildrenCount = ChildCount;
        Result.UserData = Accel;
        Result.ExprCount = ChildCount + Commands.Count;
        ReleaseScratch(Scratch);
    }

    return Result;
}


EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Schema)
{
    AppSchemaExpandAccel* Accel = (AppSchemaExpandAccel*) UserData;
    r1u64 CommandsIndexRange = Rng(0ULL, Accel->Commands.Count);
    r1u64 ChildIndexRange = Rng(
        CommandsIndexRange.Max,
        CommandsIndexRange.Max + Accel->ChildrenCount
    );
    u64 I = 0;

    {
        r1u64 ReadRange = Intersect(
            IndexRange,
            CommandsIndexRange
        );

        for (
            u64 Index = ReadRange.Min;
            Index < ReadRange.Max;
            ++Index, ++I
        ) {
            Evals[I] = EvalEvalFromStrFmt(
                "query:commands.%S",
                PRINT_STR(Accel->Commands.Data[Index - CommandsIndexRange.Min])
            );
        }
    }

    {
        r1u64 ReadRange = Intersect(
            IndexRange,
            ChildIndexRange
        );

        for (
            u64 Index = ReadRange.Min;
            Index < ReadRange.Max;
            ++Index, ++I
        ) {
            MDNode* ChildSchema = Accel->Children[Index - ChildIndexRange.Min];
            Evals[I] = EvalEvalWrapFmt(
                Eval,
                "$.%S",
                PRINT_STR(ChildSchema->String)
            );
        }
    }
}

EVAL_TYPE_ACCESS_FUNC_DEF(Configs)
{
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };
    EvalExpr* RHS = Expr->Head->Next;

    if (
        RHS->Kind == EVAL_EXPR_KIND_LEAF_IDENT &&
        StrMatch(StrPrefix(RHS->String, 1), "$"_s8, 0)
    ) {
        Str8 NumericPart = StrSkip(RHS->String, 1);
        ConfigID ID = U64FromStr(NumericPart, 16);
        ConfigNode* Cfg = ConfigNodeFromID(ID);
        EvalSpace Space = AppEvalSpaceFromConfig(Cfg);

        Result.Root = EvalIRTreeSetSpace(
            MemPool, 
            Space, 
            EvalIRTreeConstU(MemPool, 0)
        );
        Result.TypeKey = EvalStr2TypeKeyMapLookup(
            APP_STATE->MetaName2TypeMap, 
            Cfg->String
        );
        Result.Mode = EVAL_MODE_OFFSET;
    }

    return Result;
}

EVAL_TYPE_IR_EXT_FUNC_DEF(ConfigsSlice)
{
    AppConfigsIRExt* Ext = ArenaPushArrayZero(
        MemPool,
        AppConfigsIRExt,
        1
    );

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        EvalTypeKey TypeKey = IRTree->TypeKey;
        EvalType* __Type = EvalTypeFromKey(TypeKey);
        Str8 ConfigName = AppConfigNameFromCollectionName(__Type->Name);
        ConfigNodePtrList ConfigsList = {};

        {
            ConfigNodePtrList ConfigsListAll = ConfigNodeTopLevelListFromStr(
                Scratch.MemPool,
                ConfigName
            );

            for (
                ConfigNodePtrNode* N = ConfigsListAll.Head;
                N;
                N = N->Next
            ) {
                if (AppIsProjectFiltered(N->V))
                    continue;

                ListPush(
                    Scratch.MemPool,
                    &ConfigsList,
                    N->V
                );
            }
        }

        Str8List CommandsList = {};

        {
            MDNodePtrList Schemas = ConfigSchemasFromName(
                Scratch.MemPool,
                APP_STATE->CfgSchemaTable,
                ConfigName
            );

            for (
                MDNodePtrNode* N = Schemas.Head;
                N;
                N = N->Next
            ) {
                MDNode* Schema = N->V;
                MDNode* CollectionCommandsRoot = MDTagFromStr(
                    Schema, 
                    "collection_commands"_s8, 
                    0
                );

                for EACH_MDNODE(Command, CollectionCommandsRoot->Head) {
                    ListPush(
                        MemPool,
                        &CommandsList,
                        Command->String
                    );
                }
            }
        }

        Ext->ConfigName = ConfigName;
        Ext->Configs = ConfigNodePtrArrayFromList(MemPool, &ConfigsList);
        Ext->Commands = StrArrayFromList(MemPool, &CommandsList);
        ReleaseScratch(Scratch);
    }

    EvalIRExt Result = {
        Ext
    };

    return Result;
}

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(ConfigsQuery)
{
    AppConfigsExpandAccel* Accel = ArenaPushArrayZero(
        MemPool,
        AppConfigsExpandAccel,
        1
    );

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        ConfigNode* RootConfig = AppConfigFromEvalSpace(Eval.Space);
        Str8 ChildKey = EvalStrFromID(Eval.Space.U64s[1]);
        Str8 ChildKeySingular = AppSingularFromCodeNamePlural(ChildKey);

        if (ChildKeySingular.Size)
            ChildKey = ChildKeySingular;

        Str8List Commands = {};
        MDNodePtrList Schemas = ConfigSchemasFromName(
            Scratch.MemPool,
            APP_STATE->CfgSchemaTable,
            ChildKey
        );

        for (
            MDNodePtrNode* N = Schemas.Head;
            N;
            N = N->Next
        ) {
            MDNode* Schema = N->V;
            MDNode* CollectionCommandsRoot = MDTagFromStr(
                Schema, 
                "collection_commands"_s8, 
                0
            );

            for EACH_MDNODE(Command, CollectionCommandsRoot->Head) {
                ListPush(
                    Scratch.MemPool,
                    &Commands,
                    Command->String
                );
            }
        }

        ConfigNodePtrList Children = ConfigNodeChildListFromStr(
            Scratch.MemPool,
            RootConfig,
            ChildKey
        );
        ConfigNodePtrList ChildrenFiltered = Children;

        if (Filter.Size) {
            MemZero(&ChildrenFiltered, sizeof(ChildrenFiltered));

            for (
                ConfigNodePtrNode* N = Children.Head;
                N;
                N = N->Next
            ) {
                FancyStrList ConfigFancyStrings = AppTitleFStrFromConfig(
                    Scratch.MemPool,
                    N->V,
                    TRUE
                );
                Str8 ConfigString = Str8FromFancyStrList(
                    Scratch.MemPool,
                    &ConfigFancyStrings
                );
                FMRangeList Ranges = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    ConfigString
                );

                if (Ranges.Count == Ranges.StrPartCount) {
                    ListPush(
                        Scratch.MemPool,
                        &ChildrenFiltered,
                        N->V
                    );
                }
            }
        }

        Accel->Commands = StrArrayFromList(MemPool, &Commands);
        Accel->CommandsIndexRange = Rng(0ULL, Accel->Commands.Count);
        Accel->Configs = ConfigNodePtrArrayFromList(
            MemPool,
            &ChildrenFiltered
        );
        Accel->ConfigsIndexRange = Rng(
            Accel->Commands.Count,
            Accel->Commands.Count + Accel->Configs.Count
        );
        ReleaseScratch(Scratch);
    }

    EvalTypeExpandInfo Result = {
        Accel,
        Accel->Configs.Count + Accel->Commands.Count
    };

    return Result;
}

EVAL_TYPE_ACCESS_FUNC_DEF(ConfigsSlice)
{
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };
    ConfigNode* Config = EMPTY_CFG_NODE_VALUE;
    AppConfigsIRExt* Ext = (AppConfigsIRExt*) LHSIRTree->UserData;

    switch (Expr->Kind) {
        default: {} break;

        case EVAL_EXPR_KIND_ARRAY_INDEX: {
            EvalValue RHSValue = EvalValueFromExpr(Expr->Head->Next);
            u64 RHSIndex = RHSValue.U64;

            if (RHSIndex >= 0 && RHSIndex < Ext->Configs.Count)
                Config = Ext->Configs.V[RHSIndex];
        } break;

        case EVAL_EXPR_KIND_MEMBER_ACCESS: {
            Str8 RHSName = Expr->Head->Next->String;
            ConfigID ID = 0;

            if (
                StrMatch(
                    StrPrefix(RHSName, 1), 
                    "$"_s8, 
                    0
                )
            ) {
                ID = U64FromStr(StrSkip(RHSName, 1), 16);
                Config = ConfigNodeFromID(ID);
            }
        } break;
    }

    if (Config != EMPTY_CFG_NODE_VALUE) {
        Result.Root = EvalIRTreeSetSpace(
            MemPool,
            AppEvalSpaceFromConfig(Config),
            EvalIRTreeConstU(MemPool, 0)
        );
        Result.Mode = EVAL_MODE_OFFSET;
        Result.TypeKey = EvalStr2TypeKeyMapLookup(
            APP_STATE->MetaName2TypeMap,
            Ext->ConfigName
        );
    }

    return Result;
}

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(ConfigsSlice)
{
    AppConfigsExpandAccel* Accel = ArenaPushArrayZero(
        MemPool,
        AppConfigsExpandAccel,
        1
    );
    EvalTypeExpandInfo Result = {
        Accel
    };
    TempArena Scratch = GetScratch(&MemPool, 1);

    {
        AppConfigsIRExt* Ext = (AppConfigsIRExt*) Eval.IRTree.UserData;
        ConfigNodePtrArray ConfigsFiltered = Ext->Configs;

        if (Filter.Size) {
            ConfigNodePtrList ConfigsListFiltered = {};

            for (
                u64 Index = 0;
                Index < Ext->Configs.Count;
                ++Index
            ) {
                ConfigNode* Config = Ext->Configs.V[Index];

                if (AppIsProjectFiltered(Config))
                    continue;

                FancyStrList FancyStrings = AppTitleFStrFromConfig(
                    Scratch.MemPool,
                    Config,
                    TRUE
                );
                Str8 String = Str8FromFancyStrList(
                    Scratch.MemPool, 
                    &FancyStrings
                );
                FMRangeList FuzzyMatches = FuzzyFind(
                    Scratch.MemPool,
                    Filter,
                    String
                );

                if (FuzzyMatches.Count == FuzzyMatches.StrPartCount) {
                    ListPush(
                        Scratch.MemPool,
                        &ConfigsListFiltered,
                        Config
                    );
                }
            }

            ConfigsFiltered = ConfigNodePtrArrayFromList(
                MemPool,
                &ConfigsListFiltered
            );
        }

        if (
            ConfigNodeChildFromStr(
                ConfigNodeFromID(Registers()->View),
                "lister"_s8
            ) == EMPTY_CFG_NODE_VALUE
        ) {
            Accel->Commands = Ext->Commands;
            Accel->CommandsIndexRange = Rng(0ULL, Accel->Commands.Count);
        }

        Accel->Configs = ConfigsFiltered;
        Accel->ConfigsIndexRange = Rng(
            Accel->CommandsIndexRange.Max,
            Accel->CommandsIndexRange.Max + Accel->Configs.Count
        );
        Result.ExprCount = (Accel->Commands.Count + Accel->Configs.Count);
    }

    ReleaseScratch(Scratch);

    return Result;
}

EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(ConfigsSlice)
{
    AppConfigsExpandAccel* Accel = (AppConfigsExpandAccel*) UserData;
    r1u64 CommandsIndexRange = Accel->CommandsIndexRange;
    r1u64 ConfigsIndexRange = Accel->ConfigsIndexRange;
    u64 DstIndex = 0;

    {
        r1u64 ReadRange = Intersect(CommandsIndexRange, IndexRange);
        u64 ReadCount = Length(ReadRange);
        EvalEval CommandsEval = EvalEvalFromStr("query:commands"_s8);

        for (
            u64 Index = 0; 
            Index < ReadCount; 
            ++Index, ++DstIndex
        ) {
            Str8 CommandName = Accel->Commands.Data[
                Index + ReadRange.Min - CommandsIndexRange.Min
            ];
            EvalEval CommandEval = EvalEvalWrapFmt(
                CommandsEval,
                "$.%S",
                PRINT_STR(CommandName)
            );
            Evals[DstIndex] = CommandEval;
        }
    }

    {
        r1u64 ReadRange = Intersect(ConfigsIndexRange, IndexRange);
        u64 ReadCount = Length(ReadRange);
        EvalEval CommandsEval = EvalEvalFromStr("query:commands"_s8);

        for (
            u64 Index = 0; 
            Index < ReadCount; 
            ++Index, ++DstIndex
        ) {
            ConfigNode* Config = Accel->Configs.V[
                Index + ReadRange.Min - ConfigsIndexRange.Min
            ];

            Evals[DstIndex] = EvalEvalFromStrFmt(
                "query:config.$%llx",
                Config->ID
            );
        }
    }
}

EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(ConfigsSlice)
{
    u64 Result = 0;
    AppConfigsExpandAccel* Accel = (AppConfigsExpandAccel*) UserData;

    if (Num) {
        u64 Index = Num - 1;

        if (InRange(Accel->ConfigsIndexRange, Index)) {
            ConfigNode* Config = Accel->Configs.V[
                Index - Accel->ConfigsIndexRange.Min
            ];

            Result = Config->ID;
        } else if (InRange(Accel->CommandsIndexRange, Index)) {
            Result = Num;
            Result |= (1ULL << 63);
        }
    }

    return Result;
}

EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(ConfigsSlice)
{
    u64 Num = 0;
    AppConfigsExpandAccel* Accel = (AppConfigsExpandAccel*) UserData;

    if (ID) {
        if (ID & (1ULL << 63)) {
            Num = ID;
            Num &= ~(1ULL << 63);
        } else {
            for (u64 Index = 0; Index < Accel->Configs.Count; ++Index) {
                if (Accel->Configs.V[Index]->ID == ID) {
                    Num = Index + Accel->ConfigsIndexRange.Min + 1;
                    break;
                }
            }
        }
    }

    return Num;
}
