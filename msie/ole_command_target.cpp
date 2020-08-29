// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "../defines.h"
#include "ole_command_target.h"
#include "browser_window.h"
#include "misc/logger.h"

// This interface is NOT USED.
// See ole_client_site.cpp > QueryInterface().

OleCommandTarget::OleCommandTarget(BrowserWindow* browserWindowIn)
        : browserWindow_(browserWindowIn) {
}
HRESULT STDMETHODCALLTYPE OleCommandTarget::QueryInterface(
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObject) {
    return browserWindow_->GetOleClientSite()->QueryInterface(riid, ppvObject);
}
ULONG STDMETHODCALLTYPE OleCommandTarget::AddRef() {
    return 1;
}
ULONG STDMETHODCALLTYPE OleCommandTarget::Release() {
    return 1;
}
HRESULT STDMETHODCALLTYPE OleCommandTarget::QueryStatus(
        /* [in] */ const GUID *pguidCmdGroup,
        /* [in] */ ULONG cCmds,
        /* [out][in] */ OLECMD prgCmds[],
        /* [out][in] */ OLECMDTEXT *pCmdText) {
    if (prgCmds == NULL)
        return E_POINTER;
    bool cmdGroupFound = false;
    for (ULONG nCmd = 0; nCmd < cCmds; nCmd++) {
        FLOG_DEBUG << "OleCommandTarget::QueryStatus(): cmdID = " << prgCmds[nCmd].cmdID;
        prgCmds[nCmd].cmdf = 0;
    }
    return OLECMDERR_E_UNKNOWNGROUP;
}
HRESULT STDMETHODCALLTYPE OleCommandTarget::Exec(
        /* [in] */ const GUID *pguidCmdGroup,
        /* [in] */ DWORD nCmdID,
        /* [in] */ DWORD nCmdexecopt,
        /* [in] */ VARIANT *pvaIn,
        /* [out][in] */ VARIANT *pvaOut) {
    // FLOG_DEBUG << "OleCommandTarget::Exec(): nCmdID = " << nCmdID;

    HRESULT hr = S_OK;
	if (pguidCmdGroup && IsEqualGUID(*pguidCmdGroup, CGID_DocHostCommandHandler)) {

		switch (nCmdID)  {

		case OLECMDID_SHOWSCRIPTERROR:
			{
				IHTMLDocument2*             pDoc = NULL;
				IHTMLWindow2*               pWindow = NULL;
				IHTMLEventObj*              pEventObj = NULL;
				BSTR                        rgwszNames[5] = 
				{ 
					SysAllocString(L"errorLine"),
					SysAllocString(L"errorCharacter"),
					SysAllocString(L"errorCode"),
					SysAllocString(L"errorMessage"),
					SysAllocString(L"errorUrl")
				};
				DISPID                      rgDispIDs[5];
				VARIANT                     rgvaEventInfo[5];
				DISPPARAMS                  params;
				BOOL                        fContinueRunningScripts = true;
				int                         i;

				params.cArgs = 0;
				params.cNamedArgs = 0;

				// Get the document that is currently being viewed.
				hr = pvaIn->punkVal->QueryInterface(IID_IHTMLDocument2, (void **) &pDoc);    
				// Get document.parentWindow.
				hr = pDoc->get_parentWindow(&pWindow);
				pDoc->Release();
				// Get the window.event object.
				hr = pWindow->get_event(&pEventObj);
				// Get the error info from the window.event object.
				for (i = 0; i < 5; i++) {  
					// Get the property's dispID.
					hr = pEventObj->GetIDsOfNames(IID_NULL, &rgwszNames[i], 1, 
						LOCALE_SYSTEM_DEFAULT, &rgDispIDs[i]);
					// Get the value of the property.
					hr = pEventObj->Invoke(rgDispIDs[i], IID_NULL,
						LOCALE_SYSTEM_DEFAULT,
						DISPATCH_PROPERTYGET, &params, &rgvaEventInfo[i],
						NULL, NULL);
					SysFreeString(rgwszNames[i]);
				}

				// At this point, you would normally alert the user with 
				// the information about the error, which is now contained
				// in rgvaEventInfo[]. Or, you could just exit silently.

				(*pvaOut).vt = VT_BOOL;
				if (fContinueRunningScripts) {
					// Continue running scripts on the page.
					(*pvaOut).boolVal = VARIANT_TRUE;
				} else {
					// Stop running scripts on the page.
					(*pvaOut).boolVal = VARIANT_FALSE;   
				} 
				break;
			}
		default:
			hr = OLECMDERR_E_NOTSUPPORTED;
			break;
		}
    } else if (nCmdID == OLECMDID_CLOSE) {
        // Window is being closed.
        hr = OLECMDERR_E_UNKNOWNGROUP;
    } else {
		hr = OLECMDERR_E_UNKNOWNGROUP;
	}

	return (hr);
}
