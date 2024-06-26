/* 
 * FFS File System Driver for Windows
 *
 * write.c
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

typedef struct _FFS_FLPFLUSH_CONTEXT {

	PFFS_VCB     Vcb;
	PFFS_FCB     Fcb;
	PFILE_OBJECT FileObject;

	KDPC         Dpc;
	KTIMER       Timer;
	WORK_QUEUE_ITEM Item;

} FFS_FLPFLUSH_CONTEXT, *PFFS_FLPFLUSH_CONTEXT;

VOID
FFSFloppyFlush(
	IN PVOID Parameter);

VOID
FFSFloppyFlushDpc(
	IN PKDPC Dpc,
	IN PVOID DeferredContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2);



NTSTATUS
FFSWriteComplete(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSWriteFile(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSWriteVolume(
	IN PFFS_IRP_CONTEXT IrpContext);

VOID
FFSDeferWrite(
	IN   PFFS_IRP_CONTEXT,
	PIRP Irp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FFSFloppyFlush)
#pragma alloc_text(PAGE, FFSStartFloppyFlushDpc)
#pragma alloc_text(PAGE, FFSZeroHoles)
#pragma alloc_text(PAGE, FFSWrite)
#pragma alloc_text(PAGE, FFSWriteVolume)
#pragma alloc_text(PAGE, FFSWriteInode)
#pragma alloc_text(PAGE, FFSWriteFile)
#pragma alloc_text(PAGE, FFSWriteComplete)
#endif


VOID
FFSFloppyFlush(
	IN PVOID Parameter)
{
	PFFS_FLPFLUSH_CONTEXT Context;
	PFILE_OBJECT          FileObject;
	PFFS_FCB              Fcb;
	PFFS_VCB              Vcb;

	Context = (PFFS_FLPFLUSH_CONTEXT) Parameter;
	FileObject = Context->FileObject;
	Fcb = Context->Fcb;
	Vcb = Context->Vcb;

	FFSPrint((DBG_USER, "FFSFloppyFlushing ...\n"));

	IoSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);

	if (Vcb)
	{
		ExAcquireSharedStarveExclusive(&Vcb->PagingIoResource, TRUE);
		ExReleaseResource(&Vcb->PagingIoResource);

		CcFlushCache(&(Vcb->SectionObject), NULL, 0, NULL);
	}

	if (FileObject)
	{
		ASSERT(Fcb == (PFFS_FCB)FileObject->FsContext);

		ExAcquireSharedStarveExclusive(&Fcb->PagingIoResource, TRUE);
		ExReleaseResource(&Fcb->PagingIoResource);

		CcFlushCache(&(Fcb->SectionObject), NULL, 0, NULL);

		ObDereferenceObject(FileObject);
	}

	IoSetTopLevelIrp(NULL);

	ExFreePool(Parameter);
}


VOID
FFSFloppyFlushDpc(
	IN PKDPC Dpc,
	IN PVOID DeferredContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2)
{
	PFFS_FLPFLUSH_CONTEXT Context;

	Context = (PFFS_FLPFLUSH_CONTEXT)DeferredContext;

	FFSPrint((DBG_USER, "FFSFloppyFlushDpc is to be started...\n"));

	ExInitializeWorkItem(&Context->Item,
			FFSFloppyFlush,
			Context);

	ExQueueWorkItem(&Context->Item, CriticalWorkQueue);
}


VOID
FFSStartFloppyFlushDpc(
	PFFS_VCB     Vcb,
	PFFS_FCB     Fcb,
	PFILE_OBJECT FileObject)
{
	LARGE_INTEGER          OneSecond;
	PFFS_FLPFLUSH_CONTEXT Context;

	ASSERT(IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK));

	Context = ExAllocatePool(NonPagedPool, sizeof(PFFS_FLPFLUSH_CONTEXT));

	if (!Context)
	{
		FFSBreakPoint();
		return;
	}

	KeInitializeTimer(&Context->Timer);

	KeInitializeDpc(&Context->Dpc,
			FFSFloppyFlushDpc,
			Context);

	Context->Vcb = Vcb;
	Context->Fcb = Fcb;
	Context->FileObject = FileObject;

	if (FileObject)
	{
		ObReferenceObject(FileObject);
	}

	OneSecond.QuadPart = (LONGLONG) - 1 * 1000 * 1000 * 10;
	KeSetTimer(&Context->Timer,
			OneSecond,
			&Context->Dpc);
}


BOOLEAN
FFSZeroHoles(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFILE_OBJECT     FileObject,
	IN LONGLONG         Offset,
	IN LONGLONG         Count)
{
	LARGE_INTEGER StartAddr = {0, 0};
	LARGE_INTEGER EndAddr = {0, 0};

	StartAddr.QuadPart = (Offset + (SECTOR_SIZE - 1)) &
		~((LONGLONG)SECTOR_SIZE - 1);

	EndAddr.QuadPart = (Offset + Count + (SECTOR_SIZE - 1)) &
		~((LONGLONG)SECTOR_SIZE - 1);

	if (StartAddr.QuadPart < EndAddr.QuadPart)
	{
		return CcZeroData(FileObject,
				&StartAddr,
				&EndAddr,
				IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT));
	}

	return TRUE;
}


VOID
FFSDeferWrite(
	IN PFFS_IRP_CONTEXT IrpContext,
	PIRP Irp)
{
	ASSERT(IrpContext->Irp == Irp);

	FFSQueueRequest(IrpContext);
}


NTSTATUS
FFSWriteVolume(
	IN PFFS_IRP_CONTEXT IrpContext)
{
	NTSTATUS            Status = STATUS_UNSUCCESSFUL;

	PFFS_VCB            Vcb;
	PFFS_CCB            Ccb;
	PFFS_FCBVCB         FcbOrVcb;
	PFILE_OBJECT        FileObject;

	PDEVICE_OBJECT      DeviceObject;

	PIRP                Irp;
	PIO_STACK_LOCATION  IoStackLocation;

	ULONG               Length;
	LARGE_INTEGER       ByteOffset;

	BOOLEAN             PagingIo;
	BOOLEAN             Nocache;
	BOOLEAN             SynchronousIo;
	BOOLEAN             MainResourceAcquired = FALSE;
	BOOLEAN             PagingIoResourceAcquired = FALSE;

	BOOLEAN             bDeferred = FALSE;

	PUCHAR              Buffer;

	__try
	{
		ASSERT(IrpContext);

		ASSERT((IrpContext->Identifier.Type == FFSICX) &&
				(IrpContext->Identifier.Size == sizeof(FFS_IRP_CONTEXT)));

		DeviceObject = IrpContext->DeviceObject;

		Vcb = (PFFS_VCB)DeviceObject->DeviceExtension;

		ASSERT(Vcb != NULL);

		ASSERT((Vcb->Identifier.Type == FFSVCB) &&
				(Vcb->Identifier.Size == sizeof(FFS_VCB)));

		FileObject = IrpContext->FileObject;

		FcbOrVcb = (PFFS_FCBVCB)FileObject->FsContext;

		ASSERT(FcbOrVcb);

		if (!(FcbOrVcb->Identifier.Type == FFSVCB && (PVOID)FcbOrVcb == (PVOID)Vcb))
		{
			Status = STATUS_INVALID_DEVICE_REQUEST;
			__leave;
		}

		Ccb = (PFFS_CCB)FileObject->FsContext2;

		Irp = IrpContext->Irp;

		IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

		Length = IoStackLocation->Parameters.Write.Length;
		ByteOffset = IoStackLocation->Parameters.Write.ByteOffset;

		PagingIo = (Irp->Flags & IRP_PAGING_IO ? TRUE : FALSE);
		Nocache = (Irp->Flags & IRP_NOCACHE ? TRUE : FALSE);
		SynchronousIo = (FileObject->Flags & FO_SYNCHRONOUS_IO ? TRUE : FALSE);

		FFSPrint((DBG_INFO, "FFSWriteVolume: Off=%I64xh Len=%xh Paging=%xh Nocache=%xh\n",
					ByteOffset.QuadPart, Length, PagingIo, Nocache));

		if (Length == 0)
		{
			Irp->IoStatus.Information = 0;
			Status = STATUS_SUCCESS;
			__leave;
		}

		// For the case of "Direct Access Storage Device", we
		// need flush/purge the cache

		if (Ccb != NULL)
		{
			ExAcquireResourceExclusive(&Vcb->MainResource, TRUE);
			MainResourceAcquired = TRUE;

			Status = FFSPurgeVolume(Vcb, TRUE);

			ExReleaseResource(&Vcb->MainResource);
			MainResourceAcquired = FALSE;

			if(!IsFlagOn(Ccb->Flags, CCB_ALLOW_EXTENDED_DASD_IO))
			{
				if (ByteOffset.QuadPart + Length > Vcb->Header.FileSize.QuadPart)
				{
					Length = (ULONG)(Vcb->Header.FileSize.QuadPart - ByteOffset.QuadPart);
				}
			}

			{
				FFS_BDL BlockArray;

				if ((ByteOffset.LowPart & (SECTOR_SIZE - 1)) ||
						(Length & (SECTOR_SIZE - 1)))
				{
					Status = STATUS_INVALID_PARAMETER;
					__leave;
				}

				Status = FFSLockUserBuffer(
							IrpContext->Irp,
							Length,
							IoReadAccess);

				if (!NT_SUCCESS(Status))
				{
					__leave;
				}

				BlockArray.Irp = NULL;
				BlockArray.Lba = ByteOffset.QuadPart;;
				BlockArray.Offset = 0;
				BlockArray.Length = Length;

				Status = FFSReadWriteBlocks(IrpContext,
							Vcb,
							&BlockArray,
							Length,
							1,
							FALSE);
				Irp = IrpContext->Irp;

				__leave;
			}
		}                    

		if (Nocache &&
				(ByteOffset.LowPart & (SECTOR_SIZE - 1) ||
				 Length & (SECTOR_SIZE - 1)))
		{
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		if (FlagOn(IrpContext->MinorFunction, IRP_MN_DPC))
		{
			ClearFlag(IrpContext->MinorFunction, IRP_MN_DPC);
			Status = STATUS_PENDING;
			__leave;
		}

		if (ByteOffset.QuadPart >=
				Vcb->PartitionInformation.PartitionLength.QuadPart)
		{
			Irp->IoStatus.Information = 0;
			Status = STATUS_END_OF_FILE;
			__leave;
		}

#if FALSE

		if (!Nocache)
		{
			BOOLEAN bAgain = IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED);
			BOOLEAN bWait  = IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
			BOOLEAN bQueue = IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_REQUEUED);

			if (!CcCanIWrite(
						FileObject,
						Length,
						(bWait && bQueue),
						bAgain))
			{
				SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED);

				CcDeferWrite(FileObject,
						(PCC_POST_DEFERRED_WRITE)FFSDeferWrite,
						IrpContext,
						Irp,
						Length,
						bAgain);

				bDeferred = TRUE;

				FFSBreakPoint();

				Status = STATUS_PENDING;

				__leave;
			}
		}

#endif

		if (Nocache && !PagingIo && (Vcb->SectionObject.DataSectionObject != NULL)) 
		{
			ExAcquireResourceExclusive(&Vcb->MainResource, TRUE);
			MainResourceAcquired = TRUE;

			ExAcquireSharedStarveExclusive(&Vcb->PagingIoResource, TRUE);
			ExReleaseResource(&Vcb->PagingIoResource);

			CcFlushCache(&(Vcb->SectionObject),
					&ByteOffset,
					Length,
					&(Irp->IoStatus));

			if (!NT_SUCCESS(Irp->IoStatus.Status)) 
			{
				Status = Irp->IoStatus.Status;
				__leave;
			}

			ExAcquireSharedStarveExclusive(&Vcb->PagingIoResource, TRUE);
			ExReleaseResource(&Vcb->PagingIoResource);

			CcPurgeCacheSection(&(Vcb->SectionObject),
					(PLARGE_INTEGER)&(ByteOffset),
					Length,
					FALSE);

			ExReleaseResource(&Vcb->MainResource);
			MainResourceAcquired = FALSE;
		}

		if (!PagingIo)
		{
			if (!ExAcquireResourceExclusiveLite(
						&Vcb->MainResource,
						IrpContext->IsSynchronous))
			{
				Status = STATUS_PENDING;
				__leave;
			}

			MainResourceAcquired = TRUE;
		}
		else
		{
			/*
			ULONG ResShCnt, ResExCnt; 
			ResShCnt = ExIsResourceAcquiredSharedLite(&Vcb->PagingIoResource);
			ResExCnt = ExIsResourceAcquiredExclusiveLite(&Vcb->PagingIoResource);

			FFSPrint((DBG_USER, "PagingIoRes: %xh:%xh Synchronous=%xh\n", ResShCnt, ResExCnt, IrpContext->IsSynchronous));
			*/

			if (Ccb)
			{
				if (!ExAcquireResourceSharedLite(
							&Vcb->PagingIoResource,
							IrpContext->IsSynchronous))
				{
					Status = STATUS_PENDING;
					__leave;
				}

				PagingIoResourceAcquired = TRUE;
			}
		}

		if (!Nocache)
		{
			if ((ByteOffset.QuadPart + Length) >
					Vcb->PartitionInformation.PartitionLength.QuadPart
			)
			{
				Length = (ULONG) (
						Vcb->PartitionInformation.PartitionLength.QuadPart -
						ByteOffset.QuadPart);

				Length &= ~((ULONG)SECTOR_SIZE - 1);
			}

			if (FlagOn(IrpContext->MinorFunction, IRP_MN_MDL))
			{

				CcPrepareMdlWrite(
						Vcb->StreamObj,
						&ByteOffset,
						Length,
						&Irp->MdlAddress,
						&Irp->IoStatus);

				Status = Irp->IoStatus.Status;
			}
			else
			{
				Buffer = FFSGetUserBuffer(Irp);

				if (Buffer == NULL)
				{
					FFSBreakPoint();

					Status = STATUS_INVALID_USER_BUFFER;
					__leave;
				}

				if (!CcCopyWrite(Vcb->StreamObj,
							(PLARGE_INTEGER)(&ByteOffset),
							Length,
							TRUE,
							Buffer))
				{
					Status = STATUS_PENDING;
					__leave;
				}

				Status = Irp->IoStatus.Status;
				FFSAddMcbEntry(Vcb, ByteOffset.QuadPart, (LONGLONG)Length);
			}

			if (NT_SUCCESS(Status))
			{
				Irp->IoStatus.Information = Length;
			}
		}
		else
		{
			PFFS_BDL            ffs_bdl = NULL;
			ULONG               Blocks = 0;

			LONGLONG            DirtyStart;
			LONGLONG            DirtyLba;
			LONGLONG            DirtyLength;
			LONGLONG            RemainLength;

			if ((ByteOffset.QuadPart + Length) >
					Vcb->PartitionInformation.PartitionLength.QuadPart)
			{
				Length = (ULONG)(
						Vcb->PartitionInformation.PartitionLength.QuadPart -
						ByteOffset.QuadPart);

				Length &= ~((ULONG)SECTOR_SIZE - 1);
			}

			Status = FFSLockUserBuffer(
					IrpContext->Irp,
					Length,
					IoReadAccess);

			if (!NT_SUCCESS(Status))
			{
				__leave;
			}

			ffs_bdl = ExAllocatePool(PagedPool, 
					(Length / Vcb->BlockSize) *
					sizeof(FFS_BDL));

			if (!ffs_bdl)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				__leave;
			}

			DirtyLba = ByteOffset.QuadPart;
			RemainLength = (LONGLONG)Length;

			while (RemainLength > 0)
			{
				DirtyStart = DirtyLba;

				if (FFSLookupMcbEntry(Vcb, 
							DirtyStart,
							&DirtyLba,
							&DirtyLength,
							(PLONGLONG)NULL,
							(PLONGLONG)NULL,
							(PULONG)NULL))
				{

					if (DirtyLba == -1)
					{
						DirtyLba = DirtyStart + DirtyLength;

						RemainLength = ByteOffset.QuadPart + 
							(LONGLONG)Length -
							DirtyLba;
						continue;
					}

					ffs_bdl[Blocks].Irp = NULL;
					ffs_bdl[Blocks].Lba = DirtyLba;
					ffs_bdl[Blocks].Offset = (ULONG)((LONGLONG)Length +
							DirtyStart -
							RemainLength - 
							DirtyLba);

					if (DirtyLba + DirtyLength > DirtyStart + RemainLength)
					{
						ffs_bdl[Blocks].Length = (ULONG)(DirtyStart +
								RemainLength -
								DirtyLba);
						RemainLength = 0;
					}
					else
					{
						ffs_bdl[Blocks].Length = (ULONG)DirtyLength;
						RemainLength =  (DirtyStart + RemainLength) -
							(DirtyLba + DirtyLength);
					}

					DirtyLba = DirtyStart + DirtyLength;
					Blocks++;
				}
				else
				{
					if (Blocks == 0)
					{
						if (ffs_bdl)
							ExFreePool(ffs_bdl);

						//
						// Lookup fails at the first time, ie. 
						// no dirty blocks in the run
						//

						FFSBreakPoint();

						if (RemainLength == (LONGLONG)Length)
							Status = STATUS_SUCCESS;
						else
							Status = STATUS_UNSUCCESSFUL;

						__leave;
					}
					else
					{
						break;
					}
				}
			}

			if (Blocks > 0)
			{
				Status = FFSReadWriteBlocks(IrpContext,
							Vcb,
							ffs_bdl,
							Length,
							Blocks,
							FALSE);
				Irp = IrpContext->Irp;

				if (NT_SUCCESS(Status))
				{
					ULONG   i;

					for (i = 0; i < Blocks; i++)
					{
						FFSRemoveMcbEntry(Vcb,
								ffs_bdl[i].Lba,
								ffs_bdl[i].Length);
					}
				}

				if (ffs_bdl)
					ExFreePool(ffs_bdl);

				if (!Irp)
					__leave;

			}
			else
			{
				if (ffs_bdl)
					ExFreePool(ffs_bdl);

				Irp->IoStatus.Information = Length;

				Status = STATUS_SUCCESS;
				__leave;
			}
		}
	}

	__finally
	{
		if (PagingIoResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Vcb->PagingIoResource,
					ExGetCurrentResourceThread());
		}

		if (MainResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Vcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (!IrpContext->ExceptionInProgress)
		{
			if (Irp)
			{
				if (Status == STATUS_PENDING)
				{
					if(!bDeferred)
					{
						Status = FFSLockUserBuffer(
								IrpContext->Irp,
								Length,
								IoReadAccess);

						if (NT_SUCCESS(Status))
						{
							Status = FFSQueueRequest(IrpContext);
						}
						else
						{
							FFSCompleteIrpContext(IrpContext, Status);
						}
					}
				}
				else
				{
					if (NT_SUCCESS(Status))
					{
						if (SynchronousIo && !PagingIo)
						{
							FileObject->CurrentByteOffset.QuadPart =
								ByteOffset.QuadPart + Irp->IoStatus.Information;
						}

						if (!PagingIo)
						{
							SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
						}
					}

					FFSCompleteIrpContext(IrpContext, Status);
				}
			}
			else
			{
				FFSFreeIrpContext(IrpContext);
			}
		}
	}

	return Status;
}


