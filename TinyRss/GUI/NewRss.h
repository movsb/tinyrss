#pragma once

class INewRssDlgCallback
{
public:
	DuiLib::CDuiString strTitle;
	DuiLib::CDuiString strLink;
	DuiLib::CDuiString strCategory;

public:
	virtual bool CheckValid(LPCTSTR* msg) = 0;
};

class CNewRssDlgImpl;

class CNewRssDlg
{
private:
	CNewRssDlgImpl* pDlg;
public:
	enum _RET_CODE{kClose,kOK,kCancel};
	CNewRssDlg(HWND hParent, INewRssDlgCallback* nrcb, CSource* pSource=nullptr);
	~CNewRssDlg();
	_RET_CODE GetDlgCode();
public:

};

