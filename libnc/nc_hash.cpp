#include "nc_hash.h"
#include "martins_hash/md5.h"

MD5Hash 
MD5FromStr(Str8 String)
{
    MD5Hash Result = {};
    md5_ctx Ctx = {};

    md5_init(&Ctx);
    md5_update(&Ctx, String.Str, String.Size);
    md5_finish(&Ctx, Result.U8);

    return Result;
}
