#include "GuiPch.h"

#include "../RssManager.h"
#include "NewRss.h"
#include "MainDlg.h"
#include "resource.h"
#include <process.h>

using namespace DuiLib;

fstream __debug_file;
CRssManager* theRss;

class CRssSourceFilterUI : public CHorizontalLayoutUI
{
public:
	CRssSourceFilterUI(){}
	static LPCTSTR GetUIClass()
	{
		return _T("RssSourceFilter");
	}

protected:
	virtual void DoInit()override
	{
		__super::DoInit();

		m_lblPrompt = GetItemAt(0)->ToLabelUI();
		m_riText = GetItemAt(1)->ToRichEditUI();
		SMART_ASSERT(m_lblPrompt && m_riText).Fatal();
		LayoutControls();
	}

private:
	void LayoutControls()
	{
		SIZE szPrompt = CRenderEngine::GetTextSize(
			GetManager()->GetPaintDC(),GetManager(),
			m_lblPrompt->GetText(),m_lblPrompt->GetFont(),0);
		auto iHeightText = GetManager()->GetFontInfo(m_riText->GetFont())->tm.tmHeight;

		m_lblPrompt->SetFixedWidth(szPrompt.cx+4);

		auto thisheight = max(szPrompt.cy, iHeightText)+8;
		SetFixedHeight(thisheight);

		auto inset = (thisheight-iHeightText)/2;
		m_riText->SetInset(CDuiRect(0,inset,0,inset));
	}

private:
	CLabelUI*		m_lblPrompt;
	CRichEditUI*	m_riText;
};

// 这是作者信息栏: 右上角那一块
class CRssAuthorUI : public CVerticalLayoutUI
{
public:
	CRssAuthorUI(){}
	static LPCTSTR GetUIClass()
	{
		return _T("RssAuthor");
	}

public:
	void SetTitle(LPCTSTR strTitle)
	{
		m_pTitle->SetText(strTitle);
	}
	void SetLink(LPCTSTR strLink)
	{
		m_pLink->SetText(strLink);
	}
	void SetDescription(LPCTSTR strDesc)
	{
		m_pDescription->SetText(strDesc);
	}
	void Empty()
	{
		m_pTitle->SetText(_T(""));
		m_pLink->SetText(_T(""));
		m_pDescription->SetText(_T(""));
	}
protected:
	virtual void DoInit() override
	{
		__super::DoInit();

		m_pTitle = GetItemAt(0)->ToLabelUI();
		m_pLink = GetItemAt(1)->ToLabelUI();
		m_pDescription = GetItemAt(2)->ToLabelUI();
		SMART_ASSERT(m_pTitle && m_pLink && m_pDescription);

		m_pTitle->SetFont(2);

		auto s1 = GetManager()->GetFontInfo(m_pTitle		->GetFont())->tm.tmHeight + 4;
		auto s2 = GetManager()->GetFontInfo(m_pLink			->GetFont())->tm.tmHeight + 4;
		auto s3 = GetManager()->GetFontInfo(m_pDescription	->GetFont())->tm.tmHeight + 4;
		auto inset = GetInset().top + GetInset().bottom;

		SetFixedHeight(s1 + s2 + s3 + inset);

		SetAttribute("mousechild","false");
	}

private:
	CLabelUI* m_pTitle;
	CLabelUI* m_pLink;
	CLabelUI* m_pDescription;
};


// 此对象用来构造刷新
class CRssSourceUI;
class CRssSourceRefresh
{
public:
	enum class ErrorCode{
		kOK,
		kError,
	};

public:
	HANDLE				hEvent;				// 是否正在刷新进程之中
	CRssSourceUI*		pObject;			// 所属CRssSourceUI对象指针
	CRssSource*			pRssSource;			// 保存item的对象指针
	CRssManager*		pManager;			// 用于获取rss内容
	std::string			errstr;				// 错误消息
	ErrorCode			errcode;			// 错误码
};

// 这个是左边的RSS源的列表中的某一项
class CRssSourceUI : public CListContainerElementUI
{
public:
	static const int kRefreshTimerID = 0;
public:
	void SetTitle(LPCTSTR str)
	{
		m_pTitle->SetText(str);
	}

