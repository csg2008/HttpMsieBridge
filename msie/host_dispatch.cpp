// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "../defines.h"
#include "host_dispatch.h"
#include "browser_window.h"
#include <MsHtmdid.h>
#include "misc/logger.h"
#include "../settings.h"

HostDispatch::HostDispatch(BrowserWindow* inBrowserWindow)
        : browserWindow_(inBrowserWindow), ref(0) {
	idMap.insert(std::make_pair(L"execute", DISPID_USER_EXECUTE));
	idMap.insert(std::make_pair(L"writefile", DISPID_USER_WRITEFILE));
	idMap.insert(std::make_pair(L"readfile", DISPID_USER_READFILE));
	idMap.insert(std::make_pair(L"getevar", DISPID_USER_GETVAL));
	idMap.insert(std::make_pair(L"setevar", DISPID_USER_SETVAL));
}
HRESULT STDMETHODCALLTYPE HostDispatch::QueryInterface(
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObject) {
    *ppvObject = 0;
    return E_NOTIMPL;

/*
	*ppv = NULL;

	if (riid == IID_IUnknown || riid == IID_IDispatch) {
		*ppv = static_cast<IDispatch*>(this);
	}

	if (*ppv != NULL) {
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
*/
}
ULONG STDMETHODCALLTYPE HostDispatch::AddRef(void) {
    return 1;
}
ULONG STDMETHODCALLTYPE HostDispatch::Release(void) {
    return 1;
}
HRESULT STDMETHODCALLTYPE HostDispatch::GetTypeInfoCount(
        /* [out] */ UINT *pctinfo) {
    return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE HostDispatch::GetTypeInfo(
        /* [in] */ UINT iTInfo,
        /* [in] */ LCID lcid,
        /* [out] */ ITypeInfo **ppTInfo) {
    return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE HostDispatch::GetIDsOfNames(
        /* [in] */ REFIID riid,
        /* [in] */ LPOLESTR *rgszNames,
        /* [in] */ UINT cNames,
        /* [in] */ LCID lcid,
        /* [out] */ DISPID *rgDispId) {
    if (riid != IID_NULL)
        return DISP_E_UNKNOWNINTERFACE;
    for (UINT i = 0; i < cNames; i++) {
        rgDispId[i] = DISPID_UNKNOWN;
    }
    return DISP_E_UNKNOWNNAME;

/*
	HRESULT hr = S_OK;

	for (UINT i = 0; i < cNames; i++) {
		std::map<std::wstring, DISPID>::iterator iter = idMap.find(rgszNames[i]);
		if (iter != idMap.end()) {
			rgDispId[i] = iter->second;
		} else {
			rgDispId[i] = DISPID_UNKNOWN;
			hr = DISP_E_UNKNOWNNAME;
		}
	}

	return hr;
*/
}
HRESULT STDMETHODCALLTYPE HostDispatch::Invoke(
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

    if (riid != IID_NULL)
        return DISP_E_UNKNOWNINTERFACE;
    pExcepInfo = 0;
    puArgErr = 0;
    nlohmann::json* settings = GetApplicationSettings();
    bool silent_operations = (*settings)["msie"]["silent_operations"];

    // DLCTL_* constants:
    // http://msdn.microsoft.com/en-us/library/aa741313(v=vs.85).aspx
    // #Download_Control

    if (dispId == DISPID_AMBIENT_DLCONTROL) {
        FLOG_DEBUG << "DISPID_AMBIENT_DLCONTROL";
        // DLCTL_BGSOUNDS - The browsing component will play background
        //                  sounds associated with the document.
        // DLCTL_DLIMAGES - The browsing component will download images from
        //                  the server.
        // DLCTL_VIDEOS - The browsing component will play any video clips
        //                that are contained in the document.
        // DLCTL_PRAGMA_NO_CACHE - The browsing component will force the
        //                         request through to the server and ignore
        //                         the proxy, even if the proxy indicates that
        //                         the data is up to date.
        // DLCTL_RESYNCHRONIZE - The browsing component will ignore what is
        //                       in the cache and ask the server for updated
        //                       information. The cached information will be
        //                       used if the server indicates that the cached
        //                       information is up to date.
        // DLCTL_SILENT - The browsing component will not display any user
        //                interface.
        pVarResult->vt = VT_I4;
        pVarResult->lVal = DLCTL_DLIMAGES | DLCTL_VIDEOS
                | DLCTL_PRAGMA_NO_CACHE | DLCTL_RESYNCHRONIZE;
        if (silent_operations) {
            pVarResult->lVal |= DLCTL_SILENT;
        }
        return S_OK;
    }

    return DISP_E_MEMBERNOTFOUND;

/*
	if (wFlags & DISPATCH_METHOD) {
		HRESULT hr = S_OK;

		std::string *args = new std::string[pDispParams->cArgs];
		for (size_t i = 0; i < pDispParams->cArgs; ++i) {
			BSTR bstrArg = pDispParams->rgvarg[i].bstrVal;
			LPSTR arg = NULL;
			arg = BSTRToLPSTR(bstrArg, arg);
			args[pDispParams->cArgs - 1 - i] = arg; // also re-reverse order of arguments
			delete [] arg;
		}

		switch (dispIdMember) {
			case DISPID_USER_EXECUTE: {
				//LSExecute(NULL, args[0].c_str(), SW_NORMAL);

				break;
			}
			case DISPID_USER_WRITEFILE: {
				std::ofstream outfile;
				std::ios_base::openmode mode = std::ios_base::out;

				if (args[1] == "overwrite") {
					mode |= std::ios_base::trunc;
				} else if (args[1] == "append") {
					mode |= std::ios_base::app;
				}

				outfile.open(args[0].c_str());
				outfile << args[2];
				outfile.close();
				break;
			}
			case DISPID_USER_READFILE: {
				std::string buffer;
				std::string line;
				std::ifstream infile;
				infile.open(args[0].c_str());

				while(std::getline(infile, line)) {
					buffer += line;
					buffer += "\n";
				}

				int lenW = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buffer.c_str(), -1, NULL, 0);
				BSTR bstrRet = SysAllocStringLen(0, lenW - 1);
				MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buffer.c_str(), -1, bstrRet, lenW);

				pVarResult->vt = VT_BSTR;
				pVarResult->bstrVal = bstrRet;

				break;
			}
			case DISPID_USER_GETVAL: {
				char *buf = new char[256];
				strncpy(buf, values[args[0]].c_str(), 256);

				int lenW = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buf, -1, NULL, 0);
				BSTR bstrRet = SysAllocStringLen(0, lenW - 1);
				MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buf, -1, bstrRet, lenW);

				pVarResult->vt = VT_BSTR;
				pVarResult->bstrVal = bstrRet;

				break;
			}
			case DISPID_USER_SETVAL: {
				std::map<std::string, std::string>::iterator itr = values.find(args[0]);
				if (itr == values.end()) {
					values.insert(std::make_pair(args[0], args[1]));
				} else {
					values[args[0]] = args[1];
				}

				break;
			}
			default:
				hr = DISP_E_MEMBERNOTFOUND;
		}

		delete [] args;

		return hr;
	}

	return E_FAIL;
*/
}

char* HostDispatch::BSTRToLPSTR(BSTR bStr, LPSTR lpstr) {
	int lenW = SysStringLen(bStr);
	int lenA = WideCharToMultiByte(CP_ACP, 0, bStr, lenW, 0, 0, NULL, NULL);

	if (lenA > 0) {
		lpstr = new char[lenA + 1]; // allocate a final null terminator as well
		WideCharToMultiByte(CP_ACP, 0, bStr, lenW, lpstr, lenA, NULL, NULL);
		lpstr[lenA] = '\0'; // Set the null terminator yourself
	} else {
		lpstr = NULL;
	}

	return lpstr;
}