#include <array>
#include <cassert>
#include <utility>
#include <time.h>
#include "../resource.h"
#include "bind.h"
#include "filesystem.h"
#include "CMiniblink.h"

namespace HttpBridge {
	CNetJob::CNetJob(wkeNetJob job) :m_job(job) {}
	void CNetJob::SetHTTPHeaderField(wchar_t * key, wchar_t * value, bool response)
	{
		::wkeNetSetHTTPHeaderField(m_job, key, value, response);
	}
	void CNetJob::SetMIMEType(const char * type)
	{
		::wkeNetSetMIMEType(m_job, const_cast<char*>(type));
	}
	const char * CNetJob::GetMIMEType() const
	{
		return ::wkeNetGetMIMEType(m_job, NULL);
	}
	void CNetJob::CancelRequest()
	{
		::wkeNetCancelRequest(m_job);
	}
	wkeRequestType CNetJob::GetRequestMethod()
	{
		return ::wkeNetGetRequestMethod(m_job);
	}
	wkePostBodyElements * CNetJob::GetPostBody()
	{
		return ::wkeNetGetPostBody(m_job);
	}

	void CNetJob::SetResponseData(void* buf, int len)
	{
		::wkeNetSetData(m_job, buf, len);
	}

	CMiniblink::CMiniblink(HINSTANCE hInstance, nlohmann::json* setting) :m_cursor(-1), settings(setting) {
		int width = (*setting)["window"]["default_size"][0];
    	int height = (*setting)["window"]["default_size"][1];
		m_wkeWebView = ::wkeCreateWebWindow(WKE_WINDOW_TYPE_POPUP, NULL, 0, 0, width, height);

        HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_MAINAPP));
        ::SendMessage(GetHWND(), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(GetHWND(), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		counter++;
		DoInit();
		install_filesystem(m_wkeWebView);

		// bind("add", [this](int a, int b) {
        //     this->m_wkeWebView->call("setValue", a + b);
        // });
	}

	CMiniblink::~CMiniblink()
	{
		counter--;
		if (!m_released && NULL != m_wkeWebView) {
			m_released = true;
			wkeDestroyWebView(m_wkeWebView);
			m_wkeWebView = NULL;
		}
		if (counter <= 0) {
        	::PostQuitMessage(0);
		}
	}

	LPCTSTR CMiniblink::GetClass() const
	{
		return L"Miniblink";
	}

	// LRESULT CALLBACK subClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	// 	if (g_isMoving && (uMsg == WM_NCHITTEST)) {
	// 		POINT pt = { (LONG)GET_X_LPARAM(lParam), (LONG)GET_Y_LPARAM(lParam) };
	// 		// POINT pt = getWndCenter(GetDesktopWindow());
	// 		::ScreenToClient(hWnd, &pt);
	// 		// printf("pt: %d %d\n", pt.x, pt.y);
	// 		// printf("rect: %d %d %d %d\n", move_rect.left, move_rect.top, move_rect.right, move_rect.bottom);
	// 		if (::PtInRect(&g_moveRect, pt)) {
	// 			return HTCAPTION;
	// 		}
	// 	}

	// 	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	// }

	void CMiniblink::DoInit() {
		static bool isInitialized = ::wkeIsInitialize==NULL?false:(::wkeIsInitialize());
		if (!isInitialized) {
			::wkeInitialize();
			isInitialized = true;
		}

		::wkeSetTransparent(m_wkeWebView, false);
		//::wkeOnPaintUpdated(m_wkeWebView, MBPaintUpdate, this);
		::wkeSetHandle(m_wkeWebView, GetHWND());

		::wkeOnWindowClosing(m_wkeWebView, MBWindowClosing, this);
		::wkeOnWindowDestroy(m_wkeWebView, MBWindowDestroy, this);
		::wkeOnTitleChanged(m_wkeWebView, MBTitleChanged, this);
		::wkeOnURLChanged2(m_wkeWebView, MBUrlChanged, this);
		::wkeOnAlertBox(m_wkeWebView, MBAlert, this);
		::wkeOnConfirmBox(m_wkeWebView, MBConfirm, this);
		::wkeOnPromptBox(m_wkeWebView, MBPromptBox, this);
		::wkeOnNavigation(m_wkeWebView, MBNavigation, this);
		::wkeOnCreateView(m_wkeWebView, MBCreateView, this);
		::wkeOnDocumentReady2(m_wkeWebView, MBDocumentReady, this);
		::wkeOnDownload(m_wkeWebView, MBDownload, this);
		::wkeOnDownload2(m_wkeWebView, MBDownload2, this);
		::wkeNetOnResponse(m_wkeWebView, MBNetResponse, this);
		::wkeOnConsole(m_wkeWebView, MBConsole, this);
		::wkeOnLoadUrlBegin(m_wkeWebView, MBLoadUrlBegin, this);
		::wkeOnLoadUrlEnd(m_wkeWebView, MBLoadUrlEnd, this);
		::wkeOnDidCreateScriptContext(m_wkeWebView, MBCreateScriptContext, this);
		::wkeOnWillReleaseScriptContext(m_wkeWebView, MBReleaseScriptContext, this);
		::wkeOnWillMediaLoad(m_wkeWebView, MBMediaLoad, this);
		::wkeOnLoadingFinish(m_wkeWebView, MBLoadedFinish, this);

		::wkeMoveToCenter(m_wkeWebView);

		::wkeJsBindFunction("url", &urlCallback, this, 1);
		::wkeJsBindFunction("eMsg", &onMsg, this, 5);

		::wkeSetDebugConfig(m_wkeWebView, "decodeUrlRequest", nullptr);
		::wkeSetWindowTitleW(m_wkeWebView, L"miniblink demo");

		//::SetWindowSubclass(wkeGetWindowHandle(m_wkeWebView), subClassProc, 0, 0);
	}

	void  CMiniblink::Goto(LPCTSTR url) {
	// 	if (_tcslen(url) >= 8 && (((url[0] == 'H' || url[0] == 'h') && (url[1] == 'T' || url[1] == 't') && (url[2] == 'T' || url[2] == 't') && (url[3] == 'P' || url[3] == 'p') && url[4] == ':'&&url[5] == '/'&&url[6] == '/')
	// 		|| ((url[0] == 'H' || url[0] == 'h') && (url[1] == 'T' || url[1] == 't') && (url[2] == 'T' || url[2] == 't') && (url[3] == 'P' || url[3] == 'p') && (url[4] == 'S' || url[4] == 's') && url[5] == ':'&&url[6] == '/'&&url[7] == '/')
	// 		)||StrCmp(url,L"about:blank")==0
	// 		)
	// 	{
	// #ifdef UNICODE
	// 		::wkeLoadURLW(m_wkeWebView, url);
	// #else
	// 		::wkeLoadURL(m_wkeWebView, url);
	// #endif
	// 	}
	// 	else {
	// #ifdef UNICODE
	// 		::wkeLoadFileW(m_wkeWebView, url);
	// #else
	// 		::wkeLoadFile(m_wkeWebView, url);
	// #endif
	// 	}
	}
	void CMiniblink::SetPos(RECT rc, bool bNeedUpdate) {
		wkeResize(m_wkeWebView, rc.right - rc.left, rc.bottom - rc.top);
	}

	// void CMiniblink::DoEvent(TEventUI& event) {
	// 	if (!this->IsEnabled() || !this->IsVisible()) return;
	// 	RECT rc = GetPos();
	// 	POINT pt = { event.ptMouse.x - rc.left, event.ptMouse.y - rc.top };
	// 	static WkeCursorInfoType cursorInfo = WkeCursorInfoType::WkeCursorInfoPointer;
	// 	switch (event.Type)
	// 	{
	// 	case UIEVENT_MOUSEENTER:
	// 	case UIEVENT_MOUSELEAVE:
	// 	case UIEVENT_MOUSEMOVE:
	// 	{
	// 		unsigned int flags = 0;
	// 		if (event.wParam & MK_CONTROL)
	// 			flags |= WKE_CONTROL;
	// 		if (event.wParam & MK_SHIFT)
	// 			flags |= WKE_SHIFT;
	// 		if (event.wParam & MK_LBUTTON)
	// 			flags |= WKE_LBUTTON;
	// 		if (event.wParam & MK_MBUTTON)
	// 			flags |= WKE_MBUTTON;
	// 		if (event.wParam & MK_RBUTTON)
	// 			flags |= WKE_RBUTTON;
	// 		wkeFireMouseEvent(m_wkeWebView, WKE_MSG_MOUSEMOVE, pt.x, pt.y, flags);
	// 		updateCursor();
	// 	}
	// 	break;
	// 	case UIEVENT_BUTTONDOWN:
	// 		this->SetFocus();
	// 		wkeFireMouseEvent(m_wkeWebView, WKE_MSG_LBUTTONDOWN, pt.x, pt.y, event.wKeyState);
	// 		break;
	// 	case UIEVENT_BUTTONUP:
	// 		wkeFireMouseEvent(m_wkeWebView, WKE_MSG_LBUTTONUP, pt.x, pt.y, event.wKeyState);
	// 		break;
	// 	case UIEVENT_RBUTTONDOWN:
	// 		this->SetFocus();
	// 		wkeFireMouseEvent(m_wkeWebView, WKE_MSG_RBUTTONDOWN, pt.x, pt.y, event.wKeyState);
	// 		break;
	// 	case UIEVENT_DBLCLICK:
	// 		wkeFireMouseEvent(m_wkeWebView, WKE_MSG_LBUTTONDBLCLK, pt.x, pt.y, event.wKeyState);
	// 		break;
	// 	case UIEVENT_SCROLLWHEEL:
	// 		wkeFireMouseWheelEvent(m_wkeWebView, pt.x, pt.y, event.wParam == SB_LINEUP ? 120 : -120, event.wKeyState);
	// 		break;
	// 	case UIEVENT_KEYDOWN:
	// 		this->SetFocus();
	// 		wkeFireKeyDownEvent(m_wkeWebView, event.chKey, event.lParam, false);
	// 		break;
	// 	case UIEVENT_KEYUP:
	// 		wkeFireKeyUpEvent(m_wkeWebView, event.chKey, event.lParam, false);
	// 		break;
	// 	case UIEVENT_CHAR:
	// 		wkeFireKeyPressEvent(m_wkeWebView, event.chKey, event.lParam, false);
	// 		break;
	// 	case UIEVENT_SETFOCUS:
	// 		this->SetFocus();
	// 		wkeSetFocus(m_wkeWebView);
	// 		this->Invalidate();
	// 		break;
	// 	case UIEVENT_KILLFOCUS:
	// 		wkeKillFocus(m_wkeWebView);
	// 		this->Invalidate();
	// 		break;
	// 	case UIEVENT_SETCURSOR: {
	// 		/*HWND hwnd = this->GetHWND();
	// 		if (wkeFireWindowsMessage(m_wkeWebView, hwnd, WM_SETCURSOR, 0, 0, NULL)) {
	// 			return;
	// 		}*/
	// 		//return;
	// 	}
	// 							break;
	// 	case UIEVENT_CONTEXTMENU:
	// 	{
	// 		unsigned int flags = 0;
	// 		if (event.wParam & MK_CONTROL)
	// 			flags |= WKE_CONTROL;
	// 		if (event.wParam & MK_SHIFT)
	// 			flags |= WKE_SHIFT;
	// 		wkeFireContextMenuEvent(m_wkeWebView, pt.x, pt.y, flags);
	// 		break;
	// 	}
	// 	/*case UIEVENT_TIMER:
	// 		if (event.wParam == EVENT_TICK_TIEMER_ID) {
	// 			Invalidate();
	// 		}
	// 		break;*/

	// 	default: break;
	// 	}
	// 	CControlUI::DoEvent(event);
	// }

	LRESULT CMiniblink::MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, bool & bHandled)
	{

		if (uMsg == GetRunJsMessageId()) {
			bHandled = true;
			JS_ARG *arg = (JS_ARG *)wParam;
			if (arg->webView == NULL || arg->webView == (wkeWebView)0xcccccccc) return S_OK;
			jsValue value;
			jsExecState es;

			if (arg->frameId == NULL||arg->frameId== (wkeWebFrameHandle)0xcccccccc) {
				value = ::wkeRunJS(arg->webView, arg->js);
				es = ::wkeGlobalExec(arg->webView);
			}
			else
			{
				value = ::wkeRunJsByFrame(arg->webView, arg->frameId, arg->js, true);
				es = ::wkeGetGlobalExecByFrame(arg->webView,arg->frameId);
			}
			switch (arg->type)
			{
			case JS_RESULT_TYPE::JS_INT: {
				int* ret = (int*)arg->result;
				*ret = jsToInt(es, value);
				break;
			}
			case JS_RESULT_TYPE::JS_BOOL: {
				bool* ret = (bool*)arg->result;
				*ret = jsToBoolean(es, value);
				break;
			}
			case JS_RESULT_TYPE::JS_CHAR:
			{
				char* ret = (char*)arg->result;
				int* maxLength = (int*)arg->param;
				strcpy_s(ret, *maxLength, jsToString(es, value));
				break;
			}
			case JS_RESULT_TYPE::JS_WCHAR:
			{
				WCHAR* ret = (WCHAR*)arg->result;
				//StrCpyW(ret, jsToStringW(es, value));
				//strcpy_s(ret, (rsize_t)arg->param, jsToStringW(es, value));
				break;
			}
			case JS_RESULT_TYPE::JS_DOUBLE: {
				double* ret = (double*)arg->result;
				*ret = jsToDouble(es, value);
				break;
			}
			default:
				break;
			}
			return S_OK;
		}
		else if (uMsg == GetActionMessageId()) {
			bHandled = true;
			wkeWebView web = (wkeWebView)wParam;
			MB_ACTION_ITEM *action = (MB_ACTION_ITEM *)lParam;
			switch (action->sender)
			{
			case MB_ACTION_SENDER::KEY:
			{
				MB_ACTION_KEY_DATA *data = (MB_ACTION_KEY_DATA *)action->data;
				if (data->event == MB_ACTION_KEY_EVENT::DOWN) {
					::wkeFireKeyDownEvent(web, data->code, data->flags, data->flags);
				}
				else if (data->event == MB_ACTION_KEY_EVENT::PRESS) {
					::wkeFireKeyPressEvent(web, data->code, data->flags, data->flags);
				}
				else if (data->event == MB_ACTION_KEY_EVENT::UP) {
					::wkeFireKeyUpEvent(web, data->code, data->flags, data->flags);
				}
				break;
			}
			case MB_ACTION_SENDER::MENU:{
				MB_ACTION_MENU_DATA *data = (MB_ACTION_MENU_DATA *)action->data;
				::wkeFireContextMenuEvent(web, data->x, data->y, data->flags);
				break;
			}
			case MB_ACTION_SENDER::MOUSE: {
				MB_ACTION_MOUSE_DATA *data = (MB_ACTION_MOUSE_DATA *)action->data;
				::wkeFireMouseEvent(web, data->message, data->x, data->y, data->flags);
				break;
			}
			case MB_ACTION_SENDER::WHEEL: {
				MB_ACTION_WHEEL_DATA *data = (MB_ACTION_WHEEL_DATA *)action->data;
				::wkeFireMouseWheelEvent(web, data->x, data->y, data->delta, data->flags);
				break;
			}
			default:
				break;
			}
			return S_OK;
		}

		//CControlUI *current = GetManager()->FindControl(GetManager()->GetMousePos());
		//
		//if (current != this||!current->IsEnabled()) return S_OK;
		// if (uMsg == WM_IME_STARTCOMPOSITION) {
		// 	const RECT controlPos = this->GetPos();

		// 	wkeRect caret = wkeGetCaretRect(m_wkeWebView);
		// 	COMPOSITIONFORM COMPOSITIONFORM;
		// 	COMPOSITIONFORM.dwStyle = CFS_POINT | CFS_FORCE_POSITION;
		// 	COMPOSITIONFORM.ptCurrentPos.x = caret.x + controlPos.left;
		// 	COMPOSITIONFORM.ptCurrentPos.y = caret.y + controlPos.top;

		// 	HIMC hIMC = ::ImmGetContext(GetHWND());
		// 	::ImmSetCompositionWindow(hIMC, &COMPOSITIONFORM);
		// 	::ImmReleaseContext(GetHWND(), hIMC);

		// 	bHandled = true;
		// }
		else if (uMsg == WM_SETCURSOR) {
			bHandled = true;
		}
		return S_OK;
	}

	bool CMiniblink::OnClosing() {
		return true;
	}
	void CMiniblink::OnTitleChanged(LPCWSTR title)
	{
		//OutputDebugString(title);
		wkeSetWindowTitleW(m_wkeWebView, title);
	}

	void CMiniblink::OnUrlChanged(LPCWSTR url, CWebFrame *frame)
	{
	}

	void CMiniblink::OnAlert(LPCWSTR msg)
	{
		::MessageBox(GetHWND(), msg, L"确定", MB_OK|MB_ICONINFORMATION);
	}

	bool CMiniblink::OnConfirm(LPCWSTR msg)
	{
		return ::MessageBox(GetHWND(), msg, L"询问", MB_OKCANCEL | MB_ICONQUESTION) == IDOK;
		return false;
	}

	bool CMiniblink::OnPrompt(LPCWSTR msg, LPCWSTR defaultResult, LPCWSTR result)
	{

		return false;
	}

	bool CMiniblink::OnNavigation(wkeNavigationType navigationType, LPCWSTR url)
	{
		return true;
	}

	CMiniblink * CMiniblink::OnNewOpen(wkeNavigationType navigationType, LPCWSTR url, const wkeWindowFeatures * windowFeatures)
	{
		//wkeNavigationType::
		//this->Goto(url);
		return this;
	}

	void CMiniblink::OnDocumentReady(CWebFrame * frame)
	{

	}

	bool CMiniblink::OnDownload(const char * url)
	{
		return false;
	}

	bool CMiniblink::OnResponse(const char * url, CNetJob * job)
	{
		return false;
	}

	void CMiniblink::OnConsole(wkeConsoleLevel level, LPCWSTR message, LPCWSTR sourceName, unsigned sourceLine, LPCWSTR stackTrace)
	{
	}

	bool CMiniblink::OnRequestBegin(const char * url, CNetJob * job)
	{
		return false;
	}

	void CMiniblink::OnRequestEnd(const char * url, CNetJob * job, void * buf, int len)
	{
	}

	void CMiniblink::OnCreateScriptContext(CWebFrame * frame, void * context, int extensionGroup, int worldId)
	{
	}

	void CMiniblink::OnReleaseScriptContext(CWebFrame * frame, void * context, int worldId)
	{
	}

	void CMiniblink::OnMediaLoad(const char * url, wkeMediaLoadInfo * info)
	{
	}
	void CMiniblink::OnLoadingFinish(LPCWSTR url, wkeLoadingResult result, LPCWSTR failedReason)
	{
	}
	void CMiniblink::onDataRecv(void* ptr, wkeNetJob job, const char* data, int length)
	{

	}

	void CMiniblink::onDataFinish(void* ptr, wkeNetJob job, wkeLoadingResult result)
	{

	}
	unsigned CMiniblink::GetVersion() {
		return wkeGetVersion();
	}
	wkeWebView CMiniblink::GetWebView() {
		return m_wkeWebView;
	}
	bool CMiniblink::IsDocumentReady() {
		return ::wkeIsDocumentReady(m_wkeWebView);
	}
	void CMiniblink::Reload() {
		::wkeReload(m_wkeWebView);
	}
	LPCWSTR CMiniblink::GetTitle() {
	#ifdef UNICODE
		return ::wkeGetTitleW(m_wkeWebView);
	#else
		return ::wkeGetTitle(m_wkeWebView);
	#endif // UNICODE
	}
	int CMiniblink::GetWidth() {
		return ::wkeGetWidth(m_wkeWebView);
	}
	int CMiniblink::GetHeight() {
		return ::wkeGetHeight(m_wkeWebView);
	}
	int CMiniblink::GetContentWidth() {
		return ::wkeGetContentWidth(m_wkeWebView);
	}
	int CMiniblink::GetContentHeight() {
		return ::wkeGetContentHeight(m_wkeWebView);
	}
	bool CMiniblink::CanGoBack() {
		return ::wkeCanGoBack(m_wkeWebView);
	}
	bool CMiniblink::GoBack() {
		if (this->CanGoBack()) {
			return ::wkeGoBack(m_wkeWebView);
		}
		return false;
	}
	bool CMiniblink::CanGoForward() {
		return ::wkeCanGoForward(m_wkeWebView);
	}
	bool CMiniblink::GoForward() {
		if (this->CanGoForward()) {
			return ::wkeGoForward(m_wkeWebView);
		}
		return false;
	}
	void CMiniblink::EditorSelectAll() {
		::wkeEditorSelectAll(m_wkeWebView);
	}
	void CMiniblink::EditorUnSelect() {
		::wkeEditorUnSelect(m_wkeWebView);
	}
	void CMiniblink::EditorCopy() {
		::wkeEditorCopy(m_wkeWebView);
	}
	void CMiniblink::EditorCut() {
		::wkeEditorCut(m_wkeWebView);
	}
	void CMiniblink::EditorDelete() {
		::wkeEditorDelete(m_wkeWebView);
	}
	void CMiniblink::EditorUndo() {
		::wkeEditorUndo(m_wkeWebView);
	}
	void CMiniblink::EditorRedo() {
		::wkeEditorRedo(m_wkeWebView);
	}
	LPCWSTR CMiniblink::GetCookie() {
	#ifdef UNICODE
		return ::wkeGetCookieW(m_wkeWebView);
	#else
		return ::wkeGetCookie(m_wkeWebView);
	#endif // UNICODE
	}
	void CMiniblink::SetCookie(const char *cookie, const char *url) {
		const char *_url = url;
		if (_url == NULL) {
			_url = GetUrl();
		}
		::wkeSetCookie(m_wkeWebView, _url, cookie);
	}
	const char *CMiniblink::GetUrl() const {
		return ::wkeGetURL(m_wkeWebView);
	}
	void CMiniblink::SetCookieEnabled(bool enable) {
		::wkeSetCookieEnabled(m_wkeWebView, enable);
	}
	bool CMiniblink::IsCookieEnabled() {
		return ::wkeIsCookieEnabled(m_wkeWebView);
	}
	void CMiniblink::SetCookieFile(LPCWSTR file) {
		::wkeSetCookieJarFullPath(m_wkeWebView, file);
	}
	void CMiniblink::SetLocalStoragePath(LPCWSTR path) {
		::wkeSetLocalStorageFullPath(m_wkeWebView, path);
	}
	void CMiniblink::SetZoom(float factor) {
		::wkeSetZoomFactor(m_wkeWebView, factor);
	}
	float CMiniblink::GetZoom() {
		return ::wkeGetZoomFactor(m_wkeWebView);
	}
	void CMiniblink::SetProxy(const wkeProxy* proxy) {
		::wkeSetViewProxy(m_wkeWebView, const_cast<wkeProxy*>(proxy));
	}
	void CMiniblink::SetMemoryCacheEnable(bool enable) {
		::wkeSetMemoryCacheEnable(m_wkeWebView, enable);
	}
	void CMiniblink::SetTouchEnabled(bool enable) {
		::wkeSetTouchEnabled(m_wkeWebView, enable);
	}
	void CMiniblink::SetMouseEnabled(bool enable) {
		::wkeSetMouseEnabled(m_wkeWebView, enable);
	}
	void CMiniblink::SetNavigationToNewWindowEnable(bool enable) {
		::wkeSetNavigationToNewWindowEnable(m_wkeWebView, enable);
	}
	void CMiniblink::SetCspCheckEnable(bool enable) {
		::wkeSetCspCheckEnable(m_wkeWebView, enable);
	}
	void CMiniblink::SetNpapiPluginsEnabled(bool enable) {
		::wkeSetNpapiPluginsEnabled(m_wkeWebView, enable);
	}
	void CMiniblink::SetHeadlessEnabled(bool enable) {
		::wkeSetHeadlessEnabled(m_wkeWebView, enable);
	}
	void CMiniblink::SetDebugConfig(const char* key, const char* value) {
		::wkeSetDebugConfig(m_wkeWebView, key, key);
	}
	void CMiniblink::SetTransparent(bool enable) {
		::wkeSetTransparent(m_wkeWebView, enable);
	}
	void CMiniblink::SetUserAgent(LPCWSTR userAgent) {
	#ifdef UNICODE
		return ::wkeSetUserAgentW(m_wkeWebView, userAgent);
	#else
		return ::wkeSetUserAgent(m_wkeWebView, userAgent);
	#endif // UNICODE
	}

	const char * CMiniblink::GetUserAgent() const
	{
		return ::wkeGetUserAgent(m_wkeWebView);
	}

	void CMiniblink::SetHtml(const char * html, const char * baseUrl)
	{
		if (baseUrl == NULL) {
			::wkeLoadHTML(m_wkeWebView, html);
		}
		else {
			::wkeLoadHtmlWithBaseUrl(m_wkeWebView, html, baseUrl);
		}
	}

	void CMiniblink::RunJs(const char* js)
	{
		JS_ARG arg;
		arg.frameId = NULL;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_UNDEFINED;
		arg.webView = m_wkeWebView;
		::SendMessage(GetHWND(), GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CMiniblink::RunJs(const char* js, int * result)
	{
		JS_ARG arg;
		arg.frameId = NULL;
		arg.js = js;
		arg.result = result;
		arg.type = JS_RESULT_TYPE::JS_INT;
		arg.webView = m_wkeWebView;
		::SendMessage(GetHWND(), GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CMiniblink::RunJs(const char* js, double * result)
	{
		JS_ARG arg;
		arg.frameId = NULL;
		arg.js = js;
		arg.result = result;
		arg.type = JS_RESULT_TYPE::JS_DOUBLE;
		arg.webView = m_wkeWebView;
		::SendMessage(GetHWND(), GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CMiniblink::RunJs(const char* js, bool * result)
	{
		JS_ARG arg;
		arg.frameId = NULL;
		arg.js = js;
		arg.result = result;
		arg.type = JS_RESULT_TYPE::JS_BOOL;
		arg.webView = m_wkeWebView;
		::SendMessage(GetHWND(), GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CMiniblink::RunJs(const char* js, char * result, int len)
	{
		JS_ARG arg;
		arg.frameId = NULL;
		arg.js = js;
		arg.result = result;
		arg.type = JS_RESULT_TYPE::JS_CHAR;
		arg.param = &len;
		arg.webView = m_wkeWebView;
		::SendMessage(GetHWND(), GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CMiniblink::RunJs(const char* js, wchar_t * result, int len)
	{
		JS_ARG arg;
		arg.frameId = NULL;
		arg.js = js;
		arg.result = result;
		arg.type = JS_RESULT_TYPE::JS_WCHAR;
		arg.param = &len;
		arg.webView = m_wkeWebView;
		::SendMessage(GetHWND(), GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CMiniblink::SetSize(int width, int height)
	{
		::wkeResize(m_wkeWebView, width, height);
	}

	void CMiniblink::SetDeviceParameter(const char * device, const char * paramStr, int paramInt, float paramFloat)
	{
		::wkeSetDeviceParameter(m_wkeWebView, device, paramStr, paramInt, paramFloat);
	}

	void CMiniblink::SetSettings(const wkeViewSettings * settings)
	{
		::wkeSetViewSettings(m_wkeWebView, settings);
	}

	void CMiniblink::SimulationAction(MB_ACTION_ITEM * action)
	{
		if (action == NULL || action->data == NULL) return;
		if (action->async) {
			::PostMessage(GetHWND(), GetActionMessageId(), (WPARAM)m_wkeWebView, (LPARAM)action);
		}
		else
		{
			::SendMessage(GetHWND(), GetActionMessageId(), (WPARAM)m_wkeWebView, (LPARAM)action);
		}

	}
	void CMiniblink::SetFullScreen() {
		static bool fullScreen = false;
		static LONG oldStyle = 0;
		static RECT oldRect = {0};

		if (!fullScreen) {
			GetWindowRect(GetHWND(), &oldRect);
			oldStyle = GetWindowLong(GetHWND(), GWL_STYLE);

			int frameWidth =  GetSystemMetrics(SM_CXFRAME);
			int frameHeight = GetSystemMetrics(SM_CYFRAME);
			int captionHeight = GetSystemMetrics(SM_CYCAPTION);
			int menuHeight = GetSystemMetrics(SM_CYMENU);
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);

			SetWindowLong(GetHWND(), GWL_STYLE, WS_POPUP & ~(WS_CAPTION | WS_THICKFRAME));
			SetWindowPos(GetHWND(), HWND_TOPMOST, 0, -menuHeight, screenWidth, screenHeight + menuHeight, SWP_SHOWWINDOW);
		} else {
			SetWindowLong(GetHWND(), GWL_STYLE, oldStyle);
			SetWindowPos(GetHWND(), HWND_NOTOPMOST, oldRect.left, oldRect.top, oldRect.right - oldRect.left, oldRect.bottom - oldRect.top, SWP_SHOWWINDOW);
		}
		
		fullScreen = !fullScreen;
	}
	void CMiniblink::convertFilename(wchar_t* filename) {
		int i;
		for (i = 0; filename[i]; ++i) {
			if (filename[i] == L'\\'
				|| filename[i] == L'/'
				|| filename[i] == L':'
				|| filename[i] == L'*'
				|| filename[i] == L'?'
				|| filename[i] == L'\"'
				|| filename[i] == L'<'
				|| filename[i] == L'>'
				|| filename[i] == L'|') {
				filename[i] = L'_';
			}
		}
	}
	void CMiniblink::saveBitmap(void* pixels, int w, int h, const wchar_t* title) {
		BITMAPFILEHEADER fileHdr = { 0 };
		BITMAPINFOHEADER infoHdr = { 0 };
		FILE * fp = NULL;

		fileHdr.bfType = 0x4d42; //'BM'
		fileHdr.bfOffBits = sizeof(fileHdr) + sizeof(infoHdr);
		fileHdr.bfSize = w * h * 4 + fileHdr.bfOffBits;

		infoHdr.biSize = sizeof(BITMAPINFOHEADER);
		infoHdr.biWidth = w;
		infoHdr.biHeight = -h;
		infoHdr.biPlanes = 1;
		infoHdr.biBitCount = 32;
		infoHdr.biCompression = 0;
		infoHdr.biSizeImage = w * h * 4;
		infoHdr.biXPelsPerMeter = 3780;
		infoHdr.biYPelsPerMeter = 3780;

		struct tm t;
		time_t utc_time;
		time(&utc_time);
		localtime_s(&t, &utc_time);

		wchar_t name[1024];
		swprintf(name, 1024, L"%s_%4d%02d%02d_%02d%02d%02d.bmp", title,
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

		convertFilename(name);

		wchar_t pathname[1024];
		swprintf(pathname, 1024, L"screenshots\\%s", name);
		_wmkdir(L"screenshots");
		_wfopen_s(&fp, pathname, L"wb");
		if (fp == NULL)
			return;

		fwrite(&fileHdr, sizeof(fileHdr), 1, fp);
		fwrite(&infoHdr, sizeof(infoHdr), 1, fp);
		fwrite(pixels, infoHdr.biSizeImage, 1, fp);
		fclose(fp);
	}

	void CMiniblink::takeScreenshot() {
		if (m_wkeWebView == NULL)
			return;

		wkeRunJS(m_wkeWebView, "document.body.style.overflow='hidden'");
		int w = wkeGetContentWidth(m_wkeWebView);
		int h = wkeGetContentHeight(m_wkeWebView);

		int oldwidth = wkeGetWidth(m_wkeWebView);
		int oldheight = wkeGetHeight(m_wkeWebView);
		wkeResize(m_wkeWebView, w, h);
		wkeUpdate();

		void* pixels = malloc(w * h * 4);
		wkePaint(m_wkeWebView, pixels, 0);

		//save bitmap
		saveBitmap(pixels, w, h, wkeGetTitleW(m_wkeWebView));

		free(pixels);

		wkeResize(m_wkeWebView, oldwidth, oldheight);
		wkeRunJS(m_wkeWebView, "document.body.style.overflow='visible'");
	}
	void CMiniblink::FlushCookie()
	{
		::wkePerformCookieCommand(m_wkeWebView, wkeCookieCommand::wkeCookieCommandFlushCookiesToFile);
	}

	void CMiniblink::SetWkeDllPath(LPCTSTR dllPath)
	{
		::wkeSetWkeDllPath(dllPath);
	}
	void CMiniblink::SetGlobalProxy(const wkeProxy* proxy) {
		::wkeSetProxy(proxy);
	}

	UINT CMiniblink::GetRunJsMessageId() {
		static UINT id = ::RegisterWindowMessage(L"MB_JS_EXECUTE");
		return id;
	}

	UINT CMiniblink::GetActionMessageId()
	{
		static UINT id = ::RegisterWindowMessage(L"MB_ACTION_EXECUTE");
		return id;
	}

	/*回调函数*/
	void CMiniblink::MBPaintUpdate(wkeWebView webView, void* param, const HDC hdc, int x, int y, int cx, int cy) {
		CMiniblink *cmb = (CMiniblink *)param;
		//cmb->Invalidate();
	}

    // 使用默认浏览器打开url
    jsValue WKE_CALL_TYPE CMiniblink::urlCallback(jsExecState es, void *param) {
        assert(param != nullptr);

        jsValue arg0 = jsArg(es, 0);
        const utf8* url = jsToTempString(es, arg0);
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);

        return jsUndefined();
    }

    // JS 调用 C++ 原始函数
    jsValue WKE_CALL_TYPE CMiniblink::onMsg(jsExecState es, void* param) {
        assert(param != nullptr);
        CMiniblink* app = static_cast<CMiniblink *>(param);
        HWND hwnd = app->GetHWND();

        int argCount = jsArgCount(es);
        if (argCount < 1)
            return jsUndefined();

        jsType type = jsArgType(es, 0);
        if (JSTYPE_STRING != type)
            return jsUndefined();

        jsValue arg0 = jsArg(es, 0);
        std::string msgOutput = "eMsg:";
        std::string msg = jsToTempString(es, arg0);
        msgOutput = msgOutput + msg;
        msgOutput += "\n";
        OutputDebugStringA(msgOutput.c_str());

        if ("close" == msg) {
            ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
        } else if ("max" == msg) {
            static bool isMax = true;
            if (isMax)
                ::PostMessageW(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
            else
                ::PostMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
            isMax = !isMax;
        } else if ("min" == msg) {
            ::PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        }

        return jsUndefined();
    }

	void WKE_CALL_TYPE CMiniblink::MBTitleChanged(wkeWebView webView, void * param, const wkeString title)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
	#ifdef UNICODE
		cmb->OnTitleChanged(::wkeGetStringW(title));
	#else
		cmb->OnTitleChanged(::wkeGetString(title));
	#endif // UNICODE
	}

	void WKE_CALL_TYPE CMiniblink::MBUrlChanged(wkeWebView webView, void * param, wkeWebFrameHandle frameId, const wkeString url)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
		CWebFrame frame(frameId, cmb->GetWebView(),cmb->GetHWND());
	#ifdef UNICODE
		cmb->OnUrlChanged(::wkeGetStringW(url), &frame);
	#else
		cmb->OnUrlChanged(::wkeGetString(url), &frame);
	#endif // UNICODE
	}

	void WKE_CALL_TYPE CMiniblink::MBAlert(wkeWebView webView, void * param, const wkeString msg)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
	#ifdef UNICODE
		cmb->OnAlert(::wkeGetStringW(msg));
	#else
		cmb->OnAlert(::wkeGetString(msg));
	#endif // UNICODE
	}

	bool WKE_CALL_TYPE CMiniblink::MBConfirm(wkeWebView webView, void * param, const wkeString msg)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return false;
	#ifdef UNICODE
		return cmb->OnConfirm(::wkeGetStringW(msg));
	#else
		return cmb->OnConfirm(::wkeGetString(msg));
	#endif // UNICODE
	}

	bool WKE_CALL_TYPE CMiniblink::MBPromptBox(wkeWebView webView, void * param, const wkeString msg, const wkeString defaultResult, wkeString result)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return false;
	#ifdef UNICODE
		return cmb->OnPrompt(::wkeGetStringW(msg), ::wkeGetStringW(defaultResult), ::wkeGetStringW(result));
	#else
		return cmb->OnPrompt(::wkeGetString(msg), ::wkeGetString(defaultResult), ::wkeGetString(result));
	#endif // UNICODE
	}

	bool WKE_CALL_TYPE CMiniblink::MBNavigation(wkeWebView webView, void * param, wkeNavigationType navigationType, const wkeString url)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return false;
	#ifdef UNICODE
		return cmb->OnNavigation(navigationType, ::wkeGetStringW(url));
	#else
		return cmb->OnNavigation(navigationType, ::wkeGetString(url));
	#endif // UNICODE
	}

	wkeWebView WKE_CALL_TYPE CMiniblink::MBCreateView(wkeWebView webView, void * param, wkeNavigationType navigationType, const wkeString url, const wkeWindowFeatures * windowFeatures)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return NULL;
	#ifdef UNICODE
		return cmb->OnNewOpen(navigationType, ::wkeGetStringW(url), windowFeatures)->GetWebView();
	#else
		return cmb->OnNewOpen(navigationType, ::wkeGetString(url), windowFeatures)->GetWebView();
	#endif // UNICODE
	}

	void WKE_CALL_TYPE CMiniblink::MBDocumentReady(wkeWebView webView, void * param, wkeWebFrameHandle frameId)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
		CWebFrame frame(frameId, cmb->GetWebView(), cmb->GetHWND());
		return cmb->OnDocumentReady(&frame);
	}

	bool WKE_CALL_TYPE CMiniblink::MBDownload(wkeWebView webView, void * param, const char * url)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return false;
		return cmb->OnDownload(url);
	}
	wkeDownloadOpt WKE_CALL_TYPE CMiniblink::MBDownload2(wkeWebView webView, void* param, size_t expectedContentLength, const char* url, const char* mime, const char* disposition, wkeNetJob job, wkeNetJobDataBind* dataBind)
	{
		static wkeNetJobDataBind s_dataBind;
		s_dataBind.recvCallback = onDataRecv;
		s_dataBind.finishCallback = onDataFinish;
		dataBind = &s_dataBind;
		return wkeDownloadOpt::kWkeDownloadOptCacheData;
	}
	bool WKE_CALL_TYPE CMiniblink::MBWindowClosing(wkeWebView, void *param) {
		CMiniblink *cmb = (CMiniblink *)param;

		return cmb->OnClosing();
	}
	void WKE_CALL_TYPE CMiniblink::MBWindowDestroy(wkeWebView, void *param) {
		CMiniblink *cmb = (CMiniblink *)param;

		if (!cmb -> m_released) {
			cmb->~CMiniblink();
		}
	}
	bool WKE_CALL_TYPE CMiniblink::MBNetResponse(wkeWebView webView, void * param, const char * url, wkeNetJob job) {
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return false;
		CNetJob _job(job);
		return cmb->OnResponse(url, &_job);
	}

	void WKE_CALL_TYPE CMiniblink::MBConsole(wkeWebView webView, void * param, wkeConsoleLevel level, const wkeString message, const wkeString sourceName, unsigned sourceLine, const wkeString stackTrace)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
	#ifdef UNICODE
		return cmb->OnConsole(level, ::wkeGetStringW(message), ::wkeGetStringW(sourceName), sourceLine, ::wkeGetStringW(stackTrace));
	#else
		return cmb->OnConsole(level, ::wkeGetString(message), ::wkeGetString(sourceName), sourceLine, ::wkeGetString(stackTrace));
	#endif // UNICODE
	}

	bool WKE_CALL_TYPE CMiniblink::MBLoadUrlBegin(wkeWebView webView, void * param, const char * url, wkeNetJob job)
	{

		// const char kPreHead[] = "http://hook.test/";
		// const char* pos = strstr(url, kPreHead);
		// if (pos) {
		// 	const utf8* decodeURL = wkeUtilDecodeURLEscape(url);
		// 	if (!decodeURL)
		// 		return false;
		// 	std::string urlString(decodeURL);
		// 	std::string localPath = urlString.substr(sizeof(kPreHead) - 1);

		// 	std::wstring path = getResourcesPath(utf8ToUtf16(localPath));
		// 	std::vector<char> buffer;

		// 	readJsFile(path.c_str(), &buffer);

		// 	wkeNetSetData(job, buffer.data(), buffer.size());

		// 	return true;
		// } else if (strncmp(url, "http://localhost:12222", 22) == 0) {
		// 	wkeNetSetMIMEType(job, (char*)"text/html");
		// 	wkeNetChangeRequestUrl(job, url);
		// 	wkeNetSetData(job, (char*)"\"test1111\"", 10);
		// 	return true;
		// } else if (strcmp(url, "http://www.baidu.com/") == 0) {
		// 	wkeNetHookRequest(job);
		// }
		
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return false;
		CNetJob _job(job);
		
		return cmb->OnRequestBegin(url, &_job);
	}

	void WKE_CALL_TYPE CMiniblink::MBLoadUrlEnd(wkeWebView webView, void * param, const char * url, wkeNetJob job, void * buf, int len)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
		CNetJob _job(job);
		return cmb->OnRequestEnd(url, &_job, buf, len);
	}

	void WKE_CALL_TYPE CMiniblink::MBCreateScriptContext(wkeWebView webView, void * param, wkeWebFrameHandle frameId, void * context, int extensionGroup, int worldId)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
		CWebFrame frame(frameId, cmb->GetWebView(), cmb->GetHWND());
		return cmb->OnCreateScriptContext(&frame, context, extensionGroup, worldId);
	}

	void WKE_CALL_TYPE CMiniblink::MBReleaseScriptContext(wkeWebView webView, void * param, wkeWebFrameHandle frameId, void * context, int worldId)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
		CWebFrame frame(frameId, cmb->GetWebView(), cmb->GetHWND());
		return cmb->OnReleaseScriptContext(&frame, context, worldId);
	}

	void WKE_CALL_TYPE CMiniblink::MBMediaLoad(wkeWebView webView, void * param, const char * url, wkeMediaLoadInfo * info)

	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
		return cmb->OnMediaLoad(url, info);
	}
	void WKE_CALL_TYPE CMiniblink::MBLoadedFinish(wkeWebView webView, void* param, const wkeString url, wkeLoadingResult result, const wkeString failedReason)
	{
		CMiniblink *cmb = (CMiniblink *)param;
		if (cmb == NULL || cmb->m_released) return;
	#ifdef UNICODE
		return cmb->OnLoadingFinish(::wkeGetStringW(url), result, ::wkeGetStringW(failedReason));
	#else
		return cmb->OnLoadingFinish(::wkeGetString(url), result, ::wkeGetString(failedReason));
	#endif // UNICODE

	}
	void CMiniblink::updateCursor()
	{
		int cursorInfo = wkeGetCursorInfoType(m_wkeWebView);
		if (m_cursor != cursorInfo) {
			m_cursor = cursorInfo;
			HCURSOR curosr = ::LoadCursor(NULL, IDC_ARROW);
			switch (cursorInfo)
			{
			case WkeCursorInfoPointer:
				curosr = ::LoadCursor(NULL, IDC_ARROW);
				break;
			case WkeCursorInfoCross:
				curosr = ::LoadCursor(NULL, IDC_CROSS);
				break;
			case WkeCursorInfoHand:
				curosr = ::LoadCursor(NULL, IDC_HAND);
				break;
			case WkeCursorInfoIBeam:
				curosr = ::LoadCursor(NULL, IDC_IBEAM);
				break;
			case WkeCursorInfoWait:
				curosr = ::LoadCursor(NULL, IDC_WAIT);
				break;
			case WkeCursorInfoHelp:
				curosr = ::LoadCursor(NULL, IDC_HELP);
				break;
			case WkeCursorInfoEastResize:
				curosr = ::LoadCursor(NULL, IDC_SIZEWE);
				break;
			case WkeCursorInfoNorthResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENS);
				break;
			case WkeCursorInfoNorthEastResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENESW);
				break;
			case WkeCursorInfoNorthWestResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENWSE);
				break;
			case WkeCursorInfoSouthResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENS);
				break;
			case WkeCursorInfoSouthEastResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENWSE);
				break;
			case WkeCursorInfoSouthWestResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENESW);
				break;
			case WkeCursorInfoWestResize:
				curosr = ::LoadCursor(NULL, IDC_SIZEWE);
				break;
			case WkeCursorInfoNorthSouthResize:
				curosr = ::LoadCursor(NULL, IDC_SIZENS);
				break;
			case WkeCursorInfoEastWestResize:
				curosr = ::LoadCursor(NULL, IDC_SIZEWE);
				break;
			case WkeCursorInfoNorthEastSouthWestResize:
				curosr = ::LoadCursor(NULL, IDC_SIZEALL);
				break;
			case WkeCursorInfoNorthWestSouthEastResize:
				curosr = ::LoadCursor(NULL, IDC_SIZEALL);
				break;
			case WkeCursorInfoColumnResize:
			case WkeCursorInfoRowResize:
				curosr = ::LoadCursor(NULL, IDC_ARROW);
				break;
			case WkeCursorInfoMove:
				curosr = ::LoadCursor(NULL, IDC_SIZEALL);
				break;
			case WkeCursorInfoNotAllowed:
				curosr = ::LoadCursor(NULL, IDC_NO);
				break;
			case WkeCursorInfoGrab:
				curosr = ::LoadCursor(NULL, IDC_HAND);
				break;
			default:
				break;
			}
			::SetCursor(curosr);
		}
	}

	CWebFrame::CWebFrame(wkeWebFrameHandle frameId, wkeWebView webview,HWND hwnd) :m_webFrame(frameId), m_web(webview),m_hwnd(hwnd)
	{
	}

	bool CWebFrame::IsMainFrame()
	{
		return ::wkeIsMainFrame(m_web, m_webFrame);
	}

	const char * CWebFrame::GetUrl() const
	{
		return ::wkeGetFrameUrl(m_web, m_webFrame);
	}

	void CWebFrame::RunJs(const char * js)
	{
		JS_ARG arg;
		arg.frameId = m_webFrame;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_UNDEFINED;
		arg.webView = m_web;
		::SendMessage(m_hwnd, CMiniblink::GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CWebFrame::RunJs(const char * js, int * result)
	{
		JS_ARG arg;
		arg.frameId = m_webFrame;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_INT;
		arg.webView = m_web;
		::SendMessage(m_hwnd, CMiniblink::GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CWebFrame::RunJs(const char * js, double * result)
	{
		JS_ARG arg;
		arg.frameId = m_webFrame;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_DOUBLE;
		arg.webView = m_web;
		::SendMessage(m_hwnd, CMiniblink::GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CWebFrame::RunJs(const char * js, bool * result)
	{
		JS_ARG arg;
		arg.frameId = m_webFrame;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_BOOL;
		arg.webView = m_web;
		::SendMessage(m_hwnd, CMiniblink::GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CWebFrame::RunJs(const char * js, char * result, int len)
	{
		JS_ARG arg;
		arg.frameId = m_webFrame;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_CHAR;
		arg.webView = m_web;
		arg.param = &len;
		::SendMessage(m_hwnd, CMiniblink::GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CWebFrame::RunJs(const char * js, wchar_t * result, int len)
	{
		JS_ARG arg;
		arg.frameId = m_webFrame;
		arg.js = js;
		arg.type = JS_RESULT_TYPE::JS_WCHAR;
		arg.webView = m_web;
		arg.param = &len;
		::SendMessage(m_hwnd, CMiniblink::GetRunJsMessageId(), (WPARAM)&arg, NULL);
	}

	void CNetJob::HookRequest()
	{
		::wkeNetHookRequest(m_job);
	}

	void CMiniblink::call(const char *name) {
		if (!m_wkeWebView) {
			return;
		}
		jsExecState es = wkeGlobalExec(m_wkeWebView);
		jsValue app = jsGetGlobal(es, "app");
		jsValue func = jsGet(es, app, name);
		jsCall(es, func, app, nullptr, 0);
	}

	jsValue to_js_value(jsExecState, bool value) {
		return jsBoolean(value);
	}

	jsValue to_js_value(jsExecState, int value) {
		return jsInt(value);
	}

	jsValue to_js_value(jsExecState, double value) {
		return jsDouble(value);
	}

	jsValue to_js_value(jsExecState es, const char *value) {
		return jsString(es, value);
	}

	jsValue to_js_value(jsExecState es, const wchar_t *value) {
		return jsStringW(es, value);
	}

	jsValue to_js_value(jsExecState es, const std::string &value) {
		return jsString(es, value.c_str());
	}

	jsValue to_js_value(jsExecState es, const std::wstring &value) {
		return jsStringW(es, value.c_str());
	}
}