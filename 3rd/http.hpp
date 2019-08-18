/*
// net11
// Simple embeddable C++11 async tcp,http and websocket serving.
//
// What is it?
// An easily embeddable C++11 networking library designed to make building application specific
// servers a breeze, small yet with a focus on enabling high performance with modern tools.

#include <iostream>
#include <http.hpp>

static net11::scheduler sched;

int main(int argc,char **argv) {
	net11::tcp listen;

	// start listening for http requests
	if (listen.listen(8090,
		// creates a new server instance once we've started listening
		net11::http::make_server(
			// the routing function
			[](net11::http::connection &c)->net11::http::action* {
				std::cout<<c.method()<<" on url:"<<c.url()<<"\n";

				// simple function returning data on a url
				if (c.url()=="/hello") {
					return net11::http::make_text_response(200,"Hello world!");
				}

				// for the correct url, try making a websocket echo service.
				if (c.url()=="/sockettest")
					if (auto r=net11::http::make_websocket(c,65536,[](net11::http::websocket &ws,std::vector<char> &msg){
						std::cout<<"WebSockMsg["<<std::string(msg.data(),msg.size())<<"]\n";

						// reply to the socket immediately
						std::string reply1="Immediate echo:"+std::string(msg.data(),msg.size());
						ws.send(reply1);

						// wait a second before sending the second reply
						// this method together with weak points can be used
						// for handling websocket updates from other systems
						auto sws=ws.shared_from_this();
						std::string reply2="Delayed echo:"+std::string(msg.data(),msg.size());
						sched.timeout(1000,[sws,reply2](){
							sws->send(reply2);
						});

						return true;
					})) {
						r->set_header("Sec-Websocket-Protocol","beta");
						return r;
					}

				if (c.url()=="/echo") {
					if (auto r=net11::http::make_websocket(c,16*1024*1024,[](net11::http::websocket &ws,std::vector<char> &msg) {
						ws.send(ws.get_input_type(),msg.data(),msg.size());
						return true;
					})) {
						return r;
					}
				}

				// change the / url to the indexpage
				if (c.url()=="/") {
					c.url()="/index.html";
				}

				// if no other urls has hit yet try running a file matching service.
				if (auto r=net11::http::match_file(c,"/","public_html/")) {
					return r;
				}

				// return null for a 404 response
				return nullptr;
			}
		)
	)) {
		printf("Error listening\n");
		return -1;
	}

	while(listen.poll()) {
		sched.poll();
		sched.yield();
	}

	return 0;
}
*/

#pragma once

#ifndef __HTTP_HPP__
#define __HTTP_HPP__

#include <tchar.h>
#include <string>
#include <map>
#include <algorithm>

#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <stdio.h>

#include <stdint.h>
#include <functional>
#include <exception>
#include <vector>
#include <array>
#include <utility>

#include <cstring>
#include <time.h>
#include <cctype>
#include <memory>
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>

#include "string_utils.h"

#pragma comment(lib,"wsock32.lib")
#pragma comment(lib, "WinInet.lib")

#ifdef _MSC_VER
#pragma warning(disable : 4312)
#pragma warning(disable : 4267)
#endif

#if defined(UNICODE) || defined(_UNICODE)
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

#ifndef HINTERNET
#define HINTERNET LPVOID
#endif

#define READ_BUFFER_SIZE        4096
#define DOWNLOAD_BUFFER_SIZE    8*1024*1024

#define IE8_USER_AGENT  _T("Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1;Trident/4.0)")

namespace net11 {
	enum DownloadState {
		DS_Loading = 0,
		DS_Failed,
		DS_Finished,
	};

	enum HttpInterfaceError {
		Hir_Success = 0,
		Hir_InitErr,
		Hir_ConnectErr,
		Hir_SendErr,
		Hir_QueryErr,
		Hir_404,
		Hir_IllegalUrl,
		Hir_CreateFileErr,
		Hir_DownloadErr,
		Hir_QueryIPErr,
		Hir_SocketErr,
		Hir_UserCancel,
		Hir_BufferErr,
		Hir_HeaderErr,
		Hir_ParamErr,
		Hir_UnknowErr,
	};

	enum HTTP_REQ_METHOD {
		REQ_METHOD_GET,
		REQ_METHOD_POST,
	};

	enum HTTP_STATUS_EVENT  {
		CONNECTED_EVENT,
		REQUEST_OPENED_EVENT,
		REQUEST_COMPLETE_EVENT,
		USER_CANCEL_EVENT
	};

	typedef struct ProxyConfig {
		ProxyConfig() : auto_detect_settings(false) {};

		bool enable;
		bool auto_detect_settings;
		std::wstring auto_config_url;

		std::wstring proxy_server;
		std::wstring bypass_domain_list;

	} ProxyConfig;

	class ie_proxy {
	public:
		bool setProxyConfig(ProxyConfig *config){
			bool result;

			INTERNET_PER_CONN_OPTION options[] = {
				{ INTERNET_PER_CONN_FLAGS, 0 },
				{ INTERNET_PER_CONN_AUTOCONFIG_URL, 0 },
				{ INTERNET_PER_CONN_PROXY_SERVER, 0 },
				{ INTERNET_PER_CONN_PROXY_BYPASS, 0 },
			};

			INTERNET_PER_CONN_OPTION_LIST list;

			int list_size = sizeof(list);

			list.dwSize = list_size;
			list.pszConnection = NULL;
			list.dwOptionCount = 4;
			list.dwOptionError = 0;
			list.pOptions = options;//new INTERNET_PER_CONN_OPTION[3];

			// pointer to options
			//INTERNET_PER_CONN_OPTION *options = list.pOptions;

			// setting INTERNET_PER_CONN_FLAGS
			options[0].Value.dwValue = PROXY_TYPE_DIRECT;

			if (!config->auto_config_url.empty()) {
				// append INTERNET_PER_CONN_AUTOCONFIG_URL to INTERNET_PER_CONN_FLAGS 
				options[0].Value.dwValue |= PROXY_TYPE_AUTO_PROXY_URL;

				// pszValue takes in LPTSTR 
				options[1].Value.pszValue = (TCHAR*)config->auto_config_url.c_str();
			}

			if (config->auto_detect_settings) {
				// setting INTERNET_PER_CONN_FLAGS
				options[0].Value.dwValue |= PROXY_TYPE_AUTO_DETECT;
			}

			if (!config->proxy_server.empty()) {
				// setting INTERNET_PER_CONN_FLAGS
				options[0].Value.dwValue |= PROXY_TYPE_PROXY;

				// setting INTERNET_PER_CONN_PROXY_SERVER
				options[2].Value.pszValue = (TCHAR*)config->proxy_server.c_str();
			}

			if (!config->bypass_domain_list.empty()) {
				// INTERNET_PER_CONN_PROXY_BYPASS
				options[3].Value.pszValue = (TCHAR*)config->bypass_domain_list.c_str();
			}

			result = InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, list_size);
			//delete[] list.pOptions;

			return result;
		}

		bool getProxyConfig(ProxyConfig *config){

			INTERNET_PER_CONN_OPTION options[] = {
				{ INTERNET_PER_CONN_FLAGS, 0 },
				{ INTERNET_PER_CONN_AUTOCONFIG_URL, 0 },
				{ INTERNET_PER_CONN_PROXY_SERVER, 0 },
				{ INTERNET_PER_CONN_PROXY_BYPASS, 0 },
			};

			INTERNET_PER_CONN_OPTION_LIST list;

			DWORD size = sizeof(INTERNET_PER_CONN_OPTION_LIST);

			list.dwSize = size;
			list.pszConnection = NULL;
			list.dwOptionCount = 4;
			list.dwOptionError = 0;
			list.pOptions = options;

			if (InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size)) {
				DWORD flags = list.pOptions[0].Value.dwValue;

				if (flags & PROXY_TYPE_AUTO_DETECT) {
					config->auto_detect_settings = true;
				}

				if (flags & INTERNET_PER_CONN_AUTOCONFIG_URL) {
					config->auto_config_url = std::wstring(list.pOptions[1].Value.pszValue);
				}

				if (flags & INTERNET_PER_CONN_PROXY_SERVER) {
					config->proxy_server = list.pOptions[2].Value.pszValue;
				}

				if (flags & INTERNET_PER_CONN_PROXY_BYPASS) {
					wchar_t *url_list = list.pOptions[3].Value.pszValue;

					if (url_list != NULL) {
						config->bypass_domain_list = std::wstring(url_list);
					}
				}

				return true;
			}

