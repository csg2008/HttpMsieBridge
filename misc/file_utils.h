// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include <cstdio>
#include <string>
#include <windows.h>
#include <shellapi.h>

int closeProcess(LPSHELLEXECUTEINFO se);
SHELLEXECUTEINFO exec(std::string exeFile, int num = 3, bool show = true, bool wait = false);
std::string GetExecutablePath();
std::string GetExecutableName();
std::string GetExecutableFilename();
std::string GetExecutableDirectory();
std::string GetExecutablePathQuoted();
std::string GetAnsiTempDirectory();
std::string GetFileContents(std::string file);
std::string GetRealPath(std::string path);
std::string GetAbsolutePath(std::string path);
std::string GetDirectoryPath(const std::string& path);
bool DirectoryExists(std::string directory);
bool AnsiDirectoryExists(std::string directory);
bool IsDirectoryWritable(std::string directory);
bool ReadFileContent(const char* pFilePath, const char* pReadMode, std::string& strContent);
bool CreateFileShortcut(LPCWSTR lpszFileName, LPCWSTR lpszLnkFileDir, LPCWSTR lpszLnkFileName, LPCWSTR lpszWorkDir, WORD wHotkey, LPCTSTR lpszDescription, int iShowCmd = SW_SHOWNORMAL);