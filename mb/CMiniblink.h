#pragma once
#include <windows.h>
#include <array>
#include <functional>
#include <string>
#include "wke.h"


namespace HttpBridge {
	enum JS_RESULT_TYPE {
		JS_UNDEFINED,
		JS_INT,
		JS_DOUBLE,
		JS_CHAR,
		JS_WCHAR,
		JS_BOOL
	};
	struct JS_ARG {
		const char* js;
		void* result;
		JS_RESULT_TYPE type;
		void* param;
		wkeWebFrameHandle frameId;
		wkeWebView webView;
	};
	enum MB_ACTION_SENDER {
		MOUSE,
		MENU,
		WHEEL,
		KEY
	};
	struct MB_ACTION_MOUSE_DATA {
		unsigned message;
		int x;
		int y;
		unsigned flags;
	};
	struct MB_ACTION_MENU_DATA {
		int x;
		int y;
		unsigned flags;
	};
	struct MB_ACTION_WHEEL_DATA {
		int delta;
		int x;
		int y;
		unsigned flags;
	};
	enum MB_ACTION_KEY_EVENT {
		UP,
		DOWN,
		PRESS
	};
	struct MB_ACTION_KEY_DATA {
		MB_ACTION_KEY_EVENT event;
		unsigned code;
		unsigned flags;
		bool systemKey;
	};
	struct MB_ACTION_ITEM {
		MB_ACTION_SENDER sender;
		void *data = NULL;
		bool async = false;
		~MB_ACTION_ITEM() {
			if (data != NULL) {
				if (sender == MB_ACTION_SENDER::KEY) {
					MB_ACTION_KEY_DATA *d = (MB_ACTION_KEY_DATA *)data;
					delete d;
					d = NULL;
					data = NULL;
				}
				else if(sender== MB_ACTION_SENDER::MENU)
				{
					MB_ACTION_MENU_DATA *d = (MB_ACTION_MENU_DATA *)data;
					delete d;
					d = NULL;
					data = NULL;
				}
				else if (sender == MB_ACTION_SENDER::MOUSE)
				{
					MB_ACTION_MOUSE_DATA *d = (MB_ACTION_MOUSE_DATA *)data;
					delete d;
					d = NULL;
					data = NULL;
				}
				else if (sender == MB_ACTION_SENDER::WHEEL)
				{
					MB_ACTION_WHEEL_DATA *d = (MB_ACTION_WHEEL_DATA *)data;
					delete d;
					d = NULL;
					data = NULL;
				}
			}
		}
	};
	struct MB_ACTION {
		MB_ACTION_ITEM *action = NULL;
		MB_ACTION *next = NULL;
		int sleep = 0;
		~MB_ACTION() {
			if (action != NULL) {
				delete action;
				action = NULL;
			}
			if (next != NULL) {
				delete next;
				next = NULL;
			}
		}
	};
	class CWebFrame {
	public:
		CWebFrame(wkeWebFrameHandle frameId, wkeWebView webview, HWND hwnd);
		bool IsMainFrame();
		const char* GetUrl() const;
		void RunJs(const char* js);
		void RunJs(const char* js, int* result);
		void RunJs(const char* js, double* result);
		void RunJs(const char* js, bool* result);
		void RunJs(const char* js, char* result, int len);
		void RunJs(const char* js, wchar_t* result, int len);
	private:
		wkeWebFrameHandle m_webFrame;
		wkeWebView m_web;
		HWND m_hwnd;
	};
	class CNetJob {
	public:
		CNetJob(wkeNetJob);
		void SetHTTPHeaderField(wchar_t* key, wchar_t* value, bool response = false);
		void SetMIMEType(const char* type);
		const char *GetMIMEType() const;
		void CancelRequest();
		wkeRequestType GetRequestMethod();
		wkePostBodyElements* GetPostBody();
		void SetResponseData(void* buf, int len);
		void HookRequest();
	private:
		wkeNetJob m_job;
	};
	class CMiniblink {
	public:
		CMiniblink();
		CMiniblink(wkeWebView mbWebView);
		CMiniblink(wkeWindowType type, HWND parent_hwnd, int width, int height);
		~CMiniblink();

