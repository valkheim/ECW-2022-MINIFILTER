#pragma once

#include "main.h"

namespace kl
{
    enum class FileNameOptions
    {
        Normalized = FLT_FILE_NAME_NORMALIZED,
        Opened = FLT_FILE_NAME_OPENED,
        Short = FLT_FILE_NAME_SHORT,

        QueryDefault = FLT_FILE_NAME_QUERY_DEFAULT,
        QueryCacheOnly = FLT_FILE_NAME_QUERY_CACHE_ONLY,
        QueryFileSystemOnly = FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY,

        RequestFromCurrentProvider = FLT_FILE_NAME_REQUEST_FROM_CURRENT_PROVIDER,
        DoNotCache = FLT_FILE_NAME_DO_NOT_CACHE,
        AllowQueryOnReparse = FLT_FILE_NAME_ALLOW_QUERY_ON_REPARSE
    };

    DEFINE_ENUM_FLAG_OPERATORS(FileNameOptions);

    class FilterFileNameInformation
    {
        PFLT_FILE_NAME_INFORMATION info;

    public:
        FilterFileNameInformation(PFLT_CALLBACK_DATA data, FileNameOptions options = FileNameOptions::QueryDefault | FileNameOptions::Normalized);
        ~FilterFileNameInformation();
        [[nodiscard]] operator bool();
        [[nodiscard]] auto operator->() -> PFLT_FILE_NAME_INFORMATION const;
        [[nodiscard]] auto Parse() -> NTSTATUS;
        
    };
};