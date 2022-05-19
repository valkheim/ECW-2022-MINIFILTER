#include "main.h"

PFLT_FILTER FilterHandle = nullptr;

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {            // The minifilter driver usees callbacks to indicate which operations it's interested in
    {
        IRP_MJ_CREATE,                                      // Major function code to identify the operation 
        0,                                                  // Set of flags affecting read and write operations
                                                            // FLTFL_OPERATION_REGISTRATION_SKIP_CACHED_IO: bypass callbacks if it's cached I/O (e.g. Fast I/O)
                                                            // FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO: bypass callbacks for paging I/O (IRP-based operations)
                                                            // FLTFL_OPERATION_REGISTRATION_SKIP_NON_DASD_IO: bypass callbacks for direct access volumes (DAX/DAS)
        nullptr,                                            // Pre operation
        (PFLT_POST_OPERATION_CALLBACK)PostCreateOperation   // Post operation
    },
    {
        IRP_MJ_WRITE,
        FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
        (PFLT_PRE_OPERATION_CALLBACK)PreWriteOperation,     // Pre operation
        nullptr,
    },
    {
        IRP_MJ_CLEANUP,
        0,
        nullptr,
        (PFLT_POST_OPERATION_CALLBACK)PostCleanupOperation,
    },
    { IRP_MJ_OPERATION_END }
};

const FLT_CONTEXT_REGISTRATION Contexts[] = {
    {
        FLT_FILE_CONTEXT,
        0,
        nullptr,
        sizeof(FileContext),
        DRIVER_CONTEXT_TAG,
    },
    {FLT_CONTEXT_END}
};

static bool IsValidDirectory(_In_ PUNICODE_STRING directory)
{
    ULONG maxSize = 1024;
    if (directory->Length > maxSize)
    {
        DbgPrint("IsValidDirectory: invalid length\n");
        return false;
    }

    auto copy = (WCHAR*)ExAllocatePoolWithTag(PagedPool, maxSize + sizeof(WCHAR), DRIVER_TAG);
    if (!copy)
    {
        DbgPrint("IsValidDirectory: cannot allocate copy\n");
        return false;
    }

    RtlZeroMemory(copy, maxSize + sizeof(WCHAR));
    wcsncpy_s(copy, maxSize / sizeof(WCHAR) + 1, directory->Buffer, directory->Length / sizeof(WCHAR));
    _wcslwr(copy);
    
    auto ret = wcsstr(copy, L"\\secret\\") || wcsstr(copy, L"\\private\\");
    ExFreePool(copy);
    return ret;
}