	public:
		virtual LPCTSTR GetClass() const;
		void Goto(LPCTSTR url);
		virtual void DoInit();
		virtual void SetPos(RECT rc, bool bNeedUpdate = true);
		//virtual void DoEvent(TEventUI& event);
		virtual LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled);
	public:
		virtual bool OnClosing();
		virtual void OnTitleChanged(LPCWSTR title);
		virtual void OnUrlChanged(LPCWSTR url, CWebFrame *frame);
		virtual void OnAlert(LPCWSTR msg);
		virtual bool OnConfirm(LPCWSTR msg);
		virtual bool OnPrompt(LPCWSTR msg, LPCWSTR defaultResult, LPCWSTR result);
		virtual bool OnNavigation(wkeNavigationType navigationType, LPCWSTR url);
		virtual CMiniblink* OnNewOpen(wkeNavigationType navigationType, LPCWSTR url, const wkeWindowFeatures* windowFeatures);
		virtual void OnDocumentReady(CWebFrame* frame);
		virtual bool OnDownload(const char *url);
		virtual bool OnResponse(const char* url, CNetJob* job);
		virtual void OnConsole(wkeConsoleLevel level, LPCWSTR message, LPCWSTR sourceName, unsigned sourceLine, LPCWSTR stackTrace);
		virtual bool OnRequestBegin(const char * url, CNetJob* job);
		virtual void OnRequestEnd(const char* url, CNetJob* job, void* buf, int len);
		virtual void OnCreateScriptContext(CWebFrame* frame, void* context, int extensionGroup, int worldId);
		virtual void OnReleaseScriptContext(CWebFrame* frame, void* context, int worldId);
		virtual void OnMediaLoad(const char* url, wkeMediaLoadInfo* info);
		virtual void OnLoadingFinish(LPCWSTR url, wkeLoadingResult result, LPCWSTR failedReason);
	public:
		static void Initialize();
		static void SetWkeDllPath(LPCTSTR dllPath);
		static void SetGlobalProxy(const wkeProxy* proxy);
		static UINT GetRunJsMessageId();
		static UINT GetActionMessageId();
	public:
		unsigned GetVersion();
		wkeWebView GetWebView();
		bool IsDocumentReady();
		void Reload();
		LPCWSTR GetTitle();
		int GetWidth();
		int GetHeight();
		int GetContentWidth();
		int GetContentHeight();
		bool CanGoBack();
		bool GoBack();
		bool CanGoForward();
		bool GoForward();
		void EditorSelectAll();
		void EditorUnSelect();
		void EditorCopy();
		void EditorCut();
		void EditorDelete();
		void EditorUndo();
		void EditorRedo();
		LPCWSTR GetCookie();
		void SetCookie(const char *cookie, const char *url = NULL);
		const char *GetUrl() const;
		void SetCookieEnabled(bool);
		bool IsCookieEnabled();
		void SetCookieFile(LPCWSTR file);
		void SetLocalStoragePath(LPCWSTR file);
		void SetZoom(float factor);
		float GetZoom();
		void SetProxy(const wkeProxy* proxy);
		void SetMemoryCacheEnable(bool);
		void SetTouchEnabled(bool);
		void SetMouseEnabled(bool);
		void SetNavigationToNewWindowEnable(bool);
		void SetCspCheckEnable(bool);
		void SetNpapiPluginsEnabled(bool);
		void SetHeadlessEnabled(bool);
		void SetDebugConfig(const char*, const char*);
		void SetTransparent(bool);
		void SetUserAgent(LPCWSTR userAgent);
		const char *GetUserAgent() const;
		void SetHtml(const char* html, const char* baseUrl = NULL);
		void RunJs(const char* js);
		void RunJs(const char* js, int* result);
		void RunJs(const char* js, double* result);
		void RunJs(const char* js, bool* result);
		void RunJs(const char* js, char* result, int len);
		void RunJs(const char* js, wchar_t* result, int len);
		void SetSize(int width, int height);
		void SetDeviceParameter(const char* device, const char* paramStr, int paramInt, float paramFloat);
		void SetSettings(const wkeViewSettings* settings);
		void SimulationAction(MB_ACTION_ITEM* action);
		void SetFullScreen();
		/*将cookie写入到磁盘，方便后续应用程序读取*/
		void FlushCookie();
		void convertFilename(wchar_t* filename);
		void saveBitmap(void* pixels, int w, int h, const wchar_t* title);
		void takeScreenshot();

