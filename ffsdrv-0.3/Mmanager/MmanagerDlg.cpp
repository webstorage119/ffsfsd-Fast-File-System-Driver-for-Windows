// MmanagerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Mmanager.h"
#include "MmanagerDlg.h"

#include <winioctl.h>
#include <winsvc.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define DEVICE_NAME "ffs"


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CMmanagerDlg dialog



CMmanagerDlg::CMmanagerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMmanagerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMmanagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DISK_COMBO, m_DiskCombo);
	DDX_Control(pDX, IDC_PARTITION_COMBO, m_PartitionCombo);
	DDX_Control(pDX, IDC_DRIVE_COMBO, m_DriveCombo);
	DDX_Control(pDX, IDC_LOADDRIVER, m_btnLoadDriver);
}

BEGIN_MESSAGE_MAP(CMmanagerDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_LOADDRIVER, OnBnClickedLoaddriver)
	ON_BN_CLICKED(IDC_MOUNT, OnBnClickedMount)
	ON_BN_CLICKED(IDC_UNMOUNT, OnBnClickedUnmount)
END_MESSAGE_MAP()


// CMmanagerDlg message handlers

BOOL CMmanagerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	/* Init */
	char *Diskitem[] = { "0", "1", "2", "3", "4", "5", "6", "7" };
	for (int i = 0; i < sizeof(Diskitem) / sizeof(*Diskitem); i++)
	{
		m_DiskCombo.AddString(Diskitem[i]);
	}
	m_DiskCombo.SetCurSel(0);

    char *Partitem[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
	for (int i = 0; i < sizeof(Partitem) / sizeof(*Partitem); i++)
	{
		m_PartitionCombo.AddString(Partitem[i]);
	}
	m_PartitionCombo.SetCurSel(0);
	
	char *Driveitem[] = {"X:\\", "Y:\\", "Z:\\"};
	for (int i = 0; i < sizeof(Driveitem) / sizeof(*Driveitem); i++)
	{
		m_DriveCombo.AddString(Driveitem[i]);
	}
	m_DriveCombo.SetCurSel(0);

	/* Detect Loaded Driver */
	hDriver = CreateFile("\\\\.\\" DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, 0, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDriver == INVALID_HANDLE_VALUE)
	{
		hDriver = NULL;
	}
	else
	{
		m_btnLoadDriver.EnableWindow(FALSE);
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMmanagerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMmanagerDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMmanagerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMmanagerDlg::OnBnClickedLoaddriver()
{
	/* Load driver */
	if (!LoadDriver(DEVICE_NAME))
	{
		MessageBox("Driver install failed.", "Mount Manager");
		return;
	}


	/* open */
	hDriver = CreateFile("\\\\.\\" DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, 0, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDriver == INVALID_HANDLE_VALUE)
	{
		MessageBox("Driver open error!", "Mount Manager");
		hDriver = NULL;
		return;
	}


	MessageBox("Driver Load Complete.", "Mount Manager");
	m_btnLoadDriver.EnableWindow(FALSE);
}

void CMmanagerDlg::OnBnClickedMount()
{
	BOOL bMount = FALSE;
	BOOL bUmount = FALSE;
	int nDisk = 0;
	int nPart = 0;
	char DevName[256];
	char temp[4];
	char volume_name[] = " :";

	m_DiskCombo.GetLBText(m_DiskCombo.GetCurSel(), temp);
	nDisk = atoi(&temp[0]);

	m_PartitionCombo.GetLBText(m_PartitionCombo.GetCurSel(), temp);
	nPart = atoi(&temp[0]);

	m_DriveCombo.GetLBText(m_DriveCombo.GetCurSel(), temp);
	volume_name[0] = temp[0];

	sprintf(DevName,"\\Device\\Harddisk%d\\Partition%d", nDisk, nPart);

	if (Mount(DevName, volume_name) == TRUE)
		MessageBox("Mount Complete.");
	else
		MessageBox("Mount Failed.");
}

void CMmanagerDlg::OnBnClickedUnmount()
{
	char temp[4];
	char volume_name[] = " :";

	m_DriveCombo.GetLBText(m_DriveCombo.GetCurSel(), temp);
	volume_name[0] = temp[0];

	if(Unmount(volume_name) == TRUE)
		MessageBox("Unmount Complete."); 
}


/* Driver Functions */

BOOL CMmanagerDlg::LoadDriver(char *pDeviceName)
{
	SC_HANDLE hManager, hService;
	char strPath[MAX_PATH], temp[MAX_PATH];

	GetCurrentDirectory(MAX_PATH, strPath);
	sprintf(temp, "\\%s.sys", pDeviceName);
	strcpy(strPath + strlen(strPath), temp);


	hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (!hManager)
		return FALSE;


	hService = CreateService(hManager, pDeviceName, pDeviceName, SERVICE_ALL_ACCESS, 
		SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, strPath, 
		NULL, NULL, NULL, NULL, NULL);

	if (!hService)
	{
		hService = OpenService(hManager, pDeviceName, SERVICE_ALL_ACCESS);
		if (!hService)
		{
			CloseServiceHandle(hManager);
			return FALSE;
		}
	}

	StartService(hService, 0, NULL); 
	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);


	return TRUE;
}

/* Volume Mount & Unmount Functions */
BOOL CMmanagerDlg::Mount(char *pDeviceName, char *drive)
{

	if (!DefineDosDevice(
		DDD_RAW_TARGET_PATH,
		drive,
		pDeviceName))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CMmanagerDlg::Unmount(char *drive)
{
	char DosDevName[256];
	HANDLE hdevice;
	ULONG dwBytes;
	char msg[256];

	wsprintf(DosDevName, "\\\\.\\%s", drive);

	wsprintf(msg, "Umount: DosDevName: %s\n", DosDevName);
	MessageBox(msg);

	hdevice = CreateFile(
		DosDevName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hdevice == INVALID_HANDLE_VALUE)
	{
		wsprintf(msg, "Unmount: open %s error.\n", DosDevName);
		MessageBox(msg);
		return FALSE;
	}

	if (!DeviceIoControl(
		hdevice,
		FSCTL_LOCK_VOLUME,
		NULL,
		0,
		NULL,
		0,
		&dwBytes,
		NULL))
	{
		wsprintf(msg, "Unmount: ioctl: LockVolume %s error.\n", DosDevName);
		MessageBox(msg);

		CloseHandle(hdevice);
		return FALSE;
	}

	if (!DeviceIoControl(
		hdevice,
		FSCTL_DISMOUNT_VOLUME,
		NULL,
		0,
		NULL,
		0,
		&dwBytes,
		NULL))
	{
		wsprintf(msg, "Unmount: ioctl: DisMount %s error.\n", DosDevName);
		MessageBox(msg);
		CloseHandle(hdevice);
		return TRUE;
	}

	CloseHandle(hdevice);

	if (!DefineDosDevice(
		DDD_REMOVE_DEFINITION,
		drive,
		NULL))
	{
		wsprintf(msg, "Unmount: Remove %s error.\n", drive);
		CMmanagerDlg::MessageBox(msg);
		return FALSE;
	}

	return TRUE;
}

