#include "nc_defines.h"
#include "nc_types.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "nc_object_cache.h"
#include "nc_content.h"
#include "nc_file_stream.h"
#include "nc_sync.h"

#include "fli.h"

#include "flv_compile.h"
#include "flv_bind.h"
#include "flv_parser.h"
#include "flv_lexer.h"
#include "flv_run.h"
#include "flv_type_check.h"

FLVCompiler* FLV_COMPILER_SHARED = NULL;

INTERNAL void
FLVCompilerSelect(FLVCompiler* Compiler)
{
    ThreadLocalStorage* const TLS = GetTLS();

    TLS->FLV_COMPILER = Compiler;
    TLS->SYMBOL_TABLE = Compiler->SymbolTable;
    TLS->PARSER_STATE = Compiler->Parser;
}

void 
FLVCompilerInit(void)
{
    Arena* MemPool = ArenaAlloc();
    FLVCompiler* Result = ArenaPushArrayZero(
        MemPool,
        FLVCompiler,
        1
    );

    Result->MemPool = MemPool;
    Result->SchemaMemPool = ArenaAlloc();
    Result->Mtx = MutexAlloc();
    Result->KeywordNameBase = NAME_INDEX_INVALID_VALUE;
    Result->StdLib = ArenaPushArray(
        MemPool,
        FLVBuiltin,
        FLV_STDLIB_ID_COUNT
    );
    MemCpy(
        Result->StdLib,
        FLV_STDLIB_TEMPLATE,
        FLV_STDLIB_ID_COUNT * sizeof(FLVBuiltin)
    );
    GetTLS()->FLV_COMPILER = Result;
    Result->Names = NamePoolInit();
    SymTabInit(Result->Names);
    Result->SymbolTable = GetTLS()->SYMBOL_TABLE;
    BuiltinTypesInit();
    TypeTableInit(Result->Names);
    LexerInternKeywords(Result->Names);
    ParserInternNames(Result->Names);
    FLVStdLibInit();
    Result->Parser = ArenaPushArrayZero(
        MemPool,
        ParserState,
        1
    );
    ParserStateInit(Result->Parser);
    FLVCompilerSelect(Result);
    FLV_COMPILER_SHARED = Result;
}

FLVCompiler* 
FLVCompilerLock(void)
{
    FLVCompiler* Result = FLV_COMPILER_SHARED;

    MutexLock(Result->Mtx);
    FLVCompilerSelect(Result);

    return Result;
}

void 
FLVCompilerUnlock(void)
{
    MutexUnlock(FLV_COMPILER_SHARED->Mtx);
}
