/* 
 * FFS File System Driver for Windows
 *
 * ffs.c
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
extern PFFS_GLOBAL   FFSGlobal;


/* Definitions */

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FFSLoadSuper)
#pragma alloc_text(PAGE, FFSSaveSuper)

#pragma alloc_text(PAGE, FFSLoadGroup)
#pragma alloc_text(PAGE, FFSSaveGroup)

#pragma alloc_text(PAGE, FFSGetInodeLba)
#pragma alloc_text(PAGE, FFSLoadInode)
#pragma alloc_text(PAGE, FFSSaveInode)

#pragma alloc_text(PAGE, FFSLoadBlock)
#pragma alloc_text(PAGE, FFSSaveBlock)

#pragma alloc_text(PAGE, FFSSaveBuffer)

#pragma alloc_text(PAGE, FFSGetBlock)
#pragma alloc_text(PAGE, FFSBlockMap)

#pragma alloc_text(PAGE, FFSBuildBDL)

#pragma alloc_text(PAGE, FFSNewBlock)
#pragma alloc_text(PAGE, FFSFreeBlock)

#pragma alloc_text(PAGE, FFSExpandBlock)
#pragma alloc_text(PAGE, FFSExpandInode)

#pragma alloc_text(PAGE, FFSNewInode)
#pragma alloc_text(PAGE, FFSFreeInode)

#pragma alloc_text(PAGE, FFSAddEntry)
#pragma alloc_text(PAGE, FFSRemoveEntry)

#pragma alloc_text(PAGE, FFSTruncateBlock)
#pragma alloc_text(PAGE, FFSTruncateInode)

#pragma alloc_text(PAGE, FFSAddMcbEntry)
#pragma alloc_text(PAGE, FFSRemoveMcbEntry)
#pragma alloc_text(PAGE, FFSLookupMcbEntry)

#endif


PFFS_SUPER_BLOCK
FFSLoadSuper(
	IN PFFS_VCB       Vcb,
	IN BOOLEAN        bVerify)
{
	NTSTATUS         Status;
	PFFS_SUPER_BLOCK FFSSb = NULL;

	FFSSb = (PFFS_SUPER_BLOCK)ExAllocatePool(PagedPool, SUPER_BLOCK_SIZE);
	if (!FFSSb)
	{
		return NULL;
	}

	Status = FFSReadDisk(Vcb,
				(ULONGLONG)SUPER_BLOCK_OFFSET,
				SUPER_BLOCK_SIZE,
				(PVOID)FFSSb,
				bVerify);

	if (!NT_SUCCESS(Status))
	{
		FFSPrint((DBG_ERROR, "FFSReadDisk: Read Block Device error.\n"));

		ExFreePool(FFSSb);
		return NULL;
	}

	return FFSSb;
}


BOOLEAN
FFSSaveSuper(
	IN PFFS_IRP_CONTEXT    IrpContext,
	IN PFFS_VCB            Vcb)
{
	LONGLONG    Offset;
	BOOLEAN     bRet;

	Offset = (LONGLONG) SUPER_BLOCK_OFFSET;

	bRet = FFSSaveBuffer(IrpContext,
				Vcb,
				Offset,
				SUPER_BLOCK_SIZE,
				Vcb->ffs_super_block);

	if (IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK))
	{
		FFSStartFloppyFlushDpc(Vcb, NULL, NULL);
	}

	return bRet;
}


#if 0
BOOLEAN
FFSLoadGroup(
	IN PFFS_VCB Vcb)
{
	ULONG       Size;
	PVOID       Buffer;
	LONGLONG    Lba;
	NTSTATUS    Status;

	PFFS_SUPER_BLOCK FFSSb;

	FFSSb = Vcb->ffs_super_block;

	Vcb->BlockSize  = FFSSb->fs_bsize;
	Vcb->SectorBits = FFSLog2(SECTOR_SIZE);
	ASSERT(BLOCK_BITS == FFSLog2(BLOCK_SIZE));

	Vcb->ffs_groups = (FFSSb->s_blocks_count - FFSSb->s_first_data_block +
			FFSSb->s_blocks_per_group - 1) / FFSSb->s_blocks_per_group;

	Size = sizeof(FFS_GROUP_DESC) * Vcb->ffs_groups;

	if (Vcb->BlockSize == MINBSIZE)
	{
		Lba = (LONGLONG)2 * Vcb->BlockSize;
	}

	if (Vcb->BlockSize > MINBSIZE)
	{
		Lba = (LONGLONG)(Vcb->BlockSize);
	}

	if (Lba == 0)
	{
		return FALSE;
	}

	Buffer = ExAllocatePool(PagedPool, Size);
	if (!Buffer)
	{
		FFSPrint((DBG_ERROR, "FFSLoadSuper: no enough memory.\n"));
		return FALSE;
	}

	FFSPrint((DBG_INFO, "FFSLoadGroup: Lba=%I64xh Size=%xh\n", Lba, Size));

	Status = FFSReadDisk(Vcb,
				Lba,
				Size,
				Buffer,
				FALSE);

	if (!NT_SUCCESS(Status))
	{
		ExFreePool(Buffer);
		Buffer = NULL;

		return FALSE;
	}

	Vcb->ffs_group_desc = (PFFS_GROUP_DESC) Buffer;

	return TRUE;
}


BOOLEAN
FFSSaveGroup(
	IN PFFS_IRP_CONTEXT    IrpContext,
	IN PFFS_VCB            Vcb)
{
	LONGLONG    Offset;
	ULONG       Len;
	BOOLEAN     bRet;

	if (Vcb->BlockSize == FFS_MIN_BLOCK) {

		Offset = (LONGLONG)(2 * Vcb->BlockSize);

	} else {

		Offset = (LONGLONG)(Vcb->BlockSize);
	}

	Len = (ULONG)(sizeof(struct ffs_group_desc) * Vcb->ffs_groups);

	bRet = FFSSaveBuffer(IrpContext, Vcb, Offset,
			Len, Vcb->ffs_group_desc);

	if (IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK))
	{
		FFSStartFloppyFlushDpc(Vcb, NULL, NULL);
	}

	return bRet;
}
#endif


BOOLEAN
FFSGetInodeLba(
	IN PFFS_VCB   Vcb,
	IN  ULONG     inode,
	OUT PLONGLONG offset)
{
	LONGLONG loc;

#if 0
	if (inode < 1 || inode > INODES_COUNT)
	{
		FFSPrint((DBG_ERROR, "FFSGetInodeLba: Inode value %xh is invalid.\n",inode));
		*offset = 0;
		return FALSE;
	}
#endif

	loc = cgimin(Vcb->ffs_super_block, ino_to_cg(Vcb->ffs_super_block, inode)) 
		* Vcb->ffs_super_block->fs_fsize + ((inode % Vcb->ffs_super_block->fs_ipg) * 128);

	*offset = loc;
	KdPrint(("FFSGetInodeLba() inode : %d, loc : %x, offset : %x\n", inode, loc, offset));

	return TRUE;
}


BOOLEAN
FFSLoadInode(
	IN PFFS_VCB   Vcb,
	IN ULONG      inode,
	IN PFFS_INODE dinode)
{
	IO_STATUS_BLOCK     IoStatus;
	LONGLONG            Offset; 

	if (!FFSGetInodeLba(Vcb, inode, &Offset))
	{
		FFSPrint((DBG_ERROR, "FFSLoadInode: error get inode(%xh)'s addr.\n", inode));
		return FALSE;
	}

	if (!FFSCopyRead(
				Vcb->StreamObj,
				(PLARGE_INTEGER)&Offset,
				sizeof(FFS_INODE),
				PIN_WAIT,
				(PVOID)dinode,
				&IoStatus));

	if (!NT_SUCCESS(IoStatus.Status))
	{
		return FALSE;
	}

	return TRUE;
}


/*
BOOLEAN
FFSSaveInode(
	IN PFFS_VCB      Vcb,
	IN ULONG         inode,
	IN struct dinode *dinode)
{
	ULONG       lba;
	ULONG       offset;
	NTSTATUS    Status;

	if (!FFSGetInodeLba(Vcb, inode, &lba, &offset))
	{
		FFSPrint((DBG_ERROR, "FFSLoadInode: error get inode(%xh)'s addr.\n", inode));
		return FALSE;
	}

	Status = FFSWriteDisk(Vcb->TargetDeviceObject,
		lba,
		offset,
		sizeof(FFS_INODE),
		(PVOID)dinode);

	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}

	return TRUE;
}
*/


