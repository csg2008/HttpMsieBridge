// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

// Complete documentation on programming IE interfaces:
// http://msdn.microsoft.com/en-us/library/ie/aa752038(v=vs.85).aspx

// WebBrowser customization:
// http://msdn.microsoft.com/en-us/library/aa770041(v=vs.85).aspx

// WebBrowser example C code:
// http://www.codeproject.com/Articles/3365/Embed-an-HTML-control
// Download source zip, see dll/dll.c

// C# WebBrowser control:
// http://code.google.com/p/csexwb2/source/browse/trunk/cEXWB.cs

// Embedding ActiveX (IOle interfaces):
// http://cpansearch.perl.org/src/GMPASSOS/Wx-ActiveX-0.05/activex/wxactivex.cpp

// C++ strings (variant, bstr, others):
// http://www.codeproject.com/Articles/3004/The-Complete-Guide-to-C-Strings

#include "../defines.h"
#include "com_utils.h"
#include "browser_window.h"

#include <map>
#include <string>
#include <wininet.h>

#include "misc/logger.h"
#include "misc/debug.h"
#include "../settings.h"
#include "misc/string_utils.h"
#include "misc/file_utils.h"
#include "msie.h"

#define EXTERNAL_CLOSE_WINDOW           1

#define WEBPAGE_BACK                    0
#define WEBPAGE_FORWARD                 1
#define WEBPAGE_HOME                    2
#define WEBPAGE_SEARCH                  3
#define WEBPAGE_REFRESH                 4
#define WEBPAGE_STOP                    5

// The following macro can be found in IE8 SDK
#ifndef INTERNET_COOKIE_NON_SCRIPT
#define INTERNET_COOKIE_NON_SCRIPT      0x00001000
#endif

#ifndef INTERNET_COOKIE_HTTPONLY
#define INTERNET_COOKIE_HTTPONLY        0x00002000
#endif

#pragma comment(lib, "Wininet.lib")

std::map<std::string, BrowserWindow*> g_browserWindows;

std::map<std::string, std::string> GetBrowserList() {
    std::map<std::string, std::string> list;
    std::map<std::string, BrowserWindow*>::iterator iter;
    for(iter = g_browserWindows.begin(); iter != g_browserWindows.end(); iter++) {
        list[iter->first] = iter -> second -> GetOrgUrl();
    }

    return list;
}
BrowserWindow* GetBrowserWindow(std::string hwnd) {
    std::map<std::string, BrowserWindow*>::iterator it;
    it = g_browserWindows.find(hwnd);
    if (it != g_browserWindows.end()) {
        return it->second;
    }

    // GetBrowserWindow() may fail during window creation, so log
    // severity is only DEBUG.
    FLOG_DEBUG << "GetBrowserWindow(): not found, hwnd = " << hwnd;
    return NULL;
}
void StoreBrowserWindow(std::string hwnd, BrowserWindow* browser) {
    FLOG_DEBUG << "StoreBrowserWindow(): hwnd = " << hwnd;
    std::map<std::string, BrowserWindow*>::iterator it;
    it = g_browserWindows.find(hwnd);
    if (it == g_browserWindows.end()) {
        g_browserWindows[hwnd] = browser;
    } else {
        FLOG_WARNING << "StoreBrowserWindow() failed: already stored";
    }
}
void RemoveBrowserWindow(std::string hwnd) {
    FLOG_DEBUG << "RemoveBrowserWindow(): hwnd = " << hwnd;
    std::map<std::string, BrowserWindow*>::iterator it;
    it = g_browserWindows.find(hwnd);
    if (it != g_browserWindows.end()) {
        BrowserWindow* browser = it->second;
        g_browserWindows.erase(it);
        delete browser;
    } else {
        FLOG_WARNING << "RemoveBrowserWindow() failed: not found";
    }
}
void PCStrToPPostData(LPCTSTR postData, VARIANT* pvNewPostData) {
    int ipdSize = WideCharToMultiByte(CP_ACP, 0, postData, -1, 0, 0, 0, 0);
    // int ipdSize = strlen(ppcPostData) ;
    if(ipdSize>0) {
        char* pcPostData = new char[ipdSize + 1];
        // strcpy(pcPostData,ppcPostData) ;
        WideCharToMultiByte(CP_ACP, 0, postData, -1, pcPostData, ipdSize, 0, 0);
        SAFEARRAY FAR* psaPostData = NULL;
        SAFEARRAYBOUND saBound;
        saBound.cElements = (ULONG) (strlen(pcPostData));
        saBound.lLbound = 0;
        psaPostData = SafeArrayCreate(VT_UI1, 1, &saBound);
        char* pChar = pcPostData ;
        for (long lIndex = 0; lIndex < (signed)saBound.cElements; lIndex++) {
            SafeArrayPutElement(psaPostData, &lIndex, (void*)((pChar++)));
        }
        (*pvNewPostData).vt = VT_ARRAY | VT_UI1;
        (*pvNewPostData).parray = psaPostData ;
        // SafeArrayDestroy(psaPostData) ;
        delete[] pcPostData;
        pcPostData = NULL;
        pChar = NULL;
    }
}

