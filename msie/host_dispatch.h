// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include "../defines.h"
#include <map>
#include <string>
#include <OAIdl.h>
class BrowserWindow;

class HostDispatch : public IDispatch {
private:
    BrowserWindow* browserWindow_;
    
    static const DISPID DISPID_USER_EXECUTE = DISPID_VALUE + 1;
    static const DISPID DISPID_USER_WRITEFILE = DISPID_VALUE + 2;
    static const DISPID DISPID_USER_READFILE = DISPID_VALUE + 3;
    static const DISPID DISPID_USER_GETVAL = DISPID_VALUE + 4;
    static const DISPID DISPID_USER_SETVAL = DISPID_VALUE + 5;

    long ref;
    std::map<std::wstring, DISPID> idMap;
    std::map<std::string, std::string> values;

    char *BSTRToLPSTR(BSTR bStr, LPSTR lpstr);

public:
    HostDispatch(BrowserWindow* inBrowserWindow);
    HRESULT STDMETHODCALLTYPE QueryInterface(
            /* [in] */ REFIID riid,
            /* [out] */ void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef(void);
    ULONG STDMETHODCALLTYPE Release(void);
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(
            /* [out] */ UINT *pctinfo);
    HRESULT STDMETHODCALLTYPE GetTypeInfo(
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(
            /* [in] */ REFIID riid,
            /* [in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [out] */ DISPID *rgDispId);
    HRESULT STDMETHODCALLTYPE Invoke(
            /* [in] */ DISPID dispId,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
};