NTSTATUS
FFSWriteInode(
	IN PFFS_IRP_CONTEXT     IrpContext,
	IN PFFS_VCB             Vcb,
	IN PFFS_INODE           dinode,
	IN ULONG                offset,
	IN PVOID                Buffer,
	IN ULONG                size,
	IN BOOLEAN              bWriteToDisk,
	OUT PULONG              dwRet)
{
	PFFS_BDL    ffs_bdl = NULL;
	ULONG       blocks, i;
	NTSTATUS    Status = STATUS_UNSUCCESSFUL;
	ULONG       Totalblocks;
	LONGLONG    AllocSize;

	if (dwRet)
	{
		*dwRet = 0;
	}

	Totalblocks = (dinode->di_blocks);
	AllocSize = ((LONGLONG)(FFSDataBlocks(Vcb, Totalblocks)) << BLOCK_BITS);

	if ((LONGLONG)offset >= AllocSize)
	{
		FFSPrint((DBG_ERROR, "FFSWritenode: beyond the file range.\n"));
		return STATUS_SUCCESS;
	}

	if ((LONGLONG)offset + size > AllocSize)
	{
		size = (ULONG)(AllocSize - offset);
	}

	blocks = FFSBuildBDL(IrpContext, Vcb, dinode, offset, size, &ffs_bdl);

	if (blocks <= 0)
	{
		return STATUS_SUCCESS;
	}

#if DBG
	{
		ULONG   dwTotal = 0;
		FFSPrint((DBG_INFO, "FFSWriteInode: BDLCount = %xh Size=%xh Off=%xh\n",
					blocks, size, offset));
		for(i = 0; i < blocks; i++)
		{
			FFSPrint((DBG_INFO, "FFSWriteInode: Lba=%I64xh Len=%xh Off=%xh\n",
						ffs_bdl[i].Lba, ffs_bdl[i].Length, ffs_bdl[i].Offset));
			dwTotal += ffs_bdl[i].Length;
		}

		if (dwTotal != size)
		{
			FFSBreakPoint();
		}

		FFSPrint((DBG_INFO, "FFSWriteInode: Total = %xh (WriteToDisk=%x)\n",
					dwTotal, bWriteToDisk));
	}
#endif

	if (bWriteToDisk)
	{

#if 0
		for(i = 0; i < blocks; i++)
		{
			{
				CcFlushCache(&(Vcb->SectionObject),
						(PLARGE_INTEGER)&(ffs_bdl[i].Lba),
						ffs_bdl[i].Length,
						NULL);

				if (Vcb->SectionObject.DataSectionObject != NULL)
				{
					ExAcquireSharedStarveExclusive(&Vcb->PagingIoResource, TRUE);
					ExReleaseResource(&Vcb->PagingIoResource);

					CcPurgeCacheSection(&(Vcb->SectionObject),
							(PLARGE_INTEGER)&(ffs_bdl[i].Lba),
							ffs_bdl[i].Length,
							FALSE);
				}
			}
		}
#endif

		// assume offset is aligned.
		Status = FFSReadWriteBlocks(IrpContext, Vcb, ffs_bdl, size, blocks, FALSE);
	}
	else
	{
		for(i = 0; i < blocks; i++)
		{
			if(!FFSSaveBuffer(IrpContext, Vcb, ffs_bdl[i].Lba, ffs_bdl[i].Length, (PVOID)((PUCHAR)Buffer + ffs_bdl[i].Offset)))
				goto errorout;
		}

		if (IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK))
		{
			FFSPrint((DBG_USER, "FFSWriteInode is starting FlushingDpc...\n"));
			FFSStartFloppyFlushDpc(Vcb, NULL, NULL);
		}

		Status = STATUS_SUCCESS;
	}

