/* 
 * FFS File System Driver for Windows
 *
 * init.c
 *
 * 2004.5.6 ~
 *
 * Lee Jae-Hong, http://www.pyrasis.com
 *
 * See License.txt
 *
 */

#include "ntifs.h"
#include "ffsdrv.h"

/* Globals */

PFFS_GLOBAL FFSGlobal = NULL;


/* Definitions */

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, FFSQueryParameters)
#pragma alloc_text(INIT, DriverEntry)
#if FFS_UNLOAD
#pragma alloc_text(PAGE, DriverUnload)
#endif
#endif

/* FUNCTIONS ***************************************************************/

#if FFS_UNLOAD

/*
 * FUNCTION: Called by the system to unload the driver
 * ARGUMENTS:
 *           DriverObject = object describing this driver
 * RETURNS:  None
 */

VOID
DriverUnload(
	IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING  DosDeviceName;

	FFSPrint((DBG_FUNC, "ffsdrv: Unloading routine.\n"));

	RtlInitUnicodeString(&DosDeviceName, DOS_DEVICE_NAME);
	IoDeleteSymbolicLink(&DosDeviceName);

	ExDeleteResourceLite(&FFSGlobal->LAResource);
	ExDeleteResourceLite(&FFSGlobal->CountResource);
	ExDeleteResourceLite(&FFSGlobal->Resource);

	ExDeletePagedLookasideList(&(FFSGlobal->FFSMcbLookasideList));
	ExDeleteNPagedLookasideList(&(FFSGlobal->FFSCcbLookasideList));
	ExDeleteNPagedLookasideList(&(FFSGlobal->FFSFcbLookasideList));
	ExDeleteNPagedLookasideList(&(FFSGlobal->FFSIrpContextLookasideList));

	IoDeleteDevice(FFSGlobal->DeviceObject);
}

#endif

BOOLEAN
FFSQueryParameters(
	IN PUNICODE_STRING  RegistryPath)
{
	NTSTATUS                    Status;
	UNICODE_STRING              ParameterPath;
	RTL_QUERY_REGISTRY_TABLE    QueryTable[2];

	ULONG                       WritingSupport;
	ULONG                       CheckingBitmap;

	ParameterPath.Length = 0;

	ParameterPath.MaximumLength =
		RegistryPath->Length + sizeof(PARAMETERS_KEY) + sizeof(WCHAR);

	ParameterPath.Buffer =
		(PWSTR) ExAllocatePool(PagedPool, ParameterPath.MaximumLength);

	if (!ParameterPath.Buffer)
	{
		return FALSE;
	}

	WritingSupport = 0;
	CheckingBitmap = 0;

	RtlCopyUnicodeString(&ParameterPath, RegistryPath);

	RtlAppendUnicodeToString(&ParameterPath, PARAMETERS_KEY);

	RtlZeroMemory(&QueryTable[0], sizeof(RTL_QUERY_REGISTRY_TABLE) * 2);

	QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = WRITING_SUPPORT;
	QueryTable[0].EntryContext = &WritingSupport;

	Status = RtlQueryRegistryValues(
				RTL_REGISTRY_ABSOLUTE,
				ParameterPath.Buffer,
				&QueryTable[0],
				NULL,
				NULL);

	FFSPrint((DBG_USER, "FFSQueryParameters: WritingSupport=%xh\n", WritingSupport));

	RtlZeroMemory(&QueryTable[0], sizeof(RTL_QUERY_REGISTRY_TABLE) * 2);

	QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
	QueryTable[0].Name = CHECKING_BITMAP;
	QueryTable[0].EntryContext = &CheckingBitmap;

	Status = RtlQueryRegistryValues(
				RTL_REGISTRY_ABSOLUTE,
				ParameterPath.Buffer,
				&QueryTable[0],
				NULL,
				NULL);

	FFSPrint((DBG_USER, "FFSQueryParameters: CheckingBitmap=%xh\n", CheckingBitmap));

	RtlZeroMemory(&QueryTable[0], sizeof(RTL_QUERY_REGISTRY_TABLE) * 2);

	QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;

	Status = RtlQueryRegistryValues(
				RTL_REGISTRY_ABSOLUTE,
				ParameterPath.Buffer,
				&QueryTable[0],
				NULL,
				NULL);


	{
		if (WritingSupport)
		{
			SetFlag(FFSGlobal->Flags, FFS_SUPPORT_WRITING);
		}
		else
		{
			ClearFlag(FFSGlobal->Flags, FFS_SUPPORT_WRITING);
		}

		if (CheckingBitmap)
		{
			SetFlag(FFSGlobal->Flags, FFS_CHECKING_BITMAP);
		}
		else
		{
			ClearFlag(FFSGlobal->Flags, FFS_CHECKING_BITMAP);
		}

	}

	ExFreePool(ParameterPath.Buffer);

	return TRUE;
}


