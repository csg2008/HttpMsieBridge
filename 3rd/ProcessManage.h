#pragma once

/*
	Because of https://connect.microsoft.com/VisualStudio/feedback/details/888527/warnings-on-dbghelp-h
	So when include DbgHelp.h (Even when using visual studio 2015 update 3 with windows 8.1 SDK) two 4091 warrning will be shown
	duirng build process. So just disable showing them. The WindowsKillLibrary will be built without any problem.
*/
#pragma warning( disable : 4091 )
#pragma comment(lib,"Dbghelp.lib")

#include <tchar.h>
#include <Windows.h>
#include <DbgHelp.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <fstream>
#include <iomanip>
#include <memory>
#include <thread>
#include <list>
#include <sstream>

namespace Process {
	/// <summary>
	/// Signal class to store and validate the signal we want to send.
	/// </summary>
	class Signal {
	private:
		/// <summary>
		/// The process id (pid) that this signal will be sent to.
		/// </summary>
		DWORD pid;

		/// <summary>
		/// Signal type.
		/// Types:
		///		1) 0 CTRL_C		SIGINT
		///		2) 1 CTRL_BREAK	SIGBREAK
		/// </summary>
		DWORD_PTR type;

		/// <summary>
		/// Validates the type of signal to see if supported.
		/// </summary>
		/// <param name="type">type</param>
		static void validateType(DWORD_PTR type) {
			if (!(type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT)) {
				throw std::invalid_argument(std::string("EINVAL"));
			}
		}

		/// <summary>
		/// Checks that process with given pid exist.
		/// </summary>
		/// <param name="pid">The pid.</param>
				// TODO: Maybe there are better solution to check that pid exist.
		// SOURCE:  http://cppip.blogspot.co.uk/2013/01/check-if-process-is-running.html
		static void Signal::checkPidExist(DWORD pid) {
			bool exist = false;
			HANDLE pss = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
			PROCESSENTRY32 pe = { 0 };
			pe.dwSize = sizeof(pe);
			if (Process32First(pss, &pe)) {
				do {
					if (pe.th32ProcessID == pid) {
						exist = true;
					}
				} while (Process32Next(pss, &pe));
			}
			CloseHandle(pss);

			if (!exist) {
				throw std::invalid_argument(std::string("ESRCH"));
			}
		}
	public:
		/// <summary>
		/// Initializes a new instance of the <see cref="Signal"/> class.
		/// </summary>
		/// <param name="pid">Target process id</param>
		/// <param name="type">type</param>
		Signal(DWORD pid, DWORD_PTR type) {
			Signal::validateType(type);
			this->type = type;
			Signal::checkPidExist(pid);
			this->pid = pid;
		}

		/// <summary>
		/// Gets the type of signal.
		/// </summary>
		/// <returns>Signal's type</returns>
		DWORD_PTR getType() {
			return this->type;
		}

		/// <summary>
		/// Gets the pid of target process.
		/// </summary>
		/// <returns>Target process id</returns>
		DWORD getPid() {
			return this->pid;
		}
	};

	/// <summary>
	/// Ctrl routine for finding and caching the address of specific event type.
	/// The address will be chached. So no need to do duplicate address finding.
	/// </summary>
	class CtrlRoutine {
	private:
		/// <summary>
		/// The event type of ctrl routine.
		/// </summary>
		DWORD event_type;

		/// <summary>
		/// The address of ctrl routine.
		/// </summary>
		LPTHREAD_START_ROUTINE address;

		/// <summary>
		/// The found address event.
		/// </summary>
		HANDLE found_address_event;

		static CtrlRoutine* current_routine;

		/// <summary>
		/// Gets the type of ctrl routine event.
		/// </summary>
		/// <returns>Ctrl Routine Event Type</returns>
		DWORD getEventType(void) {
			return this->event_type;
		}

