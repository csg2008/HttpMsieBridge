// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once
#ifndef __LOG_H__
#define __LOG_H__

#include <io.h>
#include <fcntl.h>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "defines.h"
#include "file_utils.h"
#include "string_utils.h"


inline std::string NowTime();
static HANDLE g_logFileHandle = NULL;

enum TLogLevel {logCRITICAL, logERROR, logWARNING, logINFO, logDEBUG, logDEBUG1, logDEBUG2, logDEBUG3, logDEBUG4, logTRACE};

template <typename T>
class Log {
public:
    Log();
    virtual ~Log();
    std::ostringstream& Get(TLogLevel level = logINFO);
public:
    static TLogLevel& ReportingLevel();
    static std::string ToString(TLogLevel level);
    static TLogLevel FromString(const std::string& level);
protected:
    std::ostringstream os;
private:
    Log(const Log&);
    Log& operator =(const Log&);
};

template <typename T>
Log<T>::Log() {
}

template <typename T>
std::ostringstream& Log<T>::Get(TLogLevel level) {
    os << "- " << NowTime();
    os << " " << ToString(level) << ": ";
    os << std::string(level > logDEBUG ? level - logDEBUG : 0, '\t');
    return os;
}

template <typename T>
Log<T>::~Log() {
    os << std::endl;
    if (T::Stream() != stderr) {
        fprintf(stderr, "%s", os.str().c_str());
        fflush(stderr);
    }
    T::Output(os.str());
}

template <typename T>
TLogLevel& Log<T>::ReportingLevel() {
    static TLogLevel reportingLevel = logDEBUG4;
    return reportingLevel;
}

template <typename T>
std::string Log<T>::ToString(TLogLevel level) {
	static const char* const buffer[] = {"CRITICAL", "ERROR", "WARNING", "INFO", "DEBUG", "DEBUG1", "DEBUG2", "DEBUG3", "DEBUG4", "TRACE"};
    return buffer[level];
}

template <typename T>
TLogLevel Log<T>::FromString(const std::string& level) {
    if (level == "TRACE")
        return logTRACE;
    if (level == "DEBUG4")
        return logDEBUG4;
    if (level == "DEBUG3")
        return logDEBUG3;
    if (level == "DEBUG2")
        return logDEBUG2;
    if (level == "DEBUG1")
        return logDEBUG1;
    if (level == "DEBUG")
        return logDEBUG;
    if (level == "INFO")
        return logINFO;
    if (level == "WARNING")
        return logWARNING;
    if (level == "ERROR")
        return logERROR;
    if (level == "CRITICAL")
        return logCRITICAL;
    Log<T>().Get(logWARNING) << "Unknown logging level '" << level << "'. Using INFO level as default.";
    return logINFO;
}

class Output2FILE
{
public:
    static FILE*& Stream();
    static void Output(const std::string& msg);
};

inline FILE*& Output2FILE::Stream() {
    static FILE* pStream = stderr;
    return pStream;
}

inline void Output2FILE::Output(const std::string& msg) {
    FILE* pStream = Stream();
    if (!pStream)
        return;
    fprintf(pStream, "%s", msg.c_str());
    fflush(pStream);
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#   if defined (BUILDING_FILELOG_DLL)
#       define FILELOG_DECLSPEC   __declspec (dllexport)
#   elif defined (USING_FILELOG_DLL)
#       define FILELOG_DECLSPEC   __declspec (dllimport)
#   else
#       define FILELOG_DECLSPEC
#   endif // BUILDING_DBSIMPLE_DLL
#else
#   define FILELOG_DECLSPEC
#endif // _WIN32

class FILELOG_DECLSPEC FILELog : public Log<Output2FILE> {};
//typedef Log<Output2FILE> FILELog;

#ifndef FILELOG_MAX_LEVEL
#define FILELOG_MAX_LEVEL logTRACE
#endif

#define LOG(level) \
    if (level > FILELOG_MAX_LEVEL) ;\
    else if (level > FILELog::ReportingLevel() || !Output2FILE::Stream()) ; \
    else FILELog().Get(level)

#define LOG_CRITICAL LOG(logCRITICAL)
#define LOG_ERROR LOG(logERROR)
#define LOG_WARNING LOG(logWARNING)
#define LOG_INFO LOG(logINFO)
#define LOG_DEBUG LOG(logDEBUG)
#define LOG_TRACE LOG(logTRACE)

inline std::string NowTime() {
    std::stringstream ss;
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	ss << std::put_time(std::localtime(&t), "%F %T");
	return ss.str();
}

static void InitializeLogging(bool show_console, std::string log_level, std::string log_file) {
    if (show_console) {
        AllocConsole();
        FILE* freopen_file;
        freopen_s(&freopen_file, "CONIN$", "rb", stdin);
        freopen_s(&freopen_file, "CONOUT$", "wb", stdout);
        freopen_s(&freopen_file, "CONOUT$", "wb", stderr);
    }

    if (log_level.length()) {
        FILELog::ReportingLevel() = FILELog::FromString(log_level);
    } else {
        FILELog::ReportingLevel() = logINFO;
    }

    if (log_file.length()) {
        // The log file needs to be opened in shared mode so that
        // CEF subprocesses can also append to this file.
        // Converting HANDLE to FILE*:
        // http://stackoverflow.com/a/7369662/623622
        // Remember to call ShutdownLogging() to close the g_logFileHandle.
        g_logFileHandle = CreateFile(
                    Utf8ToWide(log_file).c_str(),
                    GENERIC_WRITE,
                    FILE_SHARE_WRITE,
                    NULL,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
        if (g_logFileHandle == INVALID_HANDLE_VALUE) {
            g_logFileHandle = NULL;
            LOG_ERROR << "Opening log file for appending failed";
            return;
        }
        int fd = _open_osfhandle((intptr_t)g_logFileHandle, _O_APPEND | _O_RDONLY);
        if (fd == -1) {
            LOG_ERROR << "Opening log file for appending failed, _open_osfhandle() failed";
            return;
        }
        FILE* pFile = _fdopen(fd, "a+");
        if (pFile == 0) {
            _close(fd);
            LOG_ERROR << "Opening log file for appending failed, _fdopen() failed";
            return;
        }
        // TODO: should we call fclose(pFile) later?
        Output2FILE::Stream() = pFile;
    }
}

static void ShutdownLogging() {
    if (g_logFileHandle) {
        CloseHandle(g_logFileHandle);
    }
}

#endif //__LOG_H__