/*
 * NAME: DriverEntry
 * FUNCTION: Called by the system to initalize the driver
 *
 * ARGUMENTS:
 *           DriverObject = object describing this driver
 *           RegistryPath = path to our configuration entries
 * RETURNS: Success or failure
 */
NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT   DriverObject,
	IN PUNICODE_STRING  RegistryPath)
{
	PDEVICE_OBJECT              DeviceObject;
	PFAST_IO_DISPATCH           FastIoDispatch;
	PCACHE_MANAGER_CALLBACKS    CacheManagerCallbacks;
	PFFS_EXT                    DeviceExt;

	UNICODE_STRING              DeviceName;
	NTSTATUS                    Status;

#if FFS_UNLOAD
	UNICODE_STRING              DosDeviceName;
#endif

	DbgPrint(
			"ffsdrv --"
			" Version " 
			FFSDRV_VERSION
#if FFS_READ_ONLY
			" (ReadOnly)"
#endif
#if DBG
			" Checked"
#else
			" Free" 
#endif
			" - Built at "
			__DATE__" "
			__TIME__".\n");

	FFSPrint((DBG_FUNC, "FFS DriverEntry ...\n"));

	RtlInitUnicodeString(&DeviceName, DEVICE_NAME);

	Status = IoCreateDevice(
				DriverObject,
				sizeof(FFS_EXT),
				&DeviceName,
				FILE_DEVICE_DISK_FILE_SYSTEM,
				0,
				FALSE,
				&DeviceObject);

	if (!NT_SUCCESS(Status))
	{
		FFSPrint((DBG_ERROR, "IoCreateDevice fs object error.\n"));
		return Status;
	}

	DeviceExt = (PFFS_EXT)DeviceObject->DeviceExtension;
	RtlZeroMemory(DeviceExt, sizeof(FFS_EXT));

	FFSGlobal = &(DeviceExt->FFSGlobal);

	FFSGlobal->Identifier.Type = FFSFGD;
	FFSGlobal->Identifier.Size = sizeof(FFS_GLOBAL);
	FFSGlobal->DeviceObject = DeviceObject;
	FFSGlobal->DriverObject = DriverObject;

	FFSQueryParameters(RegistryPath);

	DriverObject->MajorFunction[IRP_MJ_CREATE]              = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]               = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_READ]                = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_WRITE]               = FFSBuildRequest;

	DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]       = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]            = FFSBuildRequest;

	DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]   = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]     = FFSBuildRequest;

	DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION]    = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION]      = FFSBuildRequest;

	DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]   = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]      = FFSBuildRequest;
	DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL]        = FFSBuildRequest;

	DriverObject->MajorFunction[IRP_MJ_CLEANUP]             = FFSBuildRequest;

#if (_WIN32_WINNT >= 0x0500)
	DriverObject->MajorFunction[IRP_MJ_PNP]                 = FFSBuildRequest;
#endif //(_WIN32_WINNT >= 0x0500)

#if FFS_UNLOAD
	DriverObject->DriverUnload                              = DriverUnload;
#else
	DriverObject->DriverUnload                              = NULL;
#endif

	//
	// Initialize the fast I/O entry points
	//

	FastIoDispatch = &(FFSGlobal->FastIoDispatch);

	FastIoDispatch->SizeOfFastIoDispatch        = sizeof(FAST_IO_DISPATCH);
	FastIoDispatch->FastIoCheckIfPossible       = FFSFastIoCheckIfPossible;
