#include <ntddk.h>
#include <wdf.h>
DRIVER_INITIALIZE DriverEntry;
VOID UnloadRoutine(IN PDRIVER_OBJECT DriverObject);

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    DbgPrint("Start");
    DriverObject->DriverUnload = UnloadRoutine;

    //Open configuration file

    OBJECT_ATTRIBUTES obj_attribute;
    UNICODE_STRING fileName = RTL_CONSTANT_STRING(L"\\??\\c:\\inf.inf");

    InitializeObjectAttributes(&obj_attribute, &fileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    HANDLE   handle;
    NTSTATUS ntstatus;
    IO_STATUS_BLOCK    ioStatusBlock = {0};

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        return STATUS_INVALID_DEVICE_STATE;

    LARGE_INTEGER      byteOffset;
    #define  BUFFER_SIZE 8000
    CHAR     buffer[BUFFER_SIZE];


    ntstatus = ZwCreateFile(&handle,
        GENERIC_READ,
        &obj_attribute, &ioStatusBlock,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
        NULL, 0);
    //DbgPrint(ntstatus);
    if (NT_SUCCESS(ntstatus)) {
        DbgPrint("start read");
        byteOffset.LowPart = byteOffset.HighPart = 0;
        ntstatus = ZwReadFile(handle, NULL, NULL, NULL, &ioStatusBlock,
            buffer, BUFFER_SIZE, &byteOffset, NULL);
        if (NT_SUCCESS(ntstatus)) {
            buffer[BUFFER_SIZE - 1] = '\0';
            DbgPrint("%s\n", buffer);
        }
    }

    if (handle != NULL) {
        ZwClose(handle);
    }

    // End read configuration file



    DbgPrint("Start write");;
    //Open configuration file

    OBJECT_ATTRIBUTES obj_attribute_port;
    UNICODE_STRING fileNamePort = RTL_CONSTANT_STRING(L"\\DosDevices\\COM2");

    InitializeObjectAttributes(&obj_attribute_port, &fileNamePort, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    HANDLE   handlePort;
    NTSTATUS ntstatusPort;
    IO_STATUS_BLOCK    ioStatusBlockPort = { 0 };

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        return STATUS_INVALID_DEVICE_STATE;

    ntstatusPort = ZwCreateFile(&handlePort,
        GENERIC_WRITE,
        &obj_attribute_port, &ioStatusBlockPort, NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OVERWRITE_IF,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL, 0);

    if (NT_SUCCESS(ntstatusPort)) {
        DbgPrint("write successfully");
        //ntstatus = RtlStringCbPrintfA(buffer, sizeof(buffer), "This is %d test\r\n", 0x0);
        ntstatusPort = ZwWriteFile(handlePort, NULL, NULL, NULL, &ioStatusBlockPort, buffer, BUFFER_SIZE, NULL, NULL);
        if (handlePort != NULL) {
            ZwClose(handlePort);
        }
    }
    else {
        DbgPrint("not successfully");
    }

    DbgPrint("statusOpen= %X.", ntstatusPort);

    // End read configuration file
    DbgPrint("End write");


    DbgPrint("End");
    return STATUS_SUCCESS;
}



VOID UnloadRoutine(IN PDRIVER_OBJECT DriverObject)
{
    DbgPrint("Goodbye!\n");
}