BrowserWindow::BrowserWindow(HWND inWindowHandle, std::string url)
        : windowHandle_(inWindowHandle),
          parentHandle_(0),
          oleClientSite_(), // initialized in constructor
          externalDispatch_(), // initialized in constructor
          clickEvents_(), // initialized in constructor
          clickEventsAttached_(false),
          documentUniqueID_(),
          clickDispatch_(),
          // allowedUrl_ (initialized in constructor)
          // webBrowser2_
          pHtmlWnd2_(NULL),
          dWebBrowserEvents2Cookie_(0),
          isResizing_(false),
          focusedAfterCreation_(false) {

    // Passing "this" in member initialization throws a warning,
    // the solution is to initialize these classes in constructor.
    oleClientSite_.reset(new OleClientSite(this));
    externalDispatch_.reset(new ExternalDispatch(this));
    clickEvents_.reset(new ClickEvents(this));

    _ASSERT(oleClientSite_);
    _ASSERT(externalDispatch_);
    _ASSERT(clickEvents_);

    str_hwnd = std::to_string((long long) windowHandle_);
    parentHandle_ = GetParentWindow(windowHandle_);
    FLOG_DEBUG << "BrowserWindow(): parentHandle = " << parentHandle_;

    clickDispatch_.vt = VT_DISPATCH;
    clickDispatch_.pdispVal = static_cast<IDispatch*>(clickEvents_.get());
    wcsncpy_s(allowedUrl_, _countof(allowedUrl_), L"nohttp", _TRUNCATE);

    if ("" == url) {
        url = "about:blank";
    }

    _ASSERT(windowHandle_);
    SetTitleFromSettings();
    SetIconFromSettings();
    SetAllowedUrl(Utf8ToWide(url).c_str());
    if (IsPopup()) {
        if (!CreateBrowserControl("")) {
            FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed";
            _ASSERT(false);
            SendMessage(windowHandle_, WM_CLOSE, 0, 0);
            return;
        }
    } else {
        if (!CreateBrowserControl(url)) {
            FatalError(windowHandle_, "Could not create Browser control.\n"
                    "Exiting application.");
        }
    }
}
BrowserWindow::~BrowserWindow() {
    CloseBrowserControl();
}
bool BrowserWindow::CreateBrowserControl(std::string url) {
    // navigateUrl might be NULL, if so Navigate2() won't be called.
    HRESULT hr;
    BOOL b;
    nlohmann::json* settings = GetApplicationSettings();

    // Create browser control.
    hr = CoCreateInstance(CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, IID_IOleObject, (void**)&oleObject_);
    if (FAILED(hr) || !oleObject_) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: CoCreateInstance(CLSID_WebBrowser) failed";
        _ASSERT(false);
        return false;
    }

    hr = oleObject_->SetClientSite(oleClientSite_.get());
    if (FAILED(hr)) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: SetClientSite() failed";
        _ASSERT(false);
        return false;
    }

    RECT rect;
    b = GetClientRect(windowHandle_, &rect);
    if (!b) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: GetClientRect() failed";
        _ASSERT(false);
        return false;
    }

    hr = oleObject_->SetHostNames(CLASS_NAME_MSIE_EX, 0);
    if (FAILED(hr)) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: IOleObject->SetHostNames() failed";
        _ASSERT(false);
        return false;
    }

    hr = OleSetContainedObject(static_cast<IUnknown*>(oleObject_), TRUE);
    if (FAILED(hr)) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: OleSetContaintedObject() failed";
        _ASSERT(false);
        return false;
    }

    hr = oleObject_->DoVerb(OLEIVERB_SHOW, NULL, static_cast<IOleClientSite*>(oleClientSite_.get()), 0, windowHandle_, &rect);
    if (FAILED(hr)) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: DoVerb(OLEIVERB_INPLACEACTIVATE) failed";
        _ASSERT(false);
        return false;
    }

    hr = oleObject_->QueryInterface(IID_IOleInPlaceActiveObject, (void**)&oleInPlaceActiveObject_);
    if (FAILED(hr) || !oleInPlaceActiveObject_) {
        FLOG_DEBUG << "BrowserWindow::TranslateAccelerator() failed: IOleObject->QueryInterface(IOleInPlaceActiveObject) failed";
        return false;
    }

    oleObject_->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser2_);
    if (FAILED(hr) || !webBrowser2_) {
        FLOG_ERROR << "BrowserWindow::CreateBrowserControl() failed: QueryInterface(IID_IWebBrowser2) failed";
        _ASSERT(false);
        return false;
    }

    bool hide_dialog_boxes = (*settings)["msie"]["hide_dialog_boxes"];
    if (hide_dialog_boxes) {
        hr = webBrowser2_->put_Silent(VARIANT_TRUE);
        if (FAILED(hr)) {
            FLOG_WARNING << "Hiding dialog boxes failed";
        }
    }

    // Position display area.
    webBrowser2_->put_Left(0);
    webBrowser2_->put_Top(0);
    webBrowser2_->put_Width(rect.right);
    webBrowser2_->put_Height(rect.bottom);

    // Do not allow displaying files dragged into the window.
    hr = webBrowser2_->put_RegisterAsDropTarget(VARIANT_FALSE);
    if (FAILED(hr)) {
        FLOG_WARNING << "BrowserWindow::CreateBrowserControl(): put_RegisterAsDropTarget(False) failed";
    }

    hr = webBrowser2_->put_RegisterAsBrowser(VARIANT_FALSE);
    _ASSERT(SUCCEEDED(hr));

    // AdviseEvent() takes care of logging errors.
    AdviseEvent(webBrowser2_, DIID_DWebBrowserEvents2, &dWebBrowserEvents2Cookie_);

    // Initial navigation.
    if (!url.empty() && !Navigate(false, url)) {
        return false;
    }

    return true;
}

