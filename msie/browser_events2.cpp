// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

// DWebBrowserEvents2 interface for WebBrowser control:
// http://msdn.microsoft.com/en-us/library/aa768283(v=vs.85).aspx
// (DWebBrowserEvents2 for MSHTML has different methods).

#include "../defines.h"
#include <map>
#include <string>
#include <MsHtmdid.h>
#include "3rd/httplib.h"
#include "3rd/urlparser.h"
#include "browser_events2.h"
#include "browser_window.h"

#include "misc/logger.h"
#include "../main.h"
#include "3rd/json.hpp"
#include "../settings.h"
#include "misc/string_utils.h"
#include "../window_utils.h"
#include "../resource.h"

BrowserEvents2::BrowserEvents2(BrowserWindow* inBrowserWindow)
        : browserWindow_(inBrowserWindow) {
}
HRESULT STDMETHODCALLTYPE BrowserEvents2::QueryInterface(
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObject) {
    return browserWindow_->GetOleClientSite()->QueryInterface(
            riid, ppvObject);
}
ULONG STDMETHODCALLTYPE BrowserEvents2::AddRef(void) {
    return 1;
}
ULONG STDMETHODCALLTYPE BrowserEvents2::Release(void) {
    return 1;
}
HRESULT STDMETHODCALLTYPE BrowserEvents2::GetTypeInfoCount(
        /* [out] */ UINT *pctinfo) {
    return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE BrowserEvents2::GetTypeInfo(
        /* [in] */ UINT iTInfo,
        /* [in] */ LCID lcid,
        /* [out] */ ITypeInfo **ppTInfo) {
    return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE BrowserEvents2::GetIDsOfNames(
        /* [in] */ REFIID riid,
        /* [in] */ LPOLESTR *rgszNames,
        /* [in] */ UINT cNames,
        /* [in] */ LCID lcid,
        /* [out] */ DISPID *rgDispId) {

		return S_OK;
}
HRESULT STDMETHODCALLTYPE BrowserEvents2::Invoke(
        /* [in] */ DISPID dispId,
        /* [in] */ REFIID riid,
        /* [in] */ LCID lcid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS *pDispParams,
        /* [out] */ VARIANT *pVarResult,
        /* [out] */ EXCEPINFO *pExcepInfo,
        /* [out] */ UINT *puArgErr) {

    // When returning a result, you must check whether pVarResult
    // is not NULL and initialize it using VariantInit(). If it's
    // NULL then it doesn't expect a result.
    if (riid != IID_NULL) {
        return DISP_E_UNKNOWNINTERFACE;
    }

    pExcepInfo = 0;
    puArgErr = 0;
    HRESULT hr;
    std::map<std::string, std::string> IEMessage;
    nlohmann::json* settings = GetApplicationSettings();

    if (dispId == DISPID_NEWWINDOW3) {
        /* When calling window.open() you get an error "Class
           not registered". Before this error appears
           DWebBrowserEvents2::NewWindow3 event is dispatched,
           you need to create the popup window in this event
           and assign the dispatch interface of the new popup
           browser to the first parameter of NewWindow3. */
        LOG_DEBUG << "BrowserEvents2::NewWindow3()";
        if (pDispParams->cArgs != 5) {
            LOG_WARNING << "BrowserEvents2::NewWindow3() failed: Expected 5 arguments";
            _ASSERT(false);
            return DISP_E_BADPARAMCOUNT;
        }
        // ppDisp
        _ASSERT(pDispParams->rgvarg[4].vt == (VT_DISPATCH | VT_BYREF));
        // Cancel
        _ASSERT(pDispParams->rgvarg[3].vt == (VT_BOOL | VT_BYREF));
        // dwFlags
        _ASSERT(pDispParams->rgvarg[2].vt == (VT_I4 | VT_UI4));
        // bstrUrlContext
        _ASSERT(pDispParams->rgvarg[1].vt == VT_BSTR);
        // bstrUrl
        _ASSERT(pDispParams->rgvarg[0].vt == VT_BSTR);

        HWND popupHandle = CreatePopupWindow(browserWindow_->GetWindowHandle());
        _ASSERT(popupHandle);
        BrowserWindow* browserWindow = GetBrowserWindow(std::to_string((long long) popupHandle));
        if (!browserWindow) {
            LOG_WARNING << "BrowserEvents2::NewWindow3() failed: CreatePopupWindow() failed";
            // Cancel parameter. Current navigation should be cancelled.
            *pDispParams->rgvarg[3].pboolVal = VARIANT_TRUE;
            return S_OK;
        }
        const IWebBrowser2Ptr webBrowser2 = browserWindow->GetWebBrowser2();
        IDispatchPtr dispatch;
        hr = webBrowser2->get_Application(&dispatch);
        if (FAILED(hr) || !dispatch) {
            LOG_WARNING << "BrowserEvents2::NewWindow3() failed: webBrowser2->get_Application() failed";
            return S_OK;
        }

        *pDispParams->rgvarg[4].ppdispVal = dispatch.Detach();
        *pDispParams->rgvarg[3].pboolVal = VARIANT_FALSE;

        // Following events (DWebBrowserEvents2) will appear
        // after popup creation, they inform about "features"
        // passed to "window.open", such as width, height and others:
        // DISPID_ONTOOLBAR
        // DISPID_ONADDRESSBAR
        // DISPID_WINDOWSETRESIZABLE
        // DISPID_ONMENUBAR
        // DISPID_ONSTATUSBAR
        // DISPID_ONFULLSCREEN
        // DISPID_CLIENTTOHOSTWINDOW
        // DISPID_WINDOWSETWIDTH
        // DISPID_WINDOWSETHEIGHT
        // DISPID_WINDOWSETTOP
        // DISPID_WINDOWSETLEFT
        // DISPID_NAVIGATECOMPLETE2
        return S_OK;
    } else if (dispId == DISPID_WINDOWSETWIDTH) {
        _ASSERT(pDispParams->cArgs == 1);
        _ASSERT(pDispParams->rgvarg[0].vt == VT_I4); // nWidth
        long width = pDispParams->rgvarg[0].lVal;
        // LOG_DEBUG << "BrowserEvents2::WindowSetWidth(): width = " << width;
        browserWindow_->SetWidth(width);
    } else if (dispId == DISPID_WINDOWSETHEIGHT) {
        _ASSERT(pDispParams->cArgs == 1);
        _ASSERT(pDispParams->rgvarg[0].vt == VT_I4); // nHeight
        long height = pDispParams->rgvarg[0].lVal;
        // LOG_DEBUG << "BrowserEvents2::WindowSetHeight(): height = " << height;
        browserWindow_->SetHeight(height);
    } else if (dispId == DISPID_WINDOWSETTOP) {
        _ASSERT(pDispParams->cArgs == 1);
        _ASSERT(pDispParams->rgvarg[0].vt == VT_I4); // nTop
        long top = pDispParams->rgvarg[0].lVal;
        // LOG_DEBUG << "BrowserEvents2::WindowSetTop(): top = " << top;
        browserWindow_->SetTop(top);
    } else if (dispId == DISPID_WINDOWSETLEFT) {
        _ASSERT(pDispParams->cArgs == 1);
        _ASSERT(pDispParams->rgvarg[0].vt == VT_I4); // nLeft
        long left = pDispParams->rgvarg[0].lVal;
        // LOG_DEBUG << "BrowserEvents2::WindowSetLeft(): left = " << left;
        browserWindow_->SetLeft(left);
    } else if (dispId == DISPID_TITLECHANGE) {
        if (browserWindow_->IsPopup() && browserWindow_->IsUsingMetaTitle()) {
            _ASSERT(pDispParams->cArgs == 1);
            _ASSERT(pDispParams->rgvarg[0].vt == VT_BSTR); // Text
            BSTR title = pDispParams->rgvarg[0].bstrVal;
            // LOG_DEBUG << "BrowserEvents2::TitleChange(): "
            //              "setting popup title = " << WideToUtf8(title);
            browserWindow_->SetTitle(title);
        }
    } else if (dispId == DISPID_BEFORENAVIGATE || dispId == DISPID_BEFORENAVIGATE2) {
        if (pDispParams->cArgs != 7) {
            LOG_WARNING << "BrowserEvents2::BeforeNavigate() failed: Expected 7 arguments";
            _ASSERT(false);
            return DISP_E_BADPARAMCOUNT;
        }

        _ASSERT(pDispParams->rgvarg[5].vt == (VT_VARIANT | VT_BYREF)); // Text
        const wchar_t* navigateUrl = pDispParams->rgvarg[5].pvarVal->bstrVal;
        browserWindow_ -> GetOrgUrl(WideToUtf8(navigateUrl));
    } else if (dispId == DISPID_NAVIGATEERROR) {
        if (pDispParams->cArgs != 5) {
            LOG_WARNING << "BrowserEvents2::NavigateError() failed: Expected 5 arguments";
            _ASSERT(false);
            return DISP_E_BADPARAMCOUNT;
        }
        // pDisp
        _ASSERT(pDispParams->rgvarg[4].vt == VT_DISPATCH);
        // URL
        _ASSERT(pDispParams->rgvarg[3].vt == (VT_VARIANT | VT_BYREF));
        _ASSERT(pDispParams->rgvarg[3].pvarVal->vt == VT_BSTR);
        // TargetFrameName
        _ASSERT(pDispParams->rgvarg[2].vt == (VT_VARIANT | VT_BYREF));
        _ASSERT(pDispParams->rgvarg[2].pvarVal->vt == VT_BSTR);
        // StatusCode
        _ASSERT(pDispParams->rgvarg[1].vt == (VT_VARIANT | VT_BYREF));
        _ASSERT(pDispParams->rgvarg[1].pvarVal->vt == VT_I4);
        // Cancel
        _ASSERT(pDispParams->rgvarg[0].vt == (VT_BOOL | VT_BYREF));

        const wchar_t* navigateUrl = pDispParams->rgvarg[3].pvarVal->bstrVal;
        int statusCode = pDispParams->rgvarg[1].pvarVal->lVal;

        LOG_DEBUG << "BrowserEvents2::NavigateError(), url: " << WideToUtf8(navigateUrl) << " status code: " << statusCode;

        if (browserWindow_->DisplayErrorPage(navigateUrl, statusCode)) {
            *pDispParams->rgvarg[0].pboolVal = VARIANT_TRUE;
        } else {
            *pDispParams->rgvarg[0].pboolVal = VARIANT_FALSE;
        }

        return S_OK;
    } else if (dispId == DISPID_WINDOWCLOSING) {
        // Seems like this event is never being called, it should be
        // called when executing "window.close()", but it's not.
        // Use WM_PARENTNOTIFY instead to be notified when window is closing.
        LOG_DEBUG << "BrowserEvents2::WindowClosing()";
        return S_OK;
        /*
        if (pDispParams->cArgs != 2) {
            LOG_WARNING << "BrowserEvents2::WindowClosing() failed: "
                    "Expected 2 arguments";
            _ASSERT(false);
            return DISP_E_BADPARAMCOUNT;
        }
        // bIsChildWindow
        _ASSERT(pDispParams->rgvarg[1].vt == VT_BOOL);
        // Cancel
        _ASSERT(pDispParams->rgvarg[0].vt == (VT_BOOL | VT_BYREF));

        // VARIANT_FALSE - window is allowed to close..
        *pDispParams->rgvarg[0].pboolVal = VARIANT_FALSE;
        return S_OK;
        */
    } else if (dispId == DISPID_DOCUMENTCOMPLETE) {
        browserWindow_ -> Ready(false);

        _bstr_t bstrUrl;
        _bstr_t bstrMime;
        _bstr_t bstrCharset;
        std::string callback = (*settings)["bridge"]["integration"]["callback"];

        if (browserWindow_ -> GetURL(bstrUrl.GetAddress()) && browserWindow_ -> GetMime(bstrMime.GetAddress()) && browserWindow_ -> GetCharset(bstrCharset.GetAddress())) {
            std::string url(bstrUrl);
            std::string charset(bstrCharset);

            LOG_DEBUG << "document complete charset:" << charset << " url:" << url;

            if ("about:blank" != url && "" != callback) {
                IEMessage["url"] = url;
                IEMessage["html"] = browserWindow_ -> GetHtml();
                IEMessage["mime"] = std::string(bstrMime);
                IEMessage["charset"] = charset;
                IEMessage["event"] = "DocumentComplete";
                IEMessage["hwnd"] = browserWindow_ -> GetWindowHandleString();

                std::string msg = nlohmann::json(IEMessage).dump(-1, ' ', false, nlohmann::detail::error_handler_t::ignore);

                UrlParser parser;
                if( parser.parse(callback) ) {
                    httplib::Client cli(parser.hostname().c_str(), parser.httpPort());
                    auto res = cli.Post(parser.path().c_str(), msg.c_str(), "text/json");

                    LOG_DEBUG << "notify:" << callback << " code:" << (int) res->status;
                }
            }
        }
    } else if (dispId == DISPID_AMBIENT_DLCONTROL) {
        pVarResult->vt = VT_I4;
        pVarResult->lVal = DLCTL_DLIMAGES | DLCTL_VIDEOS | DLCTL_BGSOUNDS | DLCTL_SILENT;
    }

    return S_OK;
}
