// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include "../defines.h"
#include <OAIdl.h>
#include <vector>
#include "JavaScriptFunction.h"

class BrowserWindow;

class ExternalDispatch : public IDispatch {
private:
    BrowserWindow* browserWindow_;
    std::vector<JavaScriptFunction> javaScriptFunctions;
public:
    ExternalDispatch(BrowserWindow* inBrowserWindow);
    void AddJavaScriptFunction(wchar_t * functionName, ApiToJavaScript cppFunctionPtr);
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