		/// <summary>
		/// Finds the address by stackbacktrace.
		/// </summary>
		// TODO: a rewrite needed.
		void findAddressByStackBackTrace() {
			LPVOID ctrlRoutine;

			USHORT count = CaptureStackBackTrace((ULONG)2, (ULONG)1, &ctrlRoutine, NULL);
			if (count != 1) {
				return;
				/*	TODO: Remove. No Exception Available here.
					throw std::runtime_error(std::string("Cannot capture stack back trace."));
				*/
			}

			HANDLE hProcess = GetCurrentProcess();
			if (!SymInitialize(hProcess, NULL, TRUE)) {
				return;
				/*	TODO: Remove. No Exception Available here.
					throw std::runtime_error(std::string("Cannot SymInitialize. Code: ") + std::to_string(GetLastError()));
				*/
			}

			ULONG64 buffer[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
			PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
			pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSymbol->MaxNameLen = MAX_SYM_NAME;

			LPVOID funcCtrlRoutine = NULL;
			DWORD64 dwDisplacement = 0;
			if (!SymFromAddr(hProcess, (DWORD64)ctrlRoutine, &dwDisplacement, pSymbol)) {
				return;
				/*	TODO: Remove. No Exception Available here.
					throw std::runtime_error(std::string("Cannot SymFromAddr. Code: ") + std::to_string(GetLastError()));
				*/
			}
			funcCtrlRoutine = reinterpret_cast<LPVOID>(pSymbol->Address);

			SymCleanup(hProcess);
			this->address = (LPTHREAD_START_ROUTINE)funcCtrlRoutine;
		}

		/// <summary>
		/// Remove custom consoleCtrlHandler
		/// </summary>
		void removeCustomConsoleCtrlHandler(void) {
			if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlRoutine::customConsoleCtrlHandler, false)) {
				throw std::system_error(std::error_code(GetLastError(), std::system_category()), "ctrl-routine:removeCustomConsoleCtrlHandler");
			}
		}