FLT_POSTOP_CALLBACK_STATUS PostCreateOperation(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, _In_opt_ PVOID CompletionContext, _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    // UNREFERENCED_PARAMETER(Data);               // Pointer to the callback data structure for the I/O operation
    //UNREFERENCED_PARAMETER(FltObjects);         // Pointer to an FLT_RELATED_OBJECTS strcture that contains opaque pointers for the objects related to the current I/O request
    UNREFERENCED_PARAMETER(CompletionContext);  // A pointer that was returned by the minifilter pre-operation callback.
    //UNREFERENCED_PARAMETER(Flags);              // A bitmask of flags that specifies how the post-operation callback is to be performed
    if (Flags & FLTFL_POST_OPERATION_DRAINING || FltObjects->FileObject->DeletePending)
    {
        //DbgPrint("PostCreateOperation: the filter instance is being detached or the file is opened for deletion\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    const auto& params = Data->Iopb->Parameters.Create;
    if (Data->RequestorMode == KernelMode
        || (params.SecurityContext->DesiredAccess & FILE_WRITE_DATA) == 0
        || Data->IoStatus.Information == FILE_DOES_NOT_EXIST)
    {
        //DbgPrint("PostCreateOperation: kernel caller or not exists or no write access\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    auto fileNameInfo = kl::FilterFileNameInformation(Data);
    if (!fileNameInfo)
    {
        DbgPrint("PostCreateOperation: no filename info\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!NT_SUCCESS(fileNameInfo.Parse()))
    {
        DbgPrint("PostCreateOperation: cannot parse filename info\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    //DbgPrint("PostCreateOperation:got %wZ\n", fileNameInfo->Name);
    if (fileNameInfo->Stream.Length > 0) // only the default data stream. Should check ::$DATA
    {
        DbgPrint("PostCreateOperation: only the default data stream\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!IsValidDirectory(&fileNameInfo->ParentDir))
    {
        //DbgPrint("PostCreateOperation: invalid parent directory\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FileContext* context = nullptr;
    auto status = FltAllocateContext(FltObjects->Filter, FLT_FILE_CONTEXT, sizeof(*context), PagedPool, (PFLT_CONTEXT*)&context);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to allocate file context (0x%08x)\n", status);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    context->Written = FALSE;
    context->FileName.MaximumLength = fileNameInfo->Name.Length;
    context->FileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, fileNameInfo->Name.Length, DRIVER_TAG);
    if (!context->FileName.Buffer)
    {
        DbgPrint("PostCreateOperation: no failed to allocate file buffer\n");
        FltReleaseContext(context);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    // copy file name (this might be done at a later stage but the sooner the better)
    RtlCopyUnicodeString(&context->FileName, &fileNameInfo->Name);
    // if more than one thread within the client process writes to the file at roughly the same time
    context->Lock.Init();
    // attach the context to the file object
    DbgPrint("Set context for %wZ on FltObjects %p\n", context->FileName, FltObjects);
    status = FltSetFileContext(FltObjects->Instance, FltObjects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, context, nullptr);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to set file context (0x%08x)\n", status);
        ExFreePool(context->FileName.Buffer);
    }

    // decrement ref counter set by FltSetFileContext (refcount == 1 by FltAllocateContext)
    FltReleaseContext(context);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS HandleFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    HANDLE hTargetFile = nullptr;
    HANDLE hSourceFile = nullptr;
    IO_STATUS_BLOCK ioStatus;
    auto status = STATUS_SUCCESS;
    void* buffer = nullptr;
    LARGE_INTEGER fileSize;

    DbgPrint("HandleFile: handle %wZ\n", FileName);
    // Return if no data (size == 0)
    status = FsRtlGetFileSize(FltObjects->FileObject, &fileSize);
    if (!NT_SUCCESS(status) || fileSize.QuadPart == 0)
    {
        DbgPrint("HandleFile: cannot get file size (0x%08x)\n", status);
        return status;
    }

    do {
        OBJECT_ATTRIBUTES sourceFileAttr;
        InitializeObjectAttributes(&sourceFileAttr, FileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        // Open the source file (ZwCreateFile would send I/O requests to the top of the file system driver stack)
        status = FltCreateFile(
            FltObjects->Filter,		                                // filter object
            FltObjects->Instance,	                                // filter instance
            &hSourceFile,			                                // resulting handle
            FILE_READ_DATA | SYNCHRONIZE,                           // access mask
            &sourceFileAttr,		                                // object attributes
            &ioStatus,				                                // resulting status
            nullptr, FILE_ATTRIBUTE_NORMAL, 	                    // allocation size, file attributes
            FILE_SHARE_READ | FILE_SHARE_WRITE,		                // share flags
            FILE_OPEN,		                                        // create disposition
            FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,    // create options (sync I/O)
            nullptr, 0,				                                // extended attributes, EA length
            IO_IGNORE_SHARE_ACCESS_CHECK                            // flags
        );
        if (!NT_SUCCESS(status))
        {
            DbgPrint("HandleFile: cannot open the source file (0x%08x)\n", status);
            break;
        }

        // Open the target file (source ADS)
        UNICODE_STRING targetFileName;
        const WCHAR backupStream[] = L".lock";
        targetFileName.MaximumLength = FileName->Length + sizeof(backupStream);
        targetFileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, targetFileName.MaximumLength, DRIVER_TAG);
        if (targetFileName.Buffer == nullptr)
        {
            DbgPrint("HandleFile: cannot allocate target file buffer\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlCopyUnicodeString(&targetFileName, FileName);
        RtlAppendUnicodeToString(&targetFileName, backupStream);

        OBJECT_ATTRIBUTES targetFileAttr;
        InitializeObjectAttributes(&targetFileAttr, &targetFileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        status = FltCreateFile(
            FltObjects->Filter,		                                // filter object
            FltObjects->Instance,	                                // filter instance
            &hTargetFile,			                                // resulting handle
            GENERIC_WRITE | SYNCHRONIZE,                            // access mask
            &targetFileAttr,		                                // object attributes
            &ioStatus,				                                // resulting status
            nullptr, FILE_ATTRIBUTE_NORMAL, 	                    // allocation size, file attributes
            0,		                                                // share flags
            FILE_OVERWRITE_IF,		                                // create disposition
            FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY,    // create options (sync I/O)
            nullptr, 0,		                                        // extended attributes, EA length
            0 /*IO_IGNORE_SHARE_ACCESS_CHECK*/	                    // flags
        );
        ExFreePool(targetFileName.Buffer);
        if (!NT_SUCCESS(status))
        {
            DbgPrint("HandleFile: cannot open target file (0x%08x)\n", status);
            break;
        }

        // copy chunks from source to target
        // allocate buffer for copying purposes
        ULONG size = 1 << 21;	// 2 MB
        buffer = ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
        if (!buffer)
        {
            DbgPrint("HandleFile: cannot allocate chunk\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        // loop - read from source, write to target
        LARGE_INTEGER offset = { 0 };		// read
        LARGE_INTEGER writeOffset = { 0 };	// write
        ULONG bytes;
        auto saveSize = fileSize;
        while (fileSize.QuadPart > 0)
        {
            status = ZwReadFile(
                hSourceFile,
                nullptr,	                                    // optional KEVENT
                nullptr, nullptr,	                            // no APC
                &ioStatus,
                buffer,
                (ULONG)min((LONGLONG)size, fileSize.QuadPart),  // number of bytes
                &offset,	                                    // offset
                nullptr  	                                    // optional key
            );
            if (!NT_SUCCESS(status))
            {
                DbgPrint("HandleFile: cannot read source chunk (0x%08x)\n", status);
                break;
            }

            bytes = (ULONG)ioStatus.Information;
            // write to target file
            status = ZwWriteFile(
                hTargetFile,	    // target handle
                nullptr,		    // optional KEVENT
                nullptr, nullptr,   // APC routine, APC context
                &ioStatus,		    // I/O status result
                buffer,			    // data to write
                bytes,              // number of bytes to write
                &writeOffset,	    // offset
                nullptr		        // optional key
            );
            if (!NT_SUCCESS(status))
            {
                DbgPrint("HandleFile: cannot write target chunk (0x%08x)\n", status);
                break;
            }

            // update byte count and offsets
            offset.QuadPart += bytes;
            writeOffset.QuadPart += bytes;
            fileSize.QuadPart -= bytes;
        }

        // write target file
        FILE_END_OF_FILE_INFORMATION info;
        info.EndOfFile = saveSize;
        NT_VERIFY(NT_SUCCESS(ZwSetInformationFile(hTargetFile, &ioStatus, &info, sizeof(info), FileEndOfFileInformation)));

        // delete source file
        FILE_DISPOSITION_INFORMATION delete_info;
        delete_info.DeleteFile = TRUE;
        NT_VERIFY(NT_SUCCESS(ZwSetInformationFile(hSourceFile, &ioStatus, &delete_info, sizeof(delete_info), FileDispositionInformation)));
    } while(false);

    if (buffer)
        ExFreePool(buffer);

    if (hSourceFile)
        FltClose(hSourceFile);

    if (hTargetFile)
        FltClose(hTargetFile);

    return status;
}

FLT_PREOP_CALLBACK_STATUS PreWriteOperation(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, _Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);               // Pointer to the callback data structure for the I/O operation
    //UNREFERENCED_PARAMETER(FltObjects);         // Pointer to an FLT_RELATED_OBJECTS strcture that contains opaque pointers for the objects related to the current I/O request
    UNREFERENCED_PARAMETER(CompletionContext);  // Pointer to an optional context in case this callacks returns FLT_PREOP_SUCCESS_WITH_CALLBACK or FLT_PREOP_SYNCHRONIZE
    FileContext* context = nullptr;
    //DbgPrint("Get context on FltObjects %p\n", FltObjects);
    auto status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
    if (!NT_SUCCESS(status) || context == nullptr)
    {
        //DbgPrint("PreWriteOperation: cannot get file context (0x%08x)\n", status);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    {
        context->Lock.Lock();
        DbgPrint("context filename %wZ", &context->FileName);
        if (!context->Written)
        {
            status = HandleFile(&context->FileName, FltObjects);
            if (!NT_SUCCESS(status))
            {
                DbgPrint("PreWriteOperation: failed to handle file (0x%08x)\n", status);
            }

            context->Written = TRUE;
        }
        context->Lock.Unlock();
    }

    FltReleaseContext(context);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    //return FLT_PREOP_COMPLETE;
}

FLT_POSTOP_CALLBACK_STATUS PostCleanupOperation(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, _In_opt_ PVOID CompletionContext, _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);               // Pointer to the callback data structure for the I/O operation
    UNREFERENCED_PARAMETER(FltObjects);         // Pointer to an FLT_RELATED_OBJECTS strcture that contains opaque pointers for the objects related to the current I/O request
    UNREFERENCED_PARAMETER(CompletionContext);  // A pointer that was returned by the minifilter pre-operation callback.
    UNREFERENCED_PARAMETER(Flags);              // A bitmask of flags that specifies how the post-operation callback is to be performed
    FileContext* context = nullptr;
    auto status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
    if (!NT_SUCCESS(status) || context == nullptr)
    {
        // DbgPrint("PostCleanupOperation: cannot get file context (0x%08x)\n", status);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (context->FileName.Buffer)
        ExFreePool(context->FileName.Buffer);

    FltReleaseContext(context);
    FltDeleteContext(context);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS FilterUnloadCallback(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    /*
        The filter manager calls the FilterUnloadCallback routine to notify the minifilter driver that the filter manager is about to unload the minifilter driver.
    */
    UNREFERENCED_PARAMETER(Flags);              // A bitmask of flags describing the unload request
    PAGED_CODE();
    FltUnregisterFilter(FilterHandle);
    DbgPrint("Driver unloaded\n");
    return STATUS_SUCCESS;
}

NTSTATUS InstanceSetupCallback(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_SETUP_FLAGS Flags, _In_ DEVICE_TYPE VolumeDeviceType, _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
    /*
        The filter manager calls this routine on the first operation after a new volume is mounted.
        The filter manager calls this routine to allow the minifilter driver to respond to an automatic or manual attachment request.
        If this routine returns an error or warning NTSTATUS code, the minifilter driver instance is not attached to the given volume.
        Otherwise, the minifilter driver instance is attached to the given volume.
    */
    UNREFERENCED_PARAMETER(FltObjects);             // Pointer to an FLT_RELATED_OBJECTS structure that contains opaque pointers for the objects related to the current operation.
    UNREFERENCED_PARAMETER(Flags);                  // Bitmask of flags that indicate why the instance is being attached
    UNREFERENCED_PARAMETER(VolumeDeviceType);       // Device type of the file system volume (FILE_DEVICE_CD_ROM_FILE_SYSTEM, FILE_DEVICE_DISK_FILE_SYSTEM, FILE_DEVICE_NETWORK_FILE_SYSTEM)
    // UNREFERENCED_PARAMETER(VolumeFilesystemType);   // File system type of the volume (FLT_FILESYSTEM_TYPE enum)
    PAGED_CODE();

    DbgPrint("VolumeFilesystemType %d. Want %d.\n", VolumeFilesystemType, FLT_FSTYPE_NTFS);
    if (VolumeFilesystemType != FLT_FSTYPE_NTFS || VolumeDeviceType != FILE_DEVICE_DISK_FILE_SYSTEM)
    {
        DbgPrint("Not attaching to non-NTFS volume (%d)\n", VolumeFilesystemType); // (13) and (5)
        return STATUS_FLT_DO_NOT_ATTACH;
    }

    return STATUS_SUCCESS;
}


NTSTATUS InstanceQueryTeardownCallback(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    /*
        The filter manager calls this routine to allow the minifilter driver to respond to a manual detach request.
        Such a request is received when a user-mode application calls FilterDetach or a kernel-mode component calls FltDetachVolume.
        This routine is not called for automatic or mandatory detach requests, for example, when the minifilter driver is unloaded or a volume is dismounted.
    */
    UNREFERENCED_PARAMETER(FltObjects);             // Pointer to an FLT_RELATED_OBJECTS structure that contains opaque pointers for the objects related to the current operation.
    UNREFERENCED_PARAMETER(Flags);                  // Bitmask of flags that describe why the given isntance query teardown callback routine was called (no flags currently defined)
    PAGED_CODE();
    return STATUS_SUCCESS;
}

const FLT_REGISTRATION FilterRegistration = {                               // A driver uses FLT_REGISTRATION to register itself with the filter manager
    sizeof(FLT_REGISTRATION),                                               // Size
    FLT_REGISTRATION_VERSION,                                               // Version
    (FLT_REGISTRATION_FLAGS)0,                                              // Flags
    (PFLT_CONTEXT_REGISTRATION)Contexts,                                    // Pointer to contexts where each represents a context that the driver may use in its work. Context refers to some driver-defined data that can be attached to file system entries, such as files and volumes
    (PFLT_OPERATION_REGISTRATION)Callbacks,                                 // Operation callbacks, each specifying and operation of interest and associated pre/post callbacks
    (PFLT_FILTER_UNLOAD_CALLBACK)FilterUnloadCallback,                      // Function to be called when the driver is about to be unloaded
    (PFLT_INSTANCE_SETUP_CALLBACK)InstanceSetupCallback,                    // Allows the driver to be notified when an instance is about to be attached to a new volume
    (PFLT_INSTANCE_QUERY_TEARDOWN_CALLBACK)InstanceQueryTeardownCallback,   // InstanceQueryTeardown
    nullptr, // (opt) InstanceTeardownStart
    nullptr, // (opt) InstanceTeardownComplete
    nullptr, // GenerateFileName
    nullptr, // GenerateDestinationFileName
    nullptr, // NormalizeNameComponent
};

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    DbgPrint("Driver loading\n");
    UNREFERENCED_PARAMETER(RegistryPath);
    auto status = FltRegisterFilter(    // Registers a minifilter driver
        DriverObject,                   // Pointer to the driver object for the minifilter driver
        &FilterRegistration,            // Pointer to a minifilter driver registration structure
        &FilterHandle                   // Pointer to a variable that receives an opaque filter pointer for the caller
    );
    FLT_ASSERT(NT_SUCCESS(status));
    if (!NT_SUCCESS(status))
        return status;

    // FltCreateCommunicationPort
    status = FltStartFiltering(FilterHandle);
    if (!NT_SUCCESS(status))
        FltUnregisterFilter(FilterHandle);

    return status;
}