		void set_title(const wchar_t *s) { wkeSetWindowTitleW(m_wkeWebView, s); }
		void on_document_ready(std::function<void()> callback);
		void on_window_closing(std::function<bool()> callback);
		void load(const wchar_t *url) { wkeLoadURLW(m_wkeWebView, url); }
		void call(const char *name);
		template <typename T0>
		void call(const char *name, T0 arg0);
		template <typename T0, typename T1>
		void call(const char *name, T0 arg0, T1 arg1);
		template <typename T0, typename T1, typename T2>
		void call(const char *name, T0 arg0, T1 arg1, T2 arg2);
		template <typename T0, typename T1, typename T2, typename T3>
		void call(const char *name, T0 arg0, T1 arg1, T2 arg2, T3 arg3);

		void show() { wkeShowWindow(m_wkeWebView, true); }
		void hide() { wkeShowWindow(m_wkeWebView, false); }
		void minimize() { ShowWindow(GetHWND(), SW_MINIMIZE); }
		void enable() { wkeEnableWindow(m_wkeWebView, true); }
		void disable() { wkeEnableWindow(m_wkeWebView, false); }
		void clear_quit_on_close() { quit_on_close_ = false; }
		void set_quit_on_close() { quit_on_close_ = true; }
		void close() { PostMessageW(GetHWND(), WM_CLOSE, 0, 0); }