	void SetCategory(LPCTSTR str)
	{
		m_pCategory->SetText(str);
	}

	void SetContents(CSource* pSource)
	{
		SetTitle(pSource->title.c_str());
		SetCategory(pSource->category.c_str());
	}

public:
	CRssSourceUI()
		: m_pTitle(nullptr)
		, m_pCategory(nullptr)
		, m_pSource(nullptr)
	{
		auto pVert = new CVerticalLayoutUI;
		m_pTitle = new CLabelUI;
		m_pCategory = new CLabelUI;
		pVert->Add(m_pTitle);
		pVert->Add(m_pCategory);

		Add(pVert);
	}

	virtual LPCTSTR GetClass() const override
	{
		return _T("RssSource");
	}

	static LPCTSTR GetUIClass()
	{
		return _T("RssSource");
	}

	void SetSource(CSource* src)
	{
		m_pSource = src;
	}

	CSource* GetSource()
	{
		return m_pSource;
	}

	void SetRefresh(CRssSourceRefresh* pRefresh)
	{
		m_pRefresh = pRefresh;
	}

	CRssSourceRefresh* GetRefresh()
	{
		return m_pRefresh;
	}

protected:
	virtual void SetPos(RECT rc) override
	{
		__super::SetPos(rc);
	}

	virtual void DoInit() override
	{
		__super::DoInit();

		SetInset(CDuiRect(5,5,5,5));

		m_pTitle->SetFont(2);

		auto s1 = GetManager()->GetFontInfo(m_pTitle->GetFont())	->tm.tmHeight+4;
		auto s2 = GetManager()->GetFontInfo(m_pCategory->GetFont())	->tm.tmHeight+4;
		auto s3 = GetInset().top + GetInset().bottom;

		SetFixedHeight(s1 + s2 + s3);
		SetAttribute("mousechild","false");
	}

	virtual void DoEvent(TEventUI& event) override
	{
		// 默认右键会被处理成itemselect, 真是无语
		if(IsMouseEnabled() && event.Type == UIEVENT_RBUTTONDOWN)
		{
			return;
		}

		if(event.Type == UIEVENT_TIMER){
			if(event.wParam == kRefreshTimerID){	// for fresh proc
				DWORD dwRet = ::WaitForSingleObject(GetRefresh()->hEvent,0);
				switch(dwRet)
				{
				case WAIT_OBJECT_0:
					SMART_ENSURE(GetManager()->KillTimer(this, event.wParam),==true).Fatal();
					::CloseHandle(GetRefresh()->hEvent);
					GetManager()->SendNotify(this, DUI_MSGTYPE_TIMER, event.wParam);
					return;

				case WAIT_TIMEOUT:
					return;

				default:
					SMART_ASSERT("WaitForSingleObjext" && 0)(dwRet).Fatal();
					return;
				}
				return;
			}
		}
		__super::DoEvent(event);
	}

protected:
	CLabelUI* m_pTitle;
	CLabelUI* m_pCategory;

	CSource* m_pSource;
	CRssSourceRefresh* m_pRefresh;
};

// 这个是左边的RSS源列表, 用来装CRssSourceUI
class CRssSourceListUI : public CListUI
{
public:
	CRssSourceListUI()
	{}
	~CRssSourceListUI()
	{}

	CRssSourceUI* GetCurSourceUI()
	{
		if(GetCurSel()==-1) return nullptr;
		
		return static_cast<CRssSourceUI*>(GetItemAt(GetCurSel()));
	}

	static LPCTSTR GetUIClass()
	{
		return _T("RssSourceList");
	}

	virtual LPCTSTR GetClass() const override
	{
		return _T("RssSourceList");
	}
};

