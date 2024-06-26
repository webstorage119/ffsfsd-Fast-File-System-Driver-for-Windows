/* 
 * FFS File System Driver for Windows
 *
 * ffsdrv.h
 *
 * 2004.5.6 ~
 *
 * Lee Jae-Hong, http://www.pyrasis.com
 *
 */

#ifndef _FFS_HEADER_
#define _FFS_HEADER_

/* include files */
#include "fs.h"
#include "dinode.h"
#include "dir.h"
#include <ntdddisk.h>

#pragma pack(1)

/* debug */
#if DBG
	#define FFSBreakPoint()    __asm int 3 //DbgBreakPoint()
#else
	#define FFSBreakPoint()
#endif

/* Structs & Consts */

#define FFSDRV_VERSION  "0.3"

/*
 * ffsdrv build options
 */

/* To build read-only driver */

#define FFS_READ_ONLY  TRUE


/* To support driver dynamics unload */

#define FFS_UNLOAD     TRUE

/*
 * Constants
 */

#define FFS_BLOCK_TYPES                 (0x04)

#define MAXIMUM_RECORD_LENGTH           (0x10000)

#define SECTOR_BITS                     (Vcb->SectorBits)
#define SECTOR_SIZE                     (Vcb->DiskGeometry.BytesPerSector)
#define DEFAULT_SECTOR_SIZE             (0x200)

#define SUPER_BLOCK_OFFSET              (0x2000)
#define SUPER_BLOCK_SIZE                (0x5DC)

#define READ_AHEAD_GRANULARITY          (0x10000)

#define SUPER_BLOCK                     (Vcb->ffs_super_block)

#define BLOCK_SIZE                      (Vcb->BlockSize)
#define BLOCK_BITS                      (13) /* 8192 (0x2000) */

#define INODES_COUNT                    (Vcb->ffs_super_block->s_inodes_count)

#define INODES_PER_GROUP                (SUPER_BLOCK->fs_ipg)
#define BLOCKS_PER_GROUP                (SUPER_BLOCK->fs_fpg)
#define TOTAL_BLOCKS                    (SUPER_BLOCK->fs_size)



/* File System Releated */

#define DRIVER_NAME     "FFS"
#define DEVICE_NAME     L"\\FileSystem\\FFS"

/* Registry */

#define PARAMETERS_KEY    L"\\Parameters"

#define WRITING_SUPPORT     L"WritingSupport"
#define CHECKING_BITMAP     L"CheckingBitmap"

/* To support ffsdrv unload routine */
#if FFS_UNLOAD
#define DOS_DEVICE_NAME L"\\DosDevices\\ffs"

/*
 * Private IOCTL to make the driver ready to unload
 */
#define IOCTL_PREPARE_TO_UNLOAD \
CTL_CODE(FILE_DEVICE_UNKNOWN, 2048, METHOD_NEITHER, FILE_WRITE_ACCESS)

#endif // FFS_UNLOAD

#ifndef SetFlag
#define SetFlag(x,f)    ((x) |= (f))
#endif

#ifndef ClearFlag
#define ClearFlag(x,f)  ((x) &= ~(f))
#endif

#define IsFlagOn(a,b) ((BOOLEAN)(FlagOn(a,b) == b))

#define FFSRaiseStatus(IRPCONTEXT,STATUS) {  \
	(IRPCONTEXT)->ExceptionCode = (STATUS); \
	ExRaiseStatus( (STATUS) );                \
}

#define FFSNormalizeAndRaiseStatus(IRPCONTEXT,STATUS) {                        \
	/* (IRPCONTEXT)->ExceptionStatus = (STATUS);  */                            \
	if ((STATUS) == STATUS_VERIFY_REQUIRED) { ExRaiseStatus((STATUS)); }        \
	ExRaiseStatus(FsRtlNormalizeNtstatus((STATUS),STATUS_UNEXPECTED_IO_ERROR)); \
}

/*
 * Define IsEndofFile for read and write operations
 */

#define FILE_WRITE_TO_END_OF_FILE       0xffffffff
#define FILE_USE_FILE_POINTER_POSITION  0xfffffffe

#define IsEndOfFile(Pos) ((Pos.LowPart == FILE_WRITE_TO_END_OF_FILE) && \
                          (Pos.HighPart == FILE_USE_FILE_POINTER_POSITION ))

#define IsDirectory(Fcb) IsFlagOn(Fcb->FFSMcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY)

/*
 * Bug Check Codes Definitions
 */

#define FFS_FILE_SYSTEM   (FILE_SYSTEM)

#define FFS_BUGCHK_BLOCK               (0x00010000)
#define FFS_BUGCHK_CLEANUP             (0x00020000)
#define FFS_BUGCHK_CLOSE               (0x00030000)
#define FFS_BUGCHK_CMCB                (0x00040000)
#define FFS_BUGCHK_CREATE              (0x00050000)
#define FFS_BUGCHK_DEBUG               (0x00060000)
#define FFS_BUGCHK_DEVCTL              (0x00070000)
#define FFS_BUGCHK_DIRCTL              (0x00080000)
#define FFS_BUGCHK_DISPATCH            (0x00090000)
#define FFS_BUGCHK_EXCEPT              (0x000A0000)
#define FFS_BUGCHK_FFS                 (0x000B0000)
#define FFS_BUGCHK_FASTIO              (0x000C0000)
#define FFS_BUGCHK_FILEINFO            (0x000D0000)
#define FFS_BUGCHK_FLUSH               (0x000E0000)
#define FFS_BUGCHK_FSCTL               (0x000F0000)
#define FFS_BUGCHK_INIT                (0x00100000)
#define FFS_BUGCHK_LOCK                (0x0011000)
#define FFS_BUGCHK_MEMORY              (0x0012000)
#define FFS_BUGCHK_MISC                (0x0013000)
#define FFS_BUGCHK_READ                (0x00140000)
#define FFS_BUGCHK_SHUTDOWN            (0x00150000)
#define FFS_BUGCHK_VOLINFO             (0x00160000)
#define FFS_BUGCHK_WRITE               (0x00170000)