		/// <summary>
		/// Closes the opend found address event.
		/// </summary>
		void closeFoundAddressEvent(void) {
			if (this->found_address_event != NULL) {
				if (!CloseHandle(this->found_address_event)) {
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "ctrl-routine:closeFoundAddressEvent");
				}
				this->found_address_event = (HANDLE)NULL;
			}
		}

		/// <summary>
		/// Custom console ctrl handler.
		/// </summary>
		/// <param name="ctrl_type">Type of the ctrl event.</param>
		/// <returns></returns>
		static BOOL customConsoleCtrlHandler(DWORD ctrl_type) {
			if (ctrl_type != CtrlRoutine::current_routine->getEventType()) {
				return FALSE;
			}
			// TODO: remove;
			//std::cout << "TYPE: " << CtrlRoutine::current_routine->getType() << "\n";

			CtrlRoutine::current_routine->findAddressByStackBackTrace();

			SetEvent(CtrlRoutine::current_routine->found_address_event);

			/* TODO: Remove. No Exception Available here.
			if (!SetEvent(CtrlRoutine::current_routine->found_address_event)) {
				throw std::runtime_error(std::string("Cannot set event for found address event handle. Code: ") + std::to_string(GetLastError()));
			}
			*/

			return TRUE;
		}
	public:
		/// <summary>
		/// Initializes a new instance of the <see cref="CtrlRoutine" /> class.
		/// </summary>
		/// <param name="event_type">Type of the event.</param>
		CtrlRoutine(DWORD event_type) {
			this->event_type = event_type;
			this->address = (LPTHREAD_START_ROUTINE)NULL;
			this->found_address_event = (HANDLE)NULL;
		}

		/// <summary>
		/// Find and set the address of ctrl routine.
		/// </summary>
		void findAddress() {
			if (this->address == NULL) {
				/*
					Why set a refrence to "this"? Well because it's impossible set a non static class method as ctrl handler of SetConsoleCtrlHandler.
					So set a refrence to "this" and then use this refrence "CtrlRoutine::current_routine" for future needs.
				*/
				CtrlRoutine::current_routine = &*this;

				this->found_address_event = CreateEvent(NULL, true, false, NULL);

				if (this->found_address_event == NULL) {
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "ctrl-routine:findAddress:CreateEvent");
				}

				if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlRoutine::customConsoleCtrlHandler, true)) {
					this->removeCustomConsoleCtrlHandler();
					this->closeFoundAddressEvent();
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "ctrl-routine:findAddress:SetConsoleCtrlHandlert");
				}

				if (!GenerateConsoleCtrlEvent(this->event_type, 0)) {
					this->removeCustomConsoleCtrlHandler();
					this->closeFoundAddressEvent();
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "ctrl-routine:findAddress:GenerateConsoleCtrlEvent");
				}

				DWORD dwWaitResult = WaitForSingleObject(this->found_address_event, INFINITE);
				if (dwWaitResult == WAIT_FAILED) {
					this->removeCustomConsoleCtrlHandler();
					this->closeFoundAddressEvent();
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "ctrl-routine:findAddress:WaitForSingleObject");
				}

				if (this->address == NULL) {
					this->removeCustomConsoleCtrlHandler();
					this->closeFoundAddressEvent();
					throw std::runtime_error(std::string("ctrl-routine:findAddress:checkAddressIsNotNull"));
				}

				this->removeCustomConsoleCtrlHandler();
				this->closeFoundAddressEvent();
			}
			// TODO: remove;
			//std::cout << this->event_type << " : " << this->address << "\n";
		}

		/// <summary>
		/// Gets the address of ctrl routine.
		/// </summary>
		/// <returns>Ctrl Routine Address</returns>
		LPTHREAD_START_ROUTINE getAddress(void) {
			/*
				TODO: Currently caller should call findAddress before getAddress. Otherwise NULL will be returned.
				Maybe it's better to remove the findAddress call (or make it a private method) and use below commented code
				to find the address if address is NULL.
			*/
			/*
				NOTE: The reason for commenting the below snippet code and forcing the caller to call the findAddress
				before call getAddress is performance and speed. Becasue current approach will get the ctrl routine address first and then
				the remote process open and start will happen. So if there is any prbolem (exception) in getting the ctrl routine address,
				the remote process open will not fire. If we change the current approach, the findAddress method will not fire
				until the remote process startRemoteThread method call. So the remote process open method will be called even we can't
				find the ctrl routine address and send the signal. So current approach is  better for performance and speed.
			*/
			/*
			if (this->address == NULL) {
				CtrlRoutine::findAddress();
			}
			*/
			return this->address;
		}
	};

	class RemoteProcess {
	private:
		/// <summary>
		/// The needed access for open remote process.
		/// </summary>
		static const DWORD NEEDEDACCESS = PROCESS_QUERY_INFORMATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD;

		/// <summary>
		/// The that we want to send.
		/// </summary>
		Signal* the_signal;

		/// <summary>
		/// The control routine corresponding to the signal.
		/// </summary>
		CtrlRoutine* the_ctrl_routine;

		/// <summary>
		/// The process token handle.
		/// </summary>
		HANDLE process_token;

		/// <summary>
		/// The remote process handle.
		/// </summary>
		HANDLE handle;

		/// <summary>
		/// The remote thread handle.
		/// </summary>
		HANDLE remote_thread;

		/// <summary>
		/// Closes the opend process token by handle.
		/// </summary>
		void closeProcessToken(void) {
			if (this->process_token != NULL) {
				if (!CloseHandle(this->process_token)) {
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:closeProcessToken");
				}
				this->process_token = (HANDLE)NULL;
			}
		}

		/// <summary>
		/// Sets the needed privilege for opening the remote process.
		/// </summary>
		/// <param name="enable_privilege">if set to <c>true</c> [enable privilege].</param>
		/// <returns></returns>
		bool setPrivilege(bool enable_privilege) {
			TOKEN_PRIVILEGES tp;
			LUID luid;

			if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
				return false;
			}

			tp.PrivilegeCount = 1;
			tp.Privileges[0].Luid = luid;
			if (enable_privilege) {
				tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			} else {
				tp.Privileges[0].Attributes = 0;
			}

			AdjustTokenPrivileges(this->process_token, false, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);

			if (GetLastError() != 0L) {
				return false;
			}

			return true;
		}

		/// <summary>
		/// Closes the opend remote thread by handle.
		/// </summary>
		void closeRemoteThread(void) {
			if (this->remote_thread != NULL) {
				if (!CloseHandle(this->remote_thread)) {
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:closeRemoteThread");
				}
				this->remote_thread = (HANDLE)NULL;
			}
		}

		/// <summary>
		/// Closes the opend handle.
		/// </summary>
		void closeHandle(void) {
			if (this->handle != NULL) {
				if (!CloseHandle(this->handle)) {
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:closeHandle");
				}
				this->handle = (HANDLE)NULL;
			}
		}
	public:
		/// <summary>
		/// Initializes a new instance of the <see cref="RemoteProcess"/> class.
		/// </summary>
		RemoteProcess() {
			this->process_token = (HANDLE)NULL;
			this->handle = (HANDLE)NULL;
			this->remote_thread = (HANDLE)NULL;
		}

		/// <summary>
		/// Sets the Signal that we want to send.
		/// </summary>
		/// <param name="the_signal">The signal.</param>
		void setSignal(Signal* the_signal) {
			this->the_signal = the_signal;
		}

		/// <summary>
		/// Sets the ctrl routine corresponding to the signal.
		/// </summary>
		/// <param name="the_ctrl_routine">The control routine.</param>
		void setCtrlRoutine(CtrlRoutine* the_ctrl_routine) {
			this->the_ctrl_routine = the_ctrl_routine;
		}

		/// <summary>
		/// Opens the remote process.
		/// </summary>
		void open(void) {
			this->handle = OpenProcess(RemoteProcess::NEEDEDACCESS, false, this->the_signal->getPid());

			if (this->handle == NULL) {
				if (GetLastError() != ERROR_ACCESS_DENIED) {
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:open:OpenProcess");
				}

				if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &this->process_token)) {
					this->closeProcessToken();
					throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:open:OpenProcessToken");
				}

				if (!this->setPrivilege(true)) {
					this->closeProcessToken();
					throw std::runtime_error(std::string("EPERM"));
				}

				this->handle = OpenProcess(RemoteProcess::NEEDEDACCESS, false, this->the_signal->getPid());

				if (this->handle == NULL) {
					this->setPrivilege(false);
					this->closeProcessToken();
					throw std::runtime_error(std::string("EPERM"));
					// throw std::runtime_error(std::string("remote-process:open:setPrivilege(true):OpenProcess:code:") + std::to_string(GetLastError()));
				}
			}
		}

		/// <summary>
		/// Starts the remote thread and trigger ctrl routine by address.
		/// </summary>
		void startRemoteThread(void) {
			this->remote_thread = CreateRemoteThread(
				this->handle,
				NULL,
				0,
				this->the_ctrl_routine->getAddress(),
				(LPVOID)this->the_signal->getType(),
				CREATE_SUSPENDED,
				NULL
			);

			if (this->remote_thread == NULL) {
				this->closeHandle();
				throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:startRemoteThread:CreateRemoteThread");
			}

			if (ResumeThread(this->remote_thread) == (DWORD)-1) {
				this->closeHandle();
				this->closeRemoteThread();
				throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:startRemoteThread:ResumeThread");
			}

			if (WaitForSingleObject(this->remote_thread, INFINITE) != WAIT_OBJECT_0) {
				this->closeHandle();
				this->closeRemoteThread();
				throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:startRemoteThread:WaitForSingleObject");
			}

			DWORD exit_code = 0;
			if (!GetExitCodeThread(this->remote_thread, (LPDWORD)&exit_code)) {
				this->closeHandle();
				this->closeRemoteThread();
				throw std::system_error(std::error_code(GetLastError(), std::system_category()), "remote-process:startRemoteThread:GetExitCodeThread");
			}

			if (exit_code = STATUS_CONTROL_C_EXIT) {
				this->closeHandle();
				this->closeRemoteThread();
			}
			else {
				this->closeHandle();
				this->closeRemoteThread();
				throw std::runtime_error(std::string("remote-process:startRemoteThread:exitCode"));
			}
		}
	};

	/// <summary>
	/// The main part that will send the given signal to target process.
	/// </summary>
	class Sender {
	private:
		Sender();
		static CtrlRoutine ctrl_c_routine;
		static CtrlRoutine ctrl_break_routine;
	public:
		/// <summary>
		/// Sends the signal to the target process.
		/// </summary>
		/// <param name="the_signal">Signal instance</param>
		static void send(Signal the_signal) {
			RemoteProcess the_remote_process;
			the_remote_process.setSignal(&the_signal);

			if (the_signal.getType() == CTRL_C_EVENT) {
				ctrl_c_routine.findAddress();
				the_remote_process.setCtrlRoutine(&Sender::ctrl_c_routine);
			} else {
				ctrl_break_routine.findAddress();
				the_remote_process.setCtrlRoutine(&Sender::ctrl_break_routine);
			}

			the_remote_process.open();
			the_remote_process.startRemoteThread();
		}

		/// <summary>
		/// Warm-up signal sender by finding the ctr-routine address before sending any signal.
		/// </summary>
		/// <param name="which">Which ctr-routine to warm-up</param>
		static void warmUp(const std::string& which) {
			std::string all("ALL");
			std::string sigInt("SIGINT");
			std::string sigBreak("SIGBREAK");

			if (which.compare(all) == 0) {
				ctrl_c_routine.findAddress();
				ctrl_break_routine.findAddress();
			} else if (which.compare(sigInt) == 0) {
				ctrl_c_routine.findAddress();
			} else if (which.compare(sigBreak) == 0) {
				ctrl_break_routine.findAddress();
			} else {
				throw std::invalid_argument(std::string("Invalid which argument."));
			}
		}
	};

	/// <summary>
	/// Signal type of CTRL + C
	/// </summary>
	const DWORD SIGNAL_TYPE_CTRL_C = CTRL_C_EVENT;

	/// <summary>
	/// Signal type of CTRL + Break
	/// </summary>
	const DWORD SIGNAL_TYPE_CTRL_BREAK = CTRL_BREAK_EVENT;

	/// <summary>
	/// a refrence to the *this used for using in SetConsoleCtrlHandler custom handler method (customConsoleCtrlHandler)
	/// </summary>
	CtrlRoutine* CtrlRoutine::current_routine = NULL;

	/*
		TODO: According to the test, the ctrl routine address for both of CTRL_C_EVENT and CTRL_BREAK_EVENT are same.
		So maybe it's not necessary to have separated ctr routine for each of events.
	*/
	/*
		NOTE: I've just tested the addresses on my own laptop (Windows 7 64bit). Also i can't find any document/article about
		this topic. So i think there is not enough evidence to merge these ctrl routines.
	*/
	/// <summary>
	/// Ctrl routine for CTRL_C Signal.
	/// </summary>
	CtrlRoutine Sender::ctrl_c_routine = CtrlRoutine(CTRL_C_EVENT);

	/// <summary>
	/// Ctrl routine for CTRL_BREAK Signal.
	/// </summary>
	CtrlRoutine Sender::ctrl_break_routine = CtrlRoutine(CTRL_BREAK_EVENT);

	/// <summary>
	/// Sends the signal.
	/// </summary>
	/// <param name="signal_pid">The signal target process id.</param>
	/// <param name="signal_type">The signal type.</param>
	void sendSignal(DWORD signal_pid, DWORD signal_type) {
		Signal the_signal(signal_pid, signal_type);
		Sender::send(the_signal);
	}

	/// <summary>
	/// Calls sender warm-up method.
	/// </summary>
	/// <param name="which">Which ctr-routine to warm-up</param>
	void warmUp(const std::string& which = "ALL") {
		Sender::warmUp(which);
	}

	int GenerateMiniDump(PEXCEPTION_POINTERS pExceptionPointers) {
		typedef BOOL(WINAPI * MiniDumpWriteDumpT)(
			HANDLE,
			DWORD,
			HANDLE,
			MINIDUMP_TYPE,
			PMINIDUMP_EXCEPTION_INFORMATION,
			PMINIDUMP_USER_STREAM_INFORMATION,
			PMINIDUMP_CALLBACK_INFORMATION
		);

		MiniDumpWriteDumpT pfnMiniDumpWriteDump = NULL;
		HMODULE hDbgHelp = LoadLibrary(_T("DbgHelp.dll"));
		if (NULL == hDbgHelp) {
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		pfnMiniDumpWriteDump = (MiniDumpWriteDumpT)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");

		if (NULL == pfnMiniDumpWriteDump) {
			FreeLibrary(hDbgHelp);
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		// create dump file
		TCHAR szFileName[MAX_PATH] = {0};
		TCHAR* szVersion = _T("DumpDemo_v1.0");
		SYSTEMTIME stLocalTime;
		GetLocalTime(&stLocalTime);
		wsprintf(szFileName, L"%s-%04d%02d%02d-%02d%02d%02d.dmp",
			szVersion, stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
			stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond);
		HANDLE hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
		if (INVALID_HANDLE_VALUE == hDumpFile) {
			FreeLibrary(hDbgHelp);
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		// save to dump file
		MINIDUMP_EXCEPTION_INFORMATION expParam;
		expParam.ThreadId = GetCurrentThreadId();
		expParam.ExceptionPointers = pExceptionPointers;
		expParam.ClientPointers = FALSE;
		pfnMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
			hDumpFile, MiniDumpWithDataSegs, (pExceptionPointers ? &expParam : NULL), NULL, NULL);

		// release file
		CloseHandle(hDumpFile);
		FreeLibrary(hDbgHelp);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	LONG WINAPI ExceptionFilter(LPEXCEPTION_POINTERS lpExceptionInfo) {
		if (IsDebuggerPresent()) {
			return EXCEPTION_CONTINUE_SEARCH;
		}

		return GenerateMiniDump(lpExceptionInfo);
	}


	const bool send_manual_backtrace = false;
	std::wstring minidump_filenamew = L"MiniDump.dmp";
	std::string minidump_filename = "MiniDump.dmp";
	std::string log_file_path = "./test.log";

	bool report_unsuccessful = false;

	std::time_t app_start_timestamp;

	std::string get_uuid() noexcept;
	std::string get_timestamp() noexcept;
	std::string get_logs_json() noexcept;

	double get_time_from_start() noexcept;

	void save_start_timestamp();
	std::string get_command_line() noexcept;

	void handle_exit() noexcept;
	void handle_crash(struct _EXCEPTION_POINTERS* ExceptionInfo, bool callAbort = true) noexcept;

	void print_stacktrace_sym(CONTEXT* ctx, std::ostringstream & report_stream) noexcept; //Prints stack trace based on context record

	std::string escapeJsonString(const std::string& input) noexcept;

	std::string prepare_crash_report(struct _EXCEPTION_POINTERS* ExceptionInfo, std::string minidump_result) noexcept {
		std::ostringstream json_report;

		json_report << "{";
		json_report << "	\"event_id\": \"" << get_uuid() << "\", ";
		json_report << "	\"timestamp\": \"" << get_timestamp() << "\", ";
		if (send_manual_backtrace) {
			json_report << "	\"exception\": {\"values\":[{";
			if (ExceptionInfo) {
				json_report << "		\"type\": \"" << ExceptionInfo->ExceptionRecord->ExceptionCode << "\", ";
			}
			//	json_report << "		\"value\": \"" << "ERROR_VALUE" << "\", ";
			//	json_report << "		\"module\": \"" << "MODULE_NAME" << "\", ";
			json_report << "		\"thread_id\": \"" << std::this_thread::get_id() << "\", ";

			if (ExceptionInfo) {
				std::string method;
				json_report << "		\"stacktrace\": { \"frames\" : [";
				print_stacktrace_sym(ExceptionInfo->ContextRecord, json_report);
				json_report << "		] } ";
			}
			json_report << "	}]}, ";
		}
		json_report << "	\"tags\": { ";
		json_report << "		\"app_build_timestamp\": \"" << __DATE__ << " " << __TIME__ << "\", ";
		json_report << "		\"release\": \"" << "0.0.23" << "\", ";
		json_report << "		\"os_version\": \"" << "WIN32" << "\" ";
		json_report << "	}, ";
		json_report << "	\"extra\": { ";
		json_report << "		\"app_run_time\": \"" << get_time_from_start() << "\", ";
		json_report << "		\"app_logs_listing\": " << get_logs_json() << " , ";
		json_report << "		\"minidump_result\": \"" << minidump_result << "\", ";
		json_report << "		\"console_args\": \"" << get_command_line() << "\" ";
		json_report << "	} ";
		json_report << "}";

		return json_report.str();
	}

	std::string create_mini_dump(EXCEPTION_POINTERS* pep, std::wstring filename) noexcept {
		std::string ret = "successfully";

		HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE)) {
			MINIDUMP_EXCEPTION_INFORMATION expParam = {0};

			expParam.ThreadId = GetCurrentThreadId();
			expParam.ExceptionPointers = pep;
			expParam.ClientPointers = TRUE;
			//expParam.ClientPointers = FALSE;

			const DWORD CD_Flags = MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpScanMemory | MiniDumpWithUnloadedModules | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithPrivateReadWriteMemory | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo | MiniDumpIgnoreInaccessibleMemory;

			BOOL rv = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)CD_Flags, (pep != 0) ? &expParam : 0, 0, 0);
			if(!rv) {
				ret = "failed to generate minidump: " + std::to_string(GetLastError());
			}

			CloseHandle(hFile);
		} else {
			ret = "failed to create file: " + std::to_string(GetLastError());
		}
		return ret;
	}

	void handle_crash(struct _EXCEPTION_POINTERS* ExceptionInfo, bool callAbort) noexcept {
		static bool insideCrashMethod = false;
		if (insideCrashMethod) {
			abort();
		}
		insideCrashMethod = true;

		std::string minidump_result = create_mini_dump(ExceptionInfo, minidump_filenamew);

		std::string report = prepare_crash_report(ExceptionInfo, minidump_result);

		//send_crash_to_sentry_sync(report);

		DeleteFile(minidump_filenamew.c_str());

		if (callAbort) {
			abort();
		}

		insideCrashMethod = false;
	}

	void handle_exit() noexcept {
		if (report_unsuccessful) {
			handle_crash(nullptr, false);
		}
	}

	void print_stacktrace_sym(CONTEXT* ctx, std::ostringstream & report_stream) noexcept {
		BOOL    result;
		HANDLE  process;
		HANDLE  thread;
		HMODULE hModule;

		STACKFRAME64 stack;
		ULONG        frame;
		DWORD64      displacement;

		DWORD			disp;
		IMAGEHLP_LINE64 *line;
		const int		MaxNameLen = 256;

		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		char module[MaxNameLen];
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

		memset(&stack, 0, sizeof(STACKFRAME64));

		process = GetCurrentProcess();
		thread = GetCurrentThread();
		displacement = 0;
	#if !defined(_M_AMD64)
		stack.AddrPC.Offset = (*ctx).Eip;
		stack.AddrPC.Mode = AddrModeFlat;
		stack.AddrStack.Offset = (*ctx).Esp;
		stack.AddrStack.Mode = AddrModeFlat;
		stack.AddrFrame.Offset = (*ctx).Ebp;
		stack.AddrFrame.Mode = AddrModeFlat;
	#endif

		SymInitialize(process, NULL, TRUE);
		bool first_element = true;
		for (frame = 0; ; frame++) {
			//get next call from stack
			result = StackWalk64
			(
	#if defined(_M_AMD64)
				IMAGE_FILE_MACHINE_AMD64
	#else
				IMAGE_FILE_MACHINE_I386
	#endif
				, process, thread, &stack, ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);

			if (!result) break;

			if (!first_element) {
				report_stream << ",";
			}
			report_stream << " { ";

			//get symbol name for address
			pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSymbol->MaxNameLen = MAX_SYM_NAME;
			if (SymFromAddr(process, (ULONG64)stack.AddrPC.Offset, &displacement, pSymbol)) {
				line = (IMAGEHLP_LINE64 *)malloc(sizeof(IMAGEHLP_LINE64));
				line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);



				report_stream << " 	\"function\": \"" << pSymbol->Name << "\", ";
				report_stream << " 	\"instruction_addr\": \"" << "0x" << std::uppercase << std::setfill('0') << std::setw(12) << std::hex << pSymbol->Address << "\", ";
				if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &disp, line)) {
					report_stream << " 	\"lineno\": \"" << line->LineNumber << "\", ";
					std::string file_name = line->FileName;
					file_name = escapeJsonString(file_name);
					report_stream << " 	\"filename\": \"" << file_name << "\", ";
				} else {
					//failed to get line number
				}
				free(line);
				line = NULL;
			} else {
				report_stream << " 	\"function\": \"" << "unknown" << "\", ";
				report_stream << " 	\"instruction_addr\": \"" << "0x" << std::uppercase << std::setfill('0') << std::setw(12) << std::hex << (ULONG64)stack.AddrPC.Offset << "\", ";
			}

			hModule = NULL;
			lstrcpyA(module, "");
			if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)(stack.AddrPC.Offset), &hModule)) {
				if (hModule != NULL) {
					if (GetModuleFileNameA(hModule, module, MaxNameLen)) {
						std::string module_name = module;
						module_name = escapeJsonString(module_name);
						report_stream << " 	\"module\": \"" << module_name << "\" ";
					}
				}
			}

			first_element = false;
			report_stream << " } ";
		}
	}

	void save_start_timestamp() {
		std::time(&app_start_timestamp);
	}

	double get_time_from_start() noexcept {
		time_t current_time;
		time(&current_time);
		return difftime(current_time, app_start_timestamp);
	}

	std::string get_command_line() noexcept {
		std::string ret = "empty";
		LPWSTR lpCommandLine = GetCommandLine();
		if (lpCommandLine != nullptr) {
			std::wstring ws_args = std::wstring(lpCommandLine);
			ret = std::string(ws_args.begin(), ws_args.end());
			ret = escapeJsonString(ret);
		}

		return ret;
	}

	// 新的入口点，需要与当前正在用的整合
	void setup_crash_reporting() {
		save_start_timestamp();

		std::set_terminate([]() { handle_crash(nullptr); });

		SetUnhandledExceptionFilter([](struct _EXCEPTION_POINTERS* ExceptionInfo) {
			/* don't use if a debugger is present */
			if (IsDebuggerPresent()) {
				return LONG(EXCEPTION_CONTINUE_SEARCH);
			}

			handle_crash(ExceptionInfo);

			// Unreachable statement
			return LONG(EXCEPTION_CONTINUE_SEARCH);
		});

		// The atexit will check if updater was safelly closed
		std::atexit(handle_exit);
		std::at_quick_exit(handle_exit);
	}

	std::string get_logs_json() noexcept {
		std::list<std::string> last_logs;
		try {
			std::ifstream logfile(log_file_path);

			std::string logline;
			while (std::getline(logfile, logline)) {
				last_logs.push_back(std::string("\"") + escapeJsonString(logline) + std::string("\""));
				if(last_logs.size() > 100) {
					last_logs.pop_front();
				}
			}

		} catch (...) {
			return std::string(" \"failed to read logs\" ");
		}

		std::ostringstream ss;
		bool first_line = true;

		ss << " [ ";
		for (auto const& logline : last_logs) {
			if (first_line) {
				first_line = false;
			} else {
				ss << ", \n";
			}

			ss << logline;
		}
		ss << " ] \n";

		return ss.str();
	}

	std::string get_timestamp() noexcept {
		char buf[sizeof "xxxx-xx-xxTxx:xx:xxx\0"];
		std::time_t curent_time;

		std::time(&curent_time);
		std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&curent_time));

		return std::string(buf);
	}

	std::string get_uuid() noexcept {
		char result[33] = { '\0' }; //"fc6d8c0c43fc4630ad850ee518f1b9d0";
		std::srand(static_cast<unsigned int>(std::time(nullptr)));

		for (std::size_t i = 0; i < sizeof(result) - 1; ++i) {
			const auto r = static_cast<char>(std::rand() % 16);
			if (r < 10) {
				result[i] = '0' + r;
			} else {
				result[i] = 'a' + r - static_cast<char>(10);
			}
		}

		return std::string(result);
	}

	std::string escapeJsonString(const std::string& input) noexcept {
		std::ostringstream ss;
		for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
			switch (*iter) {
			case '\\': ss << "\\\\"; break;
			case '"': ss << "\\\""; break;
			case '/': ss << "\\/"; break;
			case '\b': ss << "\\b"; break;
			case '\f': ss << "\\f"; break;
			case '\n': ss << "\\n"; break;
			case '\r': ss << "\\r"; break;
			case '\t': ss << "\\t"; break;
			default: ss << *iter; break;
			}
		}

		return ss.str();
	}

	int test(int argc,char *argv[]) {
		DWORD signal_type;
		DWORD signal_pid;
		char* endptr;

		if (argc == 1) {
			std::cout << "Not enough argument. Use -h for help." << std::endl;
			return 0;
		} else if (argc == 2) {
			if (strcmp(argv[1], "-h") == 0) {
				std::cout << "You should give the signal type and the pid to send the signal.\n"
					<< "Example: windows-kill -SIGINT 1234\n"
					<< "-l\t List of available signals type." << std::endl;
			} else if (strcmp(argv[1], "-l") == 0) {
				std::cout << "Availabe Signal Types\n"
					<< "\t(1) (SIGBREAK) : CTR + Break\n"
					<< "\t(2) (SIGINT) : CTR + C\n" << std::endl;
			} else {
				std::cout << "Not enough argument. Use -h for help." << std::endl;
			}
			return 0;
		} else if (argc == 3) {
			if (strcmp(argv[1], "-1") == 0 || strcmp(argv[1], "-SIGBREAK") == 0) {
				signal_type = Process::SIGNAL_TYPE_CTRL_BREAK;
			} else if (strcmp(argv[1], "-2") == 0 || strcmp(argv[1], "-SIGINT") == 0) {
				signal_type = Process::SIGNAL_TYPE_CTRL_C;
			} else {
				std::cout << "Signal type " << argv[1] << " not supported. Use -h for help." << std::endl;
				return 0;
			}

			signal_pid = strtoul(argv[2], &endptr, 10);

			if ((endptr == argv[1]) || (*endptr != '\0')) {
				std::cout << "Invalid pid: " << argv[2] << std::endl;
				return 0;
			}
		}

		try {
			Process::sendSignal(signal_pid, signal_type);
			std::cout << "Signal sent successfuly. type: " << signal_type << " | pid: " << signal_pid << "\n";
		} catch (const std::invalid_argument& exception) {
			if (strcmp(exception.what(), "ESRCH") == 0) {
				std::cout << "Error: Pid dosen't exist." << std::endl;
			} else if(strcmp(exception.what(), "EINVAL") == 0){
				std::cout << "Error: Invalid signal type." << std::endl;
			} else {
				std::cout << "InvalidArgument: windows-kill-library: " << exception.what() << std::endl;
			}
		} catch (const std::system_error& exception) {
			std::cout << "SystemError " << exception.code() << ": " << exception.what() << std::endl;
		} catch (const std::runtime_error& exception) {
			if (strcmp(exception.what(), "EPERM") == 0) {
				std::cout << "Not enough permission to send the signal." << std::endl;
			} else {
				std::cout << "RuntimeError: windows-kill-library: " << exception.what() << std::endl;
			}
		} catch (const std::exception& exception) {
			std::cout << "Error: windows-kill-library: " << exception.what() << std::endl;
		}

		return 0;
	}
}