// 这个是右下角的RSS文章列表中的一条
class CRssItemUI : public CListContainerElementUI
{
public:
	void SetTitle(LPCTSTR strTitle)
	{
		m_pTitle->SetText(strTitle);
	}
	void SetLink(LPCTSTR strLink)
	{
		m_pLink->SetText(strLink);
	}
	void SetDescription(LPCTSTR strDescription)
	{
		//有些Rss源不按规则, 在描述里面写全部博客内容, 导致文本特长, 有时候达好几百KB
		CDuiString t;
		const int maxch = 256;
		int i=0;
		int c;
		while(i < maxch-2 && (c=strDescription[i])){
			t += c;
			if(i==maxch-2){
				if(c>0x7F){
					t += strDescription[i+1];
				}
				break;
			}
			i++;
		}
		m_pDesc->SetText(t);
	}
	void SetRead(bool bRead)
	{
		m_pTitle->SetTextColor(bRead?0x00000000:0xFFFF0000);
	}

protected:
	bool dgSetRead(void* m)
	{
		auto msg = reinterpret_cast<TNotifyUI*>(m);
		if(msg->sType == DUI_MSGTYPE_ITEMACTIVATE){
			SetRead(true);
		}
		return true;
	}

public:
	CRssItemUI()
		: m_pTitle(nullptr)
		, m_pLink(nullptr)
		, m_pDesc(nullptr)
	{
		auto pVert = new CVerticalLayoutUI;
		m_pTitle   = new CLabelUI;
		m_pLink    = new CLabelUI;
		m_pDesc    = new CLabelUI;

		pVert->Add(m_pTitle);
		pVert->Add(m_pLink);
		pVert->Add(m_pDesc);

		Add(pVert);
	}

	void SetRssItem(CRssItem* item)
	{
		m_RssItem = item;
	}

	CRssItem* GetRssItem()
	{
		SMART_ASSERT(m_RssItem).Fatal();
		return m_RssItem;
	}

	static LPCTSTR GetUIClass()
	{
		return _T("RssItem");
	}
	virtual LPCTSTR GetClass() const override
	{
		return _T("RssItem");
	}

	virtual void DoInit() override
	{
		__super::DoInit();

		SetAttribute("mousechild","false");
		SetInset(CDuiRect(10,10,10,10));

		m_pTitle->SetFont(2);

		auto laGetHeight = [this](CLabelUI* ctrl)->int
		{
			return GetManager()->GetFontInfo(ctrl->GetFont())->tm.tmHeight + 4;
		};

		auto szAll = laGetHeight(m_pTitle);
		szAll += laGetHeight(m_pLink);
		szAll += laGetHeight(m_pDesc);
		szAll += GetInset().top + GetInset().bottom;
		SetFixedHeight(szAll);


		OnNotify += MakeDelegate(this, &CRssItemUI::dgSetRead);
	}

private:
	CLabelUI* m_pTitle;
	CLabelUI* m_pLink;
	CLabelUI* m_pDesc;

	CRssItem* m_RssItem;
};

// RssItem列表, 用来装上面的RssItemUI
class CRssItemListUI : public CListUI
{
protected:
	virtual void DoInit() override
	{
		__super::DoInit();

		SetAttribute(_T("header"), _T("hidden"));
		SetAttribute(_T("inset"), _T("5,5,5,5"));
		SetAttribute(_T("vscrollbar"), _T("true"));
		SetSelectedItemBkColor(0xFFFFFFFF);
	}

public:
	CRssItemUI* GetCurRssItemUI()
	{
		if(GetCurSel() == -1) return nullptr;

		return static_cast<CRssItemUI*>(GetItemAt(GetCurSel()));
	}

	void SetRssSource(CRssSource* src)
	{
		m_pRssSource = src;
	}

	CRssSource* GetRssSource()
	{
		return m_pRssSource;
	}

private:
	CRssSource* m_pRssSource;
};

class CRssItemListTabUI : public CTabLayoutUI
{
public:
	static LPCTSTR GetUIClass()
	{
		return _T("RssItemListTab");
	}

	CRssItemListUI* GetCurListUI()
	{
		if(GetCurSel()==-1) return nullptr;

		return static_cast<CRssItemListUI*>(GetItemAt(GetCurSel()));
	}
};


//////////////////////////////////////////////////////////////////////////