BOOLEAN
FFSSaveInode(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN ULONG            Inode,
	IN PFFS_INODE       dinode)
{
	LONGLONG        Offset = 0;
	LARGE_INTEGER   CurrentTime;
	BOOLEAN         bRet;

	KeQuerySystemTime(&CurrentTime);
	dinode->di_mtime = dinode->di_atime = 
		(ULONG)(FFSInodeTime(CurrentTime));

	FFSPrint((DBG_INFO, "FFSSaveInode: Saving Inode %xh: Mode=%xh Size=%xh\n",
				Inode, dinode->di_mode, dinode->di_size));

	if (!FFSGetInodeLba(Vcb, Inode, &Offset))
	{
		FFSPrint((DBG_ERROR, "FFSSaveInode: error get inode(%xh)'s addr.\n", Inode));
		return FALSE;
	}

	bRet = FFSSaveBuffer(IrpContext, Vcb, Offset, sizeof(FFS_INODE), dinode);

	if (IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK))
	{
		FFSStartFloppyFlushDpc(Vcb, NULL, NULL);
	}

	return bRet;
}


BOOLEAN
FFSLoadBlock(
	IN PFFS_VCB  Vcb,
	IN ULONG     dwBlk,
	IN PVOID     Buffer)
{
	IO_STATUS_BLOCK     IoStatus;
	LONGLONG            Offset; 

	Offset = (LONGLONG) dwBlk;
	//Offset = Offset * Vcb->BlockSize;
	Offset = Offset * 0x400; // 1024

	if (!FFSCopyRead(
				Vcb->StreamObj,
				(PLARGE_INTEGER)&Offset,
				Vcb->BlockSize,
				PIN_WAIT,
				Buffer,
				&IoStatus));

	if (!NT_SUCCESS(IoStatus.Status))
	{
		return FALSE;
	}

	return TRUE;
}


BOOLEAN
FFSSaveBlock(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN ULONG            dwBlk,
	IN PVOID            Buf)
{
	LONGLONG Offset;
	BOOLEAN  bRet;

	Offset = (LONGLONG)dwBlk;
	Offset = Offset * Vcb->BlockSize;

	bRet = FFSSaveBuffer(IrpContext, Vcb, Offset, Vcb->BlockSize, Buf);

	if (IsFlagOn(Vcb->Flags, VCB_FLOPPY_DISK))
	{
		FFSStartFloppyFlushDpc(Vcb, NULL, NULL);
	}

	return bRet;
}


BOOLEAN
FFSSaveBuffer(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN LONGLONG         Offset,
	IN ULONG            Size,
	IN PVOID            Buf)
{
	PBCB        Bcb;
	PVOID       Buffer;

	if(!CcPinRead(Vcb->StreamObj,
				(PLARGE_INTEGER) (&Offset),
				Size,
				PIN_WAIT,
				&Bcb,
				&Buffer))
	{
		FFSPrint((DBG_ERROR, "FFSSaveBuffer: PinReading error ...\n"));
		return FALSE;
	}


	FFSPrint((DBG_INFO, "FFSSaveBuffer: Off=%I64xh Len=%xh Bcb=%xh\n",
				Offset, Size, (ULONG)Bcb));

	RtlCopyMemory(Buffer, Buf, Size);
	CcSetDirtyPinnedData(Bcb, NULL);

	FFSRepinBcb(IrpContext, Bcb);

	CcUnpinData(Bcb);

	SetFlag(Vcb->StreamObj->Flags, FO_FILE_MODIFIED);

	FFSAddMcbEntry(Vcb, Offset, (LONGLONG)Size);

	return TRUE;
}


ULONG
FFSGetBlock(
	IN PFFS_VCB Vcb,
	ULONG       dwContent,
	ULONG       Index,
	int         layer)
{
	ULONG       *pData = NULL;
	ULONG       i = 0, j = 0, temp = 1;
	ULONG       dwBlk = 0;

	if (layer == 0)
	{
		dwBlk = dwContent;
	}
	else if (layer <= 3)
	{
		pData = (ULONG *)ExAllocatePool(PagedPool,
				Vcb->BlockSize);
		if (!pData)
		{
			FFSPrint((DBG_ERROR, "FFSGetBlock: no enough memory.\n"));
			return dwBlk;
		}

		KdPrint(("FFSGetBlock Index : %d, dwContent : %x, layer : %d\n", Index, dwContent, layer));

		if (!FFSLoadBlock(Vcb, dwContent, pData))
		{
			ExFreePool(pData);
			return 0;
		}

		temp = 1 << ((BLOCK_BITS - 2) * (layer - 1));

		i = Index / temp;
		j = Index % temp;

		dwBlk = pData[i];

		ExFreePool(pData);

		dwBlk = FFSGetBlock(Vcb, dwBlk, j, layer - 1);
	}

	return dwBlk;
}


ULONG
FFSBlockMap(
	IN PFFS_VCB   Vcb,
	IN PFFS_INODE dinode,
	IN ULONG      Index)
{
	ULONG   dwSizes[FFS_BLOCK_TYPES];
	int     i;
	ULONG   dwBlk;
	ULONG   Totalblocks;

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		dwSizes[i] = Vcb->dwData[i];
	}

	Totalblocks = (dinode->di_blocks);

	if (Index >= FFSDataBlocks(Vcb, Totalblocks))
	{
		FFSPrint((DBG_ERROR, "FFSBlockMap: error input paramters.\n"));

		FFSBreakPoint();

		return 0;
	}	

	/* 流立, 埃立, 2吝 埃立 贸府 */
	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		if (Index < dwSizes[i])
		{
			if (i == 0)
				dwBlk = dinode->di_db[Index]; /* 流立 */
			else
				dwBlk = dinode->di_ib[i - 1]; /* 埃立 */
#if DBG
			{   
				ULONG dwRet = FFSGetBlock(Vcb, dwBlk, Index , i);

				KdPrint(("FFSBlockMap: i : %d, Index : %d, dwBlk : %x, Data Block : %X\n", i, Index, dwRet, (dwRet * 0x400)));

				return dwRet;
			}
#else
			return FFSGetBlock(Vcb, dwBlk, Index , i);
#endif
		}

		Index -= dwSizes[i];
	}

	return 0;
}


ULONG
FFSBuildBDL(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_INODE       dinode,
	IN ULONG            Offset, 
	IN ULONG            Size, 
	OUT PFFS_BDL        *ffs_bdl)
{
	ULONG    nBeg, nEnd, nBlocks;
	ULONG    dwBlk, i;
	ULONG    dwBytes = 0;
	LONGLONG Lba;
	LONGLONG AllocSize;
	ULONG    Totalblocks;

	PFFS_BDL   ffsbdl;

	*ffs_bdl = NULL;


	Totalblocks = (dinode->di_blocks);
	AllocSize = FFSDataBlocks(Vcb, Totalblocks);
	AllocSize = (AllocSize << BLOCK_BITS);

	if ((LONGLONG)Offset >= AllocSize)
	{
		FFSPrint((DBG_ERROR, "FFSBuildBDL: beyond the file range.\n"));
		return 0;
	}

	if ((LONGLONG)(Offset + Size) > AllocSize)
	{
		Size = (ULONG)(AllocSize - Offset);
	}

	nBeg = (Offset >> BLOCK_BITS);
	nEnd = ((Size + Offset + Vcb->BlockSize - 1) >> BLOCK_BITS);

#if DBG
	KdPrint(("FFSBuildBDL() Offset : %x\n", Offset));
	KdPrint(("FFSBuildBDL() Size : %x\n", Size));
	KdPrint(("FFSBuildBDL() nBeg : %d, nEnd : %d\n", nBeg, nEnd));
#endif

	nBlocks = 0;

	if ((nEnd - nBeg) > 0)
	{
		ffsbdl = ExAllocatePool(PagedPool, sizeof(FFS_BDL) * (nEnd - nBeg));

		if (ffsbdl)
		{

			RtlZeroMemory(ffsbdl, sizeof(FFS_BDL) * (nEnd - nBeg));

			for (i = nBeg; i < nEnd; i++)
			{
				dwBlk = FFSBlockMap(Vcb, dinode, i);

				if (dwBlk > 0)
				{
					Lba = (LONGLONG) dwBlk;
					//Lba = Lba * Vcb->BlockSize;
					Lba = Lba * 0x400; // 1024

					if (nBeg == nEnd - 1) // ie. (nBeg == nEnd - 1)
					{
						dwBytes = Size;
						ffsbdl[nBlocks].Lba = Lba + (LONGLONG)(Offset % (Vcb->BlockSize));
						ffsbdl[nBlocks].Length = dwBytes;
						ffsbdl[nBlocks].Offset = 0;

						nBlocks++;
					}
					else
					{
						if (i == nBeg)
						{
							dwBytes = Vcb->BlockSize - (Offset % (Vcb->BlockSize));
							ffsbdl[nBlocks].Lba = Lba + (LONGLONG)(Offset % (Vcb->BlockSize));
							ffsbdl[nBlocks].Length = dwBytes;
							ffsbdl[nBlocks].Offset = 0;

							nBlocks++;
						}
						else if (i == nEnd - 1)
						{
							if (ffsbdl[nBlocks - 1].Lba + ffsbdl[nBlocks - 1].Length == Lba)
							{
								ffsbdl[nBlocks - 1].Length += Size - dwBytes;
							}
							else
							{
								ffsbdl[nBlocks].Lba = Lba;
								ffsbdl[nBlocks].Length = Size - dwBytes;
								ffsbdl[nBlocks].Offset = dwBytes;
								nBlocks++;
							}

							dwBytes = Size;

						}
						else
						{
							if (ffsbdl[nBlocks - 1].Lba + ffsbdl[nBlocks - 1].Length == Lba)
							{
								ffsbdl[nBlocks - 1].Length += Vcb->BlockSize;
							}
							else
							{
								ffsbdl[nBlocks].Lba = Lba;
								ffsbdl[nBlocks].Length = Vcb->BlockSize;
								ffsbdl[nBlocks].Offset = dwBytes;
								nBlocks++;
							}

							dwBytes +=  Vcb->BlockSize;
						}
					}
				}
				else
				{
					break;
				}
			}

			*ffs_bdl = ffsbdl;
			return nBlocks;
		}
	}

	// Error
	return 0;
}