#define FFS_BUGCHK_LAST                (0x00170000)

#define FFSBugCheck(A,B,C,D) { KeBugCheckEx(FFS_FILE_SYSTEM, A | __LINE__, B, C, D ); }


/* FFS file system definions */

/*
 * Structure of a directory entry
 */
#define FFS_NAME_LEN 255

#define FFS_ROOT_INO           2   /* Root inode */


/*
 * FFS_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define FFS_DIR_PAD		 	4
#define FFS_DIR_ROUND 			(FFS_DIR_PAD - 1)
#define FFS_DIR_REC_LEN(name_len)	(((name_len) + 8 + FFS_DIR_ROUND) & \
					 ~FFS_DIR_ROUND)




#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)
#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#define S_ISFIL(m)      (((m) & S_IFMT) == S_IFFIL)
#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)

#define S_IPERMISSION_MASK 0x1FF /*  */

#define S_IRWXU 0x01C0     /*  00700 */
#define S_IRUSR 0x0100     /*  00400 */
#define S_IWUSR 0x0080     /*  00200 */
#define S_IXUSR 0x0040     /*  00100 */

#define S_IRWXG 0x0038     /*  00070 */
#define S_IRGRP 0x0020     /*  00040 */
#define S_IWGRP 0x0010     /*  00020 */
#define S_IXGRP 0x0008     /*  00010 */

#define S_IRWXO 0x0007     /*  00007 */
#define S_IROTH 0x0004     /*  00004 */
#define S_IWOTH 0x0002     /*  00002 */
#define S_IXOTH 0x0001     /*  00001 */

#define S_ISREADABLE(m)    (((m) & S_IPERMISSION_MASK) == (S_IRUSR | S_IRGRP | S_IROTH))
#define S_ISWRITABLE(m)    (((m) & S_IPERMISSION_MASK) == (S_IWUSR | S_IWGRP | S_IWOTH))

#define FFSSetReadable(m) (m) = ((m) | (S_IRUSR | S_IRGRP | S_IROTH))
#define FFSSetWritable(m) (m) = ((m) | (S_IWUSR | S_IWGRP | S_IWOTH))

#define FFSSetReadOnly(m) (m) = ((m) & (~(S_IWUSR | S_IWGRP | S_IWOTH)))
#define FFSIsReadOnly(m)  (!((m) & (S_IWUSR | S_IWGRP | S_IWOTH)))

#define FFS_FIRST_DATA_BLOCK   (Vcb->ffs_super_block->s_first_data_block)

typedef struct fs FFS_SUPER_BLOCK, *PFFS_SUPER_BLOCK;

typedef struct ufs1_dinode FFS_INODE, *PFFS_INODE;
typedef struct direct FFS_DIR_ENTRY, *PFFS_DIR_ENTRY;


/*
 * ffsdrv Driver Definitions
 */

/*
 * FFS_IDENTIFIER_TYPE
 *
 * Identifiers used to mark the structures
 */

typedef enum _FFS_IDENTIFIER_TYPE {
	FFSFGD  = ':DGF',
	FFSVCB  = ':BCV',
	FFSFCB  = ':BCF',
	FFSCCB  = ':BCC',
	FFSICX  = ':XCI',
	FFSDRV  = ':VRD',
	FFSMCB  = ':BCM'
} FFS_IDENTIFIER_TYPE;

/*
 * FFS_IDENTIFIER
 *
 * Header used to mark the structures
 */
typedef struct _FFS_IDENTIFIER {
	FFS_IDENTIFIER_TYPE      Type;
	ULONG                    Size;
} FFS_IDENTIFIER, *PFFS_IDENTIFIER;


#define NodeType(Ptr) (*((FFS_IDENTIFIER_TYPE *)(Ptr)))

typedef struct _FFS_MCB  FFS_MCB, *PFFS_MCB;


typedef PVOID   PBCB;

/*
 * REPINNED_BCBS List
 */

#define FFS_REPINNED_BCBS_ARRAY_SIZE         (8)

typedef struct _FFS_REPINNED_BCBS {

	//
	//  A pointer to the next structure contains additional repinned bcbs
	//

	struct _FFS_REPINNED_BCBS *Next;

	//
	//  A fixed size array of pinned bcbs.  Whenever a new bcb is added to
	//  the repinned bcb structure it is added to this array.  If the
	//  array is already full then another repinned bcb structure is allocated
	//  and pointed to with Next.
	//

	PBCB Bcb[ FFS_REPINNED_BCBS_ARRAY_SIZE ];

} FFS_REPINNED_BCBS, *PFFS_REPINNED_BCBS;


/*
 * FFS_GLOBAL_DATA
 *
 * Data that is not specific to a mounted volume
 */
typedef struct _FFS_GLOBAL {

	// Identifier for this structure
	FFS_IDENTIFIER              Identifier;

	// Syncronization primitive for this structure
	ERESOURCE                   Resource;

	// Syncronization primitive for Counting
	ERESOURCE                   CountResource;

	// Syncronization primitive for LookAside Lists
	ERESOURCE                   LAResource;

	// Table of pointers to the fast I/O entry points
	FAST_IO_DISPATCH            FastIoDispatch;

	// Table of pointers to the Cache Manager callbacks
	CACHE_MANAGER_CALLBACKS     CacheManagerCallbacks;
	CACHE_MANAGER_CALLBACKS     CacheManagerNoOpCallbacks;

	// Pointer to the driver object
	PDRIVER_OBJECT              DriverObject;

	// Pointer to the main device object
	PDEVICE_OBJECT              DeviceObject;

	// List of mounted volumes
	LIST_ENTRY                  VcbList;

	// Look Aside table of IRP_CONTEXT, FCB, MCB, CCB
	USHORT                      MaxDepth;
	NPAGED_LOOKASIDE_LIST       FFSIrpContextLookasideList;
	NPAGED_LOOKASIDE_LIST       FFSFcbLookasideList;
	NPAGED_LOOKASIDE_LIST       FFSCcbLookasideList;
	PAGED_LOOKASIDE_LIST        FFSMcbLookasideList;

	// Mcb Count ...
	USHORT                      McbAllocated;

#if DBG
	// Fcb Count
	USHORT                      FcbAllocated;

	// IRP_MJ_CLOSE : FCB
	USHORT                      IRPCloseCount;
#endif

	// Global flags for the driver
	ULONG                       Flags;

} FFS_GLOBAL, *PFFS_GLOBAL;