class CMainWindow : public WindowImplBase
{
private:
	static DWORD WINAPI ThreadProcOfRefresh(LPVOID lpParameter)
	{
		auto pObj = reinterpret_cast<CRssSourceRefresh*>(lpParameter);
		try{
			pObj->pManager->GetRssSource(
				pObj->pRssSource->pSource,
				pObj->pRssSource);
			pObj->errcode = CRssSourceRefresh::ErrorCode::kOK;
			// now set event to notify the ui
			::SetEvent(pObj->hEvent);
			return 0;
		}
		catch(const char* err){
			pObj->errcode = CRssSourceRefresh::ErrorCode::kError;
			pObj->errstr = err;
			::SetEvent(pObj->hEvent);
			return 0;
		}

		return 1;	// would never get here
	}

private:
	template<class T1,class T2>
	class TabManager
	{
	private:
		struct TabItem
		{
		public:
			T1* pt1;
			T2* pt2;

			TabItem(T1* _pt1, T2* _pt2)
			{
				pt1 = _pt1;
				pt2 = _pt2;
			}
// 		private:
// 			TabItem(const TabItem&);
// 			TabItem operator=(const TabItem&);
		};

		std::list<TabItem> m_Tabs;

	public:
		bool Add(T1* pt1, T2* pt2)
		{
			m_Tabs.push_back(TabItem(pt1, pt2));
			return true;
		}

		int Size()
		{
			return m_Tabs.size();
		}

		TabItem& GetAt(int index)
		{
			SMART_ASSERT(index>=0 && index<(int)m_Tabs.size())(index).Fatal();
			int i=0;
			for(auto x : m_Tabs){
				if(i++ == index)
					return x;
			}
			return *nullptr;
		}

		TabItem& operator[](int index)
		{
			return GetAt(index);
		}

		T1* Find(T2* pt2)
		{
			for(auto& tab : m_Tabs){
				if(tab.pt2 == pt2){
					return tab.pt1;
				}
			}
			return nullptr;
		}

		T2* Find(T1* pt1)
		{
			for(auto& tab : m_Tabs){
				if(tab.pt1 == pt1){
					return tab.pt2;
				}
			}
			return nullptr;
		}

		T1* operator()(T2* pt2)
		{
			return Find(pt2);
		}

		T2* operator()(T1* pt1)
		{
			return Find(pt1);
		}

		void Remove(T1* pt1)
		{
			for(auto s=m_Tabs.begin(); s!=m_Tabs.end(); ++s){
				if(s->pt1 == pt1){
					m_Tabs.erase(s);
					break;
				}
			}
		}

		void Remove(T2* pt2)
		{
			Remove(FindFirst(pt2));
		}
	};

	TabManager<CRssSourceUI,CRssItemListUI> m_items;
	TabManager<CSource,CRssSource> m_rsses;

public:
	CMainWindow(CRssManager& rss)
		: m_rss(&rss)
	{

	}

	DUI_DECLARE_MESSAGE_MAP()

private:
	CRssManager*		m_rss;
	CRssAuthorUI*		m_pRssAuthor;
	CRssSourceListUI*	m_pRssSources;
	CRssItemListTabUI*	m_pRssListTabs;

protected:
	bool dgSysBtn(void* ud)
	{
		auto msg = reinterpret_cast<TNotifyUI*>(ud);
		auto& name = msg->pSender->GetName();
		if(msg->sType == DUI_MSGTYPE_CLICK){
			if(name == _T("closebtn")){
				Close();
			}
			else if(name == _T("maxbtn")){
				SendMessage(WM_SYSCOMMAND, SC_RESTORE);
				msg->pSender->SetName(_T("restorebtn"));
			}
			else if(name == _T("restorebtn")){
				SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE);
				msg->pSender->SetName(_T("maxbtn"));
			}
			else if(name == _T("minbtn")){
				SendMessage(WM_SYSCOMMAND, SC_MINIMIZE);
			}
		}
		return false;
	}