#if DBG
	FastIoDispatch->FastIoRead                  = FFSFastIoRead;
	FastIoDispatch->FastIoWrite                 = FFSFastIoWrite;
#else
	FastIoDispatch->FastIoRead                  = FsRtlCopyRead;
	FastIoDispatch->FastIoWrite                 = FsRtlCopyWrite;
#endif
	FastIoDispatch->FastIoQueryBasicInfo        = FFSFastIoQueryBasicInfo;
	FastIoDispatch->FastIoQueryStandardInfo     = FFSFastIoQueryStandardInfo;
	FastIoDispatch->FastIoLock                  = FFSFastIoLock;
	FastIoDispatch->FastIoUnlockSingle          = FFSFastIoUnlockSingle;
	FastIoDispatch->FastIoUnlockAll             = FFSFastIoUnlockAll;
	FastIoDispatch->FastIoUnlockAllByKey        = FFSFastIoUnlockAllByKey;
	FastIoDispatch->FastIoQueryNetworkOpenInfo  = FFSFastIoQueryNetworkOpenInfo;

	DriverObject->FastIoDispatch = FastIoDispatch;

	switch (MmQuerySystemSize())
	{
		case MmSmallSystem:

			FFSGlobal->MaxDepth = 16;
			break;

		case MmMediumSystem:

			FFSGlobal->MaxDepth = 64;
			break;

		case MmLargeSystem:

			FFSGlobal->MaxDepth = 256;
			break;
	}

	//
	// Initialize the Cache Manager callbacks
	//

	CacheManagerCallbacks = &(FFSGlobal->CacheManagerCallbacks);
	CacheManagerCallbacks->AcquireForLazyWrite  = FFSAcquireForLazyWrite;
	CacheManagerCallbacks->ReleaseFromLazyWrite = FFSReleaseFromLazyWrite;
	CacheManagerCallbacks->AcquireForReadAhead  = FFSAcquireForReadAhead;
	CacheManagerCallbacks->ReleaseFromReadAhead = FFSReleaseFromReadAhead;

	FFSGlobal->CacheManagerNoOpCallbacks.AcquireForLazyWrite  = FFSNoOpAcquire;
	FFSGlobal->CacheManagerNoOpCallbacks.ReleaseFromLazyWrite = FFSNoOpRelease;
	FFSGlobal->CacheManagerNoOpCallbacks.AcquireForReadAhead  = FFSNoOpAcquire;
	FFSGlobal->CacheManagerNoOpCallbacks.ReleaseFromReadAhead = FFSNoOpRelease;


	//
	// Initialize the global data
	//

	InitializeListHead(&(FFSGlobal->VcbList));
	ExInitializeResourceLite(&(FFSGlobal->Resource));
	ExInitializeResourceLite(&(FFSGlobal->CountResource));
	ExInitializeResourceLite(&(FFSGlobal->LAResource));

	ExInitializeNPagedLookasideList(&(FFSGlobal->FFSIrpContextLookasideList),
			NULL,
			NULL,
			0,
			sizeof(FFS_IRP_CONTEXT),
			'SFF',
			0);

	ExInitializeNPagedLookasideList(&(FFSGlobal->FFSFcbLookasideList),
			NULL,
			NULL,
			0,
			sizeof(FFS_FCB),
			'SFF',
			0);

	ExInitializeNPagedLookasideList(&(FFSGlobal->FFSCcbLookasideList),
			NULL,
			NULL,
			0,
			sizeof(FFS_CCB),
			'SFF',
			0);

	ExInitializePagedLookasideList(&(FFSGlobal->FFSMcbLookasideList),
			NULL,
			NULL,
			0,
			sizeof(FFS_MCB),
			'SFF',
			0);

#if FFS_UNLOAD
	RtlInitUnicodeString(&DosDeviceName, DOS_DEVICE_NAME);
	IoCreateSymbolicLink(&DosDeviceName, &DeviceName);
#endif

#if DBG
	ProcessNameOffset = FFSGetProcessNameOffset();
#endif

	IoRegisterFileSystem(DeviceObject);

	return Status;
}