/*
 * Flags for FFS_GLOBAL_DATA
 */
#define FFS_UNLOAD_PENDING     0x00000001
#define FFS_SUPPORT_WRITING    0x00000002
#define FFS_CHECKING_BITMAP    0x00000008

/*
 * Driver Extension define
 */
typedef struct {
	FFS_GLOBAL FFSGlobal;
} FFS_EXT, *PFFS_EXT;


typedef struct _FFS_FCBVCB {

	// FCB header required by NT
	FSRTL_COMMON_FCB_HEADER         CommonFCBHeader;
	SECTION_OBJECT_POINTERS         SectionObject;
	ERESOURCE                       MainResource;
	ERESOURCE                       PagingIoResource;
	// end FCB header required by NT

	// Identifier for this structure
	FFS_IDENTIFIER                  Identifier;
} FFS_FCBVCB, *PFFS_FCBVCB;

/*
 * FFS_VCB Volume Control Block
 *
 * Data that represents a mounted logical volume
 * It is allocated as the device extension of the volume device object
 */
typedef struct _FFS_VCB {

	// FCB header required by NT
	// The VCB is also used as an FCB for file objects
	// that represents the volume itself
	FSRTL_COMMON_FCB_HEADER     Header;
	SECTION_OBJECT_POINTERS     SectionObject;
	ERESOURCE                   MainResource;
	ERESOURCE                   PagingIoResource;
	// end FCB header required by NT

	// Identifier for this structure
	FFS_IDENTIFIER              Identifier;

	LIST_ENTRY                  Next;

	// Share Access for the file object
	SHARE_ACCESS                ShareAccess;

	// Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
	// for files on this volume.
	ULONG                       OpenFileHandleCount;

	// Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
	// for both files on this volume and open instances of the
	// volume itself.
	ULONG                       ReferenceCount;
	ULONG                       OpenHandleCount;

	//
	// Disk change count
	//

	ULONG                       ChangeCount;

	// Pointer to the VPB in the target device object
	PVPB                        Vpb;

	// The FileObject of Volume used to lock the volume
	PFILE_OBJECT                LockFile;

	// List of FCBs for open files on this volume
	LIST_ENTRY                  FcbList;

	// List of IRPs pending on directory change notify requests
	LIST_ENTRY                  NotifyList;

	// Pointer to syncronization primitive for this list
	PNOTIFY_SYNC                NotifySync;

	// This volumes device object
	PDEVICE_OBJECT              DeviceObject;

	// The physical device object (the disk)
	PDEVICE_OBJECT              TargetDeviceObject;

	// The physical device object (the disk)
	PDEVICE_OBJECT              RealDevice;

	// Information about the physical device object
	DISK_GEOMETRY               DiskGeometry;
	PARTITION_INFORMATION       PartitionInformation;

	PFFS_SUPER_BLOCK            ffs_super_block;

	// Number of Group Decsciptions
	ULONG                       ffs_groups;
	/*
	// Bitmap Block per group
	PRTL_BITMAP                 BlockBitMaps;
	PRTL_BITMAP                 InodeBitMaps;
	*/
	// Block / Cluster size
	ULONG                       BlockSize;

	// Sector size in bits
	ULONG                       SectorBits;

	ULONG                       dwData[FFS_BLOCK_TYPES];
	ULONG                       dwMeta[FFS_BLOCK_TYPES];

	// Flags for the volume
	ULONG                       Flags;

	// Streaming File Object
	PFILE_OBJECT                StreamObj;

	// Resource Lock for Mcb
	ERESOURCE                   McbResource;

	// Dirty Mcbs of modifications for volume stream
	LARGE_MCB                   DirtyMcbs;

	// Entry of Mcb Tree (Root Node)
	PFFS_MCB                    McbTree;
	LIST_ENTRY                  McbList;

} FFS_VCB, *PFFS_VCB;

/*
 * Flags for FFS_VCB
 */
#define VCB_INITIALIZED         0x00000001
#define VCB_VOLUME_LOCKED       0x00000002
#define VCB_MOUNTED             0x00000004
#define VCB_DISMOUNT_PENDING    0x00000008
#define VCB_READ_ONLY           0x00000010

#define VCB_WRITE_PROTECTED     0x10000000
#define VCB_FLOPPY_DISK         0x20000000
#define VCB_REMOVAL_PREVENTED   0x40000000
#define VCB_REMOVABLE_MEDIA     0x80000000


#define IsMounted(Vcb)    (IsFlagOn(Vcb->Flags, VCB_MOUNTED))


/*
 * FFS_FCB File Control Block
 *
 * Data that represents an open file
 * There is a single instance of the FCB for every open file
 */
typedef struct _FFS_FCB {

	// FCB header required by NT
	FSRTL_COMMON_FCB_HEADER         Header;
	SECTION_OBJECT_POINTERS         SectionObject;
	ERESOURCE                       MainResource;
	ERESOURCE                       PagingIoResource;
	// end FCB header required by NT

	// Identifier for this structure
	FFS_IDENTIFIER                  Identifier;

	// List of FCBs for this volume
	LIST_ENTRY                      Next;

	// Share Access for the file object
	SHARE_ACCESS                    ShareAccess;

	// List of byte-range locks for this file
	FILE_LOCK                       FileLockAnchor;

	// Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
	ULONG                           OpenHandleCount;

	// Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
	ULONG                           ReferenceCount;

	// Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
	// But only for Files with FO_NO_INTERMEDIATE_BUFFERING flag
	ULONG                           NonCachedOpenCount;

	// Flags for the FCB
	ULONG                           Flags;

	// Pointer to the inode
	PFFS_INODE                      dinode;

	// Hint block for next allocation
	ULONG                           BlkHint;

	// Vcb

	PFFS_VCB                        Vcb;

	// Mcb Node ...
	PFFS_MCB                        FFSMcb;

	// Full Path Name
	UNICODE_STRING                  LongName;

#if DBG
	// The Ansi Filename for debugging
	OEM_STRING                      AnsiFileName;   
#endif


} FFS_FCB, *PFFS_FCB;