protected:
	virtual CControlUI* CreateControl(LPCTSTR pstrClass) override
	{
		if(_tcscmp(pstrClass, CRssAuthorUI::GetUIClass())==0) return new CRssAuthorUI;
		else if(_tcscmp(pstrClass, CRssSourceListUI::GetUIClass())==0) return new CRssSourceListUI;
		else if(_tcscmp(pstrClass, CRssItemListTabUI::GetUIClass())==0) return new CRssItemListTabUI;
		else if(_tcscmp(pstrClass, CRssSourceFilterUI::GetUIClass())==0) return new CRssSourceFilterUI;
		return nullptr;
	}
	virtual CDuiString GetSkinFolder() override
	{
		return _T("skin/");
	}
	virtual CDuiString GetSkinFile() override
	{
		return _T("MainWindow.xml");
	}
	virtual LPCTSTR GetWindowClassName(void) const override
	{
		return _T("女孩不哭");
	}
	virtual LRESULT ResponseDefaultKeyEvent(WPARAM wParam) override
	{
		return FALSE;
	}

	virtual void InitWindow() override
	{
		FindControl(_T("closebtn"))	->OnNotify += MakeDelegate(this, &CMainWindow::dgSysBtn);
		FindControl(_T("minbtn"))	->OnNotify += MakeDelegate(this, &CMainWindow::dgSysBtn);
		FindControl(_T("maxbtn"))	->OnNotify += MakeDelegate(this, &CMainWindow::dgSysBtn);

		m_pRssAuthor = static_cast<CRssAuthorUI*>(FindControl(_T("author_info")));
		m_pRssSources = static_cast<CRssSourceListUI*>(FindControl(_T("rss_sources")));
		m_pRssListTabs = static_cast<CRssItemListTabUI*>(FindControl(_T("rss_list_tabs")));
		SMART_ASSERT(m_pRssAuthor && m_pRssSources && m_pRssListTabs).Fatal();

		std::vector<CSource*> srcs;
		m_rss->GetSources(&srcs);

		for(auto s : srcs){
			AddNewSource(s);
		}
	}

	void AddNewSource(CSource* s)
	{
		auto p = new CRssSource;
		s->pRss = p;
		p->pSource = s;
		m_rsses.Add(s, p);

		auto sui = new CRssSourceUI;
		sui->SetSource(s);
		sui->SetTitle(s->title.c_str());
		sui->SetCategory(s->category.c_str());

		auto pui = new CRssItemListUI;
		pui->SetRssSource(p);
		m_items.Add(sui, pui);

		m_pRssSources->Add(sui);
		m_pRssListTabs->Add(pui);
	}

	virtual void OnFinalMessage( HWND hWnd ) override
	{
		__super::OnFinalMessage(hWnd);
		delete this;
		::PostQuitMessage(0);
	}

	virtual void Notify(TNotifyUI& msg) override
	{
		if(msg.pSender == m_pRssSources){
			if(msg.sType == DUI_MSGTYPE_ITEMSELECT){
				m_pRssAuthor->Empty();
				m_pRssListTabs->SelectItem(m_pRssSources->GetCurSel());
				auto pRss = m_pRssListTabs->GetCurListUI()->GetRssSource();

				m_pRssAuthor->SetTitle(pRss->strTitle.c_str());
				m_pRssAuthor->SetLink(pRss->strLink.c_str());
				m_pRssAuthor->SetDescription(pRss->strDescription.c_str());
			}
		}
		
		if(_tcscmp(msg.pSender->GetClass(), CRssItemUI::GetUIClass()) == 0){
			if(msg.sType == DUI_MSGTYPE_ITEMACTIVATE){
				auto pItem = static_cast<CRssItemUI*>(msg.pSender);
				::ShellExecute(GetHWND(),"open",pItem->GetRssItem()->strLink.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
				auto rlink = m_pRssListTabs->GetCurListUI()->GetRssSource()->pSource->source.c_str();
				SMART_ENSURE(m_rss->GetDB()->MarkCacheAsRead(rlink, pItem->GetRssItem()->strLink.c_str()),==true).Fatal();
				return;
			}
		}

		if(msg.pSender->GetName() == _T("rssfilter")){
			if(msg.sType == DUI_MSGTYPE_RETURN){
				CDuiString s = msg.pSender->GetText();
				CDuiString s1,s2;	// 分类(可选)|标题
				int pos = s.Find(_T('|'));
				if(pos != -1){
					s1 = s.Left(pos);
					s2 = s.Mid(pos+1);
				}
				else{
					s2 = s;
				}

				auto sz = m_pRssSources->GetCount();
				for(auto i=0; i<sz; ++i){
					auto pctrl = static_cast<CRssSourceUI*>(m_pRssSources->GetItemAt(i));
					auto pSource = pctrl->GetSource();
					CDuiString ca(pSource->category.c_str());
					CDuiString ti(pSource->title.c_str());

					if(ca.Find(s1, 0, false) != -1
						&& ti.Find(s2, 0, false) != -1)
					{
						pctrl->SetVisible(true);
					}
					else{
						pctrl->SetVisible(false);
					}
				}
				return;
			}
		}

		return __super::Notify(msg);
	}

	virtual void DmTimer(TNotifyUI& msg)
	{
		if(_tcscmp(msg.pSender->GetClass(), CRssSourceUI::GetUIClass()) == 0){
			auto pSourceUI = static_cast<CRssSourceUI*>(msg.pSender);
			if(msg.wParam == pSourceUI->kRefreshTimerID){
				// check if succeeded
				auto pRefresh = pSourceUI->GetRefresh();
				pSourceUI->SetRefresh(nullptr);
				SMART_ASSERT(pRefresh).Fatal();
				if(pRefresh->errcode != CRssSourceRefresh::ErrorCode::kOK){
					::MessageBox(GetHWND(), pRefresh->errstr.c_str(), nullptr, MB_ICONERROR);
					delete pRefresh;
					return;
				}

				delete pRefresh;

				if(pSourceUI->GetSource()->pRss->RssItems.size() == 0){
					::MessageBox(GetHWND(), _T("未能获取到任何的RSS内容, 您确定该RSS源有效?"), nullptr, MB_OK|MB_ICONINFORMATION);
					return;
				}

				auto pList = m_items(pSourceUI);
				pList->RemoveAll();
				for(auto rss : pSourceUI->GetSource()->pRss->RssItems){
					auto p = new CRssItemUI;
					p->SetRssItem(rss);
					p->SetTitle(rss->strTitle.c_str());
					p->SetLink(rss->strLink.c_str());
					p->SetDescription(rss->strDescription.c_str());
					p->SetRead(rss->bRead);

					pList->Add(p);
				}
				if(m_pRssSources->GetCurSel() == pSourceUI->GetIndex()){
					m_pRssAuthor->SetTitle(pSourceUI->GetSource()->pRss->strTitle.c_str());
					m_pRssAuthor->SetLink(pSourceUI->GetSource()->pRss->strLink.c_str());
					m_pRssAuthor->SetDescription(pSourceUI->GetSource()->pRss->strDescription.c_str());
				}
				return;
			}
		}
	}

	virtual void DmMenu(TNotifyUI& msg)
	{
		// WARNING!!! these three lines of codes
		// Right click on Menu can produce another WM_CONTEXTMENU message
		// It may crash the program sometimes
		static bool st_bMenuPopped = false;
		if(st_bMenuPopped) return;
		MenuProtect mp(&st_bMenuPopped);

		if(_tcscmp(msg.pSender->GetClass(), CRssSourceListUI::GetUIClass()) == 0){
			auto pList = static_cast<CRssSourceListUI*>(msg.pSender);
			HMENU hMenu = ::LoadMenu(CPaintManagerUI::GetInstance(), MAKEINTRESOURCE(Menu_RssSourceUI));
			HMENU hSubMenu0 = ::GetSubMenu(hMenu, 0);
			yagc gc(hMenu, [](void* ptr){return ::DestroyMenu(HMENU(ptr))!=FALSE;});

			auto pCtrl = FindControl(msg.ptMouse);
			CRssSourceUI* pItem=nullptr;
			if(_tcscmp(pCtrl->GetClass(), CRssSourceUI::GetUIClass()) == 0){
				pItem = static_cast<CRssSourceUI*>(pCtrl);
			}

			[](HMENU hMenu, bool bMenuFlag, bool bRefreshing)->void{
				UINT flag = bMenuFlag && !bRefreshing? MF_ENABLED : MF_DISABLED|MF_GRAYED;
				::EnableMenuItem(hMenu, RSSSOURCEUIMENU_REFRESH,		flag);
				::EnableMenuItem(hMenu, RSSSOURCEUIMENU_NEWSOURCE,		MF_ENABLED);
				::EnableMenuItem(hMenu, RSSSOURCEUIMENU_DELETESOURCE,	flag);
				::EnableMenuItem(hMenu, RSSSOURCEUIMENU_EDITRSS,		flag);
			}(hSubMenu0, !!pItem, pItem?!!pItem->GetRefresh():false);
			
			POINT pt = msg.ptMouse;
			::ClientToScreen(GetHWND(), &pt);
			UINT id = ::TrackPopupMenu(hSubMenu0, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,
				pt.x, pt.y, 0, GetHWND(), nullptr);
			switch(id)
			{
			case RSSSOURCEUIMENU_REFRESH:
				{
					if(!pItem) return; // should never happen

					auto pRefresh = new CRssSourceRefresh;
					pRefresh->hEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
					pRefresh->pManager = m_rss;
					pRefresh->pObject = pItem;
					pRefresh->pRssSource = pItem->GetSource()->pRss;
					pItem->SetRefresh(pRefresh);
					GetManager()->SetTimer(pItem, CRssSourceUI::kRefreshTimerID, 1000);
					HANDLE hThread = (HANDLE)::_beginthreadex(nullptr, 0, 
						reinterpret_cast<unsigned int (__stdcall*)(void*)>(ThreadProcOfRefresh), 
						(void*)pRefresh, 0, nullptr);
					::CloseHandle(hThread);
					return;
				}
				break;
			case RSSSOURCEUIMENU_REFRESHALL:
				{
					auto sz = m_pRssSources->GetCount();
					for(auto i=0; i<sz; ++i){
						auto p = static_cast<CRssSourceUI*>(m_pRssSources->GetItemAt(i));

						CRssSourceRefresh* pRefresh = nullptr;
						if(p->IsVisible() && !(pRefresh=p->GetRefresh())){
							pRefresh = new CRssSourceRefresh;
							pRefresh->hEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
							pRefresh->pManager = m_rss;
							pRefresh->pObject = p;
							pRefresh->pRssSource = p->GetSource()->pRss;
							p->SetRefresh(pRefresh);
							GetManager()->SetTimer(p, CRssSourceUI::kRefreshTimerID, 1000);
							HANDLE hThread = (HANDLE)::_beginthreadex(nullptr, 0, 
								reinterpret_cast<unsigned int (__stdcall*)(void*)>(ThreadProcOfRefresh), 
								(void*)pRefresh, 0, nullptr);
							::CloseHandle(hThread);
						}
					}
					return ;
				}
			case RSSSOURCEUIMENU_NEWSOURCE:
				{
					class CNewSource : public INewRssDlgCallback
					{
					private:
						CRssManager& mgr;
					public:
						CNewSource(CRssManager& mgr_)
							: mgr(mgr_)
						{}

						virtual bool CheckValid(LPCTSTR* pro) override
						{
							const LPCTSTR prefix = _T("http://");
							if(_tcsnicmp(strLink,prefix,_tcslen(prefix)) != 0){
								*pro = _T("RSS源地址必须以 'http://' 开始!");
								return false;
							}
							if(mgr.GetDB().IsSourceExists(strLink)){
								*pro = _T("已存在该RSS源, 请不要重复添加!");
								return false;
							}
							return true;
						}
					};

					CNewSource cb(*m_rss);
					CNewRssDlg dlg(*this, &cb);
					if(dlg.GetDlgCode() != CNewRssDlg::kOK)
						return;

					if(m_rss->GetDB().AddSource(cb.strTitle,cb.strLink,cb.strCategory)){
						AddNewSource(new CSource(cb.strTitle, cb.strLink, cb.strCategory));
					}
					return;
				}
			case RSSSOURCEUIMENU_DELETESOURCE:
				{
					if(!pItem) return; // should never happen

					auto pSource = pItem->GetSource();
					if(::MessageBox(GetHWND(), _T("是否确定要删除该RSS源?"), _T(""), MB_ICONQUESTION|MB_OKCANCEL|MB_DEFBUTTON2) == IDOK){
						bool bDelCaches = ::MessageBox(GetHWND(), _T("是否需要删除对应的所有缓存?"), _T(""), MB_ICONQUESTION|MB_OKCANCEL|MB_DEFBUTTON2) == IDOK;
						try{
							m_rss->GetDB().DeleteSource(pSource->id, bDelCaches ? pSource->source.c_str() : nullptr);
							auto pList = m_items(pItem);
							SMART_ENSURE(m_pRssSources->Remove(pItem),==true).Stop();
							SMART_ENSURE(m_pRssListTabs->Remove(pList),==true).Stop();

							m_items.Remove(pItem);
						}
						catch(const char* msg){
							::MessageBox(GetHWND(), msg, nullptr, MB_ICONERROR);
							return;
						}
					}
					return;
				}
			case RSSSOURCEUIMENU_EDITRSS:
				{
					if(!pItem) return; // should never happen
					auto pSource = pItem->GetSource();

					class CEditSource : public INewRssDlgCallback
					{
					private:
						CSource* pSource;
					public:
						CEditSource(CSource* pSource_)
							: pSource(pSource_)
						{}

						virtual bool CheckValid(LPCTSTR* pro) override
						{
							const LPCTSTR prefix = _T("http://");
							if(_tcsnicmp(strLink,prefix,_tcslen(prefix)) != 0){
								*pro = _T("RSS源地址必须以 'http://' 开始!");
								return false;
							}
							if(strLink != pSource->source.c_str()){
								*pro = _T("抱歉, 暂时不支持在编辑界面修改RSS地址!");
								return false;
							}
							return true;
						}
					};

					CEditSource cb(pSource);
					CNewRssDlg dlg(GetHWND(), &cb, pSource);
					if(dlg.GetDlgCode() != CNewRssDlg::kOK)
						return;

					pSource->title = cb.strTitle;
					pSource->source = cb.strLink;
					pSource->category = cb.strCategory;

					try{
						m_rss->GetDB().UpdateSource(pSource->id, pSource);
						pItem->SetContents(pItem->GetSource()); // equals to pSource
					}
					catch(const char* msg){
						::MessageBox(GetHWND(), msg, nullptr, MB_ICONERROR);
						return;
					}
					return;
				}
			}
			return;
		}
	}
};

