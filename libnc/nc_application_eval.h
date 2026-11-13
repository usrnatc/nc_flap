#if !defined(__NC_APPLICATION_EVAL_H__)
#define __NC_APPLICATION_EVAL_H__

#include "nc_eval.h"
#include "nc_config.h"

// @defines____________________________________________________________________


// @types______________________________________________________________________
struct AppConfigsExpandAccel {
    Str8Array          Commands;
    ConfigNodePtrArray Configs;
    r1u64              CommandsIndexRange;
    r1u64              ConfigsIndexRange;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
EVAL_TYPE_ACCESS_FUNC_DEF(Commands);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Commands);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Commands);
EVAL_TYPE_ACCESS_FUNC_DEF(Themes);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Themes);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Themes);
EVAL_TYPE_ACCESS_FUNC_DEF(Views);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Views);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Views);
EVAL_TYPE_IR_EXT_FUNC_DEF(Schema);
EVAL_TYPE_ACCESS_FUNC_DEF(Schema);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Schema);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Schema);
EVAL_TYPE_ACCESS_FUNC_DEF(Configs);
EVAL_TYPE_IR_EXT_FUNC_DEF(ConfigsSlice);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(ConfigsQuery);
EVAL_TYPE_ACCESS_FUNC_DEF(ConfigsSlice);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(ConfigsSlice);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(ConfigsSlice);
EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(ConfigsSlice);
EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(ConfigsSlice);

#endif // __NC_APPLICATION_EVAL_H__