BOOLEAN
FFSNewBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            GroupHint,
	ULONG            BlockHint,  
	PULONG           dwRet)
{
#if 0
	RTL_BITMAP      BlockBitmap;
	LARGE_INTEGER   Offset;
	ULONG           Length;

	PBCB            BitmapBcb;
	PVOID           BitmapCache;

	ULONG           Group = 0, dwBlk, dwHint = 0;

	*dwRet = 0;
	dwBlk = 0XFFFFFFFF;

	if (GroupHint > Vcb->ffs_groups)
		GroupHint = Vcb->ffs_groups - 1;

	if (BlockHint != 0)
	{
		GroupHint = (BlockHint - FFS_FIRST_DATA_BLOCK) / BLOCKS_PER_GROUP;
		dwHint = (BlockHint - FFS_FIRST_DATA_BLOCK) % BLOCKS_PER_GROUP;
	}

ScanBitmap:

	// Perform Prefered Group
	if (Vcb->ffs_group_desc[GroupHint].bg_free_blocks_count)
	{
		Offset.QuadPart = (LONGLONG) Vcb->BlockSize;
		Offset.QuadPart = Offset.QuadPart * 
			Vcb->ffs_group_desc[GroupHint].bg_block_bitmap;

		if (GroupHint == Vcb->ffs_groups - 1)
		{
			Length = TOTAL_BLOCKS % BLOCKS_PER_GROUP;

			/* s_blocks_count is integer multiple of s_blocks_per_group */
			if (Length == 0)
			{
				Length = BLOCKS_PER_GROUP;
			}
		}
		else
		{
			Length = BLOCKS_PER_GROUP;
		}

		if (!CcPinRead(Vcb->StreamObj,
					&Offset,
					Vcb->BlockSize,
					PIN_WAIT,
					&BitmapBcb,
					&BitmapCache))
		{
			FFSPrint((DBG_ERROR, "FFSNewBlock: PinReading error ...\n"));
			return FALSE;
		}

		RtlInitializeBitMap(&BlockBitmap,
				BitmapCache,
				Length);

		Group = GroupHint;

		if (RtlCheckBit(&BlockBitmap, dwHint) == 0)
		{
			dwBlk = dwHint;
		}
		else
		{
			dwBlk = RtlFindClearBits(&BlockBitmap, 1, dwHint);
		}

		// We could not get new block in the prefered group.
		if (dwBlk == 0xFFFFFFFF)
		{
			CcUnpinData(BitmapBcb);
			BitmapBcb = NULL;
			BitmapCache = NULL;

			RtlZeroMemory(&BlockBitmap, sizeof(RTL_BITMAP));
		}
	}

	if (dwBlk == 0xFFFFFFFF)
	{
		for(Group = 0; Group < Vcb->ffs_groups; Group++)
			if (Vcb->ffs_group_desc[Group].bg_free_blocks_count)
			{

				if (Group == GroupHint)
					continue;

				Offset.QuadPart = (LONGLONG) Vcb->BlockSize;
				Offset.QuadPart = Offset.QuadPart * Vcb->ffs_group_desc[Group].bg_block_bitmap;

				if (Vcb->ffs_groups == 1)
				{
					Length = TOTAL_BLOCKS;
				}
				else
				{
					if (Group == Vcb->ffs_groups - 1)
					{
						Length = TOTAL_BLOCKS % BLOCKS_PER_GROUP;

						/* s_blocks_count is integer multiple of s_blocks_per_group */
						if (Length == 0)
						{
							Length = BLOCKS_PER_GROUP;
						}
					}
					else
					{
						Length = BLOCKS_PER_GROUP;
					}
				}

				if (!CcPinRead(Vcb->StreamObj,
							&Offset,
							Vcb->BlockSize,
							PIN_WAIT,
							&BitmapBcb,
							&BitmapCache))
				{
					FFSPrint((DBG_ERROR, "FFSNewBlock: PinReading error ...\n"));
					return FALSE;
				}

				RtlInitializeBitMap(&BlockBitmap,
						BitmapCache,
						Length);

				dwBlk = RtlFindClearBits(&BlockBitmap, 1, 0);

				if (dwBlk != 0xFFFFFFFF)
				{
					break;
				}
				else
				{
					CcUnpinData(BitmapBcb);
					BitmapBcb = NULL;
					BitmapCache = NULL;

					RtlZeroMemory(&BlockBitmap, sizeof(RTL_BITMAP));
				}
			}
	}

	if (dwBlk < Length)
	{
		RtlSetBits(&BlockBitmap, dwBlk, 1);

		CcSetDirtyPinnedData(BitmapBcb, NULL);

		FFSRepinBcb(IrpContext, BitmapBcb);

		CcUnpinData(BitmapBcb);

		FFSAddMcbEntry(Vcb, Offset.QuadPart, (LONGLONG)Vcb->BlockSize);

		*dwRet = dwBlk + FFS_FIRST_DATA_BLOCK + Group * BLOCKS_PER_GROUP;

		//Updating Group Desc / Superblock
		Vcb->ffs_group_desc[Group].bg_free_blocks_count--;
		FFSSaveGroup(IrpContext, Vcb);

		Vcb->ffs_super_block->s_free_blocks_count--;
		FFSSaveSuper(IrpContext, Vcb);

		{
			ULONG i = 0;
			for (i = 0; i < Vcb->ffs_groups; i++)
			{
				if ((Vcb->ffs_group_desc[i].bg_block_bitmap == *dwRet) ||
						(Vcb->ffs_group_desc[i].bg_inode_bitmap == *dwRet) ||
						(Vcb->ffs_group_desc[i].bg_inode_table == *dwRet))
				{
					FFSBreakPoint();
					GroupHint = Group;
					goto ScanBitmap;
				}
			}
		}

		return TRUE;
	}
#endif
	return FALSE;
}


BOOLEAN
FFSFreeBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            Block)
{
#if 0
	RTL_BITMAP      BlockBitmap;
	LARGE_INTEGER   Offset;
	ULONG           Length;

	PBCB            BitmapBcb;
	PVOID           BitmapCache;

	ULONG           Group, dwBlk;
	BOOLEAN         bModified = FALSE;

	if (Block < FFS_FIRST_DATA_BLOCK || Block > (BLOCKS_PER_GROUP * Vcb->ffs_groups))
	{
		FFSBreakPoint();
		return TRUE;
	}

	FFSPrint((DBG_INFO, "FFSFreeBlock: Block %xh to be freed.\n", Block));

	Group = (Block - FFS_FIRST_DATA_BLOCK) / BLOCKS_PER_GROUP;

	dwBlk = (Block - FFS_FIRST_DATA_BLOCK) % BLOCKS_PER_GROUP;

	{
		Offset.QuadPart = (LONGLONG) Vcb->BlockSize;
		Offset.QuadPart = Offset.QuadPart * Vcb->ffs_group_desc[Group].bg_block_bitmap;

		if (Group == Vcb->ffs_groups - 1)
		{
			Length = TOTAL_BLOCKS % BLOCKS_PER_GROUP;

			/* s_blocks_count is integer multiple of s_blocks_per_group */
			if (Length == 0)
			{
				Length = BLOCKS_PER_GROUP;
			}
		}
		else
		{
			Length = BLOCKS_PER_GROUP;
		}

		if (!CcPinRead(Vcb->StreamObj,
					&Offset,
					Vcb->BlockSize,
					PIN_WAIT,
					&BitmapBcb,
					&BitmapCache))
		{
			FFSPrint((DBG_ERROR, "FFSDeleteBlock: PinReading error ...\n"));
			return FALSE;
		}

		RtlInitializeBitMap(&BlockBitmap,
				BitmapCache,
				Length);

		if (RtlCheckBit(&BlockBitmap, dwBlk) == 0)
		{

		}
		else
		{
			RtlClearBits(&BlockBitmap, dwBlk, 1);
			bModified = TRUE;
		}

		if (!bModified)
		{
			CcUnpinData(BitmapBcb);
			BitmapBcb = NULL;
			BitmapCache = NULL;

			RtlZeroMemory(&BlockBitmap, sizeof(RTL_BITMAP));
		}
	}

	if (bModified)
	{
		CcSetDirtyPinnedData(BitmapBcb, NULL);

		FFSRepinBcb(IrpContext, BitmapBcb);

		CcUnpinData(BitmapBcb);

		FFSAddMcbEntry(Vcb, Offset.QuadPart, (LONGLONG)Vcb->BlockSize);

		//Updating Group Desc / Superblock
		Vcb->ffs_group_desc[Group].bg_free_blocks_count++;
		FFSSaveGroup(IrpContext, Vcb);

		Vcb->ffs_super_block->s_free_blocks_count++;
		FFSSaveSuper(IrpContext, Vcb);

		return TRUE;
	}
#endif
	return FALSE;
}