DUI_BEGIN_MESSAGE_MAP(CMainWindow, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_MENU,	DmMenu)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_TIMER,	DmTimer)
DUI_END_MESSAGE_MAP()

int WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd )
{
#ifdef _DEBUG
	AllocConsole();
	freopen("CONIN$","r",stdin);
	freopen("CONOUT$","w",stdout);
	freopen("CONOUT$","w",stdout);
#endif

	char path[260];
	GetModuleFileName(nullptr, path,260);
	*(strrchr(path,'\\')+1) = 0;
	SetCurrentDirectory(path);

	__debug_file.open("./debug.txt", ios_base::binary|ios_base::app);

	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::StartupGdiPlus();

	WSADATA wsa;
	WSAStartup(MAKEWORD(2,2),&wsa);

	theRss = new CRssManager;

	try{
		theRss->OpenDB("rss.db");
	}
	catch(const char* msg){
		::MessageBox(nullptr, msg, nullptr, MB_OK);
		return 1;
	}

	CMainWindow* dlg = new CMainWindow(*theRss);
	dlg->CreateDuiWindow(nullptr, _T("Tiny Rss Reader"), WS_OVERLAPPEDWINDOW|WS_SIZEBOX);
	dlg->CenterWindow();
	dlg->ShowModal();

	CPaintManagerUI::MessageLoop();

	try{
		theRss->CloseDB();
	}
	catch(const char* msg){
		::MessageBox(nullptr, msg, nullptr, MB_OK);
	}

	CPaintManagerUI::ShutdownGdiPlus();
	WSACleanup();

	return 0;
}
