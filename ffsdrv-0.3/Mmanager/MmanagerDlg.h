// MmanagerDlg.h : header file
//

#pragma once
#include "afxwin.h"


// CMmanagerDlg dialog
class CMmanagerDlg : public CDialog
{
// Construction
public:
	CMmanagerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_MMANAGER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

private:
	HANDLE hDriver;


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	BOOL LoadDriver(char *pDeviceName);
	BOOL Mount(char *pDeviceName, char *drive);
	BOOL Unmount(char *drive);
	CComboBox m_DiskCombo;
	CComboBox m_PartitionCombo;
	CComboBox m_DriveCombo;
	CButton m_btnLoadDriver;
	afx_msg void OnBnClickedLoaddriver();
	afx_msg void OnBnClickedMount();
	afx_msg void OnBnClickedUnmount();
};