BOOLEAN
FFSExpandBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	ULONG            dwContent,
	ULONG            Index,
	ULONG            layer,
	BOOLEAN          bNew,
	ULONG            *dwRet)
{
	ULONG       *pData = NULL;
	ULONG       i = 0, j = 0, temp = 1;
	ULONG       dwNewBlk = 0, dwBlk = 0;
	BOOLEAN     bDirty = FALSE;
	BOOLEAN     bRet = TRUE;

	PFFS_INODE       Inode = Fcb->dinode;
	PFFS_SUPER_BLOCK FFSSb = Vcb->ffs_super_block;

	pData = (ULONG *)ExAllocatePool(PagedPool, Vcb->BlockSize);

	if (!pData)
	{
		return FALSE;
	}

	RtlZeroMemory(pData, Vcb->BlockSize);

	if (bNew)
	{
		if (layer == 0)
		{
			if (IsDirectory(Fcb))
			{
				PFFS_DIR_ENTRY pEntry;

				pEntry = (PFFS_DIR_ENTRY) pData;
				pEntry->d_reclen = (USHORT)(Vcb->BlockSize);

				if (!FFSSaveBlock(IrpContext, Vcb, dwContent, (PVOID)pData))
				{
					bRet = FALSE;
					goto errorout;
				}
			}
			else
			{
				LARGE_INTEGER Offset;

				Offset.QuadPart = (LONGLONG)dwContent;
				Offset.QuadPart = Offset.QuadPart * Vcb->BlockSize;

				FFSRemoveMcbEntry(Vcb, Offset.QuadPart, (LONGLONG)Vcb->BlockSize);
			}
		}
		else
		{
			if (!FFSSaveBlock(IrpContext, Vcb, dwContent, (PVOID)pData))
			{
				bRet = FALSE;
				goto errorout;
			}
		}
	}

	if (layer == 0)
	{
		dwNewBlk = dwContent;
	}
	else if (layer <= 3)
	{
		if (!bNew)
		{
			bRet = FFSLoadBlock(Vcb, dwContent, (void *)pData);
			if (!bRet) goto errorout;
		}

		temp = 1 << ((BLOCK_BITS - 2) * (layer - 1));

		i = Index / temp;
		j = Index % temp;

		dwBlk = pData[i];

		if (dwBlk == 0)
		{
			if (!FFSNewBlock(IrpContext,
						Vcb, 0,
						dwContent,
						&dwBlk))
			{
				bRet = FALSE;
				FFSPrint((DBG_ERROR, "FFSExpandBlock: get new block error.\n"));
				goto errorout;
			}

			Inode->di_blocks += (Vcb->BlockSize / SECTOR_SIZE);

			pData[i] = dwBlk;
			bDirty = TRUE;
		}

		if (!FFSExpandBlock(IrpContext,
					Vcb, Fcb,
					dwBlk, j,
					layer - 1,
					bDirty,
					&dwNewBlk))
		{
			bRet = FALSE;
			FFSPrint((DBG_ERROR, "FFSExpandBlockk: ... error recuise...\n"));
			goto errorout;
		}

		if (bDirty)
		{
			bRet = FFSSaveBlock(IrpContext,
					Vcb, dwContent,
					(void *)pData);
		}
	}

errorout:

	if (pData)
		ExFreePool(pData);

	if (bRet && dwRet)
		*dwRet = dwNewBlk;

	return bRet;
}


BOOLEAN
FFSExpandInode(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	ULONG            *dwRet)
{
	ULONG    dwSizes[FFS_BLOCK_TYPES];
	ULONG    Index = 0;
	ULONG    dwTotal = 0;
	ULONG    dwBlk = 0, dwNewBlk = 0;
	ULONG    i;
	BOOLEAN  bRet = FALSE;
	BOOLEAN  bNewBlock = FALSE;

	PFFS_INODE Inode = Fcb->dinode;

	Index = (ULONG)(Fcb->Header.AllocationSize.QuadPart >> BLOCK_BITS);

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		dwSizes[i] = Vcb->dwData[i];
		dwTotal += dwSizes[i];
	}

	if (Index >= dwTotal)
	{
		FFSPrint((DBG_ERROR, "FFSExpandInode: beyond the maxinum size of an inode.\n"));
		return FALSE;
	}

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		if (Index < dwSizes[i])
		{
			dwBlk = Inode->di_db[i == 0 ? (Index) : (i + NDADDR - 1)];
			if (dwBlk == 0)
			{
				if (!FFSNewBlock(IrpContext,
							Vcb,
							Fcb->BlkHint ? 0 : ((Fcb->FFSMcb->Inode - 1) / INODES_PER_GROUP),
							Fcb->BlkHint,
							&dwBlk))
				{
					FFSPrint((DBG_ERROR, "FFSExpandInode: get new block error.\n"));
					break;
				}

				Inode->di_ib[i == 0 ? (Index):(i + NDADDR - 1)] = dwBlk;

				Inode->di_blocks += (Vcb->BlockSize / SECTOR_SIZE);

				bNewBlock = TRUE;
			}

			bRet = FFSExpandBlock(IrpContext,
					Vcb, Fcb,
					dwBlk, Index,
					i, bNewBlock,
					&dwNewBlk); 

			if (bRet)
			{
				Fcb->Header.AllocationSize.QuadPart += Vcb->BlockSize;
			}

			break;
		}

		Index -= dwSizes[i];
	}

	{
		ASSERT(FFSDataBlocks(Vcb, Inode->di_blocks/(BLOCK_SIZE/SECTOR_SIZE))
				== (Fcb->Header.AllocationSize.LowPart / BLOCK_SIZE));

		ASSERT(FFSTotalBlocks(Vcb, Fcb->Header.AllocationSize.LowPart/BLOCK_SIZE)
				== (Inode->di_blocks/(BLOCK_SIZE/SECTOR_SIZE)));
	}


	FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, Inode);

	if (bRet && dwNewBlk)
	{
		if (dwRet)
		{
			Fcb->BlkHint = dwNewBlk+1;
			*dwRet = dwNewBlk;

			FFSPrint((DBG_INFO, "FFSExpandInode: %S (%xh) i=%2.2xh Index=%8.8xh New Block=%8.8xh\n", Fcb->FFSMcb->ShortName.Buffer, Fcb->FFSMcb->Inode, i, Index, dwNewBlk));
		}

		return TRUE;
	}

	return FALSE;
}


