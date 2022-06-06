#include "FilterFileNameInformation.h"

namespace kl
{
    FilterFileNameInformation::FilterFileNameInformation(PFLT_CALLBACK_DATA data, FileNameOptions options)
    {
        auto status = FltGetFileNameInformation(data, (FLT_FILE_NAME_OPTIONS)options, &info);
        if (!NT_SUCCESS(status))
            info = nullptr;
    }

    FilterFileNameInformation::~FilterFileNameInformation()
    {
        if (info)
            FltReleaseFileNameInformation(info);
    }

    [[nodiscard]] FilterFileNameInformation::operator bool()
    {
        return info != nullptr;
    }

    [[nodiscard]] auto FilterFileNameInformation::operator->() -> PFLT_FILE_NAME_INFORMATION const
    {
        return info;
    }

    [[nodiscard]] auto FilterFileNameInformation::Parse() -> NTSTATUS
    {
        return FltParseFileNameInformation(info);
    }
};