bool BrowserWindow::Navigate(bool isPost, std::string url, std::string header, const std::string& data) {
    if (!webBrowser2_)
        return false;
    if (url.empty()) {
        FLOG_ERROR << "BrowserWindow::Navigate() failed: url is empty";
        _ASSERT(false);
        return false;
    }
    if(isPost && header.empty()) {
        header = "Content-Type: application/x-www-form-urlencoded\r\n";
    }

    HRESULT hr;
    bool ret = true;

    VARIANT vURL;
    VARIANT vFlags;
    VARIANT vPostData;
    VARIANT vHeaders;
    VARIANT vNull;

    // Init
    VariantInit(&vURL);
    VariantInit(&vFlags);
    VariantInit(&vPostData);
    VariantInit(&vHeaders);
    VariantInit(&vNull);

    // Set value
    vNull.vt = VT_BSTR;
    vNull.bstrVal = nullptr;

    vURL.vt = VT_BSTR;
    vURL.bstrVal = SysAllocString(Utf8ToWide(url).c_str());

    vFlags.vt = VT_I4;
    vFlags.lVal = navNoHistory | navNoReadFromCache | navNoWriteToCache;

    vHeaders.vt = VT_BSTR;
    vHeaders.bstrVal = SysAllocString(Utf8ToWide(header).c_str());

    if (isPost) {
        PCStrToPPostData(Utf8ToWide(data).c_str(), &vPostData);

        hr = webBrowser2_->Navigate2(&vURL, &vFlags, &vNull, &vPostData, &vHeaders);
    } else {
        hr = webBrowser2_->Navigate2(&vURL, &vFlags, 0, 0, &vHeaders);
    }

    if (FAILED(hr)) {
        FLOG_ERROR << "BrowserWindow::Navigate() failed: IWebBrowser2->Navigate() failed";
        _ASSERT(false);
        ret = false;
    }

    // Clear
    VariantClear(&vURL);
    VariantClear(&vFlags);
    VariantClear(&vPostData);
    VariantClear(&vHeaders);
    VariantClear(&vNull);

    return ret;
}
void BrowserWindow::CloseWebBrowser2() {
    FLOG_DEBUG << "BrowserWindow::CloseWebBrowser2()";

    // This function may be called from window.external.CloseWindow()
    // or BrowserWindow::CloseBrowserControl().
    if (!webBrowser2_)
        return;
    HRESULT hr;

    UnadviseEvent(webBrowser2_, DIID_DWebBrowserEvents2, dWebBrowserEvents2Cookie_);
    // This is important, otherwise getting unhandled exceptions,
    // scenario: open popup, close it, navigate in main window,
    // exception! after DispatchMessage(), msg.message = 32770.
    DetachClickEvents();

    // Remember to check if webBrowser2_ is not empty before using it,
    // in other functions like TryAttachClickEvents().
    _ASSERT(webBrowser2_);
    VARIANT_BOOL   vbFlag;
    hr = webBrowser2_->get_Busy(&vbFlag);
    if (SUCCEEDED(hr) && VARIANT_TRUE == vbFlag) {
        hr = webBrowser2_->Stop();
        _ASSERT(SUCCEEDED(hr));
    }

    hr = webBrowser2_->put_Visible(VARIANT_FALSE);
    _ASSERT(SUCCEEDED(hr));

    // WebBrowser object (CLSID_WebBrowser) cannot call Quit(),
    // it is for Internet Explorer object (CLSID_InternetExplorer).
    // hr = webBrowser2_->Quit();
    // _ASSERT(SUCCEEDED(hr));

    // When called from window.external.CloseWindow() we must execute
    // OLECMDID_CLOSE before posting WM_CLOSE message, otherwise
    // access violation occurs.
    webBrowser2_->ExecWB(OLECMDID_CLOSE, OLECMDEXECOPT_DONTPROMPTUSER, 0, 0);

    webBrowser2_.Release();
}
void BrowserWindow::CloseBrowserControl() {
    FLOG_DEBUG << "BrowserWindow::CloseBrowserControl()";

    if (!webBrowser2_)
        return;
    HRESULT hr;

    CloseWebBrowser2();

    _ASSERT(oleInPlaceActiveObject_);
    oleInPlaceActiveObject_.Release();

    IOleInPlaceObjectPtr inPlaceObject;
    hr = oleObject_->QueryInterface(IID_IOleInPlaceObject, (void**)&inPlaceObject);
    _ASSERT(SUCCEEDED(hr));
    if (SUCCEEDED(hr) && inPlaceObject) {
        hr = inPlaceObject->InPlaceDeactivate();
        _ASSERT(SUCCEEDED(hr));
        // Assertion when using OLEIVERB_SHOW:
        // hr = inPlaceObject->UIDeactivate();
        // _ASSERT(SUCCEEDED(hr));
    }

    // It is important to set client site to NULL, otherwise
    // you will get first-chance exceptions when calling Close().
    _ASSERT(oleObject_);
    hr = oleObject_->DoVerb(OLEIVERB_HIDE, NULL, oleClientSite_.get(), 0, windowHandle_, NULL);
    _ASSERT(SUCCEEDED(hr));
    hr = oleObject_->Close(OLECLOSE_NOSAVE);
    _ASSERT(SUCCEEDED(hr));
    hr = OleSetContainedObject(static_cast<IUnknown*>(oleObject_), FALSE);
    _ASSERT(SUCCEEDED(hr));
    hr = oleObject_->SetClientSite(0);
    _ASSERT(SUCCEEDED(hr));
    hr = CoDisconnectObject(static_cast<IUnknown*>(oleObject_), 0);
    _ASSERT(SUCCEEDED(hr));
    oleObject_.Release();
}
bool BrowserWindow::DetachClickEvents() {
    if (!webBrowser2_)
        return false;
    IDispatchPtr dispatch;
    HRESULT hr = webBrowser2_->get_Document(&dispatch);
    // This may fail when window is loading.
    if (FAILED(hr) || !dispatch) {
        return false;
    }
    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::DetachClickEvents() failed: QueryInterface(IHTMLDocument2)";
        return false;
    }
    _variant_t emptyClickDispatch;
    emptyClickDispatch.vt = VT_DISPATCH;
    emptyClickDispatch.pdispVal = 0;
    // If vt == (VT_BYREF | VT_DISPATCH) then you use ppdispVal:
    // emptyClickDispatch.ppdispVal = 0;
    hr = htmlDocument2->put_onclick(emptyClickDispatch);
    if (FAILED(hr)) {
        FLOG_WARNING << "BrowserWindow::DetachClickEvents() failed: htmlDocument2->put_onclick() failed";
        return false;
    }
    return true;
}
bool BrowserWindow::TryAttachClickEvents() {
    // Attach OnClick event - to catch clicking any external
    // links. Returns whether succeeded to attach click events,
    // it is required for the DOM to be ready, call this
    // function in a timer until it succeeds.After browser
    // navigation these click events need to be re-attached.

    if (!webBrowser2_) {
        // Web-browser control might be closing.
        return false;
    }
    HRESULT hr;
    VARIANT_BOOL isBusy;
    hr = webBrowser2_->get_Busy(&isBusy);
    // This may fail when window is loading/unloading.
    if (FAILED(hr) || isBusy == VARIANT_TRUE) {
        return false;
    }
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    // This may fail when window is loading.
    if (FAILED(hr) || !dispatch) {
        return false;
    }
    IHTMLDocument3Ptr htmlDocument3;
    hr = dispatch->QueryInterface(IID_IHTMLDocument3, (void**)&htmlDocument3);
    if (FAILED(hr) || !htmlDocument3) {
        FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed, QueryInterface(IHTMLDocument3) failed";
        return false;
    }
    IHTMLElementPtr htmlElement;
    hr = htmlDocument3->get_documentElement(&htmlElement);
    if (FAILED(hr) || !htmlElement) {
        FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed, get_documentElement() failed";
        return false;
    }
    _bstr_t documentID;
    hr = htmlElement->get_id(&documentID.GetBSTR());
    if (FAILED(hr)) {
        FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed, htmlElement->get_id() failed";
        return false;
    }
    if (documentID.length() && documentID == documentUniqueID_) {
        return true;
    } else {
        // Document's identifier changed, browser navigated.
        this->clickEventsAttached_ = false;
        _bstr_t uniqueID;
        hr = htmlDocument3->get_uniqueID(&uniqueID.GetBSTR());
        if (FAILED(hr)) {
            FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed: htmlDocument3->get_uniqueID() failed";
            return false;
        }
        hr = htmlElement->put_id(uniqueID.GetBSTR());
        if (FAILED(hr)) {
            FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed: htmlElement->put_id() failed";
            return false;
        }
        documentUniqueID_.Assign(uniqueID.GetBSTR());
    }
    if (this->clickEventsAttached_) {
        return true;
    }
    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed: QueryInterface(IHTMLDocument2)";
        return false;
    }
    hr = htmlDocument2->put_onclick(clickDispatch_);
    if (FAILED(hr)) {
        FLOG_WARNING << "BrowserWindow::TryAttachClickEvents() failed: htmlDocument2->put_onclick() failed";
        return false;
    }
    this->clickEventsAttached_ = true;
    return true;
}
bool BrowserWindow::AdviseEvent(IWebBrowser2Ptr webBrowser2, REFIID riid, DWORD* adviseCookie) {
    IConnectionPointContainerPtr connectionPointContainer;
    HRESULT hr = webBrowser2->QueryInterface(IID_IConnectionPointContainer, (void**)&connectionPointContainer);
    if (FAILED(hr) || !connectionPointContainer) {
        FLOG_WARNING << "BrowserWindow::AdviseEvent() failed: QueryInterface(IConnectionPointContainer) failed";
        return false;
    }
    IConnectionPointPtr connectionPoint;
    hr = connectionPointContainer->FindConnectionPoint(riid, &connectionPoint);
    if (FAILED(hr) || !connectionPoint) {
        FLOG_WARNING << "BrowserWindow::AdviseEvent() failed: FindConnectionPoint() failed";
        return false;
    }
    IUnknownPtr unknown;
    hr = oleClientSite_.get()->QueryInterface(IID_IUnknown, (void**)&unknown);
    if (FAILED(hr) || !unknown) {
        FLOG_WARNING << "BrowserWindow::AdviseEvent() failed: QueryInterface(IUnknown) failed";
        return false;
    }
    hr = connectionPoint->Advise(unknown, adviseCookie);
    if (FAILED(hr) && !(*adviseCookie)) {
        FLOG_WARNING << "BrowserWindow::AdviseEvent() failed: connectionPoint->Advise() failed";
        return false;
    }
    return true;
}
bool BrowserWindow::UnadviseEvent(IWebBrowser2Ptr webBrowser2, REFIID riid, DWORD adviseCookie) {
    IConnectionPointContainerPtr connectionPointContainer;
    HRESULT hr = webBrowser2->QueryInterface(IID_IConnectionPointContainer, (void**)&connectionPointContainer);
    if (FAILED(hr) || !connectionPointContainer) {
        FLOG_DEBUG << "BrowserWindow::UnaviseEvent() failed: QueryInterface(IConnectionPointContainer) failed";
        return false;
    }
    IConnectionPointPtr connectionPoint;
    hr = connectionPointContainer->FindConnectionPoint(riid, &connectionPoint);
    if (FAILED(hr) || !connectionPoint) {
        FLOG_DEBUG << "BrowserWindow::UnadviseEvent() failed: FindConnectionPoint() failed";
        return false;
    }
    hr = connectionPoint->Unadvise(adviseCookie);
    if (FAILED(hr)) {
        FLOG_DEBUG << "BrowserWindow::UnadviseEvent() failed: Unadvise() failed";
        return false;
    }
    return true;
}
HWND BrowserWindow::GetWindowHandle() {
    _ASSERT(windowHandle_);
    return windowHandle_;
}
std::string BrowserWindow::GetWindowHandleString() {
    _ASSERT(windowHandle_);
    return str_hwnd;
}
IOleClientSite* BrowserWindow::GetOleClientSite() {
    return static_cast<IOleClientSite*>(oleClientSite_.get());
}
BOOL BrowserWindow::GetJScripts(CComPtr<IHTMLElementCollection>& spColl) {
    CComPtr<IDispatch> spDisp = NULL;
    HRESULT hr = webBrowser2_->get_Document(&spDisp);
    if (FAILED(hr) || !spDisp) {
        FLOG_WARNING << "BrowserWindow::SetProperty() failed, get_Document() failed";
        return false;
    }

	CComPtr<IHTMLDocument2>	m_spDoc = NULL;
	hr = spDisp->QueryInterface(IID_IHTMLDocument2, (void **)&m_spDoc);
	if (FAILED(hr)) {
		FLOG_WARNING << "Failed to get HTML document COM object";
		return false;
	}

	hr = m_spDoc->get_scripts(&spColl);
	_ASSERT(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}
const IWebBrowser2Ptr BrowserWindow::GetWebBrowser2() {
    _ASSERT(webBrowser2_);
    return webBrowser2_;
}
bool BrowserWindow::GetActiveHtmlElement(wchar_t* outTag, int outTagSize, wchar_t* outType, int outTypeSize) {
    _ASSERT(outTagSize);
    _ASSERT(outTypeSize);
    // In case this function fails & returns false.
    if (outTagSize) outTag[0] = 0; else outTag = 0;
    if (outTypeSize) outType[0] = 0; else outType = 0;
    HRESULT hr;
    int ires;

    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetActiveHtmlElement() failed, get_Document() failed";
        return false;
    }
    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::GetActiveHtmlElement() failed, QueryInterface(IHTMLDocument2) failed";
        return false;
    }
    IHTMLElementPtr htmlElement;
    hr = htmlDocument2->get_activeElement(&htmlElement);
    if (FAILED(hr) || !htmlElement) {
        FLOG_WARNING << "BrowserWindow::GetActiveHtmlElement() failed, get_activeElement() failed";
        return false;
    }
    _bstr_t tag;
    hr = htmlElement->get_tagName(tag.GetAddress());
    if (FAILED(hr)) {
        FLOG_WARNING << "BrowserWindow::GetActiveHtmlElement() failed, get_tagName() failed";
        return false;
    }
    ires = swprintf_s(outTag, outTagSize, L"%s", tag.GetBSTR());
    if (-1 == ires)
        return false;
    _bstr_t type(L"type");
    VARIANT attrvalue;
    VariantInit(&attrvalue);
    hr = htmlElement->getAttribute(type, 0 | 2, &attrvalue);
    if (FAILED(hr)) {
        FLOG_WARNING << "BrowserWindow::GetActiveHtmlElement() failed, getAttribute() failed";
        return false;
    }
    if (attrvalue.vt == VT_BSTR) {
        ires = swprintf_s(outType, outTypeSize, L"%s", attrvalue.bstrVal);
        if (-1 == ires)
            return false;
    } else {
        ires = swprintf_s(outType, outTypeSize, L"%s", L"");
        if (-1 == ires)
            return false;
    }
    return true;
}
int BrowserWindow::ExternalIdentifier(wchar_t* function) {
    if (wcscmp(function, L"CloseWindow") == 0) {
        return EXTERNAL_CLOSE_WINDOW;
    }
    return 0;
}