//
// Flags for FFS_FCB
//
#define FCB_FROM_POOL               0x00000001
#define FCB_PAGE_FILE               0x00000002
#define FCB_DELETE_ON_CLOSE         0x00000004
#define FCB_DELETE_PENDING          0x00000008
#define FCB_FILE_DELETED            0x00000010
#define FCB_FILE_MODIFIED           0x00000020

// Mcb Node

struct _FFS_MCB {

	// Identifier for this structure
	FFS_IDENTIFIER                  Identifier;

	// Flags
	ULONG                           Flags;

	// Link List Info

	PFFS_MCB                        Parent; // Parent
	PFFS_MCB                        Child;  // Children
	PFFS_MCB                        Next;   // Brothers

	// Mcb Node Info

	// -> Fcb
	PFFS_FCB                        FFSFcb;

	// Short name
	UNICODE_STRING                  ShortName;

	// Inode number
	ULONG                           Inode;

	// Dir entry offset in parent
	ULONG                           DeOffset;

	// File attribute
	ULONG                           FileAttr;

	// List Link to Vcb->McbList
	LIST_ENTRY                      Link;
};

/*
 * Flags for MCB
 */
#define MCB_FROM_POOL               0x00000001
#define MCB_IN_TREE                 0x00000002
#define MCB_IN_USE                  0x00000004

#define IsMcbUsed(Mcb) IsFlagOn(Mcb->Flags, MCB_IN_USE)


/*
 * FFS_CCB Context Control Block
 *
 * Data that represents one instance of an open file
 * There is one instance of the CCB for every instance of an open file
 */
typedef struct _FFS_CCB {

	// Identifier for this structure
	FFS_IDENTIFIER   Identifier;

	// Flags
	ULONG             Flags;

	// State that may need to be maintained
	ULONG             CurrentByteOffset;
	UNICODE_STRING    DirectorySearchPattern;

} FFS_CCB, *PFFS_CCB;

/*
 * Flags for CCB
 */

#define CCB_FROM_POOL               0x00000001

#define CCB_ALLOW_EXTENDED_DASD_IO  0x80000000


/*
 * FFS_IRP_CONTEXT
 *
 * Used to pass information about a request between the drivers functions
 */
typedef struct _FFS_IRP_CONTEXT {

	// Identifier for this structure
	FFS_IDENTIFIER      Identifier;

	// Pointer to the IRP this request describes
	PIRP                Irp;

	// Flags
	ULONG               Flags;

	// The major and minor function code for the request
	UCHAR               MajorFunction;
	UCHAR               MinorFunction;

	// The device object
	PDEVICE_OBJECT      DeviceObject;

	// The real device object
	PDEVICE_OBJECT      RealDevice;

	// The file object
	PFILE_OBJECT        FileObject;

	PFFS_FCB            Fcb;
	PFFS_CCB            Ccb;

	// If the request is synchronous (we are allowed to block)
	BOOLEAN             IsSynchronous;

	// If the request is top level
	BOOLEAN             IsTopLevel;

	// Used if the request needs to be queued for later processing
	WORK_QUEUE_ITEM     WorkQueueItem;

	// If an exception is currently in progress
	BOOLEAN             ExceptionInProgress;

	// The exception code when an exception is in progress
	NTSTATUS            ExceptionCode;

	// Repinned BCBs List
	FFS_REPINNED_BCBS  Repinned;

} FFS_IRP_CONTEXT, *PFFS_IRP_CONTEXT;


#define IRP_CONTEXT_FLAG_FROM_POOL       (0x00000001)
#define IRP_CONTEXT_FLAG_WAIT            (0x00000002)
#define IRP_CONTEXT_FLAG_WRITE_THROUGH   (0x00000004)
#define IRP_CONTEXT_FLAG_FLOPPY          (0x00000008)
#define IRP_CONTEXT_FLAG_RECURSIVE_CALL  (0x00000010)
#define IRP_CONTEXT_FLAG_DISABLE_POPUPS  (0x00000020)
#define IRP_CONTEXT_FLAG_DEFERRED        (0x00000040)
#define IRP_CONTEXT_FLAG_VERIFY_READ     (0x00000080)
#define IRP_CONTEXT_STACK_IO_CONTEXT     (0x00000100)
#define IRP_CONTEXT_FLAG_REQUEUED        (0x00000200)
#define IRP_CONTEXT_FLAG_USER_IO         (0x00000400)
#define IRP_CONTEXT_FLAG_DELAY_CLOSE     (0x00000800)


/*
 * FFS_ALLOC_HEADER
 *
 * In the checked version of the driver this header is put in the beginning of
 * every memory allocation
 */
typedef struct _FFS_ALLOC_HEADER {
    FFS_IDENTIFIER Identifier;
} FFS_ALLOC_HEADER, *PFFS_ALLOC_HEADER;

typedef struct _FCB_LIST_ENTRY {
    PFFS_FCB     Fcb;
    LIST_ENTRY   Next;
} FCB_LIST_ENTRY, *PFCB_LIST_ENTRY;


/* Block Description List */
typedef struct _FFS_BDL {
    LONGLONG    Lba;
    ULONG       Offset;
    ULONG       Length;
    PIRP        Irp;
} FFS_BDL, *PFFS_BDL;

#pragma pack()


