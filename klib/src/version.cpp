#include "version.h"

namespace kl
{
    auto getVersion() -> PUNICODE_STRING
    {
        static UNICODE_STRING version = RTL_CONSTANT_STRING(L"1.0");
        return &version;
    }
};