NTSTATUS
FFSNewInode(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            GroupHint,
	ULONG            Type,
	PULONG           Inode)
{
#if 0
	RTL_BITMAP      InodeBitmap;
	PVOID           BitmapCache;
	PBCB            BitmapBcb;

	ULONG           Group, i, j;
	ULONG           Average, Length;
	LARGE_INTEGER   Offset;

	ULONG           dwInode;

	*Inode = dwInode = 0XFFFFFFFF;

repeat:

	Group = i = 0;

	if (Type == DT_DIR)
	{
		Average = Vcb->ffs_super_block->s_free_inodes_count / Vcb->ffs_groups;

		for (j = 0; j < Vcb->ffs_groups; j++)
		{
			i = (j + GroupHint) % (Vcb->ffs_groups);

			if ((Vcb->ffs_group_desc[i].bg_used_dirs_count << 8) < 
					Vcb->ffs_group_desc[i].bg_free_inodes_count)
			{
				Group = i + 1;
				break;
			}
		}

		if (!Group)
		{
			for (j = 0; j < Vcb->ffs_groups; j++)
			{
				if (Vcb->ffs_group_desc[j].bg_free_inodes_count >= Average)
				{
					if (!Group || (Vcb->ffs_group_desc[j].bg_free_blocks_count > Vcb->ffs_group_desc[Group].bg_free_blocks_count))
						Group = j + 1;
				}
			}
		}
	}
	else 
	{
		/*
		 * Try to place the inode in its parent directory (GroupHint)
		 */
		if (Vcb->ffs_group_desc[GroupHint].bg_free_inodes_count)
		{
			Group = GroupHint + 1;
		}
		else
		{
			i = GroupHint;

			/*
			 * Use a quadratic hash to find a group with a
			 * free inode
			 */
			for (j = 1; j < Vcb->ffs_groups; j <<= 1)
			{

				i += j;
				if (i > Vcb->ffs_groups) 
					i -= Vcb->ffs_groups;

				if (Vcb->ffs_group_desc[i].bg_free_inodes_count)
				{
					Group = i + 1;
					break;
				}
			}
		}

		if (!Group) {
			/*
			 * That failed: try linear search for a free inode
			 */
			i = GroupHint + 1;
			for (j = 2; j < Vcb->ffs_groups; j++)
			{
				if (++i >= Vcb->ffs_groups) i = 0;

				if (Vcb->ffs_group_desc[i].bg_free_inodes_count)
				{
					Group = i + 1;
					break;
				}
			}
		}
	}

	// Could not find a proper group.
	if (!Group)
	{
		return STATUS_DISK_FULL;
	}
	else
	{
		Group--;

		Offset.QuadPart = (LONGLONG) Vcb->BlockSize;
		Offset.QuadPart = Offset.QuadPart * Vcb->ffs_group_desc[Group].bg_inode_bitmap;

		if (Vcb->ffs_groups == 1)
		{
			Length = INODES_COUNT;
		}
		else
		{
			if (Group == Vcb->ffs_groups - 1)
			{
				Length = INODES_COUNT % INODES_PER_GROUP;
				if (!Length) 
				{
					/* INODES_COUNT is integer multiple of INODES_PER_GROUP */
					Length = INODES_PER_GROUP;
				}
			}
			else
			{
				Length = INODES_PER_GROUP;
			}
		}

		if (!CcPinRead(Vcb->StreamObj,
					&Offset,
					Vcb->BlockSize,
					PIN_WAIT,
					&BitmapBcb,
					&BitmapCache))
		{
			FFSPrint((DBG_ERROR, "FFSNewInode: PinReading error ...\n"));

			return STATUS_UNSUCCESSFUL;
		}

		RtlInitializeBitMap(&InodeBitmap,
				BitmapCache,
				Length);

		dwInode = RtlFindClearBits(&InodeBitmap, 1, 0);

		if (dwInode == 0xFFFFFFFF)
		{
			CcUnpinData(BitmapBcb);
			BitmapBcb = NULL;
			BitmapCache = NULL;

			RtlZeroMemory(&InodeBitmap, sizeof(RTL_BITMAP));
		}
	}

	if (dwInode == 0xFFFFFFFF || dwInode >= Length)
	{
		if (Vcb->ffs_group_desc[Group].bg_free_inodes_count != 0)
		{
			Vcb->ffs_group_desc[Group].bg_free_inodes_count = 0;

			FFSSaveGroup(IrpContext, Vcb);            
		}

		goto repeat;
	}
	else
	{
		RtlSetBits(&InodeBitmap, dwInode, 1);

		CcSetDirtyPinnedData(BitmapBcb, NULL);

		FFSRepinBcb(IrpContext, BitmapBcb);

		CcUnpinData(BitmapBcb);

		FFSAddMcbEntry(Vcb, Offset.QuadPart, (LONGLONG)Vcb->BlockSize);

		*Inode = dwInode + 1 + Group * INODES_PER_GROUP;

		//Updating Group Desc / Superblock
		Vcb->ffs_group_desc[Group].bg_free_inodes_count--;
		if (Type == FFS_FT_DIR)
		{
			Vcb->ffs_group_desc[Group].bg_used_dirs_count++;
		}

		FFSSaveGroup(IrpContext, Vcb);

		Vcb->ffs_super_block->s_free_inodes_count--;
		FFSSaveSuper(IrpContext, Vcb);

		return STATUS_SUCCESS;        
	}

	return STATUS_DISK_FULL;
#endif
	return STATUS_UNSUCCESSFUL;
}


BOOLEAN
FFSFreeInode(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            Inode,
	ULONG            Type)
{
#if 0
	RTL_BITMAP      InodeBitmap;
	PVOID           BitmapCache;
	PBCB            BitmapBcb;

	ULONG           Group;
	ULONG           Length;
	LARGE_INTEGER   Offset;

	ULONG           dwIno;
	BOOLEAN         bModified = FALSE;


	Group = (Inode - 1) / INODES_PER_GROUP;
	dwIno = (Inode - 1) % INODES_PER_GROUP;

	FFSPrint((DBG_INFO, "FFSFreeInode: Inode: %xh (Group/Off = %xh/%xh)\n",
				Inode, Group, dwIno));

	{
		Offset.QuadPart = (LONGLONG) Vcb->BlockSize;
		Offset.QuadPart = Offset.QuadPart * Vcb->ffs_group_desc[Group].bg_inode_bitmap;
		if (Group == Vcb->ffs_groups - 1)
		{
			Length = INODES_COUNT % INODES_PER_GROUP;
			if (!Length)
			{ /* s_inodes_count is integer multiple of s_inodes_per_group */
				Length = INODES_PER_GROUP;
			}
		}
		else
		{
			Length = INODES_PER_GROUP;
		}

		if (!CcPinRead(Vcb->StreamObj,
					&Offset,
					Vcb->BlockSize,
					PIN_WAIT,
					&BitmapBcb,
					&BitmapCache))
		{
			FFSPrint((DBG_ERROR, "FFSFreeInode: PinReading error ...\n"));
			return FALSE;
		}

		RtlInitializeBitMap(&InodeBitmap,
				BitmapCache,
				Length);

		if (RtlCheckBit(&InodeBitmap, dwIno) == 0)
		{

		}
		else
		{
			RtlClearBits(&InodeBitmap, dwIno, 1);
			bModified = TRUE;
		}

		if (!bModified)
		{
			CcUnpinData(BitmapBcb);
			BitmapBcb = NULL;
			BitmapCache = NULL;

			RtlZeroMemory(&InodeBitmap, sizeof(RTL_BITMAP));
		}
	}

	if (bModified)
	{
		CcSetDirtyPinnedData(BitmapBcb, NULL);

		FFSRepinBcb(IrpContext, BitmapBcb);

		CcUnpinData(BitmapBcb);

		FFSAddMcbEntry(Vcb, Offset.QuadPart, (LONGLONG)Vcb->BlockSize);

		//Updating Group Desc / Superblock
		if (Type == DT_DIR)
		{
			Vcb->ffs_group_desc[Group].bg_used_dirs_count--;
		}

		Vcb->ffs_group_desc[Group].bg_free_inodes_count++;
		FFSSaveGroup(IrpContext, Vcb);

		Vcb->ffs_super_block->s_free_inodes_count++;
		FFSSaveSuper(IrpContext, Vcb);

		return TRUE;
	}
#endif
	return FALSE;
}