		HWND GetHWND() const { return wkeGetWindowHandle(m_wkeWebView); }
	public:
		static int counter;
	private:
		static jsValue WKE_CALL_TYPE urlCallback(jsExecState es, void *param);
		static jsValue WKE_CALL_TYPE onMsg(jsExecState es, void* param);
		static void WKE_CALL_TYPE MBDocumentReady(wkeWebView, void *param);
		static bool WKE_CALL_TYPE MBWindowClosing(wkeWebView, void *param);
		static void WKE_CALL_TYPE MBWindowDestroy(wkeWebView, void *param);
		static void WKE_CALL_TYPE MBPaintUpdate(wkeWebView webView, void* param, const HDC hdc, int x, int y, int cx, int cy);
		static void WKE_CALL_TYPE MBTitleChanged(wkeWebView webView, void* param, const wkeString title);
		static void WKE_CALL_TYPE MBUrlChanged(wkeWebView webView, void* param, wkeWebFrameHandle frameId, const wkeString url);
		static void WKE_CALL_TYPE MBAlert(wkeWebView webView, void* param, const wkeString msg);
		static bool WKE_CALL_TYPE MBConfirm(wkeWebView webView, void* param, const wkeString msg);
		static bool WKE_CALL_TYPE MBPromptBox(wkeWebView webView, void* param, const wkeString msg, const wkeString defaultResult, wkeString result);
		static bool WKE_CALL_TYPE MBNavigation(wkeWebView webView, void* param, wkeNavigationType navigationType, const wkeString url);
		static wkeWebView WKE_CALL_TYPE MBCreateView(wkeWebView webView, void* param, wkeNavigationType navigationType, const wkeString url, const wkeWindowFeatures* windowFeatures);
		static void WKE_CALL_TYPE MBDocumentReady(wkeWebView webView, void* param, wkeWebFrameHandle frameId);
		static bool WKE_CALL_TYPE MBDownload(wkeWebView webView, void* param, const char* url);
		static wkeDownloadOpt WKE_CALL_TYPE MBDownload2(wkeWebView webView, void* param, size_t expectedContentLength, const char* url, const char* mime, const char* disposition, wkeNetJob job, wkeNetJobDataBind* dataBind);
		static bool WKE_CALL_TYPE MBNetResponse(wkeWebView webView, void* param, const char* url, wkeNetJob job);
		static void WKE_CALL_TYPE MBConsole(wkeWebView webView, void* param, wkeConsoleLevel level, const wkeString message, const wkeString sourceName, unsigned sourceLine, const wkeString stackTrace);
		static bool WKE_CALL_TYPE MBLoadUrlBegin(wkeWebView webView, void* param, const char* url, wkeNetJob job);
		static void WKE_CALL_TYPE MBLoadUrlEnd(wkeWebView webView, void* param, const char* url, wkeNetJob job, void* buf, int len);
		static void WKE_CALL_TYPE MBCreateScriptContext(wkeWebView webView, void* param, wkeWebFrameHandle frameId, void* context, int extensionGroup, int worldId);
		static void WKE_CALL_TYPE MBReleaseScriptContext(wkeWebView webView, void* param, wkeWebFrameHandle frameId, void* context, int worldId);
		static void WKE_CALL_TYPE MBMediaLoad(wkeWebView webView, void* param, const char* url, wkeMediaLoadInfo* info);
		static void WKE_CALL_TYPE MBLoadedFinish(wkeWebView webView, void* param, const wkeString url, wkeLoadingResult result, const wkeString failedReason);
		static void onDataRecv(void* ptr, wkeNetJob job, const char* data, int length);
		static void onDataFinish(void* ptr, wkeNetJob job, wkeLoadingResult result);
		void updateCursor();
	private:
		wkeWebView m_wkeWebView = NULL;
		int m_cursor;
		bool m_released = false;
		bool quit_on_close_ = false;
	};

	jsValue to_js_value(jsExecState, bool value);
	jsValue to_js_value(jsExecState, int value);
	jsValue to_js_value(jsExecState, double value);
	jsValue to_js_value(jsExecState es, const char *value);
	jsValue to_js_value(jsExecState es, const wchar_t *value);
	jsValue to_js_value(jsExecState es, const std::string &value);
	jsValue to_js_value(jsExecState es, const std::wstring &value);

	template <typename T0>
	void CMiniblink::call(const char *name, T0 arg0) {
		if (!m_wkeWebView) {
			return;
		}
		jsExecState es = wkeGlobalExec(m_wkeWebView);
		jsValue func = jsGetGlobal(es, name);
		std::array<jsValue, 1> js_args;
		js_args[0] = to_js_value(es, arg0);
		jsCall(
			es, func, jsUndefined(), js_args.data(),
			static_cast<int>(js_args.size()));
	}

	template <typename T0, typename T1>
	void CMiniblink::call(const char *name, T0 arg0, T1 arg1) {
		if (!m_wkeWebView) {
			return;
		}
		jsExecState es = wkeGlobalExec(m_wkeWebView);
		jsValue func = jsGetGlobal(es, name);
		std::array<jsValue, 2> js_args;
		js_args[0] = to_js_value(es, arg0);
		js_args[1] = to_js_value(es, arg1);
		jsCall(
			es, func, jsUndefined(), js_args.data(),
			static_cast<int>(js_args.size()));
	}

	template <typename T0, typename T1, typename T2>
	void CMiniblink::call(const char *name, T0 arg0, T1 arg1, T2 arg2) {
		if (!m_wkeWebView) {
			return;
		}
		jsExecState es = wkeGlobalExec(m_wkeWebView);
		jsValue func = jsGetGlobal(es, name);
		std::array<jsValue, 3> js_args;
		js_args[0] = to_js_value(es, arg0);
		js_args[1] = to_js_value(es, arg1);
		js_args[2] = to_js_value(es, arg2);
		jsCall(
			es, func, jsUndefined(), js_args.data(),
			static_cast<int>(js_args.size()));
	}

	template <typename T0, typename T1, typename T2, typename T3>
	void CMiniblink::call(const char *name, T0 arg0, T1 arg1, T2 arg2, T3 arg3) {
		if (!m_wkeWebView) {
			return;
		}
		jsExecState es = wkeGlobalExec(m_wkeWebView);
		jsValue func = jsGetGlobal(es, name);
		std::array<jsValue, 4> js_args;
		js_args[0] = to_js_value(es, arg0);
		js_args[1] = to_js_value(es, arg1);
		js_args[2] = to_js_value(es, arg2);
		js_args[3] = to_js_value(es, arg3);
		jsCall(
			es, func, jsUndefined(), js_args.data(),
			static_cast<int>(js_args.size()));
	}

}