errorout:

	if (ffs_bdl)
		ExFreePool(ffs_bdl);

	if (NT_SUCCESS(Status))
	{
		if (dwRet) *dwRet = size;
	}

	return Status;
}


NTSTATUS
FFSWriteFile(
	IN PFFS_IRP_CONTEXT IrpContext)
{
	NTSTATUS            Status = STATUS_UNSUCCESSFUL;

	PFFS_VCB            Vcb;
	PFFS_FCB            Fcb;
	PFFS_CCB            Ccb;
	PFILE_OBJECT        FileObject;
	PFILE_OBJECT        CacheObject;

	PDEVICE_OBJECT      DeviceObject;

	PIRP                Irp;
	PIO_STACK_LOCATION  IoStackLocation;

	ULONG               Length;
	ULONG               ReturnedLength;
	LARGE_INTEGER       ByteOffset;

	BOOLEAN             PagingIo;
	BOOLEAN             Nocache;
	BOOLEAN             SynchronousIo;
	BOOLEAN             MainResourceAcquired = FALSE;
	BOOLEAN             PagingIoResourceAcquired = FALSE;

	BOOLEAN             bNeedExtending = FALSE;
	BOOLEAN             bAppendFile = FALSE;

	BOOLEAN             bDeferred = FALSE;

	PUCHAR              Buffer;

	__try
	{
		ASSERT(IrpContext);

		ASSERT((IrpContext->Identifier.Type == FFSICX) &&
				(IrpContext->Identifier.Size == sizeof(FFS_IRP_CONTEXT)));

		DeviceObject = IrpContext->DeviceObject;

		Vcb = (PFFS_VCB)DeviceObject->DeviceExtension;

		ASSERT(Vcb != NULL);

		ASSERT((Vcb->Identifier.Type == FFSVCB) &&
				(Vcb->Identifier.Size == sizeof(FFS_VCB)));

		FileObject = IrpContext->FileObject;

		Fcb = (PFFS_FCB)FileObject->FsContext;

		ASSERT(Fcb);

		ASSERT((Fcb->Identifier.Type == FFSFCB) &&
				(Fcb->Identifier.Size == sizeof(FFS_FCB)));

		Ccb = (PFFS_CCB)FileObject->FsContext2;

		Irp = IrpContext->Irp;

		IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

		Length = IoStackLocation->Parameters.Write.Length;
		ByteOffset = IoStackLocation->Parameters.Write.ByteOffset;

		PagingIo = (Irp->Flags & IRP_PAGING_IO ? TRUE : FALSE);
		Nocache = (Irp->Flags & IRP_NOCACHE ? TRUE : FALSE);
		SynchronousIo = (FileObject->Flags & FO_SYNCHRONOUS_IO ? TRUE : FALSE);

		FFSPrint((DBG_INFO, "FFSWriteFile: Off=%I64xh Len=%xh Paging=%xh Nocache=%xh\n",
					ByteOffset.QuadPart, Length, PagingIo, Nocache));

		/*
		if (IsFlagOn(Fcb->Flags, FCB_FILE_DELETED))
		{
			Status = STATUS_FILE_DELETED;
			__leave;
		}
		*/

		if (IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING))
		{
			Status = STATUS_DELETE_PENDING;
			__leave;
		}

		if (Length == 0)
		{
			Irp->IoStatus.Information = 0;
			Status = STATUS_SUCCESS;
			__leave;
		}

		if (Nocache &&
				(ByteOffset.LowPart & (SECTOR_SIZE - 1) ||
				 Length & (SECTOR_SIZE - 1)))
		{
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		if (FlagOn(IrpContext->MinorFunction, IRP_MN_DPC))
		{
			ClearFlag(IrpContext->MinorFunction, IRP_MN_DPC);
			Status = STATUS_PENDING;
			__leave;
		}

#if FALSE
		if (!Nocache)
		{
			BOOLEAN bAgain = IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED);
			BOOLEAN bWait  = IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
			BOOLEAN bQueue = IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_REQUEUED);

			if (!CcCanIWrite(
						FileObject,
						Length,
						(bWait && bQueue),
						bAgain))
			{
				SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED);

				CcDeferWrite(FileObject,
						(PCC_POST_DEFERRED_WRITE)FFSDeferWrite,
						IrpContext,
						Irp,
						Length,
						bAgain);

				bDeferred = TRUE;

				FFSBreakPoint();

				Status = STATUS_PENDING;
				__leave;
			}
		}

