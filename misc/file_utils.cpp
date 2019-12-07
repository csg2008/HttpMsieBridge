// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include <string>
#include <cstdio>
#include <wchar.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <shlobj.h>
#include <shellapi.h>
#include <windows.h>

#include "logger.h"
#include "file_utils.h"
#include "string_utils.h"

#pragma comment( lib, "shell32.lib")

SHELLEXECUTEINFO exec(std::string exeFile, int num, bool show, bool wait, bool asAdmin) {
    int pid = 0;
    int times = 0;
    bool succ = false;
    std::string::size_type pos = LowerString(exeFile).find(".exe");

    SHELLEXECUTEINFO seInfo = { 0 };;
    seInfo.cbSize = sizeof(seInfo);
    seInfo.lpDirectory = ConvertW(GetExecutableDirectory().c_str());
    seInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_NO_CONSOLE | SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    seInfo.hwnd = 0;
    seInfo.hInstApp = 0;

    if (asAdmin)
        seInfo.lpVerb = _T("runas");                // Operation to perform
    else
        seInfo.lpVerb = _T("open");

    if (pos + 4 >= exeFile.length()) {
        seInfo.lpFile = ConvertW(exeFile.c_str());
        seInfo.lpParameters = _T("");                  // Additional parameters
    } else {
        seInfo.lpFile = ConvertW(exeFile.substr(0, pos+4).c_str());
        seInfo.lpParameters = ConvertW(exeFile.substr(pos+4).c_str());
    }
    
    if (show) {
        seInfo.nShow = SW_SHOW;
    } else {
        seInfo.nShow = SW_HIDE;
    }
    
    while (times < num) {
        if (ShellExecuteEx(&seInfo)) {
            succ = true;
            pid = GetProcessId(seInfo.hProcess);

            if (wait) {
                WaitForSingleObject(seInfo.hProcess, INFINITE);
            }

            break;
        } else {
            times++;
        }
    }

    LOG_INFO << "Executing " << exeFile << " finish, code: " << seInfo.hInstApp << " pid:" << pid;

    return seInfo;
}

int closeProcess(LPSHELLEXECUTEINFO se) {
    int code = -1;

    if (TerminateProcess(se -> hProcess, 0)) {
        code = CloseHandle(se -> hProcess);
    }

    return code;
}

std::string GetExecutablePath() {
    wchar_t path[MAX_PATH];
    std::wstring wideString;
    if (GetModuleFileName(NULL, path, _countof(path))) {
        if (GetLongPathName(path, path, _countof(path))){
            wideString = path;
        }
    }

    return WideToUtf8(wideString);
}

std::string GetExecutablePathQuoted() {
    std::string path = GetExecutablePath();
    return path.insert(0, "\"").append("\"");
}

std::string GetExecutableDirectory() {
    std::string path = GetExecutablePath();
    wchar_t drive[_MAX_DRIVE];
    wchar_t directory[_MAX_DIR];
    wchar_t filename[_MAX_FNAME];
    wchar_t extension[_MAX_EXT];
    errno_t result = _wsplitpath_s(Utf8ToWide(path).c_str(),
            drive, _countof(drive),
            directory, _countof(directory),
            filename, _countof(filename),
            extension, _countof(extension));
    char utf8Drive[_MAX_DRIVE * 2];
    WideToUtf8(drive, utf8Drive, _countof(utf8Drive));
    char utf8Directory[_MAX_DIR * 2];
    WideToUtf8(directory, utf8Directory, _countof(utf8Directory));
    path.assign(utf8Drive).append(utf8Directory);
    if (path.length()) {
        char lastCharacter = path[path.length() - 1];
        if (lastCharacter == '\\' || lastCharacter == '/') {
            path.erase(path.length() - 1);
        }
    }

    return path;
}

