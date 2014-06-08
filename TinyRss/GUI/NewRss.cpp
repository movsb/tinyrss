#include "GuiPch.h"

using namespace DuiLib;

#include "SQLite.h"
#include "NewRss.h"


class CNewRssDlgImpl : public WindowImplBase
{
private:
	INewRssDlgCallback* m_nrcb;
	CSource*  m_pSource;
	CNewRssDlg::_RET_CODE m_dlgcode;

	CRichEditUI* m_riTitle;
	CRichEditUI* m_riLink;
	CRichEditUI* m_riCategory;
	CButtonUI*   m_btnOK;
	CButtonUI*   m_btnCancel;
	CButtonUI*   m_btnClose;
public:
	CNewRssDlgImpl(INewRssDlgCallback* nrcb, CSource* pSource)
		: m_nrcb(nrcb)
		, m_pSource(pSource)
	{

	}
	CNewRssDlg::_RET_CODE GetDlgCode()
	{
		return m_dlgcode;
	}
protected:
	virtual CDuiString GetSkinFolder() override
	{
		return _T("skin/");
	}
	virtual CDuiString GetSkinFile() override
	{
		return _T("NewRss.xml");
	}
	virtual LPCTSTR GetWindowClassName(void) const override
	{
		return _T("Å®º¢²»¿Þ");
	}
	virtual LRESULT ResponseDefaultKeyEvent(WPARAM wParam) override
	{
		return FALSE;
	}

	virtual void InitWindow() override
	{
		m_riTitle = FindControl(_T("ri_title"))->ToRichEditUI();
		m_riLink = FindControl(_T("ri_link"))->ToRichEditUI();
		m_riCategory = FindControl(_T("ri_category"))->ToRichEditUI();
		SMART_ASSERT(m_riTitle && m_riLink && m_riCategory).Fatal();

		m_btnOK = FindControl(_T("btnOK"))->ToButtonUI();
		m_btnCancel = FindControl(_T("btnCancel"))->ToButtonUI();
		m_btnClose =FindControl(_T("closebtn"))->ToButtonUI();
		SMART_ASSERT(m_btnOK && m_btnCancel && m_btnClose).Fatal();

		if(m_pSource){
			m_riTitle->SetText(m_pSource->title.c_str());
			m_riLink->SetText(m_pSource->source.c_str());
			m_riCategory->SetText(m_pSource->category.c_str());
		}
	}

	virtual void OnFinalMessage( HWND hWnd ) override
	{
		__super::OnFinalMessage(hWnd);
	}

	virtual void Notify(TNotifyUI& msg) override
	{
		if(msg.sType == DUI_MSGTYPE_CLICK){
			if(msg.pSender == m_btnOK){
				m_nrcb->strTitle    = m_riTitle->GetText();
				m_nrcb->strLink     = m_riLink->GetText();
				m_nrcb->strCategory = m_riCategory->GetText();

				LPCTSTR pro = nullptr;
				if(!m_nrcb->CheckValid(&pro)){
					::MessageBox(GetHWND(), pro, nullptr, MB_ICONEXCLAMATION);
					return;
				}

				m_dlgcode = CNewRssDlg::kOK;
				Close();
				return;
			}
			else if(msg.pSender == m_btnCancel){
				m_dlgcode = CNewRssDlg::kCancel;
				Close();
				return;
			}
			else if(msg.pSender == m_btnClose){
				m_dlgcode = CNewRssDlg::kClose;
				Close();
				return;
			}
		}
		return __super::Notify(msg);
	}
};

CNewRssDlg::CNewRssDlg(HWND hParent, INewRssDlgCallback* nrcb, CSource* pSource)
{
	pDlg = new CNewRssDlgImpl(nrcb, pSource);
	pDlg->CreateDuiWindow(hParent, "", UI_WNDSTYLE_FRAME);
	pDlg->CenterWindow();
	pDlg->ShowModal();
}

CNewRssDlg::_RET_CODE CNewRssDlg::GetDlgCode()
{
	return pDlg->GetDlgCode();
}

CNewRssDlg::~CNewRssDlg()
{
	delete pDlg;
}