#endif

		if (IsEndOfFile(ByteOffset))
		{
			bAppendFile = TRUE;
			ByteOffset.QuadPart = Fcb->Header.FileSize.QuadPart;
		}

		if (FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY) && !PagingIo)
		{
			Status = STATUS_INVALID_DEVICE_REQUEST;
			__leave;
		}

		//
		//  Do flushing for such cases
		//
		if (Nocache && !PagingIo && (Fcb->SectionObject.DataSectionObject != NULL)) 
		{
			ExAcquireResourceExclusive(&Fcb->MainResource, 
					IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT));

			MainResourceAcquired = TRUE;

			ExAcquireSharedStarveExclusive(&Fcb->PagingIoResource, TRUE);
			ExReleaseResource(&Fcb->PagingIoResource);

			CcFlushCache(&(Fcb->SectionObject),
					&ByteOffset,
					Length,
					&(Irp->IoStatus));
			ClearFlag(Fcb->Flags, FCB_FILE_MODIFIED);

			if (!NT_SUCCESS(Irp->IoStatus.Status)) 
			{
				Status = Irp->IoStatus.Status;
				__leave;
			}

			ExAcquireSharedStarveExclusive(&Fcb->PagingIoResource, TRUE);
			ExReleaseResource(&Fcb->PagingIoResource);

			CcPurgeCacheSection(&(Fcb->SectionObject),
					(PLARGE_INTEGER)&(ByteOffset),
					Length,
					FALSE);

			ExReleaseResource(&Fcb->MainResource);
			MainResourceAcquired = FALSE;
		}

		if (!PagingIo)
		{
			if (!ExAcquireResourceExclusiveLite(
						&Fcb->MainResource,
						IrpContext->IsSynchronous))
			{
				Status = STATUS_PENDING;
				__leave;
			}

			MainResourceAcquired = TRUE;
		}
		else
		{
			/*
			ULONG ResShCnt, ResExCnt; 
			ResShCnt = ExIsResourceAcquiredSharedLite(&Fcb->PagingIoResource);
			ResExCnt = ExIsResourceAcquiredExclusiveLite(&Fcb->PagingIoResource);

			FFSPrint((DBG_USER, "FFSWriteFile: Inode=%xh %S PagingIo: %xh:%xh Synchronous=%xh\n",
			Fcb->FFSMcb->Inode, Fcb->FFSMcb->ShortName.Buffer, ResShCnt, ResExCnt, IrpContext->IsSynchronous));
			*/
			if (!ExAcquireResourceSharedLite(
						&Fcb->PagingIoResource,
						IrpContext->IsSynchronous))
			{
				Status = STATUS_PENDING;
				__leave;
			}

			PagingIoResourceAcquired = TRUE;
		}

		if (!PagingIo)
		{
			if (!FsRtlCheckLockForWriteAccess(
						&Fcb->FileLockAnchor,
						Irp))
			{
				Status = STATUS_FILE_LOCK_CONFLICT;
				__leave;
			}
		}

		if (Nocache)
		{
			if ((ByteOffset.QuadPart + Length) >
					Fcb->Header.AllocationSize.QuadPart)
			{
				if (ByteOffset.QuadPart >= 
						Fcb->Header.AllocationSize.QuadPart)
				{
					Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = 0;
					__leave;
				}
				else
				{
					if (Length > (ULONG)(Fcb->Header.AllocationSize.QuadPart
								- ByteOffset.QuadPart))
					{
						Length = (ULONG)(Fcb->Header.AllocationSize.QuadPart
								- ByteOffset.QuadPart);
					}
				}
			}
		}

		if (!Nocache)
		{
			if (FlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY))
			{
				__leave;
			}

			if (FileObject->PrivateCacheMap == NULL)
			{
				CcInitializeCacheMap(
						FileObject,
						(PCC_FILE_SIZES)(&Fcb->Header.AllocationSize),
						FALSE,
						&FFSGlobal->CacheManagerCallbacks,
						Fcb);

				CcSetReadAheadGranularity(
						FileObject,
						READ_AHEAD_GRANULARITY);

				CcSetFileSizes(
						FileObject, 
						(PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
			}

			CacheObject = FileObject;

			//
			//  Need extending the size of inode ?
			//
			if (bAppendFile || ((ULONG)(ByteOffset.QuadPart + Length) >
						(ULONG)(Fcb->Header.FileSize.QuadPart)))
			{

				LARGE_INTEGER   ExtendSize;
				LARGE_INTEGER   FileSize;

				bNeedExtending = TRUE;
				FileSize = Fcb->Header.FileSize;
				ExtendSize.QuadPart = (LONGLONG)(ByteOffset.QuadPart + Length);

				if (ExtendSize.QuadPart > 
						Fcb->Header.AllocationSize.QuadPart)
				{
					if (!FFSExpandFile(IrpContext, Vcb, Fcb, &ExtendSize))
					{
						Status = STATUS_INSUFFICIENT_RESOURCES;
						__leave;
					}
				}

				{
					Fcb->Header.FileSize.QuadPart = ExtendSize.QuadPart;
					Fcb->dinode->di_size = (ULONG) ExtendSize.QuadPart;
				}

				if (FileObject->PrivateCacheMap)
				{
					CcSetFileSizes(FileObject, (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));

					if (ByteOffset.QuadPart > FileSize.QuadPart)
					{
						FFSZeroHoles(IrpContext, Vcb, FileObject, FileSize.QuadPart, 
								ByteOffset.QuadPart - FileSize.QuadPart);
					}

					if (Fcb->Header.AllocationSize.QuadPart > ExtendSize.QuadPart)
					{
						FFSZeroHoles(IrpContext, Vcb, FileObject, ExtendSize.QuadPart, 
								Fcb->Header.AllocationSize.QuadPart - ExtendSize.QuadPart);
					}
				}

				if (FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, Fcb->dinode))
				{
					Status = STATUS_SUCCESS;
				}

				FFSNotifyReportChange(
						IrpContext,
						Vcb,
						Fcb,
						FILE_NOTIFY_CHANGE_SIZE,
						FILE_ACTION_MODIFIED);
			}

			if (FlagOn(IrpContext->MinorFunction, IRP_MN_MDL))
			{
				CcPrepareMdlWrite(
						CacheObject,
						(&ByteOffset),
						Length,
						&Irp->MdlAddress,
						&Irp->IoStatus);

				Status = Irp->IoStatus.Status;
			}
			else
			{
				Buffer = FFSGetUserBuffer(Irp);

				if (Buffer == NULL)
				{
					FFSBreakPoint();
					Status = STATUS_INVALID_USER_BUFFER;
					__leave;
				}

				if (!CcCopyWrite(
							CacheObject,
							(PLARGE_INTEGER)&ByteOffset,
							Length,
							IrpContext->IsSynchronous,
							Buffer))
				{
					Status = STATUS_PENDING;
					__leave;
				}

				Status = Irp->IoStatus.Status;
			}

			if (NT_SUCCESS(Status))
			{
				Irp->IoStatus.Information = Length;

				if (IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK))
				{
					FFSPrint((DBG_USER, "FFSWriteFile is starting FlushingDpc...\n"));
					FFSStartFloppyFlushDpc(Vcb, Fcb, FileObject);
				}
			}
		}
		else
		{
			ReturnedLength = Length;

			Status = FFSLockUserBuffer(
					IrpContext->Irp,
					Length,
					IoReadAccess);

			if (!NT_SUCCESS(Status))
			{
				__leave;
			}

			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = Length;

			Status = 
				FFSWriteInode(
						IrpContext,
						Vcb,
						Fcb->dinode,
						(ULONG)(ByteOffset.QuadPart),
						NULL,
						Length,
						TRUE,
						&ReturnedLength);

			Irp = IrpContext->Irp;

		}
	}

	__finally
	{
		if (PagingIoResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Fcb->PagingIoResource,
					ExGetCurrentResourceThread());
		}

		if (MainResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Fcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (!IrpContext->ExceptionInProgress)
		{
			if (Irp)
			{
				if (Status == STATUS_PENDING)
				{
					if (!bDeferred)
					{
						Status = FFSLockUserBuffer(
									IrpContext->Irp,
									Length,
									IoReadAccess);

						if (NT_SUCCESS(Status))
						{
							Status = FFSQueueRequest(IrpContext);
						}
						else
						{
							FFSCompleteIrpContext(IrpContext, Status);
						}
					}
				}
				else
				{
					if (NT_SUCCESS(Status))
					{
						if (SynchronousIo && !PagingIo)
						{
							FileObject->CurrentByteOffset.QuadPart =
								ByteOffset.QuadPart + Irp->IoStatus.Information;
						}

						if (!PagingIo)
						{
							SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
							SetFlag(Fcb->Flags, FCB_FILE_MODIFIED);
						}
					}

					FFSCompleteIrpContext(IrpContext, Status);
				}
			}
			else
			{
				FFSFreeIrpContext(IrpContext);
			}
		}
	}

	return Status;

}