std::string GetExecutableFilename() {
    std::string path = GetExecutablePath();
    wchar_t drive[_MAX_DRIVE];
    wchar_t directory[_MAX_DIR];
    wchar_t filename[_MAX_FNAME];
    wchar_t extension[_MAX_EXT];
    errno_t result = _wsplitpath_s(Utf8ToWide(path).c_str(),
            drive, _countof(drive),
            directory, _countof(directory),
            filename, _countof(filename),
            extension, _countof(extension));
    if (result != 0){
        return "";
    }

    char utf8Filename[_MAX_FNAME * 2];
    WideToUtf8(filename, utf8Filename, _countof(utf8Filename));
    char utf8Extension[_MAX_EXT * 2];
    WideToUtf8(extension, utf8Extension, _countof(utf8Extension));
    return path.assign(utf8Filename).append(utf8Extension);
}

std::string GetExecutableName() {
    std::string path = GetExecutablePath();
    wchar_t drive[_MAX_DRIVE];
    wchar_t directory[_MAX_DIR];
    wchar_t filename[_MAX_FNAME];
    wchar_t extension[_MAX_EXT];
    errno_t result = _wsplitpath_s(Utf8ToWide(path).c_str(),
            drive, _countof(drive),
            directory, _countof(directory),
            filename, _countof(filename),
            extension, _countof(extension));
    char utf8Filename[_MAX_FNAME * 2];
    WideToUtf8(filename, utf8Filename, _countof(utf8Filename));
    return path.assign(utf8Filename);
}


std::string GetAnsiTempDirectory() {
    // On Win7 GetTempPath returns path like this:
    // "C:\Users\%USER%\AppData\Local\Temp\"
    // Where %USER% may contain unicode characters and such
    // unicode path won't work with PHP.
    // --
    // On WinXP it returns a short path like this:
    // C:\DOCUME~1\UNICOD~1\LOCALS~1\Temp
    // PHP also doesn't like windows short paths. Sessions will
    // work fine, but other stuff like uploading files won't work.
    // --
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    // Whether temp is a short path.
    std::string tempPathString = WideToUtf8(tempPath);
    bool isShortPath = false;
    // We can't be 100% sure if "~1" is a pattern for short paths,
    // better check only "~".
    if (tempPathString.length() 
            && tempPathString.find("~") != std::string::npos) {
        isShortPath = true;
    }
    if (isShortPath || !AnsiDirectoryExists(WideToUtf8(tempPath))) {
        // This code will also run if the dir returned by
        // GetTempPathW was invalid.
        LOG_DEBUG << "The temp directory returned by OS contains "
                "unicode characters: " << WideToUtf8(tempPath).c_str();
        // Fallback 1: C:\\Windows\\Temp
        // -----------------------------
        TCHAR winDir[MAX_PATH];
        std::string winTemp;
        if (GetWindowsDirectory(winDir, MAX_PATH) == 0) {
            winTemp = "C:\\Windows\\Temp";
        } else {
            winTemp = WideToUtf8(winDir).append("\\Temp");
        }
        if (IsDirectoryWritable(winTemp)) {
            return winTemp;
        }
        // Fallback 2: C:\\Temp
        // --------------------
        if (IsDirectoryWritable("C:\\Temp")) {
            return "C:\\Temp";
        }
        // Fallback 3: ./temp
        // ------------------
        if (AnsiDirectoryExists(GetExecutableDirectory())) {
            // Application directory does not contain unicode characeters.
            std::string localTemp = GetExecutableDirectory().append("\\temp");
            if (DirectoryExists(localTemp) && IsDirectoryWritable(localTemp)) {
                // Local temp directory already exists and is writable.
                return localTemp;
            }
            if (CreateDirectoryA(localTemp.c_str(), NULL) != 0
                    && IsDirectoryWritable(localTemp)) {
                // CreateDirectoryA - If the function succeeds, the return 
                // value is nonzero.
                return localTemp;
            }
        }
        // Fallback 4: C:\\Users\\Public\\Temp
        // -----------------------------------
        std::string publicTemp = "C:\\Users\\Public\\Temp";
        if (DirectoryExists(publicTemp)
                && IsDirectoryWritable(publicTemp)) {
            return publicTemp;
        }
        if (CreateDirectoryA(publicTemp.c_str(), NULL) != 0
                && IsDirectoryWritable(publicTemp)) {
            // CreateDirectoryA - If the function succeeds, the return 
            // value is nonzero.
            return publicTemp;
        }
    }
    return WideToUtf8(tempPath);
}