/*
 *  The following macro is used to determine if an FSD thread can block
 *  for I/O or wait for a resource.  It returns TRUE if the thread can
 *  block and FALSE otherwise.  This attribute can then be used to call
 *  the FSD & FSP common work routine with the proper wait value.
 */

#define CanFFSWait(IRP) IoIsOperationSynchronous(Irp)


//
// Block.c
//

NTSTATUS
FFSLockUserBuffer(
	IN PIRP             Irp,
	IN ULONG            Length,
	IN LOCK_OPERATION   Operation);
PVOID
FFSGetUserBuffer(
	IN PIRP Irp);

NTSTATUS
FFSReadWriteBlocks(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_BDL         FFSBDL,
	IN ULONG            Length,
	IN ULONG            Count,
	IN BOOLEAN          bVerify);

NTSTATUS
FFSReadSync(
	IN PFFS_VCB         Vcb,
	IN ULONGLONG        Offset,
	IN ULONG            Length,
	OUT PVOID           Buffer,
	BOOLEAN             bVerify);

NTSTATUS
FFSReadDisk(
	IN PFFS_VCB        Vcb,
	IN ULONGLONG       Offset,
	IN ULONG           Size,
	IN PVOID           Buffer,
	IN BOOLEAN         bVerify);

NTSTATUS 
FFSDiskIoControl(
	IN PDEVICE_OBJECT   DeviceOjbect,
	IN ULONG            IoctlCode,
	IN PVOID            InputBuffer,
	IN ULONG            InputBufferSize,
	IN OUT PVOID        OutputBuffer,
	IN OUT PULONG       OutputBufferSize);

VOID
FFSMediaEjectControl(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB Vcb,
	IN BOOLEAN bPrevent);

NTSTATUS
FFSDiskShutDown(
	PFFS_VCB Vcb);


//
// Cleanup.c
//