			return false;
		}

		bool refreshSettings(){

			InternetSetOption(0, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
			InternetSetOption(0, INTERNET_OPTION_REFRESH, NULL, 0);

			return true;
		}

		bool IsProxy() {
			INTERNET_PER_CONN_OPTION_LIST    List;
			INTERNET_PER_CONN_OPTION         Option[1];
			unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

			Option[0].dwOption = INTERNET_PER_CONN_FLAGS;

			List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
			List.pszConnection = NULL;
			List.dwOptionCount = 1;
			List.dwOptionError = 0;
			List.pOptions = Option;


			HINTERNET Internet = InternetOpen(IE8_USER_AGENT, 1, _T("") ,_T("") ,0);

			InternetQueryOption(Internet, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &nSize);
			if ((Option[0].Value.dwValue &  PROXY_TYPE_PROXY) == PROXY_TYPE_PROXY) {
				return TRUE;
			}
			InternetCloseHandle(Internet);
			return FALSE;
		}

		bool SetProxy(bool bEnabled,std::wstring strProxyServer)   {  
			HKEY hKey = NULL;  

			LONG lret = RegOpenKeyEx(HKEY_CURRENT_USER,  
				_T("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"),  
				NULL,  
				KEY_WRITE |  
				KEY_SET_VALUE,  
				&hKey);  
			if(hKey == NULL || lret != ERROR_SUCCESS)   {  
				return FALSE;  
			}  

			// enable or disable  
			if(bEnabled)   {  
				lret = RegSetValueEx(hKey,  
					_T("ProxyServer"),  
					NULL,  
					REG_SZ,  
					(BYTE*)strProxyServer.c_str(),  
					strProxyServer.length()*2);  

				DWORD dwenable = 1;  
				lret = RegSetValueEx(hKey,  
					_T("ProxyEnable"),  
					NULL,  
					REG_DWORD,  
					(LPBYTE) & dwenable,  
					sizeof(dwenable));  
			} else {  
				DWORD dwenable = 0;  
				lret = RegSetValueEx(hKey,  
					_T("ProxyEnable"),  
					NULL,  
					REG_DWORD,  
					(LPBYTE) & dwenable,  
					sizeof(dwenable));  
			}  
			RegCloseKey(hKey);  

			return TRUE;  
		}
	};

	class IHttpCallback {
	public:
		virtual void    OnDownloadCallback(void* pParam, DownloadState state, double nTotalSize, double nLoadSize)      = 0;
		virtual bool    IsNeedStop()                                                                                    = 0;
	};

	class CHttpClient {
	public:
		CHttpClient(void) {
			m_lpParam = NULL;
			m_error = Hir_Success;
			m_pCallback = NULL;
			m_hSession = NULL;
			m_hInternet = NULL;
			m_hConnect = NULL;
			m_hRequest = NULL;

			m_hCompleteEvent = NULL;
			m_hCancelEvent = NULL;

			m_dwConnectTimeOut = 60 * 1000;
			m_dwContext = 0;
		}

		void SetConnectTimeOut(DWORD dwTimeOut) {
			m_dwConnectTimeOut = dwTimeOut;
		}

		void SetCancalEvent() {
			if (m_hCancelEvent != NULL) {
				::SetEvent(m_hCancelEvent);
			}
		}

		void SetProxy(LPCTSTR lpszServer, WORD nPort, LPCTSTR lpszUser/* = NULL*/, LPCTSTR lpszPwd/* = NULL*/) {
			if (NULL == lpszServer)
				return;

			TCHAR szProxy[INTERNET_MAX_URL_LENGTH] = { 0 };
			wsprintf(szProxy, _T("%s:%d"), lpszServer, nPort);
			m_strProxy = szProxy;
			if (lpszUser != NULL)
				m_strUser = lpszUser;

			if (lpszPwd != NULL)
				m_strPwd = lpszPwd;
		}

		void CloseRequest() {
			if (m_hCompleteEvent != NULL) {
				::CloseHandle(m_hCompleteEvent);
				m_hCompleteEvent = NULL;
			}

			if (m_hCancelEvent != NULL) {
				::CloseHandle(m_hCancelEvent);
				m_hCancelEvent = NULL;
			}

			ReleaseHandle(m_hRequest);
			ReleaseHandle(m_hConnect);
			ReleaseHandle(m_hSession);
			ReleaseHandle(m_hInternet);

			m_arrRespHeader.clear();

			m_dwContext = 0;
			m_strProxy = _T("");
			m_strUser = _T("");
			m_strPwd = _T("");
		}
		
		~CHttpClient(void) {
			CloseRequest();
		}

		/// <summary>
		/// Global callback handler. Receives WinInet data and executes the instance callback
		/// </summary>
		/// <param name="hInternet">Handle to WinINet</param>
		/// <param name="dwContext">call context - holds an instance pointer to the Inet class</param>
		/// <param name="dwInternetStatus">Status of the call</param>
		/// <param name="lpvStatusInformation">Pointer to more status data</param>
		/// <param name="dwStatusInformationLength">The size of the status information</param>
		static VOID CALLBACK StatusCallback(HINTERNET hInternet, DWORD_PTR dwContext,  DWORD dwInternetStatus, LPVOID lpStatusInfo, DWORD dwStatusInfoLen) {
			CHttpClient* lpThis = (CHttpClient *)dwContext;
			_ASSERT_EXPR(lpThis != NULL, L"context response from WinInet cannot be NULL");
			if (NULL == lpThis) {
				return;
			}

			std::wstringstream wsOut;

			wsOut << L"Instance Callback: ";

			switch (dwInternetStatus) {
			case INTERNET_STATUS_HANDLE_CREATED:
			{

				if (CONNECTED_EVENT == lpThis->m_dwContext) {

					INTERNET_ASYNC_RESULT * lpRes = (INTERNET_ASYNC_RESULT *)lpStatusInfo;

					lpThis->m_hConnect = (HINTERNET)lpRes->dwResult;

					::SetEvent(lpThis->m_hCompleteEvent);

				} else if (REQUEST_OPENED_EVENT == lpThis->m_dwContext) {
					INTERNET_ASYNC_RESULT * lpRes = (INTERNET_ASYNC_RESULT *)lpStatusInfo;
					lpThis->m_hRequest = (HINTERNET)lpRes->dwResult;
					::SetEvent(lpThis->m_hCompleteEvent);
				}
			}

				break;

			case INTERNET_STATUS_REQUEST_SENT:
			{
				DWORD * lpBytesSent = (DWORD *)lpStatusInfo;
			}

				break;

			case INTERNET_STATUS_REQUEST_COMPLETE:
			{

				INTERNET_ASYNC_RESULT * lpRes = (INTERNET_ASYNC_RESULT *)lpStatusInfo;
				::SetEvent(lpThis->m_hCompleteEvent);
			}

				break;

			case INTERNET_STATUS_RECEIVING_RESPONSE:
			{

			}
				break;

			case INTERNET_STATUS_RESPONSE_RECEIVED:
			{
				wsOut << L"Status: Response Received";
				DWORD * dwBytesReceived = (DWORD *)lpStatusInfo;
			}
				break;

			case INTERNET_STATUS_COOKIE_SENT:
				OutputDebugStringW(L"Status: Cookie found and will be sent with request\n");
				break;
				
			case INTERNET_STATUS_COOKIE_RECEIVED:
				OutputDebugStringW(L"Status: Cookie Received\n");
				break;
				
			case INTERNET_STATUS_COOKIE_HISTORY:
				OutputDebugStringW(L"Status: Cookie History\n");
				break;

			default:
				wsOut << L"Other: " << dwInternetStatus;
				break;
			}

			wsOut << L"\n";

			OutputDebugStringW(wsOut.str().c_str());
		}
		
		BOOL OpenRequest(LPCTSTR lpszUrl, HTTP_REQ_METHOD nReqMethod/* = REQ_METHOD_GET*/) {
			BOOL flag = false;

			try {
				BOOL fSecureHTTPS = false;
				TCHAR szScheme[INTERNET_MAX_URL_LENGTH] = { 0 };
				TCHAR szHostName[INTERNET_MAX_URL_LENGTH] = { 0 };
				TCHAR szUrlPath[INTERNET_MAX_URL_LENGTH] = { 0 };

				WORD nPort = 0;
				DWORD dwAccessType;

				LPCTSTR lpszProxy;
				BOOL bRet = ParseURL(lpszUrl, szScheme, INTERNET_MAX_URL_LENGTH, szHostName, INTERNET_MAX_URL_LENGTH, nPort, szUrlPath, INTERNET_MAX_URL_LENGTH, fSecureHTTPS);
				if (!bRet) {
					return FALSE;
				}

				m_hCompleteEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
				m_hCancelEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

				if (NULL == m_hCompleteEvent || NULL == m_hCancelEvent) {
					CloseRequest();
					return FALSE;
				}

				if (m_strProxy.size() > 0) {
					lpszProxy    = m_strProxy.c_str();
					dwAccessType = INTERNET_OPEN_TYPE_PROXY;
				} else {
					lpszProxy    = NULL;
					dwAccessType = INTERNET_OPEN_TYPE_PRECONFIG;
				}

				if (!InternetCheckConnection(lpszUrl, FLAG_ICC_FORCE_CONNECTION, 0)) {
					return false;
				}

				m_hInternet = ::InternetOpen(
					IE8_USER_AGENT,              // User Agent
					dwAccessType,                // Preconfig or Proxy
					lpszProxy,                   // Proxy name
					NULL,                        // Proxy bypass, do not bypass any address
					INTERNET_FLAG_ASYNC);        // 0 for Synchronous
				if (NULL == m_hInternet) {
					CloseRequest();
					return FALSE;
				}

				if (!m_strUser.empty()) {
					::InternetSetOptionEx(m_hInternet, INTERNET_OPTION_PROXY_USERNAME, (LPVOID)m_strUser.c_str(), m_strUser.size() + 1, 0);
				}

				if (!m_strPwd.empty()) {
					::InternetSetOptionEx(m_hInternet, INTERNET_OPTION_PROXY_PASSWORD, (LPVOID)m_strPwd.c_str(), m_strPwd.size() + 1, 0);
				}

				// DWORD m_dwConnectTimeOut;
				// DWORD dwSize = sizeof(m_dwConnectTimeOut);
				// ::InternetQueryOption(m_hInternet,INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&m_dwConnectTimeOut, &dwSize);

				// Set the status callback for the handle to the Callback function
				INTERNET_STATUS_CALLBACK lpCallBackFunc;
				lpCallBackFunc = ::InternetSetStatusCallback(m_hInternet, (INTERNET_STATUS_CALLBACK)&StatusCallback);
				if (INTERNET_INVALID_STATUS_CALLBACK == lpCallBackFunc) {
					CloseRequest();
					return FALSE;
				}

				m_dwContext = CONNECTED_EVENT;
				m_hConnect = ::InternetConnect(m_hInternet, szHostName, nPort, NULL, _T("HTTP/1.1"), INTERNET_SERVICE_HTTP, (fSecureHTTPS? INTERNET_FLAG_SECURE:0), (DWORD_PTR)this);
				if (NULL == m_hConnect) {
					if (::GetLastError() != ERROR_IO_PENDING) {
						CloseRequest();
						return FALSE;
					}
				}

				bRet = WaitForEvent(CONNECTED_EVENT, m_dwConnectTimeOut);
				if (!bRet) {
					CloseRequest();
					return FALSE;
				}

				TCHAR* lpMethod;
				if (nReqMethod == REQ_METHOD_GET) {
					lpMethod = _T("GET");
				} else {
					lpMethod = _T("POST");
				}

				DWORD dwFlags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
				if (INTERNET_DEFAULT_HTTPS_PORT == nPort) {
					dwFlags |= INTERNET_FLAG_SECURE;
				}

				LPCTSTR lpszReferrer = NULL;
				LPCTSTR *lpszAcceptTypes = NULL;
				m_dwContext = REQUEST_OPENED_EVENT;
				m_hRequest = ::HttpOpenRequest(m_hConnect, lpMethod, szUrlPath, _T("HTTP/1.1"), lpszReferrer, lpszAcceptTypes, dwFlags, (DWORD_PTR)this);
				if (NULL == m_hRequest) {
					if (::GetLastError() != ERROR_IO_PENDING) {
						CloseRequest();
						return FALSE;
					}
				}

				bRet = WaitForEvent(REQUEST_OPENED_EVENT, INFINITE);
				if (!bRet) {
					CloseRequest();
					return FALSE;
				}

				flag = true;
			} catch(HttpInterfaceError error) {
				flag = false;
				m_error = error;
			}
			
			return flag;
		}
		
		BOOL AddReqHeaders(LPCTSTR lpHeaders) {
			if (NULL == m_hRequest || NULL == lpHeaders) {
				return FALSE;
			}

			return ::HttpAddRequestHeaders(m_hRequest, lpHeaders, _tcslen(lpHeaders), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
		}

		BOOL SendRequest(const CHAR * lpData, DWORD dwLen) {
			if (NULL == m_hRequest)
				return FALSE;

			BOOL bRet = ::HttpSendRequest(m_hRequest, NULL, 0, (LPVOID)lpData, dwLen);
			if (!bRet) {
				if (::GetLastError() != ERROR_IO_PENDING) {
					return FALSE;
				}
			}

			bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
			if (!bRet) {
				return FALSE;
			}

			return TRUE;
		}

		BOOL SendRequestEx(DWORD dwLen) {
			if (NULL == m_hRequest) {
				return FALSE;
			}

			INTERNET_BUFFERS stInetBuf = { 0 };
			stInetBuf.dwStructSize = sizeof(INTERNET_BUFFERS);
			stInetBuf.dwBufferTotal = dwLen;

			BOOL bRet = ::HttpSendRequestEx(m_hRequest, &stInetBuf, NULL, 0, (DWORD_PTR)this);
			if (!bRet) {
				if (::GetLastError() != ERROR_IO_PENDING) {
					return FALSE;
				}

				bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
				if (!bRet) {
					return FALSE;
				}
			}

			return TRUE;
		}
		
		BOOL SendReqBodyData(const CHAR * lpBuf, DWORD dwLen, DWORD& dwSendLen) {
			if (NULL == m_hRequest || NULL == lpBuf || dwLen <= 0)
				return FALSE;

			dwSendLen = 0;
			BOOL bRet = ::InternetWriteFile(m_hRequest, lpBuf, dwLen, &dwSendLen);
			if (!bRet) {
				if (::GetLastError() != ERROR_IO_PENDING)
					return FALSE;

				bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
				if (!bRet)
					return FALSE;
			} else {
				bRet = WaitForEvent(USER_CANCEL_EVENT, 0);
				if (!bRet)
					return FALSE;
			}

			return TRUE;
		}
		
		BOOL EndSendRequest() {
			BOOL bRet;
			if (NULL == m_hRequest)
				return FALSE;

			bRet = ::HttpEndRequest(m_hRequest, NULL, HSR_INITIATE, (DWORD_PTR)this);
			if (!bRet) {
				if (::GetLastError() != ERROR_IO_PENDING)
					return FALSE;

				bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
				if (!bRet)
					return FALSE;
			}

			return TRUE;
		}
		
		DWORD GetRespCode() {
			DWORD dwRespCode = 0;
			DWORD dwSize = sizeof(dwRespCode);
			BOOL bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwRespCode, &dwSize, NULL);
			if (bRet)
				return dwRespCode;
			else
				return 0;
		}

		tstring GetRespHeader() {
			CHAR * lpRespHeader = NULL;
			DWORD dwRespHeaderLen = 0;
			tstring strRespHeader;
			BOOL bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpRespHeader, &dwRespHeaderLen, NULL);
			if (!bRet) {
				if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
					lpRespHeader = new CHAR[dwRespHeaderLen];
					if (lpRespHeader != NULL) {
						memset(lpRespHeader, 0, dwRespHeaderLen);
						bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpRespHeader, &dwRespHeaderLen, NULL);
						if (bRet) {
							strRespHeader = (TCHAR *)lpRespHeader;
						}
					}
				}
			}

			if (lpRespHeader != NULL) {
				delete[]lpRespHeader;
				lpRespHeader = NULL;
			}

			return strRespHeader;
		}

		tstring GetRespHeader(LPCTSTR lpszName, int nIndex/* = 0*/) {
			if (NULL == lpszName)
				return _T("");

			tstring strLine;
			int nNameLen, nIndex2 = 0;
			nNameLen = _tcslen(lpszName);
			if (nNameLen <= 0)
				return _T("");

			if (m_arrRespHeader.size() <= 0) {
				if (!__GetRespHeader())
					return _T("");
			}

			for (int i = 0; i < (int)m_arrRespHeader.size(); i++) {
				strLine = m_arrRespHeader[i];
				if (!_tcsnicmp(strLine.c_str(), lpszName, nNameLen)) {
					if (nIndex == nIndex2) {
						int nPos = nNameLen;
						int nLineLen = (int)strLine.size();

						for (; nPos < nLineLen &&strLine[nPos] == _T(' '); ++nPos)
							;
						if (strLine[nPos] == _T(':'))
							nPos++;

						for (; nPos < nLineLen &&strLine[nPos] == _T(' '); ++nPos)
							;

						return strLine.substr(nPos);
					}

					nIndex2++;
				}
			}

			return _T("");
		}

		int GetRespHeaderByInt(LPCTSTR lpszName, int nIndex/* = 0*/) {
			tstring strValue = GetRespHeader(lpszName, nIndex);
			return _tstoi(strValue.c_str());
		}
		
		BOOL GetRespBodyData(CHAR * lpBuf, DWORD dwLen, DWORD& dwRecvLen) {
			if (NULL == m_hRequest || NULL == lpBuf || dwLen <= 0)
				return FALSE;

			INTERNET_BUFFERSA stInetBuf = { 0 };
			BOOL bRet;
			dwRecvLen = 0;
			memset(lpBuf, 0, dwLen);

			stInetBuf.dwStructSize = sizeof(stInetBuf);
			stInetBuf.lpvBuffer = lpBuf;
			stInetBuf.dwBufferLength = dwLen;

			bRet = ::InternetReadFileExA(m_hRequest, &stInetBuf, 0, (DWORD_PTR)this);
			if (!bRet) {
				if (::GetLastError() != ERROR_IO_PENDING)
					return FALSE;

				bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
				if (!bRet)
					return FALSE;
			} else {
				bRet = WaitForEvent(USER_CANCEL_EVENT, 0);
				if (!bRet)
					return FALSE;
			}

			dwRecvLen = stInetBuf.dwBufferLength;

			return TRUE;
		}

		virtual bool DownloadFile(LPCWSTR lpUrl, LPCWSTR lpFilePath) {
			bool bResult = false;
			BYTE* pBuffer = NULL;
			try {
				if ( NULL == lpUrl || wcslen(lpUrl) == 0 )
					throw Hir_IllegalUrl;
				CloseRequest();
				m_hSession = InternetOpen(L"Http-connect", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
				if ( NULL == m_hSession ) throw Hir_InitErr;

				WORD nPort = 0;
				BOOL fSecureHTTPS = false;
				TCHAR szScheme[INTERNET_MAX_URL_LENGTH] = { 0 };
				TCHAR szHostName[INTERNET_MAX_URL_LENGTH] = { 0 };
				TCHAR szUrlPath[INTERNET_MAX_URL_LENGTH] = { 0 };

				BOOL bRet = ParseURL(lpUrl, szScheme, INTERNET_MAX_URL_LENGTH, szHostName, INTERNET_MAX_URL_LENGTH, nPort, szUrlPath, INTERNET_MAX_URL_LENGTH, fSecureHTTPS);
				if (!bRet) {
					return FALSE;
				}

				m_hConnect = InternetConnectW(m_hSession, szHostName, nPort, NULL, NULL, INTERNET_SERVICE_HTTP, NULL, NULL);
				if ( NULL == m_hConnect ) throw Hir_ConnectErr;
				m_hRequest = HttpOpenRequestW(m_hConnect, L"GET", szUrlPath, L"HTTP/1.1", NULL, NULL, INTERNET_FLAG_RELOAD, NULL);
				if ( NULL == m_hRequest ) throw Hir_InitErr;
				bRet = HttpSendRequestW(m_hRequest, NULL, 0, NULL, 0);
				if ( !bRet ) throw Hir_SendErr;
				char szBuffer[1024+1] = {0};
				DWORD dwReadSize = 1024;
				bRet = HttpQueryInfoA(m_hRequest, HTTP_QUERY_RAW_HEADERS, szBuffer, &dwReadSize, NULL);
				if ( !bRet ) throw Hir_QueryErr;
				std::string strRetHeader = szBuffer;
				if ( std::string::npos != strRetHeader.find("404") ) throw Hir_404;
				dwReadSize = 1024;
				bRet = HttpQueryInfoA(m_hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuffer, &dwReadSize, NULL);
				if ( !bRet ) throw Hir_QueryErr;
				szBuffer[dwReadSize] = '\0';
				const double uFileSize = atof(szBuffer);
				int nMallocSize = uFileSize<DOWNLOAD_BUFFER_SIZE? (int)uFileSize:DOWNLOAD_BUFFER_SIZE;
				pBuffer = (BYTE*)malloc(nMallocSize);
				int nFindPos = 0;
				std::wstring strSavePath(lpFilePath);
				while( std::wstring::npos != (nFindPos=strSavePath.find(L"\\", nFindPos)) ) {
					std::wstring strChildPath=strSavePath.substr(0, nFindPos);
					if (INVALID_FILE_ATTRIBUTES == ::GetFileAttributes(strChildPath.c_str()))
						CreateDirectory(strChildPath.c_str(), NULL);
					nFindPos++;
				}
				FILE* fp = NULL;
				_wfopen_s(&fp, strSavePath.c_str(), L"wb+");
				if ( NULL == fp ) throw Hir_CreateFileErr;
				double uWriteSize = 0;
				while( true ) {
					bRet = InternetReadFile(m_hRequest, pBuffer, nMallocSize, &dwReadSize);
					if ( !bRet || (0 == dwReadSize) )
						break;
					fwrite(szBuffer, dwReadSize, 1, fp);
					uWriteSize += dwReadSize;
					if ( m_pCallback )
						m_pCallback->OnDownloadCallback(m_lpParam, DS_Loading, uFileSize, uWriteSize);
				}
				fclose(fp);
				if ( uFileSize!=uWriteSize ) throw Hir_DownloadErr;
				if ( m_pCallback )
					m_pCallback->OnDownloadCallback(m_lpParam, DS_Finished, uFileSize, uWriteSize);
				bResult = true;
			} catch( HttpInterfaceError error ) {
				m_error = error;
				if ( m_pCallback )
					m_pCallback->OnDownloadCallback(m_lpParam, DS_Failed, 0, 0);
			}
			if ( pBuffer ) {
				free(pBuffer);
				pBuffer = NULL;
			}
			return bResult;
		}
		
		virtual bool DownloadToMem( LPCWSTR lpUrl, OUT void** ppBuffer, OUT int* nSize ) {
			bool bResult = false;
			BYTE *pDesBuffer = NULL;
			try {
				if ( NULL == lpUrl || wcslen(lpUrl) == 0 )
					throw Hir_IllegalUrl;
				CloseRequest();
				m_hSession = InternetOpen(L"Http-connect", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
				if ( NULL == m_hSession ) throw Hir_InitErr;

				WORD nPort = 0;
				BOOL fSecureHTTPS = false;
				TCHAR szScheme[INTERNET_MAX_URL_LENGTH] = { 0 };
				TCHAR szHostName[INTERNET_MAX_URL_LENGTH] = { 0 };
				TCHAR szUrlPath[INTERNET_MAX_URL_LENGTH] = { 0 };

				BOOL bRet = ParseURL(lpUrl, szScheme, INTERNET_MAX_URL_LENGTH, szHostName, INTERNET_MAX_URL_LENGTH, nPort, szUrlPath, INTERNET_MAX_URL_LENGTH, fSecureHTTPS);
				if (!bRet) {
					return FALSE;
				}

				m_hConnect = InternetConnectW(m_hSession, szHostName, nPort, NULL, NULL, INTERNET_SERVICE_HTTP, NULL, NULL);

				if ( NULL == m_hConnect ) throw Hir_ConnectErr;
				m_hRequest = HttpOpenRequestW(m_hConnect, L"GET", szUrlPath, L"HTTP/1.1", NULL, NULL, INTERNET_FLAG_RELOAD, NULL);
				if ( NULL == m_hRequest ) throw Hir_InitErr;
				bRet = HttpSendRequestW(m_hRequest, NULL, 0, NULL, 0);
				if ( !bRet ) throw Hir_SendErr;
				wchar_t szBuffer[1024+1] = {0};
				DWORD dwReadSize = 1024;
				bRet = HttpQueryInfoW(m_hRequest, HTTP_QUERY_RAW_HEADERS, szBuffer, &dwReadSize, NULL);
				if ( !bRet ) throw Hir_QueryErr;
				std::wstring strRetHeader(szBuffer);
				if ( std::string::npos != strRetHeader.find(L"404") ) throw Hir_404;
				dwReadSize = 1024;
				bRet = HttpQueryInfoW(m_hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuffer, &dwReadSize, NULL);
				if ( !bRet ) throw Hir_QueryErr;
				szBuffer[dwReadSize] = '\0';
				const int uFileSize = _wtoi(szBuffer);
				if ( uFileSize>DOWNLOAD_BUFFER_SIZE || uFileSize<0 )
					throw Hir_BufferErr;
				pDesBuffer = (BYTE*)malloc(uFileSize);
				int uWriteSize = 0;
				while( true ) {
					bRet = InternetReadFile(m_hRequest, pDesBuffer, uFileSize, &dwReadSize);
					if ( !bRet || (0 == dwReadSize) )
						break;
					uWriteSize += dwReadSize;
				}
				if ( uFileSize != uWriteSize )
					throw Hir_DownloadErr;
				*ppBuffer = pDesBuffer;
				*nSize = uWriteSize;
				bResult = true;
			} catch( HttpInterfaceError error ) {
				m_error = error;
				if ( pDesBuffer ) {
					free(pDesBuffer);
					pDesBuffer = NULL;
				}
			}
			return bResult;
		}

		virtual void SetDownloadCallback(IHttpCallback* pCallback, void* pParam) {
			m_pCallback = pCallback;
			m_lpParam = pParam;
		}

		virtual HttpInterfaceError GetErrorCode() {
			return m_error;
		}

		virtual void FreeInstance() {
			delete this;
		}

		void ReleaseHandle( HINTERNET& hInternet ) {
			if (hInternet) {
				InternetCloseHandle(hInternet);
				hInternet = NULL;
			}
		}

		BOOL ParseURL(LPCTSTR lpszUrl, LPTSTR lpszScheme, DWORD dwSchemeLength, LPTSTR lpszHostName, DWORD dwHostNameLength, WORD& nPort, LPTSTR lpszUrlPath, DWORD dwUrlPathLength, BOOL& fSecureHTTPS) {
			URL_COMPONENTS stUrlComponents = { 0 };
			stUrlComponents.dwStructSize = sizeof(URL_COMPONENTS);
			stUrlComponents.lpszScheme = lpszScheme;
			stUrlComponents.dwSchemeLength = dwSchemeLength;
			stUrlComponents.lpszHostName = lpszHostName;
			stUrlComponents.dwHostNameLength = dwHostNameLength;
			stUrlComponents.lpszUrlPath = lpszUrlPath;
			stUrlComponents.dwUrlPathLength = dwUrlPathLength;

			BOOL bRet = ::InternetCrackUrl(lpszUrl, wcslen(lpszUrl), 0, &stUrlComponents);
			if (bRet) {
				nPort = stUrlComponents.nPort;

				if (stUrlComponents.lpszScheme == NULL) {
					fSecureHTTPS = false;
				} else {
					fSecureHTTPS = (!_wcsnicmp(stUrlComponents.lpszScheme, L"https", stUrlComponents.dwSchemeLength)) && (5==stUrlComponents.dwSchemeLength);
				}
			}

			return bRet;
		}

		BOOL WaitForEvent(HTTP_STATUS_EVENT nEvent, DWORD dwTimeOut) {
			HANDLE hEvent[2] = { 0 };
			int nCount = 0;
			switch (nEvent) {
			case CONNECTED_EVENT:
			case REQUEST_OPENED_EVENT:
			case REQUEST_COMPLETE_EVENT:
			{
				hEvent[0] = m_hCancelEvent;
				hEvent[1] = m_hCompleteEvent;
				nCount = 2;
			}
				break;


			case USER_CANCEL_EVENT:
			{
				hEvent[0] = m_hCancelEvent;
				nCount = 1;
			}
				break;


			default:
				return FALSE;
			}

			if (1 == nCount) {
				DWORD dwRet = ::WaitForSingleObject(hEvent[0], dwTimeOut);
				if (WAIT_OBJECT_0 == dwRet)
					return FALSE;
				else
					return TRUE;
			} else {
				DWORD dwRet = ::WaitForMultipleObjects(2, hEvent, FALSE, dwTimeOut);
				if (dwRet != WAIT_OBJECT_0 + 1)
					return FALSE;
				else
					return TRUE;
			}
		}

		DWORD __GetRespHeaderLen() {
			BOOL bRet;
			LPVOID lpBuffer = NULL;
			DWORD dwBufferLength = 0;
			bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, lpBuffer, &dwBufferLength, NULL);
			if (!bRet) {
				if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
					return dwBufferLength;
				}
			}

			return 0;
		}

		BOOL __GetRespHeader() {
			CHAR * lpRespHeader;
			DWORD dwRespHeaderLen;
			m_arrRespHeader.clear();

			dwRespHeaderLen = __GetRespHeaderLen();
			if (dwRespHeaderLen <= 0)
				return FALSE;

			lpRespHeader = new CHAR[dwRespHeaderLen];
			if (NULL == lpRespHeader)
				return FALSE;

			memset(lpRespHeader, 0, dwRespHeaderLen);

			BOOL bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpRespHeader, &dwRespHeaderLen, NULL);
			if (!bRet) {
				delete[]lpRespHeader;
				lpRespHeader = NULL;
				return FALSE;
			}

			tstring strHeader = (TCHAR *)lpRespHeader;
			tstring strLine;
			int nStart = 0;
			tstring::size_type nPos = strHeader.find(_T("\r\n"), nStart);
			while (nPos != tstring::npos) {
				strLine = strHeader.substr(nStart, nPos - nStart);
				if (strLine != _T(""))
					m_arrRespHeader.push_back(strLine);

				nStart = nPos + 2;
				nPos = strHeader.find(_T("\r\n"), nStart);
			}

			delete[]lpRespHeader;
			lpRespHeader = NULL;
			return TRUE;
		}

	protected:
		void*                 m_lpParam;
		HttpInterfaceError    m_error;
		IHttpCallback*        m_pCallback;
		HINTERNET m_hInternet;
		HINTERNET m_hConnect;
		HINTERNET m_hRequest;
		HINTERNET m_hSession;

		HANDLE    m_hCompleteEvent;
		HANDLE    m_hCancelEvent;

		DWORD     m_dwConnectTimeOut;
		DWORD     m_dwContext;

		std::vector<tstring> m_arrRespHeader;

		tstring m_strProxy;
		tstring m_strUser, m_strPwd;
	};

	// a sink is a data receiver
	class sink;
	// buffer is a utility class built to pass along data as a memory fifo
	class buffer;

	// a utility sink that reads a full line
	class line_parser_sink;
	// a utility sink that parses RFC 822 headers
	class header_parser_sink;

	// utility functions to create producers from a string or vector
	template<typename T>
	std::function<bool(buffer &)> make_data_producer(T * in_data);
	template<typename T>
	std::function<bool(buffer &)> make_data_producer(T &in_data);

	class buffer {
		bool m_isview;
		int m_cap;    // the total number of bytes in this buffer
		int m_bottom; // the bottom index, ie the first used data element
		int m_top;    // the top index, the first unused data element
		char *m_data; // the actual data
		buffer(const buffer &)=delete;
		buffer& operator=(const buffer&)=delete;
	public:
		// Construct a view of a piece of data (nothing available to fill but plenty of used to consume)
		buffer(char *data,int amount) : m_isview(true),m_cap(amount),m_bottom(0),m_top(amount),m_data(data) {
		}
		buffer(int capacity) : m_isview(false),m_cap(capacity),m_bottom(0),m_top(0),m_data(new char[capacity]) {
			//m_data=new char[capacity];
		}
		~buffer() {
			if (!m_isview)
				delete m_data;
		}
		// returns the number of bytes corrently in the buffer
		inline int usage() {
			return m_top-m_bottom;
		}
		// returns the number of bytes available to produce as a flat array
		int direct_avail() {
			return m_cap-m_top;
		}
		// returns the total number of bytes available to produce
		int total_avail() {
			return (m_cap-m_top)+(m_bottom);
		}
		// compacts the buffer to maximize the flatly available bytes
		int compact() {
			if (m_bottom==0)
				return direct_avail();
			int sz=usage();
			std::memmove(m_data,m_data+m_bottom,sz);
			m_bottom=0;
			m_top=sz;
			return direct_avail();
		}
		inline int peek() {
			if (m_bottom>=m_top)
				return -1;
			return m_data[m_bottom]&0xff;
		}
		// consumes one byte from the currently available bytes
		inline char consume() {
			if (m_bottom>=m_top)
				throw std::out_of_range("no bytes to consume in buffer");
			return m_data[m_bottom++];
		}
		// returns the pointer to a number of bytes to consume directly.
		char* to_consume() {
			return m_data+m_bottom;
		}
		// tells the buffer how many bytes was consumed
		void consumed(int amount) {
			if (usage()<amount || amount<0)
				throw std::invalid_argument("underflow");
			m_bottom+=amount;
		}
		// adds a byte to the buffer
		inline void produce(char c) {
			if (direct_avail()<1) {
				if (compact()<1) {
					throw std::out_of_range("no bytes available in buffer");
				}
			}
			m_data[m_top++]=c;
		}
		// adds as many bytes as possible from the source to this buffer
		void produce(buffer &source) {
			// how much do we want to copy if possible?
			int to_copy=source.usage();
			// now check the actual number of bytes we can copy
			if (to_copy>total_avail())
				to_copy=total_avail();
			produce(source,to_copy);
		}
		// copy the number of bytes from the source
		void produce(buffer &source,int to_copy) {
			// if we can fit it straight away copy it directly
			if (direct_avail()<to_copy) {
				// we can't copy it directly, then compact first before copy
				if (compact()<to_copy) {
					// still not enough space, fault here!
					throw std::invalid_argument("not enough space to take the copied bytes");
				}
			}
			// now copy the amount we can take
			std::memcpy(to_produce(),source.to_consume(),to_copy);
			produced(to_copy);
			source.consumed(to_copy);
		}
		// returns a the pointer to the free bytes to be written
		char* to_produce() {
			return m_data+m_top;
		}
		// tell the buffer how many bytes were actually written
		void produced(int amount) {
			if (direct_avail()<amount)
				throw std::invalid_argument("overflow");
			m_top+=amount;
		}
		// convert the buffer contents to a string
		std::string to_string() {
			std::string out;
			for (int i=m_bottom;i<m_top;i++) {
				out.push_back(m_data[i]);
			}
			return out;
		}
		// convert the buffer contents to a char
		std::vector<char> to_char() {
			std::vector<char> out;
			out.reserve(m_top - m_bottom);
			for (int i=m_bottom;i<m_top;i++) {
				out.push_back(m_data[i]);
			}
			return out;
		}
	};

	template<typename T>
	std::function<bool(buffer &)> make_data_producer(T * in_data) {
		std::shared_ptr<int> off(new int);
		std::shared_ptr<T> data(in_data);
		*off=0;
		// return the actual producer function that writes out the contents on request
		return [data,off](buffer &ob){
			int dataleft=(int) data->size()-*off; // dl is how much we have left to send
			int outleft=ob.compact();
			int to_copy=dataleft<outleft?dataleft:outleft;
			std::memcpy(ob.to_produce(),data->data()+*off,to_copy);
			ob.produced(to_copy);
			*off+=to_copy;
			return *off!=data->size();
		};
	}

	template<typename T>
	std::function<bool(buffer &)> make_data_producer(T &in_data) {
		return make_data_producer(new T(in_data));
	}

	class sink {
	public:
		// implement this function to make a working sink
		virtual bool drain(buffer &buf)=0;
	};



	class line_parser_sink : public sink {
		std::string out;  // the output string
		const char *term; // the line terminator
		int tl;           // length of the terminator string
		int szmax;        // the maximum number of bytes in a line
		std::function<bool(std::string&)> on_line;
	public:
		line_parser_sink(
			const char *in_term,
			int in_max,
			std::function<bool(std::string&)> in_on_line
		):
			term(in_term),
			szmax(in_max),
			on_line(in_on_line)
		{
			tl= (int) strlen(term);
		}
		virtual bool drain(buffer &buf) {
			size_t sz=out.size();
			while(buf.usage()) {
				if (sz>=szmax) {
					return false;
				}
				//std::cout<<"Pre:["<<out<<"]["<<buf.to_string()<<"]\n";
				out.push_back(buf.consume());
				//std::cout<<"Post:["<<out<<"]["<<buf.to_string()<<"]\n";
				sz++;
				if (sz>tl) {
					if (!memcmp(out.data()+sz-tl,term,tl)) {
						out.resize(sz-tl);
						//std::cout<<"Line:"<<out<<":\n";
						bool rv=on_line(out);
						out.resize(0);
						return rv;
					}
				}
			}
			return true;
		}
	};

	class header_parser_sink : public sink {
		enum headerstate {
			firstlinestart=0,
			linestart,
			testemptyline,
			inkey,
			postkeyskip,
			invalue,
			postvalue
		};
		headerstate state;
		std::string k;
		std::string v;
		int count;
		int maxsz;
		int (*filter)(int c);

		std::function<bool(std::string&,std::string&)> on_header;
		std::function<bool(const char *err)> on_fin;
	public:
		header_parser_sink(
			int in_maxsz,
			int (*in_filter)(int c),
			std::function<bool(std::string&,std::string&)> in_on_header,
			std::function<bool(const char *err)> in_on_fin
		):
			state(firstlinestart),
			count(0),
			maxsz(in_maxsz),
			filter(in_filter),
			on_header(in_on_header),
			on_fin(in_on_fin)
		{}
		virtual bool drain(buffer &buf) {
			// pre-existing error condition, just return.
			if (count==-1)
				return false;
			while(buf.usage()) {
				if (count>=maxsz) {
					on_fin("Error, headers too large");
					count=-1;
					return false;
				}
				char c=buf.consume();
				count++;
				switch(state) {
				case firstlinestart :
				case linestart :
					if (c==13) {
						if (state!=firstlinestart) {
							on_header(k,v);
							k.clear();
							v.clear();
						}
						// empty line in progress
						state=testemptyline;
						continue;
					} else if (c==10) {
						on_fin("spurios LF");
						count=-1;
						return false;
					}
					if (state!=firstlinestart) {
						if (isspace(c)) {
							state=invalue;
							v.push_back(c);
							continue;
						}
						on_header(k,v);
						k.clear();
						v.clear();
					}
					if (isspace(c))
						continue;
					state=inkey;
					if (filter)
						c = filter(c);
					k.push_back(c);
					continue;
				case testemptyline :
					if (c==10) {
						// empty line encountered, we're finished with the data
						bool rv=on_fin(nullptr);
						k.clear();
						v.clear();
						state=firstlinestart;
						count=0;
						return rv;
					} else {
						on_fin("cr but no lf in empty headerline");
						count=-1;
						return false;
					}
				case inkey :
					if (c==':') {
						state=postkeyskip;
						continue;
					} else {
						if (filter)
							c=filter(c);
						k.push_back(c);
						continue;
					}
				case postkeyskip :
					if (isspace(c)) {
						continue;
					} else {
						state=invalue;
						v.push_back(c);
						continue;
					}
				case invalue :
					if (c==13) {
						state=postvalue;
						continue;
					} else {
						v.push_back(c);
						continue;
					}
				case postvalue :
					if (c==10) {
						state=linestart;
						continue;
					} else {
						on_fin("cr but no lf in headerline");
						count=-1;
						return false;
					}
				default:
					printf("headerparser unhandled state:%d\n",state);
					exit(-1);
				}
			}
			return true;
		}
	};

	class scheduler {
		struct event {
			//uint64_t next;
			uint64_t period; // for recurring events
			std::function<void()> once;
			std::function<bool()> recurring;
			event(std::function<void()> f):period(0),once(f) {}
			event(uint64_t in_period,std::function<bool()> f):period(in_period),recurring(f) {}
		};
		std::multimap<uint64_t,event> events;
	public:
		void yield() {
			// TODO: IOCP/KEVENT...
			// a utility function to give up a slice of cpu time
			Sleep(1);
		}
		uint64_t current_time_millis() {
			SYSTEMTIME st;
			GetSystemTime(&st);
			return st.wMilliseconds+(1000*(uint64_t)time(0));
		}
		void timeout(uint64_t timeout,std::function<void()> f) {
			uint64_t event_time=current_time_millis()+timeout;
			events.emplace(event_time,event(f));
		}
		void interval(uint64_t timeout,uint64_t period,std::function<bool()> f) {
			uint64_t event_time=current_time_millis()+timeout;
			events.emplace(event_time,event(period,f));
		}
		void poll() {
			uint64_t now=current_time_millis();
			while(!events.empty()) {
				auto bi=events.begin();
				if (bi->first>now)
					break;
				if (bi->second.period) {
					if (bi->second.recurring()) {
						uint64_t next=bi->first+bi->second.period;
						events.emplace(next,event(bi->second.period,bi->second.recurring));
						bi=events.begin();
					}
				} else {
					bi->second.once();
				}
				events.erase(bi);
			}
		}
	};

	template<class I,class R,class FT>
	I filterOr(R v,FT f,I elval) {
		if (v)
			return f(v);
		return elval;
	}

	// the tcp class is a container for listeners and connections
	class tcp;
	// a single connection, protocols should inherit this class
	class connection;

	class tcp {
	public:
		class connection;
	private:
		std::vector<std::pair<int,std::function<connection*()>>> listeners;
		class tcpconn {
			friend tcp;
			int sock;
			std::shared_ptr<connection> conn;
			bool want_input;
			buffer input;
			buffer output;
			tcpconn(int insize,int outsize):input(insize),output(outsize) {}
		};
		std::vector<std::shared_ptr<tcpconn>> conns;

		static void set_non_blocking_socket(int socket) {
			unsigned long nbl=1;
			ioctlsocket(socket,FIONBIO,&nbl);
		}

		static bool was_block() {
			return (WSAGetLastError()==WSAEWOULDBLOCK) ;
		}

		int input_buffer_size;
		int output_buffer_size;
		bool work_conn(tcpconn &c) {
			int fill_count = 0;
			while(c.want_input && fill_count<10) {
				// process events as long as we have data and don't have multiple
				// producers on queue to avoid denial of service scenarios where
				// the producers are too slow to drain before the sink has read.
				// Note: the sink should be called first since was_block below will break the loop on block.
				if (c.input.usage() && c.conn->producers.size()<=1) {
					c.want_input=c.conn->current_sink->drain(c.input);
					continue;
				}
				// try to fill up the buffer as much as possible.
				if (c.input.total_avail()) {
					int avail=c.input.compact();
					int rc=recv(c.sock,c.input.to_produce(),avail,0);
					if (rc<0) {
						if (was_block()) {
							//printf("WOULD BLOCK\n");
							break;
						} else {
							// not a blocking error!
							return false;
						}
					} else if (rc>0) {
						c.input.produced(rc);
						fill_count++;
						continue;
					} else {
						// 0 on recv, closed sock;
						return false;
					}
				}
				break;
			}
			while (c.output.usage() || c.conn->producers.size()) {
				if (c.output.usage()) {
					const int send_flags=0;
					int rc=send(c.sock,c.output.to_consume(),c.output.usage(),send_flags);
					if (rc<0) {
						if (!was_block()) {
							// error other than wouldblock
							return false;
						}
					} else if (rc>0) {
						//bool brk=rc!=c.output.usage();
						c.output.consumed(rc);
						//c.output.erase(c.output.begin(),c.output.begin()+rc);
						if (c.output.usage())
							break; // could not take all data, do more later
					}
				}
				if (c.output.total_avail() && c.conn->producers.size()) {
					int preuse=c.output.total_avail();
					if (!c.conn->producers.front()(c.output)) {
						// producer finished, remove it.
						c.conn->producers.erase(c.conn->producers.begin());
					} else if (preuse==c.output.total_avail()) {
						// no data was generated.
						break;
					}
				}
			}
			return c.want_input || c.output.usage() || c.conn->producers.size();
		}
	public:
		tcp(int in_input_buffer_size=4096,int in_out_buffer_size=4096):input_buffer_size(in_input_buffer_size),output_buffer_size(in_out_buffer_size) {
			WSADATA wsa_data;
			if (WSAStartup(MAKEWORD(1,0),&wsa_data)) {
				std::cerr<<"WSAStartup problem"<<std::endl;
				//throw new std::exception("WSAStartup problem");
			}
		}
		~tcp() {
			if (WSACleanup()) {
				std::cerr<<"WSACleanup shutdown error"<<std::endl;
				//throw new std::exception("WSACleanup shutdown error\n");
			}
		}
		bool poll() {
			if(listeners.size()==0 && conns.size()==0)
				return false;
			// first see if we have any new connections
			for(auto l:listeners) {
				while(true) {
					struct sockaddr_in addr;
					int addrsize=sizeof(addr);
					int newsock=(int) accept(l.first,(struct sockaddr*)&addr,&addrsize);
					if (newsock!=-1) {
						set_non_blocking_socket(newsock);
						conns.push_back(std::shared_ptr<tcpconn>(new tcpconn(input_buffer_size,output_buffer_size)));
						conns.back()->sock=newsock;
						conns.back()->want_input=true;
						conns.back()->conn.reset(l.second());
						continue;
					} else {
						break;
					}
				}
			}
			// now see if we have new data
			// TODO: rewrite this to use kevent,etc
			conns.erase(
				std::remove_if(
					conns.begin(),
					conns.end(),
					[this](std::shared_ptr<tcpconn> c) {
						//printf("running conn:%p\n",&c);
						if (!work_conn(*c)) {
							//printf("Wanting to remove conn!\n");
							closesocket(c->sock);
							return true;
						} else {
							return false;
						}
					}
				),
				conns.end()
			);
			return true; // change this somehow?
		}
		bool listen(int port,std::function<connection*()> spawn) {
			int sock=-1;
			struct sockaddr_in sockaddr;
			if (-1==(sock=(int) socket(PF_INET,SOCK_STREAM,IPPROTO_TCP))) {
				return true;
			}
			memset(&sockaddr,0,sizeof(sockaddr));
			sockaddr.sin_family=AF_INET;
			sockaddr.sin_addr.s_addr=0; // default addr
			sockaddr.sin_port=htons(port);
			if (bind(sock,(struct sockaddr*)&sockaddr,sizeof(sockaddr))) {
				closesocket(sock);
				return true;
			}
			if (::listen(sock,SOMAXCONN)) {
				closesocket(sock);
				return true;
			}
			set_non_blocking_socket(sock);
			listeners.push_back(std::make_pair(sock,spawn));
			return false;
		}

		class connection :  public std::enable_shared_from_this<connection> {
		public:
			connection() {
				//printf("conn ctor\n");
			}
			virtual ~connection() {
				//printf("conn dtor\n");
			}
			//std::function<bool(std::vector<char>&)> *sink;
			std::shared_ptr<sink> current_sink;
			//std::vector<std::function<bool(std::vector<char>&)> > producers;
			std::vector<std::function<bool(buffer&)> > producers;
		};
	};

	static const char base64chars[65]=
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static const signed char base64lookup[256]={
		// 0
		-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,62,-1,-1,-1,63, 52,53,54,55,56,57,58,59, 60,61,-1,-1,-1,-1,-1,-1,
		// 64
		-1, 0, 1, 2, 3, 4, 5, 6,  7, 8, 9,10,11,12,13,14, 15,16,17,18,19,20,21,22, 23,24,25,-1,-1,-1,-1,-1,
		-1,26,27,28,29,30,31,32, 33,34,35,36,37,38,39,40, 41,42,43,44,45,46,47,48, 49,50,51,-1,-1,-1,-1,-1,
		// 128
		-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
		// 192
		-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1
	};

	class base64encoder {
		int bits;
		int count;
		char tmp[4];
	public:
		base64encoder():bits(0),count(0) {}
		char* encode(char c) {
			bits=(bits<<8)|(c&0xff);
			count+=8;
			int i=0;
			while (count>=6) {
				count-=6;
				tmp[i++]=base64chars[(bits>>count)&0x3f];
			}
			tmp[i]=0;
			return tmp;
		}
		char* end() {
			int align=count;
			int i=0;
			while(align) {
				if (count) {
					tmp[i++]=base64chars[(bits<<(6-count))&0x3f];
					count=0;
				} else {
					tmp[i++]='=';
				}
				align=(align-6)&7;
			}
			tmp[i]=0;
			return tmp;
		}
		// stl compatible helper template
		template<typename DT,typename ST>
		void encode(DT &d,ST &s){
			encode(d,s,s.size(),true);
		}
		// stl compatible helper template
		template<typename DT,typename ST>
		void encode(DT &d,ST &s,int count,bool end) {
			for (int i=0;i<count;i++) {
				char *p=encode(s[i]);
				while(*p)
					d.push_back(*(p++));
			}
			if (end) {
				char *p=this->end();
				while(*p)
					d.push_back(*(p++));
			}
		}
	};

	class base64decoder {
		int bits;
		int count;
	public:
		base64decoder():bits(0),count(0) {}
		int decode(char i) {
			int lu=base64lookup[i&0xff];
			if (lu<0)
				return -1;
			bits=(bits<<6)|lu;
			count+=6;
			if (count<8)
				return -1;
			count-=8;
			return (bits>>count)&0xff;
		}
		template<typename DT>
		DT decode(DT &s) {
			DT out;
			decode(out,s);
			return out;
		}
		// stl compatible helper template
		template<typename DT,typename ST>
		void decode(DT &d,ST &s) {
			decode(d,s,(int) s.size());
		}
		// stl compatible helper template
		template<typename DT,typename ST>
		void decode(DT &d,ST &s,int count) {
			for (int i=0;i<count;i++) {
				int dec=decode(s[i]);
				if (0>dec)
					continue;
				d.push_back((char)dec);
			}
		}
	};

	// fast C++ sha1 class
	// most calculations is done with 32bit integers directly on the fly to
	// minimize the number of instructions and data copies.
	class sha1 {
		// accumulator used for odd bytes
		uint32_t acc;
		// input word storage
		uint32_t w[80];
		// hash state
		uint32_t h[5];
		// length of hash in BYTES
		uint64_t ml;

		// rotation utility routine.
		static inline uint32_t rol(uint32_t v,int sh) {
			return (v<<sh)|(v>>(32-sh));
		}

		// do a 512 byte block
		void block() {
			// stretch out our input words
			for (int i=16;i<80;i++) {
				w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
			}
			// initialize local state for rounds
			uint32_t
				a=h[0],
				b=h[1],
				c=h[2],
				d=h[3],
				e=h[4];

			// setup macros to do 20 rounds at a time
#define NET11_SHA1_ITER(i,code) { \
	code; \
	uint32_t tmp=rol(a,5) + f + e + k + w[i]; \
	e = d; d = c; c = rol(b,30); b=a; a=tmp; }

#define NET11_SHA1_ITER4(i,c) \
	NET11_SHA1_ITER(i,c)   NET11_SHA1_ITER(i+1,c) \
	NET11_SHA1_ITER(i+2,c) NET11_SHA1_ITER(i+3,c)
#define NET11_SHA1_ITER20(i,c) \
	NET11_SHA1_ITER4(i,c) NET11_SHA1_ITER4(i+4,c) NET11_SHA1_ITER4(i+8,c) \
	NET11_SHA1_ITER4(i+12,c) NET11_SHA1_ITER4(i+16,c)

			// rounds 0-19 with their specific parameters
			NET11_SHA1_ITER20(0, uint32_t f=(b&c)^((~b)&d); uint32_t k=0x5a827999)
			// rounds 20-39 with their specific parameters
			NET11_SHA1_ITER20(20,uint32_t f=b^c^d; uint32_t k=0x6ed9eba1)
			// rounds 40-59 with their specific parameters
			NET11_SHA1_ITER20(40,uint32_t f=(b&c)^(b&d)^(c&d); uint32_t k=0x8f1bbcdc)
			// rounds 60-79 with their specific parameters
			NET11_SHA1_ITER20(60,uint32_t f=b^c^d; uint32_t k=0xca62c1d6)

#undef NET11_SHA1_ITER20
#undef NET11_SHA1_ITER4
#undef NET11_SHA1_ITER

			// update the main state with the local words
			h[0]+=a;
			h[1]+=b;
			h[2]+=c;
			h[3]+=d;
			h[4]+=e;
		}
	public:
		// initialize default parameters
		sha1() {
			reinit();
		}
		// utility constructors to hash some input data directly.
		template<typename T>
		sha1(const T d,int count) {
			reinit();
			addbytes(d,count);
		}
		template<typename T>
		sha1(T &in) {
			reinit();
			addbytes(in);
		}

		void reinit() {
			acc = 0;
			ml = 0;
			h[0]=0x67452301;
			h[1]=0xefcdab89;
			h[2]=0x98badcfe;
			h[3]=0x10325476;
			h[4]=0xc3d2e1f0;
		}

		// add a single byte to the hash calculations
		inline void addbyte(uint8_t b) {
			acc = (acc << 8) | (b & 0xff);
			// note ml preincrement below, correct placement needed for comparison and performance.
			if (0 == (++ml & 3)) {
				int wi = ((ml-1) >> 2) & 0xf;
				w[wi] = acc;
				if (0 == (ml & 0x3f)) {
					// 512 bits added, do block
					block();
				}
			}
		}
		// function to add a counted number of bytes quickly.
		template<typename T>
		void addbytes(const T b,size_t count) {
			//uint8_t *b=(uint8_t*)b;
			size_t i = 0;
			// pre-roll out any odd bytes
			while (i < count && (ml & 3))
				addbyte(b[i++]);
			// calculate number of fast iters
			size_t fc = count>>2;
			// now do the direct word iters
			while (fc--) {
				// set the word
				w[(ml >> 2) & 0xf] = (((uint8_t)b[i]) << 24) | (((uint8_t)b[i + 1]) << 16) | (((uint8_t)b[i + 2]) << 8) | ((uint8_t)b[i + 3]);
				// increment size and dest ptr
				ml += 4;
				i += 4;
				// if we've collected 64 bytes for a block, calculate it.
				if (0 == (ml & 0x3f))
					block();
			}
			// and finally the end roll for any trailing bytes
			while (i < count)
				addbyte(b[i++]);
		}
		// utility proxies to unpack bytes from a stl like container
		template<typename T>
		void addbytes(T &cnt) {
			addbytes(cnt.data(), cnt.size());
		}
		// outputs a digest
		void digest(char *o) {
			uint64_t oml = ml << 3; // this needs to be in bits.
			addbyte(0x80);
			while((ml&0x3f)!=56) {
				addbyte(0);
			}
			for (int i=56;i>=0;i-=8)
				addbyte((uint8_t)(oml>>i));
			for (int i=0;i<5;i++) {
				uint32_t t=h[i];
				o[(i<<2)+0]=(t>>24)&0xff;
				o[(i<<2)+1]=(t>>16)&0xff;
				o[(i<<2)+2]=(t>>8)&0xff;
				o[(i<<2)+3]=(t)&0xff;
			}
		}
		template<typename T>
		T digest() {
			T t;
			char tmp[20];
			digest(tmp);
			for (int i=0;i<sizeof(tmp);i++)
				t.push_back(tmp[i]);
			return t;
		}
	};

	namespace http {
		class connection;
		class action;
		class consume_action;
		class response;
		class websocket_response;
		class websocket_sink;
		class websocket;

		// RFC 2616 sec2 Token
		static bool parse_token_byte(std::string *out,buffer &in) {
			switch(in.peek()) {
			case -1 :
			case '(' : case ')' : case '<' : case '>' : case '@' :
			case ',' : case ';' : case ':' : case '\\' : case '\"' :
			case '/' : case '[' : case ']' : case '?' : case '=' :
			case '{' : case '}' : case ' ' : case '\t' :
				return false;
			}
			char v=in.consume();
			if (out)
				out->push_back(v);
			return true;
		}

		// RFC 2616 sec2 quoted-string (requires 3 consecutive state values)
		static bool parse_quoted_string_byte(std::string *out,buffer &in, int &state,int base_state=0) {
			int lstate=state-base_state;
			if (lstate==0) {
				switch(in.peek()) {
				case -1 :
					return true;
				case '\"' :
					in.consume();
					state++;
					return true;
				default:
					return false;
				}
			} else if (lstate==1) {
				switch(in.peek()) {
				case -1 :
					return true;
				case '\\' :
					in.consume();
					state++;
					return true;
				case '\"' :
					in.consume();
					state--;
					return false;
				}
				char ov=in.consume();
				if (out)
					out->push_back(ov);
				return true;
			} else if (lstate==2) {
				char ov=in.consume();
				if (out)
					out->push_back(ov);
				state--;
				return true;
			}
			// this is an error state
			abort();
		}

		static std::string uridecode(const std::string &uristr) {
			std::string retstr;
			retstr.reserve(uristr.length()*2);
			for ( int i = 0; i < uristr.length(); i++ ) {
				if ( uristr[i] == '%' && i < ( uristr.length()-2 ) ) {
					char num[3];
					num[0] = ( char )uristr[i+1];
					num[1] = ( char )uristr[i+2];
					num[2] = 0;
					char c = (char) strtoul( num, NULL, 16 );
					retstr = retstr + c;
					i+= 2;
				} else if ( uristr[i] == '+' ) {
					retstr = retstr + ' ';
				} else {
					retstr = retstr + uristr[i];
				}
			}
			return retstr;
		}
		static std::map<std::string, std::string> decodeContentType(const std::vector<char> &uristr) {
			std::string s;
			std::vector<std::string> kv;
			std::map<std::string, std::string> kvr;

			kv.reserve(256);
			for (int i = 0; i < uristr.size(); i++) {
				if ((uristr[i] == '=') && (kv.size() % 2 == 0)) {
					kv.push_back(uridecode(s));
					s = "";
				} else if ((uristr[i] == '&') && (kv.size() % 2 == 1)) {
					kv.push_back(uridecode(s));
					s = "";
				} else s = s + uristr[i];
			}
			if(kv.size()%2) kv.push_back(uridecode(s));
			for (int i = 0; i < kv.size(); i+=2) {
				kvr[kv[i]] = kv[i+1];
			}

			return kvr;
		}
		static std::map<std::string, std::string> decodeContentType(const std::string &uristr) {
			std::vector<char> us;
			us.reserve(uristr.length());
			for (auto c : uristr) us.push_back(c);
			return decodeContentType(us);
		}

		static response* make_text_response(int code,const std::string &data);

		class action {
			friend class response;
			friend class connection;
			friend class consume_action;
			action() {}
		protected:
			virtual bool produce(connection &conn)=0;
		public:
			virtual ~action() {}
		};

		class connection : public net11::tcp::connection {
			friend std::function<net11::tcp::connection*()> make_server(std::function<action*(connection &conn)> route);
			friend class response;
			friend class consume_action;

			buffer* http_buffer;
			std::map<std::string, std::string> http_form;
			std::map<std::string, std::string> http_query;

			std::string reqline[4];
			std::shared_ptr<sink> reqlinesink;

			std::shared_ptr<sink> headsink;
			std::map<std::string,std::string> headers;
			std::shared_ptr<sink> postchunkedsink;

			std::shared_ptr<sink> m_chunkedcontentsink;

			std::function<action*(connection &conn)> router;
			std::function<response*(buffer& data,bool end)> dataconsumer;

			std::function<response*(buffer*data)> consume_fun;

			bool produced;
			bool produce(action *act) {
				if (produced)
					return true;
				consume_fun=nullptr;
				if (!act) {
					//std::map<std::string,std::string> head; //{{"connection","close"}};
					std::string msg="Error 404, "+url()+" not found";
					act=(action*)make_text_response(404,msg);
				}
				// TODO: make sure that connection lines are there?
				bool rv=act->produce(*this);
				delete act;
				return rv;
			}

			class sizedcontentsink : public sink {
				friend class connection;
				size_t clen;
				connection *conn;
				std::function<bool()> post_action;
				sizedcontentsink(connection *in_conn, std::function<bool()> in_post_action) : conn(in_conn),post_action(in_post_action),clen(0) {}
			public:
				virtual bool drain(buffer &buf) {
					bool rv=true;
					int amount=(int) (buf.usage()<clen?buf.usage():clen);
					buffer view(buf.to_consume(),amount);

					if (conn->consume_fun) {
						response *r=conn->consume_fun(&view);
						if (r)
							rv=conn->produce((action*)r);
					}
					buf.consumed(amount);
					clen-=amount;
					if (clen==0) {
						conn->current_sink=conn->reqlinesink;
						response *r=0;
						if (conn->consume_fun) {
							r=conn->consume_fun(NULL);
							rv=conn->produce((action*)r);
						} else {
							if (amount > 0) {
								conn->http_buffer = &view;
							}
							if (post_action) {
								rv = post_action();
							}
						}

						return rv;
					}
					return rv;
				}
			};

			class chunkedcontentsink : public sink {
				friend class connection;
				int state;
				int sstate;
				size_t clen;
				chunkedcontentsink() : state(0),clen(0),sstate(0) {}
				int hexcharvalue(int v) {
					if (v>='0' && v<='9')
						return v-'0';
					if (v>='a' && v<='f')
						return v-'a'+10;
					if (v>='A' && v<='F')
						return v-'A'+10;
					return -1;
				}
				connection *conn;
				std::function<bool()> post_action;
				chunkedcontentsink(connection *in_conn, std::function<bool()> in_post_action) : conn(in_conn),post_action(in_post_action),state(0),sstate(0),clen(0) {}
			public:
				virtual bool drain(buffer &buf) {
					bool rv=true;
					while(buf.usage()) {
						int cv=buf.peek();
						switch(state) {
						case 0 : // parsing chunk size
							{
								int hv=hexcharvalue(cv);
								if (hv==-1) { // check if it wasn't a hex-char
									state=1;  // decide on ext or content
									continue;
								}
								buf.consume();
								clen=clen*16 + hv;
								continue;
							}
						case 1 : // post size/ext, decide on next action
							buf.consume();
							if (cv==';') {
								state=5; // expect ext-name
								continue;
							} else if (cv==13) {
								state=2; // expect LF
								continue;
							} else return false; // syntax error here
						case 2 : // chunk-LF
							buf.consume(); // always consume
							if (cv!=10)
								return false; // syntax error
							if (clen==0) {
								state=0; sstate=0; clen=0;
								conn->current_sink=conn->postchunkedsink;
								return rv;
							} else {
								state=9; // content
								continue;
							}
						case 3 : // content-CR
							buf.consume();
							if (cv!=13)
								return false; // syntax error
							state=4;
							continue;
						case 4 : // content-LF
							buf.consume();
							if (cv!=10)
								return false; // syntax error
							state=0; // go back to reading a chunk-size
							continue;
						case 5 : // ext-name
							if (parse_token_byte(NULL,buf)) // consume as much of a name as possible
								continue;
							// but after the name token we have something else.
							cv=buf.consume();
							if (cv=='=') {
								// either require a value
								state=6;
								continue;
							} else if (cv==13) {
								// or a CRLF to go to the content
								state=2;
								continue;
							} else return false; // syntax error otherwise
						case 6 : // start of ext value, could be...
							if (cv=='\"') {
								state=8; // a quoted string
								continue;
							} else {
								state=7; // or a simple token
								continue;
							}
						case 7 : // parsing token ext value
							if (parse_token_byte(NULL,buf))
								continue;
							state=1; // decide on what to do next
							continue;
						case 8 : // parsing quoted string ext value
							if (parse_quoted_string_byte(NULL,buf,sstate))
								continue;
							state=1; // decide on what to do next
							continue;
						case 9 : // X bytes of content
							{
								int amount=(int) (buf.usage()<clen?buf.usage():clen);
								buffer view(buf.to_consume(),amount);

								if (conn->consume_fun) {
									response *r=conn->consume_fun(&view);
									if (r)
										rv=conn->produce((action*)r);
								} else {
									if (amount > 0) {
										conn->http_buffer = &view;
									}
									if (post_action) {
										rv = post_action();
									}
								}
								buf.consumed(amount);
								clen-=amount;
								if (clen==0) {
									state=3;
								}
								continue;
							}
						default: // illegal state
							abort();
						}
					}
					return rv;
				}
			};

			std::shared_ptr<sizedcontentsink> m_sizedcontentsink;

			connection(
				std::function<action*(connection &conn)> in_router
			):router(in_router),http_buffer(nullptr) {
				// TODO: urlencodings?
				auto post_action = [this]() -> bool {
					if ("" != reqline[3]) {
						this -> http_query = decodeContentType(reqline[3]);
					}
					if (nullptr != this -> http_buffer && this -> http_buffer -> usage() > 0) {
						this -> http_form = decodeContentType(this -> http_buffer -> to_char());
					}

					action *act=router(*this);
					bool rv=produce(act);

					return rv;
				};
				reqlinesink=std::shared_ptr<sink>(new net11::line_parser_sink("\r\n",4096,[this](std::string &l){
					bool in_white=false;
					int outidx=0;
					reqline[0].resize(0);
					reqline[1].resize(0);
					reqline[2].resize(0);
					reqline[3].resize(0);
					headers.clear();
					produced=false;
					for (int i=0;i<l.size();i++) {
						char c=l[i];
						if (isspace(c)) {
							in_white=true;
						} else {
							if (in_white && outidx<2) {
								outidx++;
							}
							reqline[outidx].push_back(c);
							in_white=false;
						}
					}

					auto pos = reqline[1].find("?");
					if(pos == std::string::npos){
						reqline[3] = "";
					} else {
						reqline[3] = reqline[1].substr(pos+1);
						reqline[1] = reqline[1].substr(0, pos);
					}
					if (outidx>0 && reqline[0].size() && reqline[1].size()) {
						this->current_sink=headsink;
						return true;
					} else {
						// TODO: error handling?!
						return false;
					}
				}));
				headsink=std::shared_ptr<sink>(new header_parser_sink(128*1024,tolower,
					[this](std::string &k,std::string &v){
						//std::cout<<"HeadKey=["<<k<<"] HeadValue=["<<v<<"]\n";
						headers[k]=v;
						return true;
					},
					[this, post_action](const char *err) {
						std::cout<<"req:"<<reqline[0]<<" url:"<<reqline[1]<<" ver:"<<reqline[2]<<"\n";
						bool need_post_action = false;  // todo: if content-length equal 0 or doesn't handle, need do default action
						consume_fun=nullptr;
						// reset our sink early in case the encoding, router and/or action wants to hijack it
						// determine content based on RFC 2616 pt 4.4
						auto tehead=this->header("transfer-encoding");
						if ( tehead && *tehead!="identity" ) {
							// Chunked encoding if transfer-encoding header exists and isn't set to identity
							//std::cerr<<"CHUNKED!\n";
							this->current_sink=m_chunkedcontentsink;
						} else if (auto clhead=this->header("content-length")) {
							//std::cerr<<"Content LEN\n";
							// only allow content-length influence IFF no transfer-enc is present
							::trim(*clhead);
							size_t clen=std::stoi(*clhead);
							m_sizedcontentsink->clen=clen>=0?clen:0;
							this->current_sink=m_sizedcontentsink;

							if (0 == clen) {
								need_post_action = true;
							}
						} else {
							// this server doesn't handle other kinds of content
							this->current_sink=reqlinesink;

							need_post_action = true;
						}

						if (need_post_action) {
							return (bool) post_action();
						}

						return true;
					}
				));
				m_chunkedcontentsink=std::shared_ptr<sink>(new chunkedcontentsink(this, post_action));
				m_sizedcontentsink=std::shared_ptr<sizedcontentsink>(new sizedcontentsink(this, post_action));
				postchunkedsink=std::shared_ptr<sink>(new header_parser_sink(128*1024,tolower,
					[this](std::string &k,std::string &v) {
						headers[k]=v;
						return true;
					},
					[this](const char *err){
						response *r=0;
						if (consume_fun) {
							r=consume_fun(NULL);
						}
						bool rv=produce((action*)r);
						this->current_sink=reqlinesink;
						return rv;
					}
				));
				current_sink=reqlinesink;
			}
			virtual ~connection() {
				//printf("Killed http connection\n");
			}
			// small helper for the template expansion of has_headers
			bool has_headers() {
				return true;
			}
		public:
			std::string& method() {
				return reqline[0];
			}
			std::string& url() {
				return reqline[1]; // TODO should it be pre-decoded?
			}
			std::string& query() {
				return reqline[3];
			}
			std::string query(std::string key) {
				auto iter=http_query.find(key);
				if (iter == http_query.end()) {
					return "";
				}

				return iter->second;
			}
			buffer* form() {
				return http_buffer;
			}
			std::string form(std::string key) {
				auto iter=http_form.find(key);
				if (iter == http_form.end()) {
					return "";
				}

				return iter->second;
			}
			std::string* header(const char *in_k) {
				std::string k(in_k);
				return header(k);
			}
			std::string* header(std::string &k) {
				auto f=headers.find(k);
				if (f!=headers.end())
					return &f->second;
				else
					return 0;
			}
			std::unique_ptr<std::pair<std::string,std::string>> get_basic_auth() {
				//auto bad=std::make_pair(std::string(""),std::string(""));
				auto authhead=header("authorization");
				if (!authhead)
					return nullptr;
				//std::cerr<<"To split:"<<(*authhead)<<std::endl;
				auto authsep=::split(*authhead,' ');
				//std::cerr<<"Type:"<<authsep.first<<" Val:"<<authsep.second<<std::endl;
				::trim(authsep.first);
				// only support basic auth right now
				if (::stricmp(authsep.first,"basic"))
					return nullptr;
				//std::cerr<<"To decode:"<<authsep.second<<std::endl;
				::trim(authsep.second);
				auto tmp=net11::base64decoder().decode(authsep.second);
				//std::cerr<<"Decoded auth:"<<tmp<<std::endl;
				return std::unique_ptr<std::pair<std::string,std::string>>(
					new std::pair<std::string,std::string>(::split(tmp,':'))
				);
			}
			std::string lowerheader(const char *k) {
				std::string ok(k);
				return lowerheader(ok);
			}
			std::string lowerheader(std::string &k) {
				std::string out;
				auto f=headers.find(k);
				if (f!=headers.end()) {
					for (int i=0;i<f->second.size();i++)
						out.push_back(tolower(f->second[i]));
				}
				return out;
			}
			template<class T>
			void csvheaders(const std::string &k,T& fn,bool param_tolower=false) {
				auto f=headers.find(k);
				if (f==headers.end())
					return;
				std::string h;
				auto & is=f->second;
				for(int i=0;i<=is.size();) {
					bool end=i==is.size();
					if (!end && !h.size() && isspace(is[i])) {
						i++;
						continue;
					}
					if (end || is[i]==',') {
						if (h.size())
							fn(h);
						h.clear();
						i++;
						continue;
					}
					if (param_tolower) {
						h+=tolower(is[i++]);
					} else {
						h+=is[i++];
					}
				}
			}
			bool has_header(std::string &k) {
				return headers.count(k)!=0;
			}
			bool has_header(const char *p) {
				using namespace std::literals;
				//std::cout <<"HasHeader:" <<p <<" -> " <<(headers.count(std::string(p))!=0) <<" " <<((headers.count(std::string(p))!=0)?("[[["+lowerheader(p)+"]]]"): ""s) <<"\n";

				return headers.count(std::string(p))!=0;
			}
			template<typename HEAD,typename... REST>
			bool has_headers(HEAD head,REST... rest) {
				return has_header(head)&&has_headers(rest...);
			}
		};

		static std::function<net11::tcp::connection*()> make_server(std::function<action*(connection &conn)> route) {
			// now create a connection spawn function
			return [route]() {
				return (net11::tcp::connection*)new connection(route);
			};
		};

		class consume_action : public action {
		protected:
			std::function<response*(buffer*buf)> fn;
			virtual bool produce(connection &conn) {
				//std::cerr<<"Nil production right now from consume action\n";
				conn.consume_fun=fn;
				return true;
			}
		public:
			consume_action(const std::function<response*(buffer *buf)> & in_fn) : fn(in_fn) {}
			~consume_action(){}
		};

		class response : public action {
			friend class connection;
			friend class websocket_response;
			friend class response* make_stream_response(int code,std::function<bool(buffer &data)> prod);

			std::map<std::string,std::string> head;
			std::function<bool(buffer &)> prod;

			response(){}
		protected:
			int code;

			virtual void produce_headers(connection &conn) {
				conn.produced=true;
				std::string resline="HTTP/1.1 "+std::to_string(code)+" some message\r\n";
				for(auto kv:head) {
					resline=resline+kv.first+": "+kv.second+"\r\n";
				}
				resline+="\r\n";
				conn.producers.push_back(make_data_producer(resline));
			}

			virtual bool produce(connection &conn) {
				produce_headers(conn);
				if (head.count("content-length")) {
					conn.producers.push_back(prod);
				} else {
					// TODO: implement chunked responses?
					abort(); //conn.producers.push_back([this](
				}
				return true;
			}

			//virtual void produce_headers(connection &conn);
			//virtual bool produce(connection &conn);
		public:
			response* set_header(const std::string &k,const std::string &v) {
				head[k]=v;
				return this;
			}
			virtual ~response() {}
		};

		static response* make_stream_response(int code,std::function<bool(buffer &data)> prod) {
			auto out=new response();
			out->code=code;
			out->prod=prod;
			return out;
		}

		static response* make_blob_response(int code,const std::vector<char> &in_data) {
			auto rv=make_stream_response(code,make_data_producer(in_data));
			rv->set_header(std::string("content-length"),std::to_string(in_data.size()));
			return rv;
		}
		//static response* make_blob_response(int code,std::vector<char> in_data) {
		//	auto rv=make_stream_response(code,make_data_producer(in_data));
		//	rv->set_header(std::string("content-length"),std::to_string(in_data.size()));
		//	return rv;
		//}
		static response* make_text_response(int code,const std::string &in_data) {
			auto rv=make_stream_response(code,make_data_producer(in_data));
			rv->set_header(std::string("content-length"),std::to_string(in_data.size()));
			return rv;
		}
		//static response* make_text_response(int code,std::string in_data) {
		//	auto rv=make_stream_response(code,make_data_producer(in_data));
		//	rv->set_header(std::string("content-length"),std::to_string(in_data.size()));
		//	return rv;
		//}

		class websocket : public std::enable_shared_from_this<websocket> {
			friend websocket_sink;
			std::weak_ptr<connection> conn;
			int input_type=-1;
			websocket(std::weak_ptr<connection> in_conn):conn(in_conn),input_type(-1) {}
		public:
			int get_input_type() {
				return input_type;
			}
			static const int text=1;
			static const int binary=2;
			bool send(int ty,const char *data,uint64_t sz) {
				if (auto c=conn.lock()) {
					int shift;
					int firstsize;
					if (sz<126) {
						shift=0;
						firstsize=(int) sz;
					} else if (sz<65536) {
						shift=16;
						firstsize=126;
					} else {
						shift=64;
						firstsize=127;
					}
					std::shared_ptr<buffer> b(new buffer((int) (2+(shift/8)+sz)));
					b->produce(0x80|ty);
					b->produce(firstsize);
					while(shift) {
						shift-=8;
						b->produce( (sz>>shift)&0xff );
					}
					std::memcpy(b->to_produce(),data,sz);
					b->produced((int) sz);
					c->producers.push_back([b](buffer& out) {
						out.produce(*b);
						return 0!=b->usage();
					});
					return true;
				} else {
					// Sending to killed connection!
					return false;
				}
			}
			bool send(const std::string& data) {
				return send(text,data.data(),data.size());
			}
			bool send(const std::vector<char>& data) {
				return send(binary,data.data(),data.size());
			}
		};

		class websocket_sink : public sink {
			friend websocket_response;
			enum wsstate {
				firstbyte=0,
				sizebyte,
				sizeextra,
				maskbytes,
				bodybytes
			};
			wsstate state;
			uint8_t info;
			uint64_t count;
			uint64_t size;
			bool want_mask;
			uint32_t mask;
			//std::vector<char> data;
			
			char control_data[125];
			
			bool endit() {
				bool fin=info&0x80;
				if ((info&0xf)<=2) {
					int type=info&0xf;
					if (websock->input_type==-1) {
						if (type==0)
							return false; // must get a new type!
						websock->input_type=type;
					} else {
						if (type!=0)
							return false; // already a set type!
					}
					if (!packet_end(fin,info&0xf))
						return false;
					if (fin)
						websock->input_type=-1;
				} else {
					// processing for non-data packets.
					if ((info&0xf)==8) {
						websock->send(8,control_data,0);
						return false;
					} else if ((info&0xf)==9) {
						// got a ping, need to do pong
						websock->send(10,control_data,size);
					} else if ((info&0xf)==10) {
						// 10, we ignore pongs..
					} else {
						// unknown packet type
						return false;
					}
				}
				state=firstbyte;
				return true;
			}
			bool advance() {
				count=0;
				if (state==sizebyte || state==sizeextra) {
					if (want_mask) {
						state=maskbytes;
						return true;
					}
				}
				state=bodybytes;
				bool ok=true;
				if (info&0x70)
					return false; // Not allowed to use reserved bits
				if (!(info&0x80) && ((info&0xf)>7))
					return false;
				if ((info&0xf)<=2) {
					ok&=packet_start(info&0x80,info&0xf,size);
				} else {
					if (size>sizeof(control_data)) {
						return false;
					}
					switch(info&0xf) {
					case 8 : // close
						ok=true;
						break;
					case 9 : case 10 : // ping-pong
						ok=true;
						break;
					default:
						ok=false; // do not know how to handle packet
						break;
					}
				}
				if (size==0) {
					return ok&endit();
				} else {
					return ok;
				}
			}
			std::shared_ptr<websocket> websock;
		protected:
			websocket_sink(std::weak_ptr<connection> conn):state(firstbyte),websock(new websocket(conn)) {}
		public:
			virtual bool drain(buffer &buf) {
				while(buf.usage()) {
					switch(state) {
					case firstbyte :
						info=buf.consume();
						state=sizebyte;
						mask=0;
						count=0;
						continue;
					case sizebyte :
						{
							int tmp=buf.consume();
							want_mask=tmp&0x80;
							if ((tmp&0x7f)<126) {
								size=tmp&0x7f;
								if (!advance())
									return false;
								continue;
							} else {
								if((tmp&0x7f)==126) {
									count=6; // skip "initial" 6 bytes since we only want a 2 byte size
								} else {
									count=0;
								}
								size=0;
								state=sizeextra;
								continue;
							}
						}
					case sizeextra:
						size=(size<<8)|(buf.consume()&0xff);
						if ((++count)==8) {
							if (!advance())
								return false;
						}
						continue;
					case maskbytes :
						mask=(mask<<8)|(buf.consume()&0xff);
						if ((++count)==4) {
							if (!advance())
								return false;
						}
						continue;
					case bodybytes :
						{
							int b=(buf.consume()^( mask >> ( 8*(3^(count&3))) ))&0xff;
							if ((info&0xf)<=2) {
								packet_data(b);
							} else {
								control_data[count]=b;
							}
							if (++count==size) {
								if (!endit())
									return false;
							}
							continue;
						}
					}
				}
				return true;
			}
			std::weak_ptr<websocket> get_websocket() {
				return websock;
			}
			virtual bool packet_start(bool fin,int type,uint64_t size)=0;
			virtual void packet_data(char c)=0;
			virtual bool packet_end(bool fin,int type)=0;
			virtual void websocket_closing() {}
		};

		class websocket_response : public response {
			friend response* make_websocket(connection &c,std::shared_ptr<websocket_sink> wssink);
			std::shared_ptr<websocket_sink> sink;
			websocket_response(std::shared_ptr<websocket_sink> in_sink):sink(in_sink) {
				code=101;
			}
			bool produce(connection &conn) {
				produce_headers(conn);
				conn.current_sink=sink;
				return true;
			}
		public:
			std::weak_ptr<websocket> get_websocket() {
				return sink->websock;
			}
		};

		static response* make_websocket(connection &c,std::shared_ptr<websocket_sink> wssink) {
			bool has_heads=c.has_headers(
				"connection",
				"upgrade",
				//"origin",
				"sec-websocket-version",
				"sec-websocket-key");
			//printf("Has heads?:%d\n",has_heads);
			if (!has_heads)
				return 0;
			bool has_upgrade=false;
			c.csvheaders("connection",[&](auto& hval){ if (hval=="upgrade") has_upgrade=true; },true);
			if (!has_upgrade) {
				return 0;
			}
			if (c.lowerheader("upgrade")!="websocket") {
				return 0;
			}
			if (*c.header("sec-websocket-version")!="13") {
				return 0;
			}
			// hash the key and guid to know the response hash
			net11::sha1 s;
			char hash[20];
			s.addbytes(*c.header("sec-websocket-key"));
			s.addbytes("258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
			s.digest(hash);
			// base64 encode the hash into a response token
			std::string rkey;
			net11::base64encoder().encode(rkey,hash,20,true);
			// now setup the response!
			websocket_response *ws=new websocket_response(wssink);
			ws->set_header("Upgrade","websocket");
			ws->set_header("Connection","upgrade");
			ws->set_header("Sec-Websocket-Accept",rkey);
			//ws.set_header("sec-websocket-protocol") // proto?!
			return ws;
		}

		static response* make_websocket(connection &c,int max_packet,std::function<bool(websocket &s,std::vector<char>&)> on_data,std::function<void()> on_close=std::function<void()>()) {
			struct websocket_packet_sink : public websocket_sink {
				int max_packet;
				std::vector<char> data;
				std::function<bool(websocket &s,std::vector<char>&)> on_data;
				std::function<void()> on_close;
				websocket_packet_sink(
					std::weak_ptr<connection> c,
					int in_max_packet,
					std::function<bool(websocket &s,std::vector<char>&)> in_on_data,
					std::function<void()> in_on_close)
				:websocket_sink(c),max_packet(in_max_packet),on_data(in_on_data),on_close(in_on_close) {
				}
				bool packet_start(bool fin,int type,uint64_t size) {
					if ((data.size()+size)>max_packet)
						return false;
					return true;
				}
				void packet_data(char b) {
					data.push_back(b);
				}
				bool packet_end(bool fin,int type) {
					std::shared_ptr<websocket> ws(get_websocket());
					bool ok=true;
					if (fin) {
						on_data(*ws,data);
						data.clear();
					}
					return ok;
				}
				void websocket_closing() {
					if (on_close) {
						on_close();
						on_close=std::function<void()>();
					}
				}
			};
			std::shared_ptr<connection> sc=std::static_pointer_cast<connection>(c.shared_from_this());
			std::shared_ptr<websocket_packet_sink> sink(
				new websocket_packet_sink(std::weak_ptr<connection>(sc),max_packet,on_data,on_close)
			);
			return make_websocket(c,sink);
		}

		static action* match_file(connection &c,std::string urlprefix,std::string filepath) {
			if (0!=c.url().find(urlprefix))
				return 0; // not matching the prefix.
			std::string checked=c.url().substr(urlprefix.size());
			struct stat stbuf;
			int last='/';
			int end=checked.size();
			for (int i=0;i<checked.size();i++) {
				if (checked[i]=='\\')
					return make_text_response(500,"Bad request, \\ not allowed in url");
				if (checked[i]=='?') {
					end=i;
					break;
				}
				if (last=='/') {
					if (checked[i]=='.') {
						return 0;  // an error should be returned but could be an information leakage.
					} else if (checked[i]=='/') {
						return 0;  // an error should be returned but could be an information leakage.
					}
				}
				if (checked[i]=='/') {
					// check for directory presence!
					std::string tmp=filepath+checked.substr(0,i);
					int sr=stat( tmp.c_str(),&stbuf);
					if (sr) {
						return 0;
					}
					if (!(stbuf.st_mode&S_IFDIR)) {
						return 0;
					}
				}
				last=checked[i];
			}
			std::string tmp=filepath+checked.substr(0,end);
			if (stat(tmp.c_str(),&stbuf)) {
				return 0;
			}
			if (!(stbuf.st_mode&S_IFREG)) {
				return 0;
			}

			FILE *f=fopen(tmp.c_str(),"rb");
			if (!f)
				return 0;
			struct fh {
				FILE *f;
				fh(FILE *in_f):f(in_f){}
				~fh() {
					fclose(f);
				}
			};
			std::shared_ptr<fh> fp(new fh(f));

			return make_stream_response(200,[fp](buffer &ob) {
				int osz=ob.usage();
				int tr=ob.compact();
				int rc=fread(ob.to_produce(),1,tr,fp->f);
				if (rc>=0) {
					ob.produced(rc);
				}
				if (rc<=0) {
					//if (feof(fp->f))
					return false; // always stop sending on error
				}
				return true;
			})->set_header("content-length",std::to_string(stbuf.st_size));
		}
	}
}

#endif //!__HTTP_HPP__