NTSTATUS
FFSWriteComplete(
	IN PFFS_IRP_CONTEXT IrpContext)
{
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;
	PFILE_OBJECT    FileObject;
	PIRP            Irp;
	PIO_STACK_LOCATION IrpSp;

	__try
	{
		ASSERT(IrpContext);

		ASSERT((IrpContext->Identifier.Type == FFSICX) &&
				(IrpContext->Identifier.Size == sizeof(FFS_IRP_CONTEXT)));

		FileObject = IrpContext->FileObject;

		Irp = IrpContext->Irp;
		IrpSp = IoGetCurrentIrpStackLocation(Irp);

		CcMdlWriteComplete(FileObject, &(IrpSp->Parameters.Write.ByteOffset), Irp->MdlAddress);

		Irp->MdlAddress = NULL;

		Status = STATUS_SUCCESS;
	}

	__finally
	{
		if (!IrpContext->ExceptionInProgress)
		{
			FFSCompleteIrpContext(IrpContext, Status);
		}
	}

	return Status;
}


NTSTATUS
FFSWrite(
	IN PFFS_IRP_CONTEXT IrpContext)
{
	NTSTATUS            Status;
	PFFS_FCBVCB         FcbOrVcb;
	PDEVICE_OBJECT      DeviceObject;
	PFILE_OBJECT        FileObject;
	PFFS_VCB            Vcb;
	BOOLEAN             bCompleteRequest = TRUE;

	ASSERT(IrpContext);

	ASSERT((IrpContext->Identifier.Type == FFSICX) &&
			(IrpContext->Identifier.Size == sizeof(FFS_IRP_CONTEXT)));

	__try
	{
		if (FlagOn(IrpContext->MinorFunction, IRP_MN_COMPLETE))
		{
			Status = FFSWriteComplete(IrpContext);
			bCompleteRequest = FALSE;
		}
		else
		{
			DeviceObject = IrpContext->DeviceObject;

			if (DeviceObject == FFSGlobal->DeviceObject)
			{
				Status = STATUS_INVALID_DEVICE_REQUEST;
				__leave;
			}

			Vcb = (PFFS_VCB)DeviceObject->DeviceExtension;

			if (Vcb->Identifier.Type != FFSVCB ||
					Vcb->Identifier.Size != sizeof(FFS_VCB))
			{
				Status = STATUS_INVALID_PARAMETER;
				__leave;
			}

			ASSERT(IsMounted(Vcb));

			if (IsFlagOn(Vcb->Flags, VCB_DISMOUNT_PENDING))
			{
				Status = STATUS_TOO_LATE;
				__leave;
			}

			if (IsFlagOn(Vcb->Flags, VCB_READ_ONLY))
			{
				Status = STATUS_MEDIA_WRITE_PROTECTED;
				__leave;
			}

			FileObject = IrpContext->FileObject;

			FcbOrVcb = (PFFS_FCBVCB)FileObject->FsContext;

			if (FcbOrVcb->Identifier.Type == FFSVCB)
			{
				Status = FFSWriteVolume(IrpContext);

				if (!NT_SUCCESS(Status))
				{
					FFSBreakPoint();
				}

				bCompleteRequest = FALSE;
			}
			else if (FcbOrVcb->Identifier.Type == FFSFCB)
			{
				Status = FFSWriteFile(IrpContext);

				if (!NT_SUCCESS(Status))
				{
					FFSBreakPoint();
				}

				bCompleteRequest = FALSE;
			}
			else
			{
				Status = STATUS_INVALID_PARAMETER;
			}
		}
	}

	__finally
	{
		if (bCompleteRequest)
		{
			FFSCompleteIrpContext(IrpContext, Status);
		}
	}

	return Status;
}