//inline bool ReadFileContent(const char* pFilePath, const char* pReadMode, OUT std::string& strContent) {
bool ReadFileContent(const char* pFilePath, const char* pReadMode, std::string& strContent) {
	strContent.clear();
	FILE* fp = NULL;
	fopen_s(&fp, pFilePath, pReadMode);
	if (NULL == fp) {
		LOG_ERROR<<"file path: "<<pFilePath<<" is not exists";
		return false;
	}

	fseek(fp, 0, SEEK_END);
	long nSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char* pBuffer = (unsigned char*)malloc(nSize);
	memset(pBuffer, 0, nSize);
	size_t nCount = fread(pBuffer, 1, nSize, fp);
	fclose(fp);
	strContent.assign((char*)pBuffer, nCount);
	free(pBuffer);
	return true;
}

std::string GetFileContents(std::string file) {
    std::ifstream inFile;
    inFile.open(Utf8ToWide(file).c_str(), std::ios::in);
    if (!inFile) {
        return "";
    }

    std::string contents;
    inFile.seekg(0, std::ios::end);
    contents.resize(static_cast<unsigned int>(inFile.tellg()));
    inFile.seekg(0, std::ios::beg);
    inFile.read(&contents[0], contents.size());
    inFile.close();
    return contents;
}

std::string GetRealPath(std::string path) {
    wchar_t realPath[MAX_PATH];
    GetFullPathName(Utf8ToWide(path).c_str(), _countof(realPath), realPath, NULL);
    return WideToUtf8(realPath);
}

std::string GetAbsolutePath(std::string path) {
    if (path.length() && path.find(":") == std::string::npos) {
        path = GetExecutableDirectory() + "\\" + path;
        path = GetRealPath(path);
    }

    return path;
}

bool DirectoryExists(std::string directory) {
    DWORD dwAttrib = GetFileAttributes(Utf8ToWide(directory).c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool AnsiDirectoryExists(std::string directory) {
    // To check whether directory doesn't contain unicode characters.
    DWORD dwAttrib = GetFileAttributesA(directory.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string GetDirectoryPath(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    return (std::string::npos == pos) ? "" : path.substr(0, pos);
}

bool IsDirectoryWritable(std::string directory) {
    int rand = GetRand(0, 1000000);
    std::string tempFile = directory.append("\\phpdesktop").append(IntToString(rand)).append(".tmp");
    HANDLE handle = CreateFile(Utf8ToWide(tempFile).c_str(), 
            GENERIC_WRITE,
            (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
            NULL, OPEN_ALWAYS, 
            (FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE),
            NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    } else {
        CloseHandle(handle);
        return true;
    }    
}

bool CreateFileShortcut(LPCWSTR lpszFileName, LPCWSTR lpszLnkFileDir, LPCWSTR lpszLnkFileName, LPCWSTR lpszWorkDir, WORD wHotkey, LPCTSTR lpszDescription, int iShowCmd) {
	if (lpszLnkFileDir == NULL)
		return FALSE;

	HRESULT        hr;
	IShellLink     *pLink;
	IPersistFile   *ppf;

	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pLink);
	if (FAILED(hr))
		return FALSE;

	hr = pLink->QueryInterface(IID_IPersistFile, (void**)&ppf);
	if (FAILED(hr))
	{
		pLink->Release();
		return FALSE;
	}

	if (lpszFileName == NULL)
		pLink->SetPath(_wpgmptr);
	else
		pLink->SetPath(lpszFileName);

	if (lpszWorkDir != NULL)
		pLink->SetWorkingDirectory(lpszWorkDir);

	if (wHotkey != 0)
		pLink->SetHotkey(wHotkey);

	if (lpszDescription != NULL)
		pLink->SetDescription(lpszDescription);

	pLink->SetShowCmd(iShowCmd);

	wchar_t szBuffer[MAX_PATH];
	if (lpszLnkFileName != NULL)
		wsprintf(szBuffer, L"%s\\%s", lpszLnkFileDir, lpszLnkFileName);

	hr = ppf->Save(szBuffer, TRUE);

	ppf->Release();
	pLink->Release();
	return SUCCEEDED(hr);
}