// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include <string>
#include <vector>
#include <stdio.h>
#include <tchar.h>
#include <wchar.h>

int GetRand(int start, int end);
int stricmp(const std::string &l,const std::string &r);
bool strieq(const std::string &l,const std::string &r);
std::pair<std::string,std::string> split(const std::string &s,char delim);
void ltrim(std::string &v);
void rtrim(std::string &v);
void trim(std::string &v);

char* Trim(char *src);
wchar_t* ConvertW(const char* in);
std::string TCHAR2STRING(TCHAR *STR);
wchar_t* char_to_wchar(const char *s);
char* wchar_to_char(const wchar_t *ws);

void Utf8ToWide(const char* charString, wchar_t* wideString, int wideSize);
std::wstring Utf8ToWide(const char* utf8String);
std::wstring Utf8ToWide(const std::string& utf8String);
void WideToUtf8(const wchar_t* wideString, char* utf8String, int utf8Size);
std::string WideToUtf8(const wchar_t* wideString);
std::string WideToUtf8(const std::wstring& wideString);

std::string IntToString(long number);
std::string BoolToString(bool value);

std::string LowerString(std::string str);
std::string UpperString(std::string str);

std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace);
void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace);
std::vector<std::string> Split(const std::string &s, const std::string &seperator);
std::vector<std::wstring> Split(const std::wstring &s, const std::wstring &seperator);
std::wstring string_to_wstring(const std::string& str);
std::string wstring_to_string(const std::wstring& wStr);

size_t replace(std::string& input, const std::string& from, const std::string& to);
std::string trim(const std::string& str, const std::string& whitespace = "\r\n\t ");
std::string to_lower(const std::string& str);
std::string to_upper(const std::string& str);
std::string to_string(const bool& b);
// This function is defined but NOT implemented. The reason for this is that
//    there is no way to guarantee that the to_string function is called with
//    boolean types passed into it. If a user passed in an int, it would be
//    interpreted as a bool and the function would return the correct string.
//    To guarantee the behavior we want, we force a compilation error.
template<typename T> std::string to_string(const T& t);
bool to_bool(const std::string& str);
std::vector<std::string> split(const std::string& split_on_any_delimiters, const std::string& stringToSplit);
std::vector<std::string> explode(const std::string& complete_match, const std::string& stringToExplode);
std::vector<std::string> extract(const std::string& complete_match_start, const std::string& complete_match_end, const std::string& content);
bool ContainsElement(const std::vector<std::string>& v, const std::string& s);
std::string remove_extension(const std::string& s);
std::string get_extension(const std::string& s);