/******************************* DoPageAction() **************************
 * Implements the functionality of a "Back". "Forward", "Home", "Search",
 * "Refresh", or "Stop" button.
 *
 * hwnd =    Handle to the window hosting the browser object.
 * action =    One of the following:
 *        0 = Move back to the previously viewed web page.
 *        1 = Move forward to the previously viewed web page.
 *        2 = Move to the home page.
 *        3 = Search.
 *        4 = Refresh the page.
 *        5 = Stop the currently loading page.
 *
 * NOTE: EmbedBrowserObject() must have been successfully called once with the
 * specified window, prior to calling this function. You need call
 * EmbedBrowserObject() once only, and then you can make multiple calls to
 * this function to display numerous pages in the specified window.
 */
void BrowserWindow::DoPageAction(DWORD action) {
    // Call the desired function
    switch (action) {
      case WEBPAGE_BACK:
      {
        // Call the IWebBrowser2 object's GoBack function.
        webBrowser2_->GoBack();
        break;
      }

      case WEBPAGE_FORWARD:
      {
        // Call the IWebBrowser2 object's GoForward function.
        webBrowser2_->GoForward();
        break;
      }

      case WEBPAGE_HOME:
      {
        // Call the IWebBrowser2 object's GoHome function.
        webBrowser2_->GoHome();
        break;
      }

      case WEBPAGE_SEARCH:
      {
        // Call the IWebBrowser2 object's GoSearch function.
        webBrowser2_->GoSearch();
        break;
      }

      case WEBPAGE_REFRESH:
      {
        // Call the IWebBrowser2 object's Refresh function.
        webBrowser2_->Refresh();
      }

      case WEBPAGE_STOP:
      {
        // Call the IWebBrowser2 object's Stop function.
        webBrowser2_->Stop();
      }
    }
}
void BrowserWindow::Refresh2(int Level) {
    // REFRESH_NORMAL = 0,
    // REFRESH_IFEXPIRED = 1,
    // REFRESH_COMPLETELY = 3
    // refresh level https://docs.microsoft.com/en-us/previous-versions//aa768363%28v%3dvs.85%29

    _variant_t vLevel;
    vLevel.vt=VT_I4;
    vLevel.intVal=Level;
    webBrowser2_->Refresh2(&vLevel);
}
bool BrowserWindow::Inject(BSTR* where, BSTR* html) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::Inject() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::Inject() failed:, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    IHTMLElementPtr pBody;
    hr = htmlDocument2->get_body(&pBody);
    if (FAILED(hr) || !pBody) {
        FLOG_WARNING << "BrowserWindow::Inject() failed, get_body() failed";
        return false;
    }

    hr = pBody -> insertAdjacentHTML(*where, *html);;

    return 0 == hr;
}
bool BrowserWindow::GetCookies(BSTR* cookie) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetCookies() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::GetCookies() failed:, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    hr = htmlDocument2 -> get_cookie(cookie);

    return 0 == hr;
}
bool BrowserWindow::GetHttpOnlyCookie(LPCWSTR url, LPCWSTR key, LPWSTR val) {
  DWORD dwSize = 10240;

  return InternetGetCookieEx(url, key, val, &dwSize, INTERNET_COOKIE_HTTPONLY | INTERNET_COOKIE_THIRD_PARTY , NULL);
}
std::wstring BrowserWindow::GetCookies(std::wstring domain) {
    DWORD dwSize = 20480;
    wchar_t strCookie[20480] = {0};

    InternetGetCookieEx(domain.c_str(), NULL, strCookie, &dwSize, INTERNET_COOKIE_HTTPONLY | INTERNET_COOKIE_THIRD_PARTY , NULL);
    std::wstring wsCookies = strCookie;
    return wsCookies;
}
bool BrowserWindow::GetHtml(BSTR* html) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetHtml() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::GetHtml() failed:, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    IHTMLElementPtr pBody;
    IHTMLElementPtr pHtml;
    hr = htmlDocument2->get_body(&pBody);
    if (FAILED(hr) || !pBody) {
        FLOG_WARNING << "BrowserWindow::GetHtml() failed, get_body() failed";
        return false;
    }

    hr = pBody -> get_parentElement(&pHtml);
    if (FAILED(hr) || !pHtml) {
        FLOG_WARNING << "BrowserWindow::GetHtml() failed, get_parentElement() failed";
        return false;
    }

    hr = pHtml -> get_outerHTML(html);

    return 0 == hr;
}
std::string BrowserWindow::GetHtml() {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetHtml() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "Unable to get document object, DocumentHost::GetDocument did not return a valid IHTMLDocument2 pointer";
        return "";
    }

    IHTMLDocument3Ptr doc3;
    hr = htmlDocument2->QueryInterface(IID_IHTMLDocument3, (void**)&doc3);
    if (FAILED(hr) || !doc3) {
        FLOG_WARNING << "Unable to get document object, QueryInterface to IHTMLDocument3 failed";
        return "";
    }

    IHTMLElementPtr document_element;
    hr = doc3->get_documentElement(&document_element);
    if (FAILED(hr) || !document_element) {
        FLOG_WARNING << "Unable to get document element from page, call to IHTMLDocument3::get_documentElement failed";
        return "";
    }

    WCHAR* html;
    hr = document_element->get_outerHTML(&html);
    if (FAILED(hr)) {
        FLOG_WARNING << "Have document element but cannot read source, call to IHTMLElement::get_outerHTML failed";
        return "";
    }

    return WideToUtf8(html);
}
bool BrowserWindow::GetURL(BSTR* url) {
    HRESULT hr = webBrowser2_->get_LocationURL(url);

    return 0 == hr;
}
bool BrowserWindow::GetCharset(BSTR* charset) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetCharset() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::GetCharset() failed:, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    hr = htmlDocument2 -> get_charset(charset);

    return 0 == hr;
}
bool BrowserWindow::GetMime(BSTR* mime) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetMime() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::GetMime() failed:, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    hr = htmlDocument2 -> get_mimeType(mime);

    return 0 == hr;
}
bool BrowserWindow::GetTitle(BSTR* title) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::GetTitle() failed, get_Document() failed";
        return false;
    }

    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::GetTitle() failed:, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    hr = htmlDocument2 -> get_title(title);

    return 0 == hr;
}
bool BrowserWindow::ExternalCall(int functionId) {
    if (functionId == EXTERNAL_CLOSE_WINDOW) {
        CloseWebBrowser2();
        PostMessage(windowHandle_, WM_CLOSE, 0, 0);
        return true;
    }
    return false;
}
void BrowserWindow::Ready(bool isError) {

}
void BrowserWindow::SetAllowedUrl(const wchar_t* inUrl) {
    wcsncpy_s(allowedUrl_, _countof(allowedUrl_), inUrl, _TRUNCATE);
}
bool BrowserWindow::IsUrlAllowed(const wchar_t* inUrl, int sizeInWords) {
    wchar_t* url_lower = new wchar_t[sizeInWords];
    wcsncpy_s(url_lower, sizeInWords, inUrl, _TRUNCATE);
    _wcslwr_s(url_lower, sizeInWords);
    bool ret = false;
    std::wstring url = url_lower;
    if (0 == wcscmp(allowedUrl_, L"nohttp")) {
        // Disallow: http://, https:// - case insensitive.
        if (0 == url.compare(0, wcslen(L"http://"), L"http://") || 0 == url.compare(0, wcslen(L"https://"), L"https://")) {
            ret = false;
        } else {
            ret = true;
        }
    } else {
        if (0 == url.compare(0, wcslen(allowedUrl_), allowedUrl_)) {
            ret = true;
        } else {
            ret = false;
        }
    }
    delete[] url_lower;
    return ret;
}
void BrowserWindow::SetTitle(const wchar_t* title) {
    BOOL b = SetWindowText(windowHandle_, title);
    _ASSERT(b);
}
bool BrowserWindow::IsPopup() {
    if (parentHandle_)
        return true;
    return false;
}
void BrowserWindow::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Need to re-attach click events after each browser navigation.
    TryAttachClickEvents();
    TrySetFocusAfterCreation();
}
void BrowserWindow::OnGetMinMaxInfo(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!IsPopup()) {
        nlohmann::json* settings = GetApplicationSettings();
        static long minimum_width = (*settings)["window"]["minimum_size"][0];
        static long minimum_height = (*settings)["window"]["minimum_size"][1];
        static long maximum_width = (*settings)["window"]["maximum_size"][0];
        static long maximum_height = (*settings)["window"]["maximum_size"][1];
        MINMAXINFO* pMMI = (MINMAXINFO*)lParam;
        if (minimum_width)
            pMMI->ptMinTrackSize.x = minimum_width;
        if (minimum_height)
            pMMI->ptMinTrackSize.y = minimum_height;
        if (maximum_width)
            pMMI->ptMaxTrackSize.x = maximum_width;
        if (maximum_height)
            pMMI->ptMaxTrackSize.y = maximum_height;
    }
}
void BrowserWindow::OnResize(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    DWORD clientWidth = LOWORD(lParam);
    DWORD clientHeight = HIWORD(lParam);
    if (webBrowser2_) {
        // WebBrowser2->put_Width() will dispatch an event
        // DWebBrowserEvents2::WindowSetWidth() which will call
        // BrowserWindow::SetWidth(), we should not do any resizing
        // during these events.
        isResizing_ = true;
        webBrowser2_->put_Width(clientWidth);
        webBrowser2_->put_Height(clientHeight);
        isResizing_ = false;
    } else {
        FLOG_WARNING << "BrowserWindow::OnResize() failed: WebBrowser2 object not created yet";
    }
}
void BrowserWindow::SetWidth(long newClientWidth) {
    if (isResizing_)
        return;
    RECT clientRect;
    HWND shellBrowserHandle = GetShellBrowserHandle();
    if (!shellBrowserHandle)
        return;
    BOOL b = GetClientRect(shellBrowserHandle, &clientRect);
    _ASSERT(b);
    _ASSERT(clientRect.left == 0 && clientRect.top == 0);
    clientRect.right = newClientWidth;
    RECT windowRect = clientRect;
    b = AdjustWindowRectEx(&windowRect,
            GetWindowLong(windowHandle_, GWL_STYLE),
            GetMenu(windowHandle_) != NULL,
            GetWindowLong(windowHandle_, GWL_EXSTYLE));
    _ASSERT(b);
    b = SetWindowPos(windowHandle_, HWND_TOP, 0, 0,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
    _ASSERT(b);
}
void BrowserWindow::SetHeight(long newClientHeight) {
    if (isResizing_)
        return;
    RECT clientRect;
    HWND shellBrowserHandle = GetShellBrowserHandle();
    if (!shellBrowserHandle)
        return;
    BOOL b = GetClientRect(shellBrowserHandle, &clientRect);
    _ASSERT(b);
    _ASSERT(clientRect.left == 0 && clientRect.top == 0);
    clientRect.bottom = newClientHeight;
    RECT windowRect = clientRect;
    b = AdjustWindowRectEx(&windowRect,
            GetWindowLong(windowHandle_, GWL_STYLE),
            GetMenu(windowHandle_) != NULL,
            GetWindowLong(windowHandle_, GWL_EXSTYLE));
    _ASSERT(b);
    b = SetWindowPos(windowHandle_, HWND_TOP, 0, 0,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
    _ASSERT(b);
}
void BrowserWindow::SetTop(long newTop) {
    RECT rect;
    BOOL b = GetWindowRect(windowHandle_, &rect);
    _ASSERT(b);
    b = SetWindowPos(windowHandle_, HWND_TOP, newTop, rect.left, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
    _ASSERT(b);
}
void BrowserWindow::SetLeft(long newLeft) {
    RECT rect;
    BOOL b = GetWindowRect(windowHandle_, &rect);
    _ASSERT(b);
    b = SetWindowPos(windowHandle_, HWND_TOP, rect.top, newLeft, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
    _ASSERT(b);
}
void BrowserWindow::SetTitleFromSettings() {
    if (IsPopup()) {
        nlohmann::json* settings = GetApplicationSettings();
        std::string popup_title = (*settings)["window"]["popup_title"];
        if (popup_title.empty())
            popup_title = (*settings)["window"]["title"];
        if (popup_title.empty())
            popup_title = GetExecutableName();
        SetTitle(Utf8ToWide(popup_title).c_str());
    }
    // Main window title is set in CreateMainWindow().
}
void BrowserWindow::SetIconFromSettings() {
    nlohmann::json* settings = GetApplicationSettings();
    std::string iconPath;
    if (IsPopup())
        iconPath = (*settings)["window"]["popup_icon"];
    else
        iconPath = (*settings)["window"]["icon"];
    if (!iconPath.empty()) {
        wchar_t iconPathW[MAX_PATH];
        Utf8ToWide(iconPath.c_str(), iconPathW, _countof(iconPathW));

        int bigX = GetSystemMetrics(SM_CXICON);
        int bigY = GetSystemMetrics(SM_CYICON);
        HANDLE bigIcon = LoadImage(0, iconPathW, IMAGE_ICON, bigX, bigY, LR_LOADFROMFILE);
        if (bigIcon) {
            SendMessage(windowHandle_, WM_SETICON, ICON_BIG, (LPARAM)bigIcon);
        } else {
            FLOG_WARNING << "Setting icon from settings file failed, (ICON_BIG)";
        }
        int smallX = GetSystemMetrics(SM_CXSMICON);
        int smallY = GetSystemMetrics(SM_CYSMICON);
        HANDLE smallIcon = LoadImage(0, iconPathW, IMAGE_ICON, smallX, smallY, LR_LOADFROMFILE);
        if (smallIcon) {
            SendMessage(windowHandle_, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
        } else {
            FLOG_WARNING << "Setting icon from settings file failed, (ICON_SMALL)";
        }
    }
}
HWND BrowserWindow::GetShellBrowserHandle() {
    // Calling WebBrowser2->get_HWND() fails, need to go around.
    HRESULT hr;
    if (!webBrowser2_) {
        FLOG_DEBUG << "BrowserWindow::GetShellBrowserHandle() failed: WebBrowser2 is empty";
        return 0;
    }
    IServiceProviderPtr serviceProvider;
    hr = webBrowser2_->QueryInterface(IID_IServiceProvider, (void**)&serviceProvider);
    if (FAILED(hr) || !serviceProvider) {
        FLOG_WARNING << "BrowserWindow::GetShellBrowserHandle() failed: WebBrowser2->QueryInterface(IServiceProvider) failed";
        return 0;
    }
    IOleWindowPtr oleWindow;
    hr = serviceProvider->QueryService(SID_SShellBrowser, IID_IOleWindow, (void**)&oleWindow);
    if (FAILED(hr) || !oleWindow) {
        FLOG_WARNING << "BrowserWindow::GetShellBrowserHandle() failed: QueryService(SShellBrowser, IOleWindow) failed";
        return 0;
    }
    HWND shellBrowserHandle;
    hr = oleWindow->GetWindow(&shellBrowserHandle);
    if (FAILED(hr) || !shellBrowserHandle) {
        FLOG_WARNING << "BrowserWindow::GetShellBrowserHandle() failed: OleWindow->GetWindow() failed";
        return 0;
    }
    return shellBrowserHandle;
}
bool BrowserWindow::SetFocus() {
    // Calling SetFocus() on shellBrowser handle does not work.
    if (!oleInPlaceActiveObject_)
        return false;
    HRESULT hr = oleInPlaceActiveObject_->OnFrameWindowActivate(TRUE);
    if (FAILED(hr)) {
        FLOG_DEBUG << "BrowserWindow::SetFocus(): IOleInPlaceActiveObject->OnFrameWindowActivate() failed";
        return false;
    }
    hr = oleInPlaceActiveObject_->OnDocWindowActivate(TRUE);
    if (FAILED(hr)) {
        FLOG_DEBUG << "BrowserWindow::SetFocus(): IOleInPlaceActiveObject->OnDocWindowActivate() failed";
        return false;
    }
    return true;
    // Another way is to call:
    // IWebBrowser2->get_Document()
    // IDispatch->QueryInterface(IHTMLDocument2)
    // IHTMLDocument2->get_body()
    // IHTMLElement->QueryInterface(IHTMLElement2)
    // IHTMLElement2->focus()
}
bool BrowserWindow::TrySetFocusAfterCreation() {
    if (!focusedAfterCreation_) {
        if (SetFocus()) {
            focusedAfterCreation_ = true;
            return true;
        }
    }
    return false;
}
bool BrowserWindow::TranslateAccelerator(MSG* msg) {
    if (!oleInPlaceActiveObject_)
        return false;
    HRESULT hr = oleInPlaceActiveObject_->TranslateAccelerator(msg);
    if (FAILED(hr)) {
        FLOG_DEBUG << "BrowserWindow::TranslateAccelerator() failed: IOleInPlaceActiveObject->TranslateAccelerator() failed";
        return false;
    }
    // S_OK - translated
    // S_FALSE - not translated
    // other values = FAILED()
    // Remember that (hr=S_FALSE) == SUCCEEDED(hr)
    return (hr == S_OK);
}
bool BrowserWindow::DisplayHtmlString(const wchar_t* htmlString) {
    if (!webBrowser2_)
        return false;
    IDispatchPtr documentDispatch;
    HRESULT hr = webBrowser2_->get_Document(&documentDispatch);
    if (FAILED(hr) || !documentDispatch) {
        FLOG_DEBUG << "BrowserWindow::DisplayHtmlString(): IWebBrowser2->get_Document() failed";
        // If there is no document available navigate to blank page.
        bool navigated = Navigate(false, "about:blank");
        FLOG_DEBUG << "Navigated to about:blank";
        if (!navigated)
            return false;
        hr = webBrowser2_->get_Document(&documentDispatch);
        if (FAILED(hr) || !documentDispatch) {
            FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: IWebBrowser2->get_Document(about:blank) failed";
            _ASSERT(false);
            return false;
        }
    }
    IHTMLDocument2Ptr htmlDocument2;
    hr = documentDispatch->QueryInterface(&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: QueryInterface(IHTMLDocument2) failed";
        _ASSERT(false);
        return false;
    }
    SAFEARRAY *strings = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if (!strings) {
        FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: SafeArrayCreateVector() failed";
        _ASSERT(false);
        return false;
    }
    // NOTICE: From now on returning false is allowed only at the end
    // of the function, as we need to cleanup memory.
    VARIANT* variantParam;
    _bstr_t bstrParam = htmlString;
    hr = SafeArrayAccessData(strings, (void**)&variantParam);
    bool ret = false;
    if (SUCCEEDED(hr)) {
        variantParam->vt = VT_BSTR;
        variantParam->bstrVal = bstrParam.GetBSTR();
        hr = SafeArrayUnaccessData(strings);
        if (SUCCEEDED(hr)) {
            hr = htmlDocument2->write(strings);
            if (SUCCEEDED(hr)) {
                hr = htmlDocument2->close();
                if (SUCCEEDED(hr)) {
                    ret = true;
                } else {
                    FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: IHTMLDocument2->close() failed";
                    _ASSERT(false);
                }
            } else {
                FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: IHTMLDocument2->write() failed";
                _ASSERT(false);
            }
        } else {
            FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: SafeArrayUnaccessData() failed";
            _ASSERT(false);
        }
    } else {
        FLOG_ERROR << "BrowserWindow::DisplayHtmlString() failed: SafeArrayAccessData() failed";
        _ASSERT(false);
    }
    if (strings) {
        hr = SafeArrayDestroy(strings);
        strings = 0;
        _ASSERT(SUCCEEDED(hr));
    }
    return ret;
}
bool BrowserWindow::DisplayErrorPage(const wchar_t* navigateUrl, int statusCode) {
    nlohmann::json* settings = GetApplicationSettings();
    std::string error_page = (*settings)["msie"]["error_page"];
    if (error_page.empty())
        return false;
    std::string htmlFile = GetExecutableDirectory() + "\\" + error_page;
    htmlFile = GetRealPath(htmlFile);
    std::string htmlString = GetFileContents(htmlFile);
    if (htmlString.empty()) {
        FLOG_WARNING << "BrowserWindow::DisplayErrorPage() failed: file not found: " << htmlFile;
        return false;
    }
    ReplaceStringInPlace(htmlString, "{{navigate_url}}", WideToUtf8(navigateUrl));
    ReplaceStringInPlace(htmlString, "{{status_code}}", IntToString(statusCode));
    if (DisplayHtmlString(Utf8ToWide(htmlString).c_str()))
        return true;
    return false;
}

bool BrowserWindow::GetHtmlCode(PWCHAR pszCode, int *iszCount) {
	BOOL bRet = FALSE;
	IDispatch *pDispatch = NULL;
	IHTMLDocument2 *htmlDoc2 = NULL;
	IHTMLElement *pBodyElement = NULL;
	PWCHAR pHtmlCode = NULL;
	int iLen = 0;

	if (pszCode == NULL || iszCount == NULL) return bRet;
	__try {
		if (webBrowser2_->get_Document(&pDispatch) != ERROR_SUCCESS) __leave;
		if (pDispatch == NULL) __leave;
		if (pDispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDoc2) != ERROR_SUCCESS) __leave;
		if (htmlDoc2 == NULL) __leave;
		htmlDoc2->get_body(&pBodyElement);
		if (pBodyElement == NULL) __leave;
		pBodyElement->get_innerHTML(&pHtmlCode);
		if (pHtmlCode == NULL) __leave;
		iLen = lstrlenW(pHtmlCode) + 1;
		if (pszCode) {
			if (*iszCount >= iLen) {
				ZeroMemory(pszCode, *iszCount * sizeof(WCHAR));
				CopyMemory(pszCode, pHtmlCode, iLen * sizeof(WCHAR));
			}
		} else {
			*iszCount = iLen;
			__leave;
		}
		bRet = TRUE;
	}
	__finally {
		if (pBodyElement) pBodyElement->Release();
		if (htmlDoc2) htmlDoc2->Release();
		if (pDispatch) pDispatch->Release();
	}
	return bRet;
}

bool BrowserWindow::ClickElementByID(std::wstring idOrName) {
    bool bRet = FALSE;
    CComPtr<IHTMLElement> pElement = GetHTMLElementByID(idOrName);
    if (0 != pElement) {
        pElement->click();

        bRet = TRUE;
    }

    return bRet;
}

bool BrowserWindow::SetElementAttrByID(std::wstring id, std::wstring attr, std::wstring val) {
	BOOL bRet = FALSE;

	if (!val.empty()) {
        CComPtr<IHTMLElement> pElement = GetHTMLElementByID(id);
		if (0 != pElement) {
            pElement->setAttribute(_bstr_t(attr.c_str()), _variant_t(val.c_str()));
            bRet = TRUE;
        }
    }

	return bRet;
}

bool BrowserWindow::GetElementAttrByID(std::wstring id, std::wstring attr, _variant_t* pVarResult) {
    CComPtr<IHTMLElement> pElement = GetHTMLElementByID(id);
    if (0 != pElement) {
        pElement->getAttribute(_bstr_t(attr.c_str()), 0, pVarResult);

        return true;
    }

	return false;
}
bool BrowserWindow::ExecJsFun(const std::wstring& lpJsFun, const std::vector<std::wstring>& params, _variant_t* pVarResult /*= NULL*/) {
    CComPtr<IDispatch> spDisp = NULL;
    HRESULT hr = webBrowser2_->get_Document(&spDisp);
    if (FAILED(hr) || !spDisp) {
        FLOG_WARNING << "BrowserWindow::SetProperty() failed, get_Document() failed";
        return false;
    }

	CComPtr<IHTMLDocument2>	m_spDoc = NULL;
	hr = spDisp->QueryInterface(IID_IHTMLDocument2, (void **)&m_spDoc);
	if (FAILED(hr)) {
		FLOG_WARNING << "Failed to get HTML document COM object";
		return FALSE;
	}

	CComPtr<IDispatch> spScript;
    hr = m_spDoc->get_Script(&spScript);
	if (FAILED(hr)) {
		return FALSE;
	}

	CComBSTR bstrFunc(lpJsFun.c_str());
	DISPID dispid = NULL;
	hr = spScript->GetIDsOfNames(IID_NULL, &bstrFunc, 1, LOCALE_SYSTEM_DEFAULT, &dispid);
	if (FAILED(hr)) {
		return FALSE;
	}

	DISPPARAMS dispParams;
	memset(&dispParams, 0, sizeof(DISPPARAMS));
	int nParamCount	= (int) params.size();
	if (nParamCount > 0) {
		dispParams.cArgs	= nParamCount;
		dispParams.rgvarg	= new VARIANT[nParamCount];
		for (int i=0; i<nParamCount; ++i ) {
			const std::wstring& str = params[nParamCount-1-i];
			CComBSTR bstr(str.c_str());
			bstr.CopyTo(&dispParams.rgvarg[i].bstrVal);
			dispParams.rgvarg[i].vt	= VT_BSTR;
		}
	}

	EXCEPINFO excepInfo;
	memset(&excepInfo, 0, sizeof excepInfo);
	_variant_t vaResult;
	UINT nArgErr = (UINT)-1;  // initialize to invalid arg

	hr = spScript->Invoke(dispid, IID_NULL, 0, DISPATCH_METHOD, &dispParams, &vaResult, &excepInfo, &nArgErr);

	delete[] dispParams.rgvarg;
	if (FAILED(hr)) {
		return FALSE;
	}

	if (pVarResult) {
		*pVarResult = vaResult;
	}

	return TRUE;
}
bool BrowserWindow::execScript(std::wstring js, std::wstring lang, VARIANT* ret) {
    HRESULT hr;
    IDispatchPtr dispatch;
    hr = webBrowser2_->get_Document(&dispatch);
    if (FAILED(hr) || !dispatch) {
        FLOG_WARNING << "BrowserWindow::execScript() failed, get_Document() failed";
        return false;
    }
    IHTMLDocument2Ptr htmlDocument2;
    hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDocument2);
    if (FAILED(hr) || !htmlDocument2) {
        FLOG_WARNING << "BrowserWindow::execScript() failed, QueryInterface(IHTMLDocument2) failed";
        return false;
    }

    CComQIPtr<IHTMLWindow2> pHTMLWnd;
    htmlDocument2->get_parentWindow(&pHTMLWnd);
    if (FAILED(hr) || !pHTMLWnd) {
        FLOG_WARNING << "BrowserWindow::execScript() failed, QueryInterface(IHTMLWindow2) failed";
        return false;
    }

    pHTMLWnd->execScript(_bstr_t(js.c_str()), _bstr_t(lang.c_str()), ret);

    return true;
}

IHTMLElement* BrowserWindow::GetHTMLElementByID(std::wstring id) {
	IHTMLElementPtr pElement=0;
	IDispatchPtr dispatch=0;
	HRESULT hr = webBrowser2_->get_Document(&dispatch);
	if ((S_OK==hr)&&(0!=dispatch)) {
		IHTMLDocument3Ptr doc;
		dispatch->QueryInterface(IID_IHTMLDocument3,(void**)&doc);
		hr = doc->getElementById(_bstr_t(id.c_str()), &pElement);
        if (S_OK != hr) {
            pElement = 0;
        }

        dispatch->Release();
		doc->Release();
	}
	return pElement;
}

IHTMLElement* BrowserWindow::GetHTMLElementByTag(std::wstring tagName,std::wstring PropertyName, std::wstring matchValue) {
	IHTMLElement *retElement=0;
	IDispatch *dispatch=0;
	HRESULT hr = webBrowser2_->get_Document(&dispatch);
	if ((S_OK==hr)&&(0 != dispatch)) {
		IHTMLDocument2 *doc;
		dispatch->QueryInterface(IID_IHTMLDocument2,(void**)&doc);
		dispatch->Release();
		IHTMLElementCollection* doc_all;
		hr = doc->get_all(&doc_all);      // this is like doing document.all
		if (S_OK == hr) {
			VARIANT vKey;
			vKey.vt=VT_BSTR;
			vKey.bstrVal=SysAllocString(tagName.c_str());
			VARIANT vIndex;
			VariantInit(&vIndex);
			hr = doc_all->tags(vKey,&dispatch);       // this is like doing document.all["messages"]
			// clean
			SysFreeString(vKey.bstrVal);
			VariantClear(&vKey);
			VariantClear(&vIndex);
			if ((S_OK == hr) && (0 != dispatch)) {
				CComQIPtr< IHTMLElementCollection > all_tags = dispatch;
				//hr = dispatch->QueryInterface(IHTMLElementCollection,(void **)&all_tags); // it's the caller's responsibility to release
				if (S_OK == hr) {
					long nTagsCount=0; //
					hr = all_tags->get_length( &nTagsCount);
					if ( FAILED( hr ) ) {
						return retElement;
					}

					for(long i=0; i<nTagsCount; i++) {
						CComDispatchDriver spInputElement;
						hr = all_tags->item( CComVariant(i), CComVariant(i), &spInputElement );

						if ( FAILED( hr ) )
							continue;
						CComVariant vValue;
						hr = spInputElement.GetPropertyByName(PropertyName.c_str(), &vValue );
						if (VT_EMPTY != vValue.vt) {
							LPCTSTR lpValue = vValue.bstrVal? OLE2CT( vValue.bstrVal ) : NULL;
							if(NULL == lpValue)
								continue;
							std::wstring cs = (LPCTSTR)lpValue;
							if (0 == _tcscmp(cs.c_str(),matchValue.c_str())) {
								hr = spInputElement->QueryInterface(IID_IHTMLElement,(void **)&retElement);
								if (S_OK == hr) {
								} else {
									retElement = 0;
								}
								break;
							}
						}
					}
				} else {
					retElement = 0;
				}
				dispatch->Release();
			}
			doc_all->Release();
		}
		doc->Release();
	}
	return retElement;
}

IHTMLElement* BrowserWindow::GetHTMLElementByName(std::wstring Name) {
	IHTMLElement *retElement=0;
	IDispatch *dispatch=0;
	HRESULT hr = webBrowser2_->get_Document(&dispatch);
	if ((S_OK==hr)&&(0!=dispatch)) {
		IHTMLDocument2 *doc;
		dispatch->QueryInterface(IID_IHTMLDocument2,(void**)&doc);
		dispatch->Release();
		IHTMLElementCollection* doc_all;
		hr = doc->get_all(&doc_all);      // this is like doing document.all
		if (S_OK == hr) {
			VARIANT vKey;
			vKey.vt=VT_BSTR;
			vKey.bstrVal=SysAllocString(Name.c_str());
			VARIANT vIndex;
			VariantInit(&vIndex);
			hr = doc_all->item(vKey,vIndex,&dispatch);       // this is like doing document.all["messages"]
			// clean
			SysFreeString(vKey.bstrVal);
			VariantClear(&vKey);
			VariantClear(&vIndex);
			if ((S_OK == hr) && (0 != dispatch)) {
				hr = dispatch->QueryInterface(IID_IHTMLElement,(void **)&retElement); // it's the caller's responsibility to release
				if (S_OK == hr) {
				} else {
					retElement = 0;
				}
				dispatch->Release();
			}
			doc_all->Release();
		}
		doc->Release();
	}
	return retElement;
}
/*
HRESULT CWebBrowserBase::GetProperty(IDispatch *pObj, LPOLESTR pName, VARIANT *pValue)
{
	DISPID dispid = FindId(pObj, pName);
	if(dispid == -1) return E_FAIL;

	DISPPARAMS ps;
	ps.cArgs = 0;
	ps.rgvarg = NULL;
	ps.cNamedArgs = 0;
	ps.rgdispidNamedArgs = NULL;

	return pObj->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &ps, pValue, NULL, NULL);
}

HRESULT CWebBrowserBase::SetProperty(IDispatch *pObj, LPOLESTR pName, VARIANT *pValue)
{
	DISPID dispid = FindId(pObj, pName);
	if(dispid == -1) return E_FAIL;

	DISPPARAMS ps;
	ps.cArgs = 1;
	ps.rgvarg = pValue;
	ps.cNamedArgs = 0;
	ps.rgdispidNamedArgs = NULL;

	return pObj->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYPUT, &ps, NULL, NULL, NULL);
}
*/
std::string BrowserWindow::GetOrgUrl(std::string url) {
    if ("" != url) {
        org_url = url;
    }

    return org_url;
}