NTSTATUS
FFSCleanup(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Close.c
//

NTSTATUS
FFSClose(
	IN PFFS_IRP_CONTEXT IrpContext);

VOID
FFSQueueCloseRequest(
	IN PFFS_IRP_CONTEXT IrpContext);

VOID
FFSDeQueueCloseRequest(
	IN PVOID Context);


//
// Cmcb.c
//

BOOLEAN
FFSAcquireForLazyWrite(
	IN PVOID    Context,
	IN BOOLEAN  Wait);

VOID
FFSReleaseFromLazyWrite(
	IN PVOID Context);

BOOLEAN
FFSAcquireForReadAhead(
	IN PVOID    Context,
	IN BOOLEAN  Wait);

BOOLEAN
FFSNoOpAcquire(
	IN PVOID   Fcb,
	IN BOOLEAN Wait);

VOID
FFSNoOpRelease(
	IN PVOID Fcb);

VOID
FFSReleaseFromReadAhead(
	IN PVOID Context);


//
// Create.c
//

PFFS_FCB
FFSSearchFcbList(
	IN PFFS_VCB     Vcb,
	IN ULONG        inode);

NTSTATUS
FFSScanDir(
	IN PFFS_VCB        Vcb,
	IN PFFS_MCB        ParentMcb,
	IN PUNICODE_STRING FileName,
	IN OUT PULONG      Index,
	IN PFFS_INODE      dinode,
	IN PFFS_DIR_ENTRY  ffs_dir);

NTSTATUS
FFSLookupFileName(
	IN PFFS_VCB            Vcb,
	IN PUNICODE_STRING     FullFileName,
	IN PFFS_MCB            ParentMcb,
	OUT PFFS_MCB*          FFSMcb,
	IN OUT PFFS_INODE      dinode);

NTSTATUS
FFSCreateFile(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb);

NTSTATUS
FFSCreateVolume(
	IN PFFS_IRP_CONTEXT IrpContext, 
	IN PFFS_VCB         Vcb);

NTSTATUS
FFSCreate(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSCreateInode(
	PFFS_IRP_CONTEXT    IrpContext,
	PFFS_VCB            Vcb,
	PFFS_FCB            ParentFcb,
	ULONG               Type,
	ULONG               FileAttr,
	PUNICODE_STRING     FileName);

NTSTATUS
FFSSupersedeOrOverWriteFile(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	ULONG            Disposition);


//
// Debug.c
//

#define DBG_VITAL 0
#define DBG_ERROR 1
#define DBG_USER  2
#define DBG_TRACE 3
#define DBG_INFO  4
#define DBG_FUNC  5

#if DBG
#define FFSPrint(arg)          FFSPrintf   arg
#define FFSPrintNoIndent(arg)  FFSNIPrintf arg

#define FFSCompleteRequest(Irp, bPrint, PriorityBoost) \
        FFSDbgPrintComplete(Irp, bPrint); \
        IoCompleteRequest(Irp, PriorityBoost)

#else

#define FFSPrint(arg)

#define FFSCompleteRequest(Irp, bPrint, PriorityBoost) \
        IoCompleteRequest(Irp, PriorityBoost)

#endif // DBG

VOID
FFSPrintf(
	LONG  DebugPrintLevel,
	PCHAR DebugMessage,
	...);

VOID
FFSNIPrintf(
	LONG  DebugPrintLevel,
	PCHAR DebugMessage,
	...);

extern ULONG ProcessNameOffset;

#define FFSGetCurrentProcessName() ( \
    (PUCHAR) PsGetCurrentProcess() + ProcessNameOffset \
)

ULONG 
FFSGetProcessNameOffset(
	VOID);

VOID
FFSDbgPrintCall(
	IN PDEVICE_OBJECT   DeviceObject,
	IN PIRP             Irp);

VOID
FFSDbgPrintComplete(
	IN PIRP Irp,
	IN BOOLEAN bPrint);

PUCHAR
FFSNtStatusToString(
	IN NTSTATUS Status);


//
// Devctl.c
//

NTSTATUS
FFSDeviceControlNormal(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSPrepareToUnload(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSDeviceControl(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Dirctl.c
//

ULONG
FFSGetInfoLength(
	IN FILE_INFORMATION_CLASS  FileInformationClass);

ULONG
FFSProcessDirEntry(
	IN PFFS_VCB                Vcb,
	IN FILE_INFORMATION_CLASS  FileInformationClass,
	IN ULONG                   in,
	IN PVOID                   Buffer,
	IN ULONG                   UsedLength,
	IN ULONG                   Length,
	IN ULONG                   FileIndex,
	IN PUNICODE_STRING         pName,
	IN BOOLEAN                 Single);

NTSTATUS
FFSQueryDirectory(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSNotifyChangeDirectory(
    IN PFFS_IRP_CONTEXT IrpContext);

VOID
FFSNotifyReportChange(
	IN PFFS_IRP_CONTEXT  IrpContext,
	IN PFFS_VCB          Vcb,
	IN PFFS_FCB          Fcb,
	IN ULONG             Filter,
	IN ULONG             Action);

NTSTATUS
FFSDirectoryControl(
	IN PFFS_IRP_CONTEXT IrpContext);

BOOLEAN
FFSIsDirectoryEmpty(
	PFFS_VCB Vcb,
	PFFS_FCB Dcb);


//
// Dispatch.c
//

NTSTATUS
FFSQueueRequest(
	IN PFFS_IRP_CONTEXT IrpContext);

VOID
FFSDeQueueRequest(
	IN PVOID Context);

NTSTATUS
FFSDispatchRequest(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSBuildRequest(
	PDEVICE_OBJECT   DeviceObject,
	PIRP             Irp);


//
// Except.c
//

NTSTATUS
FFSExceptionFilter(
	IN PFFS_IRP_CONTEXT    IrpContext,
	IN PEXCEPTION_POINTERS ExceptionPointer);

NTSTATUS
FFSExceptionHandler(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// ffs.c
//

PFFS_SUPER_BLOCK
FFSLoadSuper(
	IN PFFS_VCB       Vcb,
	IN BOOLEAN        bVerify);

BOOLEAN
FFSSaveSuper(
	IN PFFS_IRP_CONTEXT    IrpContext,
	IN PFFS_VCB            Vcb);

BOOLEAN
FFSLoadGroup(
	IN PFFS_VCB Vcb);

BOOLEAN
FFSSaveGroup(
	IN PFFS_IRP_CONTEXT    IrpContext,
	IN PFFS_VCB            Vcb);

BOOLEAN
FFSGetInodeLba(
	IN PFFS_VCB   Vcb,
	IN  ULONG     inode,
	OUT PLONGLONG offset);

BOOLEAN
FFSLoadInode(
	IN PFFS_VCB   Vcb,
	IN ULONG      inode,
	IN PFFS_INODE dinode);

BOOLEAN
FFSSaveInode(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN ULONG            Inode,
	IN PFFS_INODE       dinode);

BOOLEAN
FFSLoadBlock(
	IN PFFS_VCB  Vcb,
	IN ULONG     dwBlk,
	IN PVOID     Buffer);

BOOLEAN
FFSSaveBlock(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN ULONG            dwBlk,
	IN PVOID            Buf);

BOOLEAN
FFSSaveBuffer(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN LONGLONG         Offset,
	IN ULONG            Size,
	IN PVOID            Buf);

ULONG
FFSGetBlock(
	IN PFFS_VCB Vcb,
	ULONG       dwContent,
	ULONG       Index,
	int         layer);

ULONG
FFSBlockMap(
	IN PFFS_VCB   Vcb,
	IN PFFS_INODE dinode,
	IN ULONG      Index);

ULONG
FFSBuildBDL(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_INODE       dinode,
	IN ULONG            Offset, 
	IN ULONG            Size, 
	OUT PFFS_BDL        *ffs_bdl);

BOOLEAN
FFSNewBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            GroupHint,
	ULONG            BlockHint,  
	PULONG           dwRet);

BOOLEAN
FFSFreeBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            Block);

BOOLEAN
FFSExpandBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	ULONG            dwContent,
	ULONG            Index,
	ULONG            layer,
	BOOLEAN          bNew,
	ULONG            *dwRet);

BOOLEAN
FFSExpandInode(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	ULONG            *dwRet);

NTSTATUS
FFSNewInode(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            GroupHint,
	ULONG            Type,
	PULONG           Inode);

BOOLEAN
FFSFreeInode(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            Inode,
	ULONG            Type);

NTSTATUS
FFSAddEntry(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Dcb,
	IN ULONG            FileType,
	IN ULONG            Inode,
	IN PUNICODE_STRING  FileName);

NTSTATUS
FFSRemoveEntry(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Dcb,
	IN ULONG            FileType,
	IN ULONG            Inode);

NTSTATUS
FFSSetParentEntry(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Dcb,
	IN ULONG            OldParent,
	IN ULONG            NewParent);

BOOLEAN
FFSTruncateBlock(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Fcb,
	IN ULONG            dwContent,
	IN ULONG            Index,
	IN ULONG            layer,
	OUT BOOLEAN         *bFreed);

BOOLEAN
FFSTruncateInode(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFFS_FCB         Fcb);

BOOLEAN
FFSAddMcbEntry(
	IN PFFS_VCB Vcb,
	IN LONGLONG Lba,
	IN LONGLONG Length);

VOID
FFSRemoveMcbEntry(
	IN PFFS_VCB Vcb,
	IN LONGLONG Lba,
	IN LONGLONG Length);

BOOLEAN
FFSLookupMcbEntry(
	IN PFFS_VCB     Vcb,
	IN LONGLONG     Lba,
	OUT PLONGLONG   pLba,
	OUT PLONGLONG   pLength,
	OUT PLONGLONG   RunStart,
	OUT PLONGLONG   RunLength,
	OUT PULONG      Index);

ULONG
FFSDataBlocks(
	PFFS_VCB Vcb,
	ULONG TotalBlocks);

ULONG
FFSTotalBlocks(
	PFFS_VCB Vcb,
	ULONG DataBlocks);


//
// Fastio.c
//

BOOLEAN
FFSFastIoCheckIfPossible(
	IN PFILE_OBJECT         FileObject,
	IN PLARGE_INTEGER       FileOffset,
	IN ULONG                Length,
	IN BOOLEAN              Wait,
	IN ULONG                LockKey,
	IN BOOLEAN              CheckForReadOperation,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoRead(
	IN PFILE_OBJECT         FileObject,
	IN PLARGE_INTEGER       FileOffset,
	IN ULONG                Length,
	IN BOOLEAN              Wait,
	IN ULONG                LockKey,
	OUT PVOID               Buffer,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoWrite(
	IN PFILE_OBJECT         FileObject,
	IN PLARGE_INTEGER       FileOffset,
	IN ULONG                Length,
	IN BOOLEAN              Wait,
	IN ULONG                LockKey,
	OUT PVOID               Buffer,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoQueryBasicInfo(
	IN PFILE_OBJECT             FileObject,
	IN BOOLEAN                  Wait,
	OUT PFILE_BASIC_INFORMATION Buffer,
	OUT PIO_STATUS_BLOCK        IoStatus,
	IN PDEVICE_OBJECT           DeviceObject);

BOOLEAN
FFSFastIoQueryStandardInfo(
	IN PFILE_OBJECT                 FileObject,
	IN BOOLEAN                      Wait,
	OUT PFILE_STANDARD_INFORMATION  Buffer,
	OUT PIO_STATUS_BLOCK            IoStatus,
	IN PDEVICE_OBJECT               DeviceObject);

BOOLEAN
FFSFastIoLock(
	IN PFILE_OBJECT         FileObject,
	IN PLARGE_INTEGER       FileOffset,
	IN PLARGE_INTEGER       Length,
	IN PEPROCESS            Process,
	IN ULONG                Key,
	IN BOOLEAN              FailImmediately,
	IN BOOLEAN              ExclusiveLock,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoUnlockSingle(
	IN PFILE_OBJECT         FileObject,
	IN PLARGE_INTEGER       FileOffset,
	IN PLARGE_INTEGER       Length,
	IN PEPROCESS            Process,
	IN ULONG                Key,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoUnlockAll(
	IN PFILE_OBJECT         FileObject,
	IN PEPROCESS            Process,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoUnlockAllByKey(
	IN PFILE_OBJECT         FileObject,
	IN PEPROCESS            Process,
	IN ULONG                Key,
	OUT PIO_STATUS_BLOCK    IoStatus,
	IN PDEVICE_OBJECT       DeviceObject);

BOOLEAN
FFSFastIoQueryNetworkOpenInfo(
	IN PFILE_OBJECT                     FileObject,
	IN BOOLEAN                          Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION  Buffer,
	OUT PIO_STATUS_BLOCK                IoStatus,
	IN PDEVICE_OBJECT                   DeviceObject);

BOOLEAN
FFSFastIoQueryNetworkOpenInfo(
	IN PFILE_OBJECT                     FileObject,
	IN BOOLEAN                          Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION  Buffer,
	OUT PIO_STATUS_BLOCK                IoStatus,
	IN PDEVICE_OBJECT                   DeviceObject);


//
// FileInfo.c
//

NTSTATUS
FFSQueryInformation(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSSetInformation(
	IN PFFS_IRP_CONTEXT IrpContext);

BOOLEAN
FFSExpandFile(
	PFFS_IRP_CONTEXT IrpContext, 
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	PLARGE_INTEGER   AllocationSize);

BOOLEAN
FFSTruncateFile(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	PLARGE_INTEGER   AllocationSize);

NTSTATUS
FFSSetDispositionInfo(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb,
	BOOLEAN          bDelete);

NTSTATUS
FFSSetRenameInfo(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb);

BOOLEAN
FFSDeleteFile(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	PFFS_FCB         Fcb);


//
// Flush.c
//

NTSTATUS
FFSFlushFiles(
	IN PFFS_VCB Vcb,
	BOOLEAN     bShutDown);

NTSTATUS
FFSFlushVolume(
	IN PFFS_VCB Vcb,
	BOOLEAN     bShutDown);

NTSTATUS
FFSFlushFile(
	IN PFFS_FCB Fcb);

NTSTATUS
FFSFlush(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Fsctl.c
//

VOID
FFSSetVpbFlag(
	IN PVPB     Vpb,
	IN USHORT   Flag);

VOID
FFSClearVpbFlag(
	IN PVPB     Vpb,
	IN USHORT   Flag);

BOOLEAN
FFSCheckDismount(
	IN PFFS_IRP_CONTEXT  IrpContext,
	IN PFFS_VCB          Vcb,
	IN BOOLEAN           bForce);

NTSTATUS
FFSPurgeVolume(
	IN PFFS_VCB Vcb,
	IN BOOLEAN  FlushBeforePurge);

NTSTATUS
FFSPurgeFile(
	IN PFFS_FCB Fcb,
	IN BOOLEAN  FlushBeforePurge);

BOOLEAN
FFSIsHandleCountZero(
	IN PFFS_VCB Vcb);

NTSTATUS
FFSLockVcb(
	IN PFFS_VCB     Vcb,
	IN PFILE_OBJECT FileObject);

NTSTATUS
FFSLockVolume(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSUnlockVcb(
	IN PFFS_VCB     Vcb,
	IN PFILE_OBJECT FileObject);

NTSTATUS
FFSUnlockVolume(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSAllowExtendedDasdIo(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSUserFsRequest(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSMountVolume(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSVerifyVolume(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSIsVolumeMounted(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSDismountVolume(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSFileSystemControl(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Init.c
//

BOOLEAN
FFSQueryParameters(
	IN PUNICODE_STRING  RegistryPath); 

VOID
DriverUnload(
	IN PDRIVER_OBJECT DriverObject);


//
// Lock.c
//

NTSTATUS
FFSLockControl(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Memory.c
//

PFFS_IRP_CONTEXT
FFSAllocateIrpContext(
	IN PDEVICE_OBJECT   DeviceObject,
	IN PIRP             Irp);

VOID
FFSFreeIrpContext(
	IN PFFS_IRP_CONTEXT IrpContext);

PFFS_FCB
FFSAllocateFcb(
	IN PFFS_VCB           Vcb,
	IN PFFS_MCB           FFSMcb,
	IN PFFS_INODE         dinode);

VOID
FFSFreeFcb(
	IN PFFS_FCB Fcb);

PFFS_CCB
FFSAllocateCcb(
	VOID);

VOID
FFSFreeMcb(
	IN PFFS_MCB Mcb);

PFFS_FCB
FFSCreateFcbFromMcb(
	PFFS_VCB Vcb,
	PFFS_MCB Mcb);

VOID
FFSFreeCcb(
	IN PFFS_CCB Ccb);

PFFS_MCB
FFSAllocateMcb(
	PFFS_VCB        Vcb,
	PUNICODE_STRING FileName,
	ULONG           FileAttr);

PFFS_MCB
FFSSearchMcbTree(
	PFFS_VCB Vcb,
	PFFS_MCB FFSMcb,
	ULONG    Inode);

PFFS_MCB
FFSSearchMcb(
	PFFS_VCB        Vcb,
	PFFS_MCB        Parent,
	PUNICODE_STRING FileName);

BOOLEAN
FFSGetFullFileName(
	PFFS_MCB        Mcb,
	PUNICODE_STRING FileName);

VOID
FFSRefreshMcb(
	PFFS_VCB Vcb, PFFS_MCB Mcb);

VOID
FFSAddMcbNode(
	PFFS_VCB Vcb,
	PFFS_MCB Parent,
	PFFS_MCB Child);

BOOLEAN
FFSDeleteMcbNode(
	PFFS_VCB Vcb,
	PFFS_MCB McbTree,
	PFFS_MCB FFSMcb);

VOID
FFSFreeMcbTree(
	PFFS_MCB McbTree);

BOOLEAN
FFSCheckSetBlock(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb,
	ULONG            Block);

BOOLEAN
FFSCheckBitmapConsistency(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb);

VOID
FFSInsertVcb(
	PFFS_VCB Vcb);

VOID
FFSRemoveVcb(
	PFFS_VCB Vcb);

NTSTATUS
FFSInitializeVcb(
	IN PFFS_IRP_CONTEXT IrpContext, 
	IN PFFS_VCB         Vcb, 
	IN PFFS_SUPER_BLOCK FFSSb,
	IN PDEVICE_OBJECT   TargetDevice,
	IN PDEVICE_OBJECT   VolumeDevice,
	IN PVPB             Vpb);

VOID
FFSFreeVcb(
	IN PFFS_VCB Vcb);

VOID
FFSRepinBcb(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PBCB             Bcb);

VOID
FFSUnpinRepinnedBcbs(
	IN PFFS_IRP_CONTEXT IrpContext);


NTSTATUS
FFSCompleteIrpContext(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN NTSTATUS         Status);

VOID
FFSSyncUninitializeCacheMap(
	IN PFILE_OBJECT FileObject);


//
// Misc.c
//

ULONG
FFSLog2(
	ULONG Value);

LARGE_INTEGER
FFSSysTime(
	IN ULONG i_time);

ULONG
FFSInodeTime(
	IN LARGE_INTEGER SysTime);

NTSTATUS
FFSOEMToUnicode(
	IN OUT PUNICODE_STRING Unicode,
	IN     POEM_STRING     Oem);

NTSTATUS
FFSUnicodeToOEM(
	IN OUT POEM_STRING Oem,
	IN PUNICODE_STRING Unicode);


//
// Pnp.c
//

NTSTATUS
FFSPnp(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSPnpQueryRemove(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb);

NTSTATUS
FFSPnpRemove(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb);

NTSTATUS
FFSPnpCancelRemove(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb);

NTSTATUS
FFSPnpSurpriseRemove(
	PFFS_IRP_CONTEXT IrpContext,
	PFFS_VCB         Vcb);


//
// Read.c
//

BOOLEAN 
FFSCopyRead(
	IN PFILE_OBJECT       FileObject,
	IN PLARGE_INTEGER     FileOffset,
	IN ULONG              Length,
	IN BOOLEAN            Wait,
	OUT PVOID             Buffer,
	OUT PIO_STATUS_BLOCK  IoStatus);

NTSTATUS
FFSReadInode(
	IN PFFS_IRP_CONTEXT     IrpContext,
	IN PFFS_VCB             Vcb,
	IN PFFS_INODE           dinode,
	IN ULONG                offset,
	IN PVOID                Buffer,
	IN ULONG                size,
	OUT PULONG              dwRet);

NTSTATUS
FFSRead(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Shutdown.c
//

NTSTATUS
FFSShutDown(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Volinfo.c
//

NTSTATUS
FFSQueryVolumeInformation(
	IN PFFS_IRP_CONTEXT IrpContext);

NTSTATUS
FFSSetVolumeInformation(
	IN PFFS_IRP_CONTEXT IrpContext);


//
// Write.c
//

NTSTATUS
FFSWriteInode(
	IN PFFS_IRP_CONTEXT     IrpContext,
	IN PFFS_VCB             Vcb,
	IN PFFS_INODE           dinode,
	IN ULONG                offset,
	IN PVOID                Buffer,
	IN ULONG                size,
	IN BOOLEAN              bWriteToDisk,
	OUT PULONG              dwRet);

VOID
FFSStartFloppyFlushDpc(
	PFFS_VCB     Vcb,
	PFFS_FCB     Fcb,
	PFILE_OBJECT FileObject);

BOOLEAN
FFSZeroHoles(
	IN PFFS_IRP_CONTEXT IrpContext,
	IN PFFS_VCB         Vcb,
	IN PFILE_OBJECT     FileObject,
	IN LONGLONG         Offset,
	IN LONGLONG         Count);

NTSTATUS
FFSWrite(
	IN PFFS_IRP_CONTEXT IrpContext);

#endif /* _FFS_HEADER_ */