NTSTATUS
FFSAddEntry(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Dcb,
	IN ULONG            FileType,
	IN ULONG            Inode,
	IN PUNICODE_STRING  FileName)
{
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

	PFFS_DIR_ENTRY          pDir = NULL;
	PFFS_DIR_ENTRY          pNewDir = NULL;
	PFFS_DIR_ENTRY          pTarget = NULL;

	ULONG                   Length = 0;
	ULONG                   dwBytes = 0;

	BOOLEAN                 bFound = FALSE;
	BOOLEAN                 bAdding = FALSE;

	BOOLEAN                 MainResourceAcquired = FALSE;

	ULONG                   dwRet;

	if (!IsDirectory(Dcb))
	{
		FFSBreakPoint();
		Status = STATUS_INVALID_PARAMETER;
		return Status;
	}

	MainResourceAcquired = ExAcquireResourceExclusiveLite(&Dcb->MainResource, TRUE);

	__try
	{
		Dcb->ReferenceCount++;

		pDir = (PFFS_DIR_ENTRY)ExAllocatePool(PagedPool,
				FFS_DIR_REC_LEN(FFS_NAME_LEN));
		if (!pDir)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

		pTarget = (PFFS_DIR_ENTRY)ExAllocatePool(PagedPool,
				2 * FFS_DIR_REC_LEN(FFS_NAME_LEN));
		if (!pTarget)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

#if 0
		if (IsFlagOn(SUPER_BLOCK->s_feature_incompat, 
					FFS_FEATURE_INCOMPAT_FILETYPE))
		{
			pDir->d_type = (UCHAR)FileType;
		}
		else
#endif
		{
			pDir->d_type = 0;
		}

		{
			OEM_STRING OemName;
			OemName.Buffer = pDir->d_name;
			OemName.MaximumLength = FFS_NAME_LEN;
			OemName.Length = 0;

			Status = FFSUnicodeToOEM(&OemName, FileName);

			if (!NT_SUCCESS(Status))
			{
				__leave;
			}

			pDir->d_namlen = (CCHAR)OemName.Length;
		}

		pDir->d_ino  = Inode;
		pDir->d_reclen = (USHORT)(FFS_DIR_REC_LEN(pDir->d_namlen));

		dwBytes = 0;

Repeat:

		while ((LONGLONG)dwBytes < Dcb->Header.AllocationSize.QuadPart)
		{
			RtlZeroMemory(pTarget, FFS_DIR_REC_LEN(FFS_NAME_LEN));

			// Reading the DCB contents
			Status = FFSReadInode(
						NULL,
						Vcb,
						Dcb->dinode,
						dwBytes,
						(PVOID)pTarget,
						FFS_DIR_REC_LEN(FFS_NAME_LEN),
						&dwRet);

			if (!NT_SUCCESS(Status))
			{
				FFSPrint((DBG_ERROR, "FFSAddDirectory: Reading Directory Content error.\n"));
				__leave;
			}

			if (((pTarget->d_ino == 0) && pTarget->d_reclen >= pDir->d_reclen) || 
					(pTarget->d_reclen >= FFS_DIR_REC_LEN(pTarget->d_namlen) + pDir->d_reclen))
			{
				if (pTarget->d_ino)
				{
					RtlZeroMemory(pTarget, 2 * FFS_DIR_REC_LEN(FFS_NAME_LEN));

					// Reading the DCB contents
					Status = FFSReadInode(
							NULL,
							Vcb,
							Dcb->dinode,
							dwBytes,
							(PVOID)pTarget,
							2 * FFS_DIR_REC_LEN(FFS_NAME_LEN),
							&dwRet);

					if (!NT_SUCCESS(Status))
					{
						FFSPrint((DBG_ERROR, "FFSAddDirectory: Reading Directory Content error.\n"));
						__leave;
					}

					Length = FFS_DIR_REC_LEN(pTarget->d_namlen);

					pNewDir = (PFFS_DIR_ENTRY) ((PUCHAR)pTarget + FFS_DIR_REC_LEN(pTarget->d_namlen));

					pNewDir->d_reclen = pTarget->d_reclen - FFS_DIR_REC_LEN(pTarget->d_namlen);

					pTarget->d_reclen = FFS_DIR_REC_LEN(pTarget->d_namlen);
				}
				else
				{
					pNewDir = pTarget;
					pNewDir->d_reclen = (USHORT)((ULONG)(Dcb->Header.AllocationSize.QuadPart) - dwBytes);
				}

				pNewDir->d_type = pDir->d_type;
				pNewDir->d_ino = pDir->d_ino;
				pNewDir->d_namlen = pDir->d_namlen;
				memcpy(pNewDir->d_name, pDir->d_name, pDir->d_namlen);
				Length += FFS_DIR_REC_LEN(pDir->d_namlen);

				bFound = TRUE;
				break;
			}

			dwBytes += pTarget->d_reclen;
		}

		if (bFound)
		{
			ULONG dwRet;

			if (FileType == DT_DIR)
			{
				if(((pDir->d_namlen == 1) && (pDir->d_name[0] == '.')) ||
						((pDir->d_namlen == 2) && (pDir->d_name[0] == '.') && (pDir->d_name[1] == '.')))
				{
				}
				else
				{
					Dcb->dinode->di_nlink++;
				}
			}

			Status = FFSWriteInode(IrpContext, Vcb, Dcb->dinode, dwBytes, pTarget, Length, FALSE, &dwRet);
		}
		else
		{
			// We should expand the size of the dir inode 
			if (!bAdding)
			{
				ULONG dwRet;

				bAdding = FFSExpandInode(IrpContext, Vcb, Dcb, &dwRet);

				if (bAdding)
				{

					Dcb->dinode->di_size = Dcb->Header.AllocationSize.LowPart;

					FFSSaveInode(IrpContext, Vcb, Dcb->FFSMcb->Inode, Dcb->dinode);

					Dcb->Header.FileSize = Dcb->Header.AllocationSize;

					goto Repeat;
				}

				__leave;

			}
			else  // Something must be error!
			{
				__leave;
			}
		}
	}

	__finally
	{

		Dcb->ReferenceCount--;

		if(MainResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Dcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (pTarget != NULL)
		{
			ExFreePool(pTarget);
		}

		if (pDir)
		{
			ExFreePool(pDir);
		}
	}

	return Status;
}


NTSTATUS
FFSRemoveEntry(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Dcb,
	IN ULONG            FileType,
	IN ULONG            Inode)
{
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

	PFFS_DIR_ENTRY          pTarget = NULL;
	PFFS_DIR_ENTRY          pPrevDir = NULL;

	USHORT                  PrevRecLen = 0;

	ULONG                   Length = 0;
	ULONG                   dwBytes = 0;

	BOOLEAN                 bRet = FALSE;
	BOOLEAN                 MainResourceAcquired = FALSE;

	ULONG                   dwRet;

	if (!IsDirectory(Dcb))
	{
		return FALSE;
	}

	MainResourceAcquired = 
		ExAcquireResourceExclusiveLite(&Dcb->MainResource, TRUE);

	__try
	{

		Dcb->ReferenceCount++;

		pTarget = (PFFS_DIR_ENTRY)ExAllocatePool(PagedPool,
				FFS_DIR_REC_LEN(FFS_NAME_LEN));
		if (!pTarget)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

		pPrevDir = (PFFS_DIR_ENTRY)ExAllocatePool(PagedPool,
				FFS_DIR_REC_LEN(FFS_NAME_LEN));
		if (!pPrevDir)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

		dwBytes = 0;

		while ((LONGLONG)dwBytes < Dcb->Header.AllocationSize.QuadPart)
		{
			RtlZeroMemory(pTarget, FFS_DIR_REC_LEN(FFS_NAME_LEN));

			Status = FFSReadInode(
						NULL,
						Vcb,
						Dcb->dinode,
						dwBytes,
						(PVOID)pTarget,
						FFS_DIR_REC_LEN(FFS_NAME_LEN),
						&dwRet);

			if (!NT_SUCCESS(Status))
			{
				FFSPrint((DBG_ERROR, "FFSRemoveEntry: Reading Directory Content error.\n"));
				__leave;
			}

			if (pTarget->d_ino == Inode)
			{
				ULONG   dwRet;
				ULONG   RecLen;

				if ((PrevRecLen + pTarget->d_reclen) < MAXIMUM_RECORD_LENGTH)
				{
					pPrevDir->d_reclen += pTarget->d_reclen;
					RecLen = FFS_DIR_REC_LEN(pTarget->d_namlen);

					RtlZeroMemory(pTarget, RecLen);

					FFSWriteInode(IrpContext, Vcb, Dcb->dinode, dwBytes - PrevRecLen, pPrevDir, 8, FALSE, &dwRet);
					FFSWriteInode(IrpContext, Vcb, Dcb->dinode, dwBytes, pTarget, RecLen, FALSE, &dwRet);
				}
				else
				{
					RecLen = (ULONG)pTarget->d_reclen;
					if (RecLen > FFS_DIR_REC_LEN(FFS_NAME_LEN))
					{
						RtlZeroMemory(pTarget, FFS_DIR_REC_LEN(FFS_NAME_LEN));
					}
					else
					{
						RtlZeroMemory(pTarget, RecLen);
					}

					pTarget->d_reclen = (USHORT)RecLen;

					FFSWriteInode(IrpContext, Vcb, Dcb->dinode, dwBytes, pTarget, RecLen, FALSE, &dwRet);
				}

				if (FileType == DT_DIR)
				{
					if(((pTarget->d_namlen == 1) && (pTarget->d_name[0] == '.')) ||
							((pTarget->d_namlen == 2) && (pTarget->d_name[0] == '.') && (pTarget->d_name[1] == '.')))
					{
						FFSBreakPoint();
					}
					else
					{
						Dcb->dinode->di_nlink--;
					}
				}

				/* Update at least mtime/atime if !FFS_FT_DIR. */
				FFSSaveInode(IrpContext, Vcb, Dcb->FFSMcb->Inode, Dcb->dinode);

				bRet = TRUE;

				break;
			}
			else
			{
				RtlCopyMemory(pPrevDir, pTarget, FFS_DIR_REC_LEN(FFS_NAME_LEN));
				PrevRecLen = pTarget->d_reclen;
			}

			dwBytes += pTarget->d_reclen;
		}
	}

	__finally
	{

		Dcb->ReferenceCount--;

		if(MainResourceAcquired)
			ExReleaseResourceForThreadLite(
					&Dcb->MainResource,
					ExGetCurrentResourceThread());

		if (pTarget != NULL)
		{
			ExFreePool(pTarget);
		}

		if (pPrevDir != NULL)
		{
			ExFreePool(pPrevDir);
		}
	}

	return bRet;
}


NTSTATUS
FFSSetParentEntry(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Dcb,
	IN ULONG            OldParent,
	IN ULONG            NewParent)
{
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

	PFFS_DIR_ENTRY          pSelf   = NULL;
	PFFS_DIR_ENTRY          pParent = NULL;

	ULONG                   dwBytes = 0;

	BOOLEAN                 MainResourceAcquired = FALSE;

	ULONG                   Offset = 0;

	if (!IsDirectory(Dcb))
	{
		Status = STATUS_INVALID_PARAMETER;
		return Status;
	}

	MainResourceAcquired = 
		ExAcquireResourceExclusiveLite(&Dcb->MainResource, TRUE);

	__try
	{
		Dcb->ReferenceCount++;

		pSelf = (PFFS_DIR_ENTRY)ExAllocatePool(PagedPool,
				FFS_DIR_REC_LEN(1) + FFS_DIR_REC_LEN(2));
		if (!pSelf)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

		dwBytes = 0;

		// Reading the DCB contents
		Status = FFSReadInode(
					NULL,
					Vcb,
					Dcb->dinode,
					Offset,
					(PVOID)pSelf,
					FFS_DIR_REC_LEN(1) + FFS_DIR_REC_LEN(2),
					&dwBytes);

		if (!NT_SUCCESS(Status))
		{
			FFSPrint((DBG_ERROR, "FFSSetParentEntry: Reading Directory Content error.\n"));
			__leave;
		}

		ASSERT(dwBytes == FFS_DIR_REC_LEN(1) + FFS_DIR_REC_LEN(2));

		pParent = (PFFS_DIR_ENTRY)((PUCHAR)pSelf + pSelf->d_reclen);

		if (pParent->d_ino != OldParent)
		{
			FFSBreakPoint();
		}

		pParent->d_ino = NewParent;

		Status = FFSWriteInode(IrpContext,
					Vcb, 
					Dcb->dinode,
					Offset,
					pSelf,
					dwBytes,
					FALSE,
					&dwBytes);
	}

	__finally
	{
		Dcb->ReferenceCount--;

		if(MainResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
					&Dcb->MainResource,
					ExGetCurrentResourceThread());
		}

		if (pSelf)
		{
			ExFreePool(pSelf);
		}
	}

	return Status;
}


