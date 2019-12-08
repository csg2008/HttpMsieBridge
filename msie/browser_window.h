// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include "../defines.h"
#include <map>
#include <stdio.h>
#include <string>
#include <vector>
#include <crtdbg.h> // _ASSERT macro.
#include <windows.h>
#include <ExDisp.h>
#include <memory> // shared_ptr

#include "com_utils.h"
#include "click_events.h"
#include "ole_client_site.h"
#include "external_dispatch.h"

class BrowserWindow;
std::map<std::string, std::string> GetBrowserList();
BrowserWindow* GetBrowserWindow(std::string hwnd);
void StoreBrowserWindow(std::string hwnd, BrowserWindow* browser);
void RemoveBrowserWindow(std::string hwnd);

class BrowserWindow {
private:
    HWND windowHandle_;
    HWND parentHandle_;
    std::shared_ptr<OleClientSite> oleClientSite_;
    std::shared_ptr<ExternalDispatch> externalDispatch_;
    std::shared_ptr<ClickEvents> clickEvents_;
    std::string org_url;
    std::string org_html;
    std::string str_hwnd;
    bool clickEventsAttached_;
    _bstr_t documentUniqueID_;
    _variant_t clickDispatch_;
    wchar_t allowedUrl_[2084];
    IWebBrowser2Ptr webBrowser2_;
    IHTMLWindow2*	pHtmlWnd2_;
    IOleObjectPtr oleObject_;
    IOleInPlaceActiveObjectPtr oleInPlaceActiveObject_;
    DWORD dWebBrowserEvents2Cookie_;
    bool isResizing_;
    bool focusedAfterCreation_;
public:
    BrowserWindow(HWND inWindowHandle, std::string url);
    ~BrowserWindow();
    bool CreateBrowserControl(std::string url);
    bool Navigate(bool isPost, std::string url, std::string header = "", const std::string& data = "");
    void CloseWebBrowser2();
    void CloseBrowserControl();
    bool DetachClickEvents();
    bool TryAttachClickEvents();
    bool TrySetFocusAfterCreation();
    bool AdviseEvent(IWebBrowser2Ptr webBrowser2, REFIID riid, DWORD* adviseCookie);
    bool UnadviseEvent(IWebBrowser2Ptr webBrowser2, REFIID riid, DWORD adviseCookie);
    HWND GetWindowHandle();
    HWND GetShellBrowserHandle();
    std::string GetWindowHandleString();
    IOleClientSite* GetOleClientSite();
    const IWebBrowser2Ptr GetWebBrowser2();
    BOOL GetJScripts(CComPtr<IHTMLElementCollection>& spColl);
    bool GetActiveHtmlElement(wchar_t* outTag, int outTagSize, wchar_t* outType, int outTypeSize);
    int ExternalIdentifier(wchar_t* function);
    void DoPageAction(DWORD action);
    void Refresh2(int Level = 1);
    bool Inject(BSTR* where, BSTR* html);
    bool GetCookies(BSTR* cookie);
    bool GetCharset(BSTR* charset);
    bool GetMime(BSTR* mime);
    bool GetTitle(BSTR* title);
    bool GetHtml(BSTR* html);
    std::string GetHtml();
    bool GetURL(BSTR* url);
    bool GetHttpOnlyCookie(LPCWSTR url, LPCWSTR key, LPWSTR val);
    bool ExternalCall(int functionIdentifier);
    void Ready(bool isError);
    void SetAllowedUrl(const wchar_t* inUrl);
    bool IsUrlAllowed(const wchar_t* inUrl, int sizeInWords);
    void SetWidth(long width);
    void SetHeight(long height);
    void SetTop(long top);
    void SetLeft(long left);
    void SetTitle(const wchar_t* title);
    bool IsPopup();
    void OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnGetMinMaxInfo(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnResize(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void SetTitleFromSettings();
    void SetIconFromSettings();
    bool SetFocus();
    bool TranslateAccelerator(MSG* msg);
    bool DisplayHtmlString(const wchar_t* htmlString);
    bool DisplayErrorPage(const wchar_t* navigateUrl, int statusCode);
    bool GetHtmlCode(OUT PWCHAR pszCode, IN OUT int *iszCount);
    bool ClickElementByID(std::wstring idOrName);
    bool SetElementAttrByID(std::wstring id, std::wstring attr, std::wstring val);
    bool GetElementAttrByID(std::wstring id, std::wstring attr, _variant_t* pVarResult);
    bool ExecJsFun(const std::wstring& lpJsFun, const std::vector<std::wstring>& params, _variant_t* pVarResult = NULL);
    bool execScript(std::wstring js, std::wstring lang, VARIANT* ret);
    IHTMLElement* GetHTMLElementByID(std::wstring id);
    IHTMLElement* GetHTMLElementByTag(std::wstring tagName,std::wstring PropertyName, std::wstring matchValue);
    IHTMLElement* GetHTMLElementByName(std::wstring Name);
    std::string GetOrgUrl(std::string url = "");
};
