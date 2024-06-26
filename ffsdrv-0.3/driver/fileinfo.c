/* 
 * FFS File System Driver for Windows
 *
 * fileinfo.c
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

extern PFFS_GLOBAL FFSGlobal;


/* Definitions */

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FFSQueryInformation)
#pragma alloc_text(PAGE, FFSSetInformation)
#pragma alloc_text(PAGE, FFSExpandFile)
#pragma alloc_text(PAGE, FFSTruncateFile)
#pragma alloc_text(PAGE, FFSSetDispositionInfo)
#pragma alloc_text(PAGE, FFSSetRenameInfo)
#pragma alloc_text(PAGE, FFSDeleteFile)
#endif


NTSTATUS
FFSQueryInformation(
	IN PFFS_IRP_CONTEXT IrpContext)
{
	PDEVICE_OBJECT          DeviceObject;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;
	PFILE_OBJECT            FileObject;
	PFFS_FCB                Fcb;
	PFFS_CCB                Ccb;
	PIRP                    Irp;
	PIO_STACK_LOCATION      IoStackLocation;
	FILE_INFORMATION_CLASS  FileInformationClass;
	ULONG                   Length;
	PVOID                   Buffer;
	BOOLEAN                 FcbResourceAcquired = FALSE;

	__try
	{
		ASSERT(IrpContext != NULL);

		ASSERT((IrpContext->Identifier.Type == FFSICX) &&
				(IrpContext->Identifier.Size == sizeof(FFS_IRP_CONTEXT)));

		DeviceObject = IrpContext->DeviceObject;

		//
		// This request is not allowed on the main device object
		//
		if (DeviceObject == FFSGlobal->DeviceObject)
		{
			Status = STATUS_INVALID_DEVICE_REQUEST;
			__leave;
		}

		FileObject = IrpContext->FileObject;

		Fcb = (PFFS_FCB)FileObject->FsContext;

		ASSERT(Fcb != NULL);

		//
		// This request is not allowed on volumes
		//
		if (Fcb->Identifier.Type == FFSVCB)
		{
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		ASSERT((Fcb->Identifier.Type == FFSFCB) &&
				(Fcb->Identifier.Size == sizeof(FFS_FCB)));
		/*        
		if (!IsFlagOn(Fcb->Vcb->Flags, VCB_READ_ONLY) &&
			!FlagOn(Fcb->Flags, FCB_PAGE_FILE))
		*/
		{
			if (!ExAcquireResourceSharedLite(
						&Fcb->MainResource,
						IrpContext->IsSynchronous))
			{
				Status = STATUS_PENDING;
				__leave;
			}

			FcbResourceAcquired = TRUE;
		}

		Ccb = (PFFS_CCB)FileObject->FsContext2;

		ASSERT(Ccb != NULL);

		ASSERT((Ccb->Identifier.Type == FFSCCB) &&
				(Ccb->Identifier.Size == sizeof(FFS_CCB)));

		Irp = IrpContext->Irp;

		IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

		FileInformationClass =
			IoStackLocation->Parameters.QueryFile.FileInformationClass;

		Length = IoStackLocation->Parameters.QueryFile.Length;

		Buffer = Irp->AssociatedIrp.SystemBuffer;

		RtlZeroMemory(Buffer, Length);

		switch (FileInformationClass)
		{
			case FileBasicInformation:
				{
					PFILE_BASIC_INFORMATION FileBasicInformation;

					if (Length < sizeof(FILE_BASIC_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileBasicInformation = (PFILE_BASIC_INFORMATION)Buffer;

					FileBasicInformation->CreationTime = FFSSysTime(Fcb->dinode->di_ctime);

					FileBasicInformation->LastAccessTime = FFSSysTime(Fcb->dinode->di_atime);

					FileBasicInformation->LastWriteTime = FFSSysTime(Fcb->dinode->di_mtime);

					FileBasicInformation->ChangeTime = FFSSysTime(Fcb->dinode->di_mtime);

					FileBasicInformation->FileAttributes = Fcb->FFSMcb->FileAttr;

					Irp->IoStatus.Information = sizeof(FILE_BASIC_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}

#if (_WIN32_WINNT >= 0x0500)

			case FileAttributeTagInformation:
				{
					PFILE_ATTRIBUTE_TAG_INFORMATION FATI;

					if (Length < sizeof(FILE_ATTRIBUTE_TAG_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FATI = (PFILE_ATTRIBUTE_TAG_INFORMATION) Buffer;

					FATI->FileAttributes = Fcb->FFSMcb->FileAttr;
					FATI->ReparseTag = 0;

					Irp->IoStatus.Information = sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}
#endif // (_WIN32_WINNT >= 0x0500)

			case FileStandardInformation:
				{
					PFILE_STANDARD_INFORMATION FileStandardInformation;

					if (Length < sizeof(FILE_STANDARD_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileStandardInformation = (PFILE_STANDARD_INFORMATION)Buffer;

					FileStandardInformation->AllocationSize.QuadPart =
						(LONGLONG)(Fcb->dinode->di_size);

					FileStandardInformation->EndOfFile.QuadPart =
						(LONGLONG)(Fcb->dinode->di_size);

					FileStandardInformation->NumberOfLinks = Fcb->dinode->di_nlink;

					if (IsFlagOn(Fcb->Vcb->Flags, VCB_READ_ONLY))
						FileStandardInformation->DeletePending = FALSE;
					else
						FileStandardInformation->DeletePending = IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING);                

					if (Fcb->FFSMcb->FileAttr & FILE_ATTRIBUTE_DIRECTORY)
					{
						FileStandardInformation->Directory = TRUE;
					}
					else
					{
						FileStandardInformation->Directory = FALSE;
					}

					Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}

			case FileInternalInformation:
				{
					PFILE_INTERNAL_INFORMATION FileInternalInformation;

					if (Length < sizeof(FILE_INTERNAL_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileInternalInformation = (PFILE_INTERNAL_INFORMATION)Buffer;

					// The "inode number"
					FileInternalInformation->IndexNumber.QuadPart = (LONGLONG)Fcb->FFSMcb->Inode;

					Irp->IoStatus.Information = sizeof(FILE_INTERNAL_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}

			case FileEaInformation:
				{
					PFILE_EA_INFORMATION FileEaInformation;

					if (Length < sizeof(FILE_EA_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileEaInformation = (PFILE_EA_INFORMATION)Buffer;

					// Romfs doesn't have any extended attributes
					FileEaInformation->EaSize = 0;

					Irp->IoStatus.Information = sizeof(FILE_EA_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}

			case FileNameInformation:
				{
					PFILE_NAME_INFORMATION FileNameInformation;

					if (Length < sizeof(FILE_NAME_INFORMATION) +
							Fcb->FFSMcb->ShortName.Length - sizeof(WCHAR))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileNameInformation = (PFILE_NAME_INFORMATION)Buffer;

					FileNameInformation->FileNameLength = Fcb->FFSMcb->ShortName.Length;

					RtlCopyMemory(
							FileNameInformation->FileName,
							Fcb->FFSMcb->ShortName.Buffer,
							Fcb->FFSMcb->ShortName.Length);

					Irp->IoStatus.Information = sizeof(FILE_NAME_INFORMATION) +
						Fcb->FFSMcb->ShortName.Length - sizeof(WCHAR);
					Status = STATUS_SUCCESS;
					__leave;
				}

			case FilePositionInformation:
				{
					PFILE_POSITION_INFORMATION FilePositionInformation;

					if (Length < sizeof(FILE_POSITION_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FilePositionInformation = (PFILE_POSITION_INFORMATION)Buffer;

					FilePositionInformation->CurrentByteOffset =
						FileObject->CurrentByteOffset;

					Irp->IoStatus.Information = sizeof(FILE_POSITION_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}

			case FileAllInformation:
				{
					PFILE_ALL_INFORMATION       FileAllInformation;
					PFILE_BASIC_INFORMATION     FileBasicInformation;
					PFILE_STANDARD_INFORMATION  FileStandardInformation;
					PFILE_INTERNAL_INFORMATION  FileInternalInformation;
					PFILE_EA_INFORMATION        FileEaInformation;
					PFILE_POSITION_INFORMATION  FilePositionInformation;
					PFILE_NAME_INFORMATION      FileNameInformation;

					if (Length < sizeof(FILE_ALL_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileAllInformation = (PFILE_ALL_INFORMATION)Buffer;

					FileBasicInformation =
						&FileAllInformation->BasicInformation;

					FileStandardInformation =
						&FileAllInformation->StandardInformation;

					FileInternalInformation =
						&FileAllInformation->InternalInformation;

					FileEaInformation =
						&FileAllInformation->EaInformation;

					FilePositionInformation =
						&FileAllInformation->PositionInformation;

					FileNameInformation =
						&FileAllInformation->NameInformation;

					FileBasicInformation->CreationTime = FFSSysTime(Fcb->dinode->di_ctime);

					FileBasicInformation->LastAccessTime = FFSSysTime(Fcb->dinode->di_atime);

					FileBasicInformation->LastWriteTime = FFSSysTime(Fcb->dinode->di_mtime);

					FileBasicInformation->ChangeTime = FFSSysTime(Fcb->dinode->di_mtime);

					FileBasicInformation->FileAttributes = Fcb->FFSMcb->FileAttr;

					FileStandardInformation->AllocationSize.QuadPart =
						(LONGLONG)(Fcb->dinode->di_size);

					FileStandardInformation->EndOfFile.QuadPart =
						(LONGLONG)(Fcb->dinode->di_size);

					FileStandardInformation->NumberOfLinks = Fcb->dinode->di_nlink;

					if (IsFlagOn(Fcb->Vcb->Flags, VCB_READ_ONLY))
						FileStandardInformation->DeletePending = FALSE;
					else
						FileStandardInformation->DeletePending = IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING);

					if (FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY))
					{
						FileStandardInformation->Directory = TRUE;
					}
					else
					{
						FileStandardInformation->Directory = FALSE;
					}

					// The "inode number"
					FileInternalInformation->IndexNumber.QuadPart = (LONGLONG)Fcb->FFSMcb->Inode;

					// Romfs doesn't have any extended attributes
					FileEaInformation->EaSize = 0;

					FilePositionInformation->CurrentByteOffset =
						FileObject->CurrentByteOffset;

					if (Length < sizeof(FILE_ALL_INFORMATION) +
							Fcb->FFSMcb->ShortName.Length - sizeof(WCHAR))
					{
						Irp->IoStatus.Information = sizeof(FILE_ALL_INFORMATION);
						Status = STATUS_BUFFER_OVERFLOW;
						__leave;
					}

					FileNameInformation->FileNameLength = Fcb->FFSMcb->ShortName.Length;

					RtlCopyMemory(
							FileNameInformation->FileName,
							Fcb->FFSMcb->ShortName.Buffer,
							Fcb->FFSMcb->ShortName.Length);

					Irp->IoStatus.Information = sizeof(FILE_ALL_INFORMATION) +
						Fcb->FFSMcb->ShortName.Length - sizeof(WCHAR);
					Status = STATUS_SUCCESS;
					__leave;
				}

			/*
			case FileAlternateNameInformation:
			   {
				// TODO: Handle FileAlternateNameInformation

				// Here we would like to use RtlGenerate8dot3Name but I don't
				// know how to use the argument PGENERATE_NAME_CONTEXT
			}
			*/

			case FileNetworkOpenInformation:
				{
					PFILE_NETWORK_OPEN_INFORMATION FileNetworkOpenInformation;

					if (Length < sizeof(FILE_NETWORK_OPEN_INFORMATION))
					{
						Status = STATUS_INFO_LENGTH_MISMATCH;
						__leave;
					}

					FileNetworkOpenInformation =
						(PFILE_NETWORK_OPEN_INFORMATION)Buffer;

					FileNetworkOpenInformation->CreationTime = FFSSysTime(Fcb->dinode->di_ctime);

					FileNetworkOpenInformation->LastAccessTime = FFSSysTime(Fcb->dinode->di_atime);

					FileNetworkOpenInformation->LastWriteTime = FFSSysTime(Fcb->dinode->di_mtime);

					FileNetworkOpenInformation->ChangeTime = FFSSysTime(Fcb->dinode->di_mtime);

					FileNetworkOpenInformation->AllocationSize.QuadPart =
						(LONGLONG)(Fcb->dinode->di_size);

					FileNetworkOpenInformation->EndOfFile.QuadPart =
						(LONGLONG)(Fcb->dinode->di_size);

					FileNetworkOpenInformation->FileAttributes =
						Fcb->FFSMcb->FileAttr;

					Irp->IoStatus.Information =
						sizeof(FILE_NETWORK_OPEN_INFORMATION);
					Status = STATUS_SUCCESS;
					__leave;
				}

			default:
				Status = STATUS_INVALID_INFO_CLASS;
		}
	}
	__finally
	{
		if (FcbResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Fcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (!IrpContext->ExceptionInProgress)
		{
			if (Status == STATUS_PENDING)
			{
				FFSQueueRequest(IrpContext);
			}
			else
			{
				FFSCompleteIrpContext(IrpContext,  Status);
			}
		}
	}

	return Status;
}


NTSTATUS
FFSSetInformation(
	IN PFFS_IRP_CONTEXT IrpContext)
{
	PDEVICE_OBJECT          DeviceObject;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;
	PFFS_VCB                Vcb;
	PFILE_OBJECT            FileObject;
	PFFS_FCB                Fcb;
	PFFS_CCB                Ccb;
	PIRP                    Irp;
	PIO_STACK_LOCATION      IoStackLocation;
	FILE_INFORMATION_CLASS  FileInformationClass;

	ULONG                   NotifyFilter = 0;

	ULONG                   Length;
	PVOID                   Buffer;
	BOOLEAN                 FcbMainResourceAcquired = FALSE;

	BOOLEAN                 VcbResourceAcquired = FALSE;
	BOOLEAN                 FcbPagingIoResourceAcquired = FALSE;


	__try
	{
		ASSERT(IrpContext != NULL);

		ASSERT((IrpContext->Identifier.Type == FFSICX) &&
				(IrpContext->Identifier.Size == sizeof(FFS_IRP_CONTEXT)));

		DeviceObject = IrpContext->DeviceObject;

		//
		// This request is not allowed on the main device object
		//
		if (DeviceObject == FFSGlobal->DeviceObject)
		{
			Status = STATUS_INVALID_DEVICE_REQUEST;
			__leave;
		}

		Vcb = (PFFS_VCB)DeviceObject->DeviceExtension;

		ASSERT(Vcb != NULL);

		ASSERT((Vcb->Identifier.Type == FFSVCB) &&
				(Vcb->Identifier.Size == sizeof(FFS_VCB)));

		ASSERT(IsMounted(Vcb));

		FileObject = IrpContext->FileObject;

		Fcb = (PFFS_FCB)FileObject->FsContext;

		ASSERT(Fcb != NULL);

		//
		// This request is not allowed on volumes
		//
		if (Fcb->Identifier.Type == FFSVCB)
		{
			FFSBreakPoint();

			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		ASSERT((Fcb->Identifier.Type == FFSFCB) &&
				(Fcb->Identifier.Size == sizeof(FFS_FCB)));

		if (IsFlagOn(Fcb->Flags, FCB_FILE_DELETED))
		{
			Status = STATUS_FILE_DELETED;
			__leave;
		}

		Ccb = (PFFS_CCB)FileObject->FsContext2;

		ASSERT(Ccb != NULL);

		ASSERT((Ccb->Identifier.Type == FFSCCB) &&
				(Ccb->Identifier.Size == sizeof(FFS_CCB)));

		Irp = IrpContext->Irp;

		IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

		FileInformationClass =
			IoStackLocation->Parameters.SetFile.FileInformationClass;

		Length = IoStackLocation->Parameters.SetFile.Length;

		Buffer = Irp->AssociatedIrp.SystemBuffer;

		if (IsFlagOn(Vcb->Flags, VCB_READ_ONLY))
		{
			if (FileInformationClass == FileDispositionInformation ||
					FileInformationClass == FileRenameInformation ||
					FileInformationClass == FileLinkInformation)
			{
				if (!ExAcquireResourceExclusiveLite(
							&Vcb->MainResource,
							IrpContext->IsSynchronous))
				{
					Status = STATUS_PENDING;
					__leave;
				}

				VcbResourceAcquired = TRUE;
			}
		}
		else if (!FlagOn(Fcb->Flags, FCB_PAGE_FILE))
		{
			if (!ExAcquireResourceExclusiveLite(
						&Fcb->MainResource,
						IrpContext->IsSynchronous))
			{
				Status = STATUS_PENDING;
				__leave;
			}

			FcbMainResourceAcquired = TRUE;
		}

		if (IsFlagOn(Vcb->Flags, VCB_READ_ONLY))
		{
			if (FileInformationClass != FilePositionInformation)
			{
				Status = STATUS_MEDIA_WRITE_PROTECTED;
				__leave;
			}
		}

		if (FileInformationClass == FileDispositionInformation ||
				FileInformationClass == FileRenameInformation ||
				FileInformationClass == FileLinkInformation ||
				FileInformationClass == FileAllocationInformation ||
				FileInformationClass == FileEndOfFileInformation)
		{
			if (!ExAcquireResourceExclusiveLite(
						&Fcb->PagingIoResource,
						IrpContext->IsSynchronous))
			{
				Status = STATUS_PENDING;
				__leave;
			}

			FcbPagingIoResourceAcquired = TRUE;
		}

		/*        
		if (FileInformationClass != FileDispositionInformation 
			 && FlagOn(Fcb->Flags, FCB_DELETE_PENDING))
		{
			Status = STATUS_DELETE_PENDING;
			__leave;
		}
		*/

		switch (FileInformationClass)
		{
			case FileBasicInformation:
				{
					PFILE_BASIC_INFORMATION FBI = (PFILE_BASIC_INFORMATION)Buffer;               
					PFFS_INODE FFSInode = Fcb->dinode;

					if(FBI->CreationTime.QuadPart)
					{
						FFSInode->di_ctime = (ULONG)(FFSInodeTime(FBI->CreationTime));
					}

					if(FBI->LastAccessTime.QuadPart)
					{
						FFSInode->di_atime = (ULONG)(FFSInodeTime(FBI->LastAccessTime));
					}

					if(FBI->LastWriteTime.QuadPart)
					{
						FFSInode->di_mtime = (ULONG)(FFSInodeTime(FBI->LastWriteTime));
					}

					if (IsFlagOn(FBI->FileAttributes, FILE_ATTRIBUTE_READONLY)) 
					{
						FFSSetReadOnly(Fcb->dinode->di_mode);
						SetFlag(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_READONLY);
					}
					else
					{
						FFSSetWritable(Fcb->dinode->di_mode);
						ClearFlag(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_READONLY);
					}

					if(FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, FFSInode))
					{
						Status = STATUS_SUCCESS;
					}

					if (FBI->FileAttributes & FILE_ATTRIBUTE_TEMPORARY) 
					{
						SetFlag(FileObject->Flags, FO_TEMPORARY_FILE);
					} 
					else 
					{
						ClearFlag(FileObject->Flags, FO_TEMPORARY_FILE);
					}

					NotifyFilter = FILE_NOTIFY_CHANGE_ATTRIBUTES |
						FILE_NOTIFY_CHANGE_CREATION |
						FILE_NOTIFY_CHANGE_LAST_ACCESS |
						FILE_NOTIFY_CHANGE_LAST_WRITE ;

					Status = STATUS_SUCCESS;
				}
				break;

			case FileAllocationInformation:
				{
					PFILE_ALLOCATION_INFORMATION FAI = (PFILE_ALLOCATION_INFORMATION)Buffer;

					if (FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY))
					{
						Status = STATUS_INVALID_DEVICE_REQUEST;
						__leave;
					}

					if (FAI->AllocationSize.QuadPart == 
							Fcb->Header.AllocationSize.QuadPart)
					{
						Status = STATUS_SUCCESS;
					}
					else if (FAI->AllocationSize.QuadPart >
							Fcb->Header.AllocationSize.QuadPart)
					{
						if(FFSExpandFile(IrpContext,
									Vcb, Fcb,
									&(FAI->AllocationSize)))
						{
							if (FFSSaveInode(IrpContext,
										Vcb,
										Fcb->FFSMcb->Inode,
										Fcb->dinode))
							{
								Status = STATUS_SUCCESS;
							}
						}
						else
						{
							Status = STATUS_INSUFFICIENT_RESOURCES;
						}
					}
					else
					{
						if (MmCanFileBeTruncated(&(Fcb->SectionObject), &(FAI->AllocationSize))) 
						{
							LARGE_INTEGER EndOfFile;

							EndOfFile.QuadPart = FAI->AllocationSize.QuadPart +
								(LONGLONG)(Vcb->BlockSize - 1);

							if(FFSTruncateFile(IrpContext, Vcb, Fcb, &(EndOfFile)))
							{
								if (FAI->AllocationSize.QuadPart < 
										Fcb->Header.FileSize.QuadPart)
								{
									Fcb->Header.FileSize.QuadPart = 
										FAI->AllocationSize.QuadPart;
								}

								FFSSaveInode(IrpContext,
										Vcb,
										Fcb->FFSMcb->Inode,
										Fcb->dinode);

								Status = STATUS_SUCCESS;
							}
						}
						else
						{
							Status = STATUS_USER_MAPPED_FILE;
							__leave;
						}
					}

					if (NT_SUCCESS(Status))
					{
						CcSetFileSizes(FileObject, 
								(PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
						SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

						NotifyFilter = FILE_NOTIFY_CHANGE_SIZE |
							FILE_NOTIFY_CHANGE_LAST_WRITE ;

					}

				}
				break;

			case FileEndOfFileInformation:
				{
					PFILE_END_OF_FILE_INFORMATION FEOFI = (PFILE_END_OF_FILE_INFORMATION) Buffer;

					BOOLEAN CacheInitialized = FALSE;

					if (IsDirectory(Fcb))
					{
						Status = STATUS_INVALID_DEVICE_REQUEST;
						__leave;
					}

					if (FEOFI->EndOfFile.HighPart != 0)
					{
						Status = STATUS_INVALID_PARAMETER;
						__leave;
					}


					if (IoStackLocation->Parameters.SetFile.AdvanceOnly)
					{
						Status = STATUS_SUCCESS;
						__leave;
					}

					if ((FileObject->SectionObjectPointer->DataSectionObject != NULL) &&
							(FileObject->SectionObjectPointer->SharedCacheMap == NULL) &&
							!FlagOn(Irp->Flags, IRP_PAGING_IO)) {

						ASSERT(!FlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE));

						CcInitializeCacheMap(
								FileObject,
								(PCC_FILE_SIZES)&(Fcb->Header.AllocationSize),
								FALSE,
								&(FFSGlobal->CacheManagerNoOpCallbacks),
								Fcb);

						CacheInitialized = TRUE;
					}

					if (FEOFI->EndOfFile.QuadPart == 
							Fcb->Header.AllocationSize.QuadPart)
					{
						Status = STATUS_SUCCESS;
					}
					else if (FEOFI->EndOfFile.QuadPart > 
							Fcb->Header.AllocationSize.QuadPart)
					{
						LARGE_INTEGER FileSize = Fcb->Header.FileSize;

						if(FFSExpandFile(IrpContext, Vcb, Fcb, &(FEOFI->EndOfFile)))
						{
							{
								Fcb->Header.FileSize.QuadPart = 
									FEOFI->EndOfFile.QuadPart;
								Fcb->dinode->di_size = (ULONG)FEOFI->EndOfFile.QuadPart;
								Fcb->Header.ValidDataLength.QuadPart = 
									(LONGLONG)(0x7fffffffffffffff);
							}

							if (FFSSaveInode(IrpContext,
										Vcb,
										Fcb->FFSMcb->Inode,
										Fcb->dinode))
							{
								Status = STATUS_SUCCESS;
							}
						}
						else
						{
							Status = STATUS_INSUFFICIENT_RESOURCES;
						}


						if (NT_SUCCESS(Status))
						{
							CcSetFileSizes(FileObject, 
									(PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));

							SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

							FFSZeroHoles(IrpContext, 
									Vcb, FileObject, 
									FileSize.QuadPart,
									Fcb->Header.AllocationSize.QuadPart - 
									FileSize.QuadPart);

							NotifyFilter = FILE_NOTIFY_CHANGE_SIZE |
								FILE_NOTIFY_CHANGE_LAST_WRITE ;

						}
					}
					else
					{
						if (MmCanFileBeTruncated(&(Fcb->SectionObject), &(FEOFI->EndOfFile))) 
						{
							LARGE_INTEGER EndOfFile = FEOFI->EndOfFile;

							EndOfFile.QuadPart = EndOfFile.QuadPart + 
								(LONGLONG)(Vcb->BlockSize - 1);

							if(FFSTruncateFile(IrpContext, Vcb, Fcb, &(EndOfFile)))
							{
								Fcb->Header.FileSize.QuadPart = 
									FEOFI->EndOfFile.QuadPart;
								Fcb->dinode->di_size = (ULONG)FEOFI->EndOfFile.QuadPart;

								FFSSaveInode(IrpContext,
										Vcb,
										Fcb->FFSMcb->Inode,
										Fcb->dinode);

								Status = STATUS_SUCCESS;
							}
						}
						else
						{
							Status = STATUS_USER_MAPPED_FILE;
							__leave;
						}

						if (NT_SUCCESS(Status))
						{
							CcSetFileSizes(FileObject, 
									(PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));

							SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

							NotifyFilter = FILE_NOTIFY_CHANGE_SIZE |
								FILE_NOTIFY_CHANGE_LAST_WRITE ;

						}
					}
				}

				break;

			case FileDispositionInformation:
				{
					PFILE_DISPOSITION_INFORMATION FDI = (PFILE_DISPOSITION_INFORMATION)Buffer;

					Status = FFSSetDispositionInfo(IrpContext, Vcb, Fcb, FDI->DeleteFile);
				}

				break;

			case FileRenameInformation:
				{
					Status = FFSSetRenameInfo(IrpContext, Vcb, Fcb);
				}

				break;

				//
				// This is the only set file information request supported on read
				// only file systems
				//
			case FilePositionInformation:
				{
					PFILE_POSITION_INFORMATION FilePositionInformation;

					if (Length < sizeof(FILE_POSITION_INFORMATION))
					{
						Status = STATUS_INVALID_PARAMETER;
						__leave;
					}

					FilePositionInformation = (PFILE_POSITION_INFORMATION) Buffer;

					if ((FlagOn(FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING)) &&
							(FilePositionInformation->CurrentByteOffset.LowPart &
							 DeviceObject->AlignmentRequirement))
					{
						Status = STATUS_INVALID_PARAMETER;
						__leave;
					}

					FileObject->CurrentByteOffset =
						FilePositionInformation->CurrentByteOffset;

					Status = STATUS_SUCCESS;
					__leave;
				}

				break;

			default:
				Status = STATUS_INVALID_INFO_CLASS;
		}
	}
	__finally
	{

		if (FcbPagingIoResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Fcb->PagingIoResource,
					ExGetCurrentResourceThread());
		}

		if (NT_SUCCESS(Status) && (NotifyFilter != 0))
		{
			FFSNotifyReportChange(
					IrpContext,
					Vcb,
					Fcb,
					NotifyFilter,
					FILE_ACTION_MODIFIED);

		}

		if (FcbMainResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Fcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (VcbResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Vcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (!IrpContext->ExceptionInProgress)
		{
			if (Status == STATUS_PENDING)
			{
				FFSQueueRequest(IrpContext);
			}
			else
			{
				FFSCompleteIrpContext(IrpContext,  Status);
			}
		}
	}

	return Status;
}


BOOLEAN
FFSExpandFile(
	PFFS_IRP_CONTEXT IrpContext, 
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	PLARGE_INTEGER   AllocationSize)
{
	ULONG   dwRet = 0;
	BOOLEAN bRet = TRUE;

	if (AllocationSize->QuadPart <= Fcb->Header.AllocationSize.QuadPart)
	{
		return TRUE;
	}

	if (((LONGLONG)SUPER_BLOCK->fs_size - (LONGLONG)SUPER_BLOCK->fs_dsize) * Vcb->BlockSize <=
			(AllocationSize->QuadPart - Fcb->Header.AllocationSize.QuadPart))
	{
		FFSPrint((DBG_ERROR, "FFSExpandFile: There is no enough disk space available.\n"));
		return FALSE;
	}

	while (bRet && (AllocationSize->QuadPart > Fcb->Header.AllocationSize.QuadPart))
	{
		bRet = FFSExpandInode(IrpContext, Vcb, Fcb, &dwRet);
	}

	return bRet;
}


BOOLEAN
FFSTruncateFile(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	PLARGE_INTEGER   AllocationSize)
{
	BOOLEAN bRet = TRUE;

	while (bRet && (AllocationSize->QuadPart <
				Fcb->Header.AllocationSize.QuadPart))
	{
		bRet = FFSTruncateInode(IrpContext, Vcb, Fcb);
	}

	return bRet;
}


NTSTATUS
FFSSetDispositionInfo(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	BOOLEAN          bDelete)
{
	PIRP    Irp = IrpContext->Irp;
	PIO_STACK_LOCATION IrpSp;

	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	FFSPrint((DBG_INFO, "FFSSetDispositionInfo: bDelete=%x\n", bDelete));

	if (bDelete)
	{
		FFSPrint((DBG_INFO, "FFSSetDispositionInformation: MmFlushImageSection on %s.\n", 
					Fcb->AnsiFileName.Buffer));

		if (!MmFlushImageSection(&Fcb->SectionObject,
					MmFlushForDelete))
		{
			return STATUS_CANNOT_DELETE;
		}

		if (Fcb->FFSMcb->Inode == FFS_ROOT_INO)
		{
			return STATUS_CANNOT_DELETE;
		}

		if (IsDirectory(Fcb))
		{
			if (!FFSIsDirectoryEmpty(Vcb, Fcb))
			{
				return STATUS_DIRECTORY_NOT_EMPTY;
			}
		}

		SetFlag(Fcb->Flags, FCB_DELETE_PENDING);
		IrpSp->FileObject->DeletePending = TRUE;

		if (IsDirectory(Fcb))
		{
			FsRtlNotifyFullChangeDirectory(Vcb->NotifySync,
					&Vcb->NotifyList,
					Fcb,
					NULL,
					FALSE,
					FALSE,
					0,
					NULL,
					NULL,
					NULL);
		}
	}
	else
	{
		ClearFlag(Fcb->Flags, FCB_DELETE_PENDING);
		IrpSp->FileObject->DeletePending = FALSE;
	}

	return STATUS_SUCCESS;
}  


NTSTATUS
FFSSetRenameInfo(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb)
{
	PFFS_FCB                TargetDcb;
	PFFS_MCB                TargetMcb;

	PFFS_MCB                Mcb;
	FFS_INODE               Inode;

	UNICODE_STRING          FileName;

	NTSTATUS                Status;

	PIRP                    Irp;
	PIO_STACK_LOCATION      IrpSp;

	PFILE_OBJECT            FileObject;
	PFILE_OBJECT            TargetObject;
	BOOLEAN                 ReplaceIfExists;

	BOOLEAN                 bMove = FALSE;

	PFILE_RENAME_INFORMATION    FRI;

	if (Fcb->FFSMcb->Inode == FFS_ROOT_INO)
	{
		Status = STATUS_INVALID_PARAMETER;
		goto errorout;
	}

	Irp = IrpContext->Irp;
	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	FileObject = IrpSp->FileObject;
	TargetObject = IrpSp->Parameters.SetFile.FileObject;
	ReplaceIfExists = IrpSp->Parameters.SetFile.ReplaceIfExists;

	FRI = (PFILE_RENAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

	if (TargetObject == NULL)
	{
		UNICODE_STRING  NewName;

		NewName.Buffer = FRI->FileName;
		NewName.MaximumLength = NewName.Length = (USHORT)FRI->FileNameLength;

		while (NewName.Length > 0 && NewName.Buffer[NewName.Length / 2 - 1] == L'\\')
		{
			NewName.Buffer[NewName.Length / 2 - 1] = 0;
			NewName.Length -= 2;
		}

		while (NewName.Length > 0 && NewName.Buffer[NewName.Length / 2 - 1] != L'\\')
		{
			NewName.Length -= 2;
		}

		NewName.Buffer = (USHORT *)((UCHAR *)NewName.Buffer + NewName.Length);
		NewName.Length = (USHORT)(FRI->FileNameLength - NewName.Length);

		FileName = NewName;

		TargetDcb = NULL;
		TargetMcb = Fcb->FFSMcb->Parent;

		if (FileName.Length >= FFS_NAME_LEN*sizeof(USHORT))
		{
			Status = STATUS_OBJECT_NAME_INVALID;
			goto errorout;
		}
	}
	else
	{
		TargetDcb = (PFFS_FCB)(TargetObject->FsContext);

		if (!TargetDcb || TargetDcb->Vcb != Vcb)
		{
			FFSBreakPoint();

			Status = STATUS_INVALID_PARAMETER;
			goto errorout;
		}

		TargetMcb = TargetDcb->FFSMcb;

		FileName = TargetObject->FileName;
	}

	if (FsRtlDoesNameContainWildCards(&FileName))
	{
		Status = STATUS_OBJECT_NAME_INVALID;
		goto errorout;
	}

	if (TargetMcb->Inode == Fcb->FFSMcb->Parent->Inode)
	{
		if (FsRtlAreNamesEqual(&FileName,
					&(Fcb->FFSMcb->ShortName),
					FALSE,
					NULL))
		{
			Status = STATUS_SUCCESS;
			goto errorout;
		}
	}
	else
	{
		bMove = TRUE;
	}

	TargetDcb = TargetMcb->FFSFcb;

	if (!TargetDcb)
		TargetDcb = FFSCreateFcbFromMcb(Vcb, TargetMcb);

	if ((TargetMcb->Inode != Fcb->FFSMcb->Parent->Inode) &&
			(Fcb->FFSMcb->Parent->FFSFcb == NULL))
	{
		FFSCreateFcbFromMcb(Vcb, Fcb->FFSMcb->Parent);
	}

	if (!TargetDcb || !(Fcb->FFSMcb->Parent->FFSFcb))
	{
		Status = STATUS_UNSUCCESSFUL;

		goto errorout;
	}

	Mcb = NULL;
	Status = FFSLookupFileName(
				Vcb,
				&FileName,
				TargetMcb,
				&Mcb,
				&Inode); 

	if (NT_SUCCESS(Status))   
	{
		if ((!ReplaceIfExists) ||
				(IsFlagOn(Mcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY)) ||
				(IsFlagOn(Mcb->FileAttr, FILE_ATTRIBUTE_READONLY)))
		{
			Status = STATUS_OBJECT_NAME_COLLISION;
			goto errorout;
		}

		if (ReplaceIfExists)
		{
			Status = STATUS_NOT_IMPLEMENTED;
			goto errorout;
		}
	}

	if (IsDirectory(Fcb))
	{

		Status = FFSRemoveEntry(IrpContext, Vcb, 
					Fcb->FFSMcb->Parent->FFSFcb,
					DT_DIR,
					Fcb->FFSMcb->Inode);

		if (!NT_SUCCESS(Status))
		{
			FFSBreakPoint();

			goto errorout;
		}

		Status = FFSAddEntry(IrpContext, Vcb, 
					TargetDcb,
					DT_DIR,
					Fcb->FFSMcb->Inode,
					&FileName);

		if (!NT_SUCCESS(Status))
		{
			FFSBreakPoint();

			FFSAddEntry(IrpContext, Vcb, 
					Fcb->FFSMcb->Parent->FFSFcb,
					DT_DIR,
					Fcb->FFSMcb->Inode,
					&Fcb->FFSMcb->ShortName);

			goto errorout;
		}

		if(!FFSSaveInode(IrpContext,
					Vcb, 
					TargetMcb->Inode,
					TargetDcb->dinode))
		{
			Status = STATUS_UNSUCCESSFUL;

			FFSBreakPoint();

			goto errorout;
		}

		if(!FFSSaveInode(IrpContext,
					Vcb, 
					Fcb->FFSMcb->Parent->Inode,
					Fcb->FFSMcb->Parent->FFSFcb->dinode))
		{
			Status = STATUS_UNSUCCESSFUL;

			FFSBreakPoint();

			goto errorout;
		}

		Status = FFSSetParentEntry(IrpContext, Vcb, Fcb,
					Fcb->FFSMcb->Parent->Inode,
					TargetDcb->FFSMcb->Inode);


		if (!NT_SUCCESS(Status))
		{
			FFSBreakPoint();
			goto errorout;
		}
	}
	else
	{
		Status = FFSRemoveEntry(IrpContext, Vcb,
					Fcb->FFSMcb->Parent->FFSFcb,
					DT_REG,
					Fcb->FFSMcb->Inode);
		if (!NT_SUCCESS(Status))
		{
			FFSBreakPoint();
			goto errorout;
		}

		Status = FFSAddEntry(IrpContext,
					Vcb, TargetDcb,
					DT_REG,
					Fcb->FFSMcb->Inode,
					&FileName);

		if (!NT_SUCCESS(Status))
		{
			FFSBreakPoint();

			FFSAddEntry(IrpContext, Vcb, 
					Fcb->FFSMcb->Parent->FFSFcb,
					DT_REG,
					Fcb->FFSMcb->Inode,
					&Fcb->FFSMcb->ShortName);

			goto errorout;
		}
	}

	if (NT_SUCCESS(Status))
	{
		if (Fcb->FFSMcb->ShortName.MaximumLength < (FileName.Length + 2))
		{
			ExFreePool(Fcb->FFSMcb->ShortName.Buffer);
			Fcb->FFSMcb->ShortName.Buffer = 
				ExAllocatePool(PagedPool, FileName.Length + 2);

			if (!Fcb->FFSMcb->ShortName.Buffer)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				goto errorout;
			}

			Fcb->FFSMcb->ShortName.MaximumLength = FileName.Length + 2;
		}

		{
			RtlZeroMemory(Fcb->FFSMcb->ShortName.Buffer,
					Fcb->FFSMcb->ShortName.MaximumLength);

			RtlCopyMemory(Fcb->FFSMcb->ShortName.Buffer,
					FileName.Buffer, FileName.Length);

			Fcb->FFSMcb->ShortName.Length = FileName.Length;
		}

#if DBG    

		Fcb->AnsiFileName.Length = (USHORT)
			RtlxUnicodeStringToOemSize(&FileName);

		if (Fcb->AnsiFileName.MaximumLength < FileName.Length)
		{
			ExFreePool(Fcb->AnsiFileName.Buffer);

			Fcb->AnsiFileName.Buffer = 
				ExAllocatePool(PagedPool, Fcb->AnsiFileName.Length + 1);

			if (!Fcb->AnsiFileName.Buffer)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				goto errorout;
			}

			RtlZeroMemory(Fcb->AnsiFileName.Buffer, 
					Fcb->AnsiFileName.Length + 1);
			Fcb->AnsiFileName.MaximumLength = 
				Fcb->AnsiFileName.Length + 1;
		}

		FFSUnicodeToOEM(&(Fcb->AnsiFileName),
				&FileName);

#endif

		if (bMove)
		{
			FFSNotifyReportChange(
					IrpContext,
					Vcb,
					Fcb,
					(IsDirectory(Fcb) ?
						FILE_NOTIFY_CHANGE_DIR_NAME :
						FILE_NOTIFY_CHANGE_FILE_NAME),
					FILE_ACTION_REMOVED);

		}
		else
		{
			FFSNotifyReportChange(
					IrpContext,
					Vcb,
					Fcb,
					(IsDirectory(Fcb) ?
						FILE_NOTIFY_CHANGE_DIR_NAME :
						FILE_NOTIFY_CHANGE_FILE_NAME),
					FILE_ACTION_RENAMED_OLD_NAME);

		}

		FFSDeleteMcbNode(Vcb, Fcb->FFSMcb->Parent, Fcb->FFSMcb);
		FFSAddMcbNode(Vcb, TargetMcb, Fcb->FFSMcb);

		if (bMove)
		{
			FFSNotifyReportChange(
					IrpContext,
					Vcb,
					Fcb,
					(IsDirectory(Fcb) ?
						FILE_NOTIFY_CHANGE_DIR_NAME :
						FILE_NOTIFY_CHANGE_FILE_NAME),
					FILE_ACTION_ADDED);
		}
		else
		{
			FFSNotifyReportChange(
					IrpContext,
					Vcb,
					Fcb,
					(IsDirectory(Fcb) ?
						FILE_NOTIFY_CHANGE_DIR_NAME :
						FILE_NOTIFY_CHANGE_FILE_NAME),
					FILE_ACTION_RENAMED_NEW_NAME);

		}
	}

errorout:

	return Status;
}


BOOLEAN
FFSDeleteFile(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb)
{
	BOOLEAN         bRet = FALSE;
	LARGE_INTEGER   AllocationSize;
	PFFS_FCB        Dcb = NULL;

	NTSTATUS        Status;

	FFSPrint((DBG_INFO, "FFSDeleteFile: File %S (%xh) will be deleted!\n",
				Fcb->FFSMcb->ShortName.Buffer, Fcb->FFSMcb->Inode));

	if (IsFlagOn(Fcb->Flags, FCB_FILE_DELETED))
	{
		return TRUE;
	}

	if (FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY))
	{
		if (!FFSIsDirectoryEmpty(Vcb, Fcb))
		{
			ClearFlag(Fcb->Flags, FCB_DELETE_PENDING);

			return FALSE;
		}
	}

	FFSPrint((DBG_INFO, "FFSDeleteFile: FFSSB->S_FREE_BLOCKS = %xh .\n",
				Vcb->ffs_super_block->fs_size - Vcb->ffs_super_block->fs_dsize));

	Status = STATUS_UNSUCCESSFUL;

	{
		if (Fcb->FFSMcb->Parent->FFSFcb)
		{
			Status = FFSRemoveEntry(
					IrpContext, Vcb, 
					Fcb->FFSMcb->Parent->FFSFcb,
					(FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY) ?
						DT_DIR : DT_REG),
					Fcb->FFSMcb->Inode);
		}
		else
		{
			Dcb = FFSCreateFcbFromMcb(Vcb, Fcb->FFSMcb->Parent);
			if (Dcb)
			{
				Status = FFSRemoveEntry(
						IrpContext, Vcb, Dcb,
						(FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY) ?
							DT_DIR : DT_REG),
						Fcb->FFSMcb->Inode);
			}
		}
	}

	if (NT_SUCCESS(Status))
	{
		Fcb->dinode->di_nlink--;

		if (IsDirectory(Fcb))
		{
			if (Fcb->dinode->di_nlink < 2)
			{
				bRet = TRUE;
			}
		}
		else
		{
			if (Fcb->dinode->di_nlink == 0)
			{
				bRet = TRUE;
			}
		}
	}


	if (bRet)
	{
		AllocationSize.QuadPart = (LONGLONG)0;
		bRet = FFSTruncateFile(IrpContext, Vcb, Fcb, &AllocationSize);

		//
		// Update the inode's data length . It should be ZERO if succeeds.
		//

		if (Fcb->dinode->di_size > Fcb->Header.AllocationSize.LowPart)
		{
			Fcb->dinode->di_size = Fcb->Header.AllocationSize.LowPart;
		}

		Fcb->Header.FileSize.QuadPart = (LONGLONG) Fcb->dinode->di_size;

		if (bRet)
		{
			{
				LARGE_INTEGER SysTime;
				KeQuerySystemTime(&SysTime);

				/*Fcb->dinode->di_dtime = FFSInodeTime(SysTime); XXX */

				FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, Fcb->dinode);
			}

			if (IsDirectory(Fcb))
			{
				bRet = FFSFreeInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, DT_DIR);
			}
			else
			{
				bRet = FFSFreeInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, DT_REG);
			}

			SetFlag(Fcb->Flags, FCB_FILE_DELETED);
			FFSDeleteMcbNode(Vcb, Fcb->FFSMcb->Parent, Fcb->FFSMcb);
		}
		else
		{
			FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, Fcb->dinode);
		}
	}
	else
	{
		FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, Fcb->dinode);
	}

	FFSPrint((DBG_INFO, "FFSDeleteFile: Succeed... FFSSB->S_FREE_BLOCKS = %xh .\n",
				Vcb->ffs_super_block->fs_size - Vcb->ffs_super_block->fs_dsize));

	return bRet;
}