BOOLEAN
FFSTruncateBlock(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Fcb,
	IN ULONG            dwContent,
	IN ULONG            Index,
	IN ULONG            layer,
	OUT BOOLEAN         *bFreed)
{
	ULONG       *pData = NULL;
	ULONG       i = 0, j = 0, temp = 1;
	BOOLEAN     bDirty = FALSE;
	BOOLEAN     bRet = FALSE;
	ULONG       dwBlk;

	LONGLONG    Offset;

	PBCB        Bcb;

	PFFS_INODE Inode = Fcb->dinode;

	*bFreed = FALSE;

	if (layer == 0)
	{
		//if (dwContent > 0 && dwContent < (BLOCKS_PER_GROUP * Vcb->ffs_groups))
		if (dwContent > 0)
		{
			bRet = FFSFreeBlock(IrpContext, Vcb, dwContent);

			if (bRet)
			{
				ASSERT(Inode->di_blocks >= (Vcb->BlockSize / SECTOR_SIZE));
				Inode->di_blocks -= (Vcb->BlockSize / SECTOR_SIZE);            
			}
		}
		else
		{
			FFSBreakPoint();
			bRet = FALSE;
		}

		*bFreed = bRet;
	}
	else if (layer <= 3)
	{
		Offset = (LONGLONG)dwContent;
		Offset = Offset * Vcb->BlockSize;

		if(!CcPinRead(Vcb->StreamObj,
					(PLARGE_INTEGER)(&Offset),
					Vcb->BlockSize,
					PIN_WAIT,
					&Bcb,
					&pData))
		{
			FFSPrint((DBG_ERROR, "FFSSaveBuffer: PinReading error ...\n"));
			goto errorout;
		}

		temp = 1 << ((BLOCK_BITS - 2) * (layer - 1));

		i = Index / temp;
		j = Index % temp;

		dwBlk = pData[i];

		if(!FFSTruncateBlock(IrpContext, Vcb, Fcb, dwBlk, j, layer - 1, &bDirty))
		{
			goto errorout;
		}

		if (bDirty)
		{
			pData[i] = 0;
		}

		if (i == 0 && j == 0)
		{
			CcUnpinData(Bcb);
			pData = NULL;

			*bFreed = TRUE;
			bRet = FFSFreeBlock(IrpContext, Vcb, dwContent);

			if (bRet)
			{
				ASSERT(Inode->di_blocks >= (Vcb->BlockSize / SECTOR_SIZE));
				Inode->di_blocks -= (Vcb->BlockSize / SECTOR_SIZE);
			}
		}
		else
		{
			CcSetDirtyPinnedData(Bcb, NULL);
			FFSRepinBcb(IrpContext, Bcb);

			FFSAddMcbEntry(Vcb, Offset, (LONGLONG)Vcb->BlockSize);

			bRet = TRUE;
			*bFreed = FALSE;
		}
	}

errorout:

	if (pData)
	{
		CcUnpinData(Bcb);
	}

	return bRet;
}


BOOLEAN
FFSTruncateInode(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Fcb)
{
	ULONG    dwSizes[FFS_BLOCK_TYPES];
	ULONG    Index = 0;
	ULONG    dwTotal = 0;
	ULONG    dwBlk = 0;

	ULONG    i;
	BOOLEAN  bRet = FALSE;
	BOOLEAN  bFreed = FALSE;

	PFFS_INODE Inode = Fcb->dinode;

	Index = (ULONG)(Fcb->Header.AllocationSize.QuadPart >> BLOCK_BITS);

	if (Index > 0) 
	{
		Index--;
	}
	else
	{
		return TRUE;
	}

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		dwSizes[i] = Vcb->dwData[i];
		dwTotal += dwSizes[i];
	}

	if (Index >= dwTotal)
	{
		FFSPrint((DBG_ERROR, "FFSExpandInode: beyond the maxinum size of an inode.\n"));
		return TRUE;
	}

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
#if 0
		if (Index < dwSizes[i])
		{
			dwBlk = Inode->i_block[i == 0 ? (Index) : (i + NDADDR - 1)];

			bRet = FFSTruncateBlock(IrpContext, Vcb, Fcb, dwBlk, Index , i, &bFreed); 

			if (bRet)
			{
				Fcb->Header.AllocationSize.QuadPart -= Vcb->BlockSize;

				if (bFreed)
				{
					Inode->i_block[i == 0 ? (Index) : (i + NDADDR - 1)] = 0;
				}
			}

			break;
		}
#endif
		Index -= dwSizes[i];
	}

	{
		ASSERT(FFSDataBlocks(Vcb, Inode->di_blocks/(BLOCK_SIZE/SECTOR_SIZE))
				== (Fcb->Header.AllocationSize.LowPart / BLOCK_SIZE));

		ASSERT(FFSTotalBlocks(Vcb, Fcb->Header.AllocationSize.LowPart/BLOCK_SIZE)
				== (Inode->di_blocks/(BLOCK_SIZE/SECTOR_SIZE)));

	}

	//
	// Inode struct saving is done externally.
	//

	FFSSaveInode(IrpContext, Vcb, Fcb->FFSMcb->Inode, Inode);


	return bRet;
}


BOOLEAN
FFSAddMcbEntry(
	IN PFFS_VCB Vcb,
	IN LONGLONG Lba,
	IN LONGLONG Length)
{
	BOOLEAN     bRet = FALSE;

	LONGLONG    Offset;

#if DBG
	LONGLONG    DirtyLba;
	LONGLONG    DirtyLen;
#endif


	Offset = Lba & (~((LONGLONG)BLOCK_SIZE - 1));

	Length = (Length + Lba - Offset + BLOCK_SIZE - 1) &
		(~((LONGLONG)BLOCK_SIZE - 1));

	ASSERT ((Offset & (BLOCK_SIZE - 1)) == 0);
	ASSERT ((Length & (BLOCK_SIZE - 1)) == 0);

	Offset = (Offset >> BLOCK_BITS) + 1;
	Length = (Length >>BLOCK_BITS);

	ExAcquireResourceExclusiveLite(
			&(Vcb->McbResource),
			TRUE);

	FFSPrint((DBG_INFO, "FFSAddMcbEntry: Lba=%I64xh Length=%I64xh\n",
				Offset, Length));

#if DBG
	bRet = FsRtlLookupLargeMcbEntry(
			&(Vcb->DirtyMcbs),
			Offset,
			&DirtyLba,
			&DirtyLen,
			NULL,
			NULL,
			NULL);

	if (bRet && DirtyLba == Offset && DirtyLen >= Length)
	{
		FFSPrint((DBG_INFO, "FFSAddMcbEntry: this run already exists.\n"));
	}
#endif

	__try
	{
		bRet = FsRtlAddLargeMcbEntry(
				&(Vcb->DirtyMcbs),
				Offset, Offset,
				Length);

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		FFSBreakPoint();
		bRet = FALSE;
	}

#if DBG
	if (bRet)
	{
		BOOLEAN     bFound = FALSE;
		LONGLONG    RunStart;
		LONGLONG    RunLength;
		ULONG       Index;

		bFound = FsRtlLookupLargeMcbEntry(
				&(Vcb->DirtyMcbs),
				Offset,
				&DirtyLba,
				&DirtyLen,
				&RunStart,
				&RunLength,
				&Index);

		if ((!bFound) || (DirtyLba == -1) ||
				(DirtyLba != Offset) || (DirtyLen < Length))
		{
			LONGLONG            DirtyVba;
			LONGLONG            DirtyLba;
			LONGLONG            DirtyLength;

			FFSBreakPoint();

			for (Index = 0; 
					FsRtlGetNextLargeMcbEntry(&(Vcb->DirtyMcbs),
						Index,
						&DirtyVba,
						&DirtyLba,
						&DirtyLength); 
					Index++)
			{
				FFSPrint((DBG_INFO, "Index = %xh\n", Index));
				FFSPrint((DBG_INFO, "DirtyVba = %I64xh\n", DirtyVba));
				FFSPrint((DBG_INFO, "DirtyLba = %I64xh\n", DirtyLba));
				FFSPrint((DBG_INFO, "DirtyLen = %I64xh\n\n", DirtyLength));
			}
		}
	}
#endif

	ExReleaseResourceForThreadLite(
			&(Vcb->McbResource),
			ExGetCurrentResourceThread());

	return bRet;
}


VOID
FFSRemoveMcbEntry(
	IN PFFS_VCB Vcb,
	IN LONGLONG Lba,
	IN LONGLONG Length)
{
	LONGLONG Offset;

	Offset = Lba & (~((LONGLONG)BLOCK_SIZE - 1));

	Length = (Length + Lba - Offset + BLOCK_SIZE - 1) &
		(~((LONGLONG)BLOCK_SIZE - 1));

	ASSERT(Offset == Lba);

	ASSERT ((Offset & (BLOCK_SIZE - 1)) == 0);
	ASSERT ((Length & (BLOCK_SIZE - 1)) == 0);

	Offset = (Offset >> BLOCK_BITS) + 1;
	Length = (Length >> BLOCK_BITS);

	FFSPrint((DBG_INFO, "FFSRemoveMcbEntry: Lba=%I64xh Length=%I64xh\n",
				Offset, Length));

	ExAcquireResourceExclusiveLite(
			&(Vcb->McbResource),
			TRUE);

	__try
	{
		FsRtlRemoveLargeMcbEntry(
				&(Vcb->DirtyMcbs),
				Offset, Length);

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		FFSBreakPoint();
	}

#if DBG
	{
		BOOLEAN  bFound = FALSE;
		LONGLONG DirtyLba, DirtyLen;

		bFound = FsRtlLookupLargeMcbEntry(
					&(Vcb->DirtyMcbs),
					Offset,
					&DirtyLba,
					&DirtyLen,
					NULL,
					NULL,
					NULL);

		if (bFound &&(DirtyLba != -1))
		{
			FFSBreakPoint();
		}
	}
#endif

	ExReleaseResourceForThreadLite(
			&(Vcb->McbResource),
			ExGetCurrentResourceThread());
}


BOOLEAN
FFSLookupMcbEntry(
	IN PFFS_VCB     Vcb,
	IN LONGLONG     Lba,
	OUT PLONGLONG   pLba,
	OUT PLONGLONG   pLength,
	OUT PLONGLONG   RunStart,
	OUT PLONGLONG   RunLength,
	OUT PULONG      Index)
{
	BOOLEAN     bRet;
	LONGLONG    Offset;


	Offset = Lba & (~((LONGLONG)BLOCK_SIZE - 1));
	ASSERT ((Offset & (BLOCK_SIZE - 1)) == 0);

	ASSERT(Lba == Offset);

	Offset = (Offset >> BLOCK_BITS) + 1;

	ExAcquireResourceExclusiveLite(
			&(Vcb->McbResource),
			TRUE);

	bRet = FsRtlLookupLargeMcbEntry(
			&(Vcb->DirtyMcbs),
			Offset,
			pLba,
			pLength,
			RunStart,
			RunLength,
			Index);

	ExReleaseResourceForThreadLite(
			&(Vcb->McbResource),
			ExGetCurrentResourceThread());

	if (bRet)
	{
		if (pLba && ((*pLba) != -1))
		{
			ASSERT((*pLba) > 0);

			(*pLba) = (((*pLba) - 1) << BLOCK_BITS);
			(*pLba) += ((Lba) & ((LONGLONG)BLOCK_SIZE - 1));
		}

		if (pLength)
		{
			(*pLength) <<= BLOCK_BITS;
			(*pLength)  -= ((Lba) & ((LONGLONG)BLOCK_SIZE - 1));
		}

		if (RunStart && (*RunStart != -1))
		{
			(*RunStart) = (((*RunStart) - 1) << BLOCK_BITS);
		}

		if (RunLength)
		{
			(*RunLength) <<= BLOCK_BITS;
		}
	}

	return bRet;
}


ULONG
FFSDataBlocks(
	PFFS_VCB Vcb,
	ULONG TotalBlocks)
{
	ULONG   dwData[FFS_BLOCK_TYPES];
	ULONG   dwMeta[FFS_BLOCK_TYPES];
	ULONG   DataBlocks = 0;
	ULONG   i, j;

	if (TotalBlocks <= NDADDR)
	{
		return TotalBlocks;
	}

	TotalBlocks -= NDADDR;

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		if (i == 0)
		{
			dwData[i] = 1;
		}
		else
		{
			dwData[i] = Vcb->dwData[i];
		}

		dwMeta[i] = Vcb->dwMeta[i];
	}

	for(i = 1; (i < FFS_BLOCK_TYPES) && (TotalBlocks > 0); i++)
	{
		if (TotalBlocks >= (dwData[i] + dwMeta[i]))
		{
			TotalBlocks -= (dwData[i] + dwMeta[i]);
			DataBlocks  += dwData[i];
		}
		else
		{
			ULONG   dwDivide = 0;
			ULONG   dwRemain = 0;

			for (j = i; (j > 0) && (TotalBlocks > 0); j--)
			{
				dwDivide = (TotalBlocks - 1) / (dwData[j - 1] + dwMeta[j - 1]);
				dwRemain = (TotalBlocks - 1) % (dwData[j - 1] + dwMeta[j - 1]);

				DataBlocks += (dwDivide * dwData[j - 1]);
				TotalBlocks = dwRemain;
			}
		}
	}

	return (DataBlocks + NDADDR);
}


ULONG
FFSTotalBlocks(
	PFFS_VCB Vcb,
	ULONG DataBlocks)
{
	ULONG   dwData[FFS_BLOCK_TYPES];
	ULONG   dwMeta[FFS_BLOCK_TYPES];
	ULONG   TotalBlocks = 0;
	ULONG   i, j;

	if (DataBlocks <= NDADDR)
	{
		return DataBlocks;
	}

	DataBlocks -= NDADDR;

	for (i = 0; i < FFS_BLOCK_TYPES; i++)
	{
		if (i == 0)
		{
			dwData[i] = 1;
		}
		else
		{
			dwData[i] = Vcb->dwData[i];
		}

		dwMeta[i] = Vcb->dwMeta[i];
	}

	for(i = 1; (i < FFS_BLOCK_TYPES) && (DataBlocks > 0); i++)
	{
		if (DataBlocks >= dwData[i])
		{
			DataBlocks  -= dwData[i];
			TotalBlocks += (dwData[i] + dwMeta[i]);
		}
		else
		{
			ULONG   dwDivide = 0;
			ULONG   dwRemain = 0;

			for (j = i; (j > 0) && (DataBlocks > 0); j--)
			{
				dwDivide = (DataBlocks) / (dwData[j - 1]);
				dwRemain = (DataBlocks) % (dwData[j - 1]);

				TotalBlocks += (dwDivide * (dwData[j - 1] + dwMeta[j - 1]) + 1);
				DataBlocks = dwRemain;
			}
		}
	}

	return (TotalBlocks + NDADDR);
}
