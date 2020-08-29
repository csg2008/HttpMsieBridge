/*
 * (c) Copyright Ascensio System SIA 2010-2019
 *
 * This program is a free software product. You can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License (AGPL)
 * version 3 as published by the Free Software Foundation. In accordance with
 * Section 7(a) of the GNU AGPL its Section 15 shall be amended to the effect
 * that Ascensio System SIA expressly excludes the warranty of non-infringement
 * of any third-party rights.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR  PURPOSE. For
 * details, see the GNU AGPL at: http://www.gnu.org/licenses/agpl-3.0.html
 *
 * You can contact Ascensio System SIA at 20A-12 Ernesta Birznieka-Upisha
 * street, Riga, Latvia, EU, LV-1050.
 *
 * The  interactive user interfaces in modified source and object code versions
 * of the Program must display Appropriate Legal Notices, as required under
 * Section 5 of the GNU AGPL version 3.
 *
 * Pursuant to Section 7(b) of the License you must retain the original Product
 * logo when distributing the program. Pursuant to Section 7(e) we decline to
 * grant you any rights under trademark law for use of our trademarks.
 *
 * All the Product's GUI elements, including illustrations and icon sets, as
 * well as technical writing content are licensed under the terms of the
 * Creative Commons Attribution-ShareAlike 4.0 International. See the License
 * terms at http://creativecommons.org/licenses/by-sa/4.0/legalcode
 *
 */

#ifndef _BUILD_FILE_CROSSPLATFORM_H_
#define _BUILD_FILE_CROSSPLATFORM_H_

#if defined(GetTempPath)
#undef GetTempPath
#endif

#if defined(CreateFile)
#undef CreateFile
#endif

#if defined(CopyFile)
#undef CopyFile
#endif

#if defined(DeleteFile)
#undef DeleteFile
#endif

#include <string>
#include <string.h>
#include <stdio.h>
#include <fstream>
#include <time.h>
#include "errno.h"
#include <stdio.h>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
    #include <wchar.h>
    #include <windows.h>
#endif

#if defined(__linux__) || defined(_MAC) && !defined(_IOS)
#include <unistd.h>
#include <string.h>
#endif

#ifndef MAX_PATH
    #define MAX_PATH 1024
#endif

#if defined(_WIN32) || defined (_WIN64)
    #include "windows.h"
    #include "windef.h"
    #include <shlobj.h>
    #include <Rpc.h>
#elif __linux__
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <dirent.h>
#elif MAC
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <mach-o/dyld.h>
#elif _IOS
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <dirent.h>
    const char* fileSystemRepresentation(const std::wstring& sFileName);
#endif

#ifndef FILE_SEPARATOR
	#if defined(_WIN32) || defined(_WIN64)
		#define FILE_SEPARATOR
		#define FILE_SEPARATOR_CHAR '\\'
        #define FILE_SEPARATOR_STR L"\\"
	#else
		#define FILE_SEPARATOR
		#define FILE_SEPARATOR_CHAR '/'
        #define FILE_SEPARATOR_STR L"/"
	#endif
#endif

#define U_TO_UTF8(val) NSFile::CUtf8Converter::GetUtf8StringFromUnicode2(val.c_str(), (LONG)val.length())
#define UTF8_TO_U(val) NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)val.c_str(), (LONG)val.length())

// реализация возможности подмены определения GetTempPath
std::wstring g_overrideTmpPath = L"";

#if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
	#include <wchar.h>
	#include <windows.h>

	std::wstring CorrectPathW(const std::wstring& path)
	{
		int len = (int)path.length();
		if (2 > len)
			return path;

		const wchar_t* path_str = path.c_str();
		if (path_str[0] == '\\' || path_str[1] == '/')
			return path;

		// local files: '\\?\' prefix
		// server files: '\\?\UNC\' prefix <== TODO!
		int nLen = GetFullPathNameW(path_str, 0, 0, 0);
		wchar_t* pBuf = new wchar_t[(4 + nLen) * sizeof(wchar_t)];

		pBuf[0] = L'\\', pBuf[1] = L'\\',  pBuf[2] = L'?', pBuf[3] = L'\\';
		GetFullPathNameW(path_str, nLen, pBuf + 4, NULL);

		std::wstring retPath(pBuf);
		delete [] pBuf;
		return retPath;
	}
#else
	std::wstring CorrectPathW(const std::wstring& path)
	{
		return path;
	}
#endif


namespace NSFile {
	#define WriteUtf16_WCHAR(code, p)				\
	if (code < 0x10000)								\
		*p++ = code;								\
	else {											\
		code -= 0x10000;							\
		*p++ = 0xD800 | ((code >> 10) & 0x03FF);	\
		*p++ = 0xDC00 | (code & 0x03FF);			\
	}

    #define CHECK_HHHH(pBuffer) \
        wchar_t code = 0; \
        if('_' == pBuffer[0] && 'x' == pBuffer[1] && 0 != pBuffer[2] && 0 != pBuffer[3] && 0 != pBuffer[4] && 0 != pBuffer[5]  && '_' == pBuffer[6]) \
        { \
            int i = 2; \
            for(; i < 6; ++i) \
            { \
                code *= 16; \
                if('0' <= pBuffer[i] && pBuffer[i] <= '9') \
                { \
                    code += pBuffer[i] - '0'; \
                } \
                else if('A' <= pBuffer[i] && pBuffer[i] <= 'F') \
                { \
                    code += pBuffer[i] - 'A' + 10; \
                } \
                else if('a' <= pBuffer[i] && pBuffer[i] <= 'f') \
                { \
                    code += pBuffer[i] - 'a' + 10; \
                } \
                else \
                { \
                    break; \
                } \
            } \
            if(i == 6) \
            { \
                if(0x005F == code) \
                { \
                    code = '_'; \
                } \
                return code; \
            } \
        } \
        return -1;

#if !defined(_WIN32) && !defined (_WIN64)
    static bool is_directory_exist(char* dir)
    {
        struct stat st;
        bool bRes = (0 == stat(dir, &st)) && S_ISDIR(st.st_mode);
        return bRes;
    }

    static bool _mkdir (const char *dir)
    {
        char tmp[MAX_PATH];
        char *p = NULL;
        size_t len;
        bool res = true;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        res = is_directory_exist(tmp);
                        if (!res)
                            res = (0 == mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
                        *p = '/';
                        if (!res)
                            break;
                }
        if (res)
            res = (0 == mkdir(tmp, S_IRWXU));
        return res;
    }
#endif

	class CStringUtf16
	{
	public:
		BYTE* Data;
		int Length;

	public:
        CStringUtf16()
        {
            Data = NULL;
            Length = 0;
        }
        ~CStringUtf16()
        {
            RELEASEARRAYOBJECTS(Data);
        }
	};   

    class CUtf8Converter
	{
	public:

        static std::wstring std::wstring GetUnicodeFromCharPtr(const char* pData, LONG lCount, INT bIsUtf8 = FALSE) {
            if (bIsUtf8)
                return GetUnicodeStringFromUTF8((BYTE*)pData, lCount);

            wchar_t* pUnicode = new wchar_t[lCount + 1];
            for (LONG i = 0; i < lCount; ++i)
                pUnicode[i] = (wchar_t)(BYTE)pData[i];

            pUnicode[lCount] = 0;

            std::wstring s(pUnicode, lCount);
            RELEASEARRAYOBJECTS(pUnicode);

            return s;
        }
        static std::wstring std::wstring GetUnicodeFromCharPtr(const std::string& sParam, INT bIsUtf8 = FALSE) {
            return GetUnicodeFromCharPtr(sParam.c_str(), (LONG)sParam.length(), bIsUtf8);
        }


        static std::wstring GetUnicodeStringFromUTF8_4bytes( BYTE* pBuffer, LONG lCount ){
            std::wstring strRes;
            GetUnicodeStringFromUTF8_4bytes(pBuffer, lCount, strRes);
            return strRes;
        }
        static std::wstring GetUnicodeStringFromUTF8_2bytes( BYTE* pBuffer, LONG lCount ){
            std::wstring strRes;
            GetUnicodeStringFromUTF8_2bytes(pBuffer, lCount, strRes);
            return strRes;
        }
        static std::wstring GetUnicodeStringFromUTF8( BYTE* pBuffer, LONG lCount ){
            std::wstring strRes;
            GetUnicodeStringFromUTF8(pBuffer, lCount, strRes);
            return strRes;
        }

		static void GetUnicodeStringFromUTF8_4bytes( BYTE* pBuffer, LONG lCount, std::wstring& sOutput ){
            WCHAR* pUnicodeString = new WCHAR[lCount + 1];
            LONG lIndexUnicode = 0;

            LONG lIndex = 0;
            while (lIndex < lCount)
            {
                BYTE byteMain = pBuffer[lIndex];
                if (0x00 == (byteMain & 0x80))
                {
                    // 1 byte
                    pUnicodeString[lIndexUnicode++] = (WCHAR)byteMain;
                    ++lIndex;
                }
                else if (0x00 == (byteMain & 0x20))
                {
                    // 2 byte
                    int val = (int)(((byteMain & 0x1F) << 6) |
                        (pBuffer[lIndex + 1] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 2;
                }
                else if (0x00 == (byteMain & 0x10))
                {
                    // 3 byte
                    int val = (int)(((byteMain & 0x0F) << 12) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 6) |
                        (pBuffer[lIndex + 2] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 3;
                }
                else if (0x00 == (byteMain & 0x0F))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x08))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x04))
                {
                    // 5 byte
                    int val = (int)(((byteMain & 0x03) << 24) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 6) |
                        (pBuffer[lIndex + 4] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 5;
                }
                else
                {
                    // 6 byte
                    int val = (int)(((byteMain & 0x01) << 30) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 24) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 4] & 0x3F) << 6) |
                        (pBuffer[lIndex + 5] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 5;
                }
            }

            pUnicodeString[lIndexUnicode] = 0;

            sOutput.append(pUnicodeString);

            delete [] pUnicodeString;
        }

		static void GetUnicodeStringFromUTF8_2bytes( BYTE* pBuffer, LONG lCount, std::wstring& sOutput ){
            WCHAR* pUnicodeString = new WCHAR[lCount + 1];
            WCHAR* pStart = pUnicodeString;

            LONG lIndex = 0;
            while (lIndex < lCount)
            {
                BYTE byteMain = pBuffer[lIndex];
                if (0x00 == (byteMain & 0x80))
                {
                    // 1 byte
                    *pUnicodeString++ = (WCHAR)byteMain;
                    ++lIndex;
                }
                else if (0x00 == (byteMain & 0x20))
                {
                    // 2 byte
                    int val = (int)(((byteMain & 0x1F) << 6) |
                        (pBuffer[lIndex + 1] & 0x3F));
                    *pUnicodeString++ = (WCHAR)(val);
                    lIndex += 2;
                }
                else if (0x00 == (byteMain & 0x10))
                {
                    // 3 byte
                    int val = (int)(((byteMain & 0x0F) << 12) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 6) |
                        (pBuffer[lIndex + 2] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 3;
                }
                else if (0x00 == (byteMain & 0x0F))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x08))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x04))
                {
                    // 5 byte
                    int val = (int)(((byteMain & 0x03) << 24) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 6) |
                        (pBuffer[lIndex + 4] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 5;
                }
                else
                {
                    // 6 byte
                    int val = (int)(((byteMain & 0x01) << 30) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 24) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 4] & 0x3F) << 6) |
                        (pBuffer[lIndex + 5] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 5;
                }
            }

            *pUnicodeString++ = 0;

            sOutput.append(pStart);

            delete [] pStart;
        }

		static void GetUnicodeStringFromUTF8( BYTE* pBuffer, LONG lCount, std::wstring& sOutput ){
            if (sizeof(WCHAR) == 2)
                GetUnicodeStringFromUTF8_2bytes(pBuffer, lCount, sOutput);
            else
                GetUnicodeStringFromUTF8_4bytes(pBuffer, lCount, sOutput);
        }

        static void GetUnicodeStringFromUTF8WithHHHH_4bytes( const BYTE* pBuffer, LONG lCount, wchar_t*& pUnicodes, LONG& lOutputCount ){
            if (NULL == pUnicodes)
            {
                pUnicodes = new wchar_t[GetUnicodeStringFromUTF8BufferSize(lCount)];
            }
            WCHAR* pUnicodeString = pUnicodes;
            LONG lIndexUnicode = 0;

            LONG lIndex = 0;
            while (lIndex < lCount)
            {
                BYTE byteMain = pBuffer[lIndex];
                if (0x00 == (byteMain & 0x80))
                {
                    // 1 byte
                    long code = CheckHHHHChar(pBuffer + lIndex);
                    if(code < 0)
                    {
                        pUnicodeString[lIndexUnicode++] = (WCHAR)byteMain;
                        ++lIndex;
                    }
                    else
                    {
                        pUnicodeString[lIndexUnicode++] = (WCHAR)code;
                        lIndex += 7;
                    }
                }
                else if (0x00 == (byteMain & 0x20))
                {
                    // 2 byte
                    int val = (int)(((byteMain & 0x1F) << 6) |
                        (pBuffer[lIndex + 1] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 2;
                }
                else if (0x00 == (byteMain & 0x10))
                {
                    // 3 byte
                    int val = (int)(((byteMain & 0x0F) << 12) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 6) |
                        (pBuffer[lIndex + 2] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 3;
                }
                else if (0x00 == (byteMain & 0x0F))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x08))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x04))
                {
                    // 5 byte
                    int val = (int)(((byteMain & 0x03) << 24) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 6) |
                        (pBuffer[lIndex + 4] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 5;
                }
                else
                {
                    // 6 byte
                    int val = (int)(((byteMain & 0x01) << 30) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 24) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 4] & 0x3F) << 6) |
                        (pBuffer[lIndex + 5] & 0x3F));
                    pUnicodeString[lIndexUnicode++] = (WCHAR)(val);
                    lIndex += 5;
                }
            }

            pUnicodeString[lIndexUnicode] = 0;
            lOutputCount = lIndexUnicode;
        }
        static void GetUnicodeStringFromUTF8WithHHHH_2bytes( const BYTE* pBuffer, LONG lCount, wchar_t*& pUnicodes, LONG& lOutputCount ){
            if (NULL == pUnicodes)
            {
                pUnicodes = new wchar_t[GetUnicodeStringFromUTF8BufferSize(lCount)];
            }
            WCHAR* pUnicodeString = pUnicodes;
            WCHAR* pStart = pUnicodeString;
            LONG lIndex = 0;
            while (lIndex < lCount)
            {
                BYTE byteMain = pBuffer[lIndex];
                if (0x00 == (byteMain & 0x80))
                {
                    // 1 byte
                    long code = CheckHHHHChar(pBuffer + lIndex);
                    if(code < 0)
                    {
                        *pUnicodeString++ = (WCHAR)byteMain;
                        ++lIndex;
                    }
                    else
                    {
                        *pUnicodeString++ = (WCHAR)code;
                        lIndex += 7;
                    }

                }
                else if (0x00 == (byteMain & 0x20))
                {
                    // 2 byte
                    int val = (int)(((byteMain & 0x1F) << 6) |
                        (pBuffer[lIndex + 1] & 0x3F));
                    *pUnicodeString++ = (WCHAR)(val);
                    lIndex += 2;
                }
                else if (0x00 == (byteMain & 0x10))
                {
                    // 3 byte
                    int val = (int)(((byteMain & 0x0F) << 12) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 6) |
                        (pBuffer[lIndex + 2] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 3;
                }
                else if (0x00 == (byteMain & 0x0F))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x08))
                {
                    // 4 byte
                    int val = (int)(((byteMain & 0x07) << 18) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 6) |
                        (pBuffer[lIndex + 3] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 4;
                }
                else if (0x00 == (byteMain & 0x04))
                {
                    // 5 byte
                    int val = (int)(((byteMain & 0x03) << 24) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 6) |
                        (pBuffer[lIndex + 4] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 5;
                }
                else
                {
                    // 6 byte
                    int val = (int)(((byteMain & 0x01) << 30) |
                        ((pBuffer[lIndex + 1] & 0x3F) << 24) |
                        ((pBuffer[lIndex + 2] & 0x3F) << 18) |
                        ((pBuffer[lIndex + 3] & 0x3F) << 12) |
                        ((pBuffer[lIndex + 4] & 0x3F) << 6) |
                        (pBuffer[lIndex + 5] & 0x3F));

                    WriteUtf16_WCHAR(val, pUnicodeString);
                    lIndex += 5;
                }
            }

            lOutputCount = pUnicodeString - pStart;
            *pUnicodeString++ = 0;
        }
        static void GetUnicodeStringFromUTF8WithHHHH( const BYTE* pBuffer, LONG lCount, wchar_t*& pUnicodes, LONG& lOutputCount ){
            if (sizeof(WCHAR) == 2)
                return GetUnicodeStringFromUTF8WithHHHH_2bytes(pBuffer, lCount, pUnicodes, lOutputCount);
            return GetUnicodeStringFromUTF8WithHHHH_4bytes(pBuffer, lCount, pUnicodes, lOutputCount);
        }

        static LONG GetUnicodeStringFromUTF8BufferSize(LONG lCount)
        {
            return lCount + 1;
        };

        static void GetUtf8StringFromUnicode_4bytes(const wchar_t* pUnicodes, LONG lCount, BYTE*& pData, LONG& lOutputCount, bool bIsBOM = false){
            if (NULL == pData)
            {
                pData = new BYTE[6 * lCount + 3 + 1 ];
            }

            BYTE* pCodesCur = pData;
            if (bIsBOM)
            {
                pCodesCur[0] = 0xEF;
                pCodesCur[1] = 0xBB;
                pCodesCur[2] = 0xBF;
                pCodesCur += 3;
            }

            const wchar_t* pEnd = pUnicodes + lCount;
            const wchar_t* pCur = pUnicodes;

            while (pCur < pEnd)
            {
                unsigned int code = (unsigned int)*pCur++;

                if (code < 0x80)
                {
                    *pCodesCur++ = (BYTE)code;
                }
                else if (code < 0x0800)
                {
                    *pCodesCur++ = 0xC0 | (code >> 6);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x10000)
                {
                    *pCodesCur++ = 0xE0 | (code >> 12);
                    *pCodesCur++ = 0x80 | (code >> 6 & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x1FFFFF)
                {
                    *pCodesCur++ = 0xF0 | (code >> 18);
                    *pCodesCur++ = 0x80 | (code >> 12 & 0x3F);
                    *pCodesCur++ = 0x80 | (code >> 6 & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x3FFFFFF)
                {
                    *pCodesCur++ = 0xF8 | (code >> 24);
                    *pCodesCur++ = 0x80 | (code >> 18 & 0x3F);
                    *pCodesCur++ = 0x80 | (code >> 12 & 0x3F);
                    *pCodesCur++ = 0x80 | (code >> 6 & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x7FFFFFFF)
                {
                    *pCodesCur++ = 0xFC | (code >> 30);
                    *pCodesCur++ = 0x80 | (code >> 24 & 0x3F);
                    *pCodesCur++ = 0x80 | (code >> 18 & 0x3F);
                    *pCodesCur++ = 0x80 | (code >> 12 & 0x3F);
                    *pCodesCur++ = 0x80 | (code >> 6 & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
            }

            lOutputCount = (LONG)(pCodesCur - pData);
            *pCodesCur++ = 0;
        }
        static void GetUtf8StringFromUnicode_2bytes(const wchar_t* pUnicodes, LONG lCount, BYTE*& pData, LONG& lOutputCount, bool bIsBOM = false){
            if (NULL == pData)
            {
                pData = new BYTE[6 * lCount + 3 + 1];
            }

            BYTE* pCodesCur = pData;
            if (bIsBOM)
            {
                pCodesCur[0] = 0xEF;
                pCodesCur[1] = 0xBB;
                pCodesCur[2] = 0xBF;
                pCodesCur += 3;
            }

            const wchar_t* pEnd = pUnicodes + lCount;
            const wchar_t* pCur = pUnicodes;

            while (pCur < pEnd)
            {
                unsigned int code = (unsigned int)*pCur++;
                if (code >= 0xD800 && code <= 0xDFFF && pCur < pEnd)
                {
                    code = 0x10000 + (((code & 0x3FF) << 10) | (0x03FF & *pCur++));
                }

                if (code < 0x80)
                {
                    *pCodesCur++ = (BYTE)code;
                }
                else if (code < 0x0800)
                {
                    *pCodesCur++ = 0xC0 | (code >> 6);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x10000)
                {
                    *pCodesCur++ = 0xE0 | (code >> 12);
                    *pCodesCur++ = 0x80 | ((code >> 6) & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x1FFFFF)
                {
                    *pCodesCur++ = 0xF0 | (code >> 18);
                    *pCodesCur++ = 0x80 | ((code >> 12) & 0x3F);
                    *pCodesCur++ = 0x80 | ((code >> 6) & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x3FFFFFF)
                {
                    *pCodesCur++ = 0xF8 | (code >> 24);
                    *pCodesCur++ = 0x80 | ((code >> 18) & 0x3F);
                    *pCodesCur++ = 0x80 | ((code >> 12) & 0x3F);
                    *pCodesCur++ = 0x80 | ((code >> 6) & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
                else if (code < 0x7FFFFFFF)
                {
                    *pCodesCur++ = 0xFC | (code >> 30);
                    *pCodesCur++ = 0x80 | ((code >> 24) & 0x3F);
                    *pCodesCur++ = 0x80 | ((code >> 18) & 0x3F);
                    *pCodesCur++ = 0x80 | ((code >> 12) & 0x3F);
                    *pCodesCur++ = 0x80 | ((code >> 6) & 0x3F);
                    *pCodesCur++ = 0x80 | (code & 0x3F);
                }
            }

            lOutputCount = (LONG)(pCodesCur - pData);
            *pCodesCur++ = 0;
        }
        static void GetUtf8StringFromUnicode(const wchar_t* pUnicodes, LONG lCount, BYTE*& pData, LONG& lOutputCount, bool bIsBOM = false){
            if (sizeof(WCHAR) == 2)
                return GetUtf8StringFromUnicode_2bytes(pUnicodes, lCount, pData, lOutputCount, bIsBOM);
            return GetUtf8StringFromUnicode_4bytes(pUnicodes, lCount, pData, lOutputCount, bIsBOM);
        }

        static std::string GetUtf8StringFromUnicode2(const wchar_t* pUnicodes, LONG lCount, bool bIsBOM = false){
            BYTE* pData = NULL;
            LONG lLen = 0;

            GetUtf8StringFromUnicode(pUnicodes, lCount, pData, lLen, bIsBOM);

            std::string s((char*)pData, lLen);

            RELEASEARRAYOBJECTS(pData);
            return s;
        }
        static std::string GetUtf8StringFromUnicode(const std::wstring& sData){
            return GetUtf8StringFromUnicode2(sData.c_str(), (LONG)sData.length());
        }

		// utf16
        static void GetUtf16StringFromUnicode_4bytes(const wchar_t* pUnicodes, LONG lCount, BYTE*& pData, int& lOutputCount, bool bIsBOM = false){
            if (NULL == pData)
            {
                pData = new BYTE[4 * lCount + 3 + 2];
            }

            BYTE* pCodesCur = pData;
            if (bIsBOM)
            {
                pCodesCur[0] = 0xEF;
                pCodesCur[1] = 0xBB;
                pCodesCur[2] = 0xBF;
                pCodesCur += 3;
            }

            const wchar_t* pEnd = pUnicodes + lCount;
            const wchar_t* pCur = pUnicodes;

            while (pCur < pEnd)
            {
                unsigned int code = (unsigned int)*pCur++;

                if (code <= 0xFFFF)
                {
                    USHORT usCode = (USHORT)(code & 0xFFFF);
                    memcpy(pCodesCur, &usCode, 2);
                    pCodesCur += 2;
                }
                else
                {
                    code -= 0x10000;
                    code &= 0xFFFFF;

                    USHORT us1 = 0xD800 | ((code >> 10) & 0x03FF);
                    USHORT us2 = 0xDC00 | (code & 0x03FF);

                    memcpy(pCodesCur, &us1, 2);
                    pCodesCur += 2;

                    memcpy(pCodesCur, &us2, 2);
                    pCodesCur += 2;
                }
            }

            lOutputCount = (LONG)(pCodesCur - pData);
            *pCodesCur++ = 0;
            *pCodesCur++ = 0;
        }

        static void GetUtf16StringFromUnicode_4bytes2(const wchar_t* pUnicodes, LONG lCount, CStringUtf16& data){
            GetUtf16StringFromUnicode_4bytes(pUnicodes, lCount, data.Data, data.Length);
        }
	
        static std::wstring GetWStringFromUTF16(const CStringUtf16& data){
            if (0 == data.Length)
                return L"";

            if (sizeof(wchar_t) == 2)
                return std::wstring((wchar_t*)data.Data, data.Length / 2);

            int nCount = data.Length / 2;
            USHORT* pShort = (USHORT*)data.Data;

            wchar_t* pWChar = new wchar_t[nCount + 1];
            wchar_t* pWCurrent = pWChar;

            int nCurrent = 0;
            while (nCurrent < nCount)
            {
                if (*pShort < 0xD800 || *pShort > 0xDBFF)
                {
                    *pWCurrent = (wchar_t)(*pShort);
                    ++pShort;
                    ++nCurrent;
                }
                else
                {
                    *pWCurrent = (wchar_t)(((((pShort[0] - 0xD800) & 0x03FF) << 10) | ((pShort[1] - 0xDC00) & 0x03FF)) + 0x10000);
                    pShort += 2;
                    nCurrent += 2;
                }
                ++pWCurrent;
            }

            std::wstring sRet(pWChar, pWCurrent - pWChar);

            RELEASEARRAYOBJECTS(pWChar);
            return sRet;
        }

        static std::wstring GetWStringFromUTF16(const unsigned short* pUtf16, LONG lCount){
            CStringUtf16 oString;
            oString.Data   = (BYTE*)pUtf16;
            oString.Length = lCount * 2;
            std::wstring wsResult = GetWStringFromUTF16(oString);
            oString.Data = NULL;
            return wsResult;
        }

        static long CheckHHHHChar(const BYTE* pBuffer){
            CHECK_HHHH(pBuffer);
        }
        static long CheckHHHHChar(const wchar_t* pBuffer){
            CHECK_HHHH(pBuffer);
        }
	};

    class CFileBinary
	{
	protected:
		FILE* m_pFile;

		long m_lFilePosition;
		long m_lFileSize;

	public:
        CFileBinary()
        {
            m_pFile = NULL;
            m_lFilePosition = 0;
            m_lFileSize = 0;
        }
        virtual ~CFileBinary()
        {
            CloseFile();
        }

        virtual void CloseFile()
        {
            m_lFilePosition = 0;
            m_lFileSize = 0;

            if (m_pFile != NULL)
            {
                fclose(m_pFile);
                m_pFile = NULL;
            }
        }

        FILE* GetFileNative()
        {
            return m_pFile;
        }
        long GetFileSize()
        {
            return m_lFileSize;
        }
        long GetFilePosition()
        {
            return m_lFilePosition;
        }

        bool OpenFile(const std::wstring& sFileName, bool bRewrite)
        {
    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            if ( 0 != _wfopen_s(&m_pFile, sFileName.c_str(), bRewrite ? L"rb+" : L"rb"))
                return false;
    #else
            BYTE* pUtf8 = NULL;
            LONG lLen = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(sFileName.c_str(), sFileName.length(), pUtf8, lLen, false);
            m_pFile = fopen((char*)pUtf8, bRewrite ? "rb+" : "rb");

            delete [] pUtf8;
    #endif
            if (NULL == m_pFile)
                return false;

            fseek(m_pFile, 0, SEEK_END);
            m_lFileSize = ftell(m_pFile);
            fseek(m_pFile, 0, SEEK_SET);

            m_lFilePosition = 0;

            if (0 < sFileName.length())
            {
                if (((wchar_t)'/') == sFileName.c_str()[sFileName.length() - 1])
                    m_lFileSize = 0x7FFFFFFF;
            }

            unsigned int err = 0x7FFFFFFF;
            unsigned int cur = (unsigned int)m_lFileSize;
            if (err == cur)
            {
                CloseFile();
                return false;
            }

            return true;
        }
        bool CreateFileW(const std::wstring& sFileName)
        {
    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            if ( 0 != _wfopen_s(&m_pFile, sFileName.c_str(), L"wb"))
                return false;
    #else
            BYTE* pUtf8 = NULL;
            LONG lLen = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(sFileName.c_str(), sFileName.length(), pUtf8, lLen, false);
            m_pFile = fopen((char*)pUtf8, "wb");
            delete [] pUtf8;
    #endif
            if (NULL == m_pFile)
                return false;

            m_lFilePosition = 0;
            return true;
        }
        
        bool CreateTempFile()
        {
    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            if (0 != tmpfile_s(&m_pFile))
                return false;
    #else
            m_pFile = tmpfile();
            if (NULL == m_pFile)
                return false;
    #endif
            m_lFilePosition = 0;
            return true;
        }
        bool SeekFile(int lFilePosition, int nSeekMode = 0);
        {
            if (!m_pFile)
                return false;

            m_lFilePosition = fseek(m_pFile, lFilePosition, nSeekMode);
            return true;
        }
        bool ReadFile(BYTE* pData, DWORD nBytesToRead, DWORD& dwSizeRead)
        {
            if (!m_pFile)
                return false;

            dwSizeRead = (DWORD)fread((void*)pData, 1, nBytesToRead, m_pFile);
            return true;
        }
        bool WriteFile(const BYTE* pData, DWORD nBytesCount)
        {
            if (!m_pFile)
                return false;

            size_t nCountWrite = fwrite((const void*)pData, 1, nBytesCount, m_pFile);
            return true;
        }

        long TellFile()
        {
            if (!m_pFile)
                return 0;

            return ftell(m_pFile);
        }
        long SizeFile()
        {
            if (!m_pFile)
                return 0;

            long lPos = TellFile();
            fseek(m_pFile, 0, SEEK_END);
            m_lFileSize = ftell(m_pFile);
            fseek(m_pFile, lPos, SEEK_SET);

            return m_lFileSize;
        }
        void WriteStringUTF8(const std::wstring& strXml, bool bIsBOM = false)
        {
            BYTE* pData = NULL;
            LONG lLen = 0;

            CUtf8Converter::GetUtf8StringFromUnicode(strXml.c_str(), (LONG)strXml.length(), pData, lLen, bIsBOM);

            WriteFile(pData, lLen);

            RELEASEARRAYOBJECTS(pData);
        }
        static bool ReadAllBytes(const std::wstring&  strFileName, BYTE** ppData, DWORD& nBytesCount)
        {
            *ppData = NULL;
            nBytesCount = 0;
            bool bRes = false;
            CFileBinary oFileBinary;
            if (oFileBinary.OpenFile(strFileName))
            {
                long nFileSize = oFileBinary.GetFileSize();
                BYTE* pData = new BYTE[nFileSize];
                DWORD dwSizeRead;
                if (oFileBinary.ReadFile(pData, nFileSize, dwSizeRead))
                {
                    oFileBinary.CloseFile();
                    *ppData = pData;
                    nBytesCount = dwSizeRead;
                    bRes = true;
                }
                else
                    RELEASEARRAYOBJECTS(pData);
            }
            return bRes;
        }
        static bool ReadAllTextUtf8(const std::wstring&  strFileName, std::wstring& sData)
        {
            bool bRes = false;
            BYTE* pData = NULL;
            DWORD nDataSize;
            if (CFileBinary::ReadAllBytes(strFileName, &pData, nDataSize))
            {
                //remove BOM if exist
                BYTE* pDataStart = pData;
                DWORD nBOMSize = 3;
                if (nDataSize > nBOMSize && 0xef == pDataStart[0] && 0xbb == pDataStart[1] && 0xbf == pDataStart[2])
                {
                    pDataStart += nBOMSize;
                    nDataSize -= nBOMSize;
                }
                sData = CUtf8Converter::GetUnicodeStringFromUTF8(pDataStart, nDataSize);
                RELEASEARRAYOBJECTS(pData);
                bRes = true;
            }
            return bRes;
        }
        static bool ReadAllTextUtf8A(const std::wstring&  strFileName, std::string& sData)
        {
            bool bRes = false;
            BYTE* pData = NULL;
            DWORD nDataSize;
            if (CFileBinary::ReadAllBytes(strFileName, &pData, nDataSize))
            {
                //remove BOM if exist
                BYTE* pDataStart = pData;
                DWORD nBOMSize = 3;
                if (nDataSize > nBOMSize && 0xef == pDataStart[0] && 0xbb == pDataStart[1] && 0xbf == pDataStart[2])
                {
                    pDataStart += nBOMSize;
                    nDataSize -= nBOMSize;
                }
                sData = std::string((char*)pDataStart, nDataSize);
                RELEASEARRAYOBJECTS(pData);
                bRes = true;
            }
            return bRes;
        }
        static bool SaveToFile(const std::wstring&  strFileName, const std::wstring& strXml, bool bIsBOM = false)
        {
            CFileBinary oFile;
            oFile.CreateFileW(strFileName);
            oFile.WriteStringUTF8(strXml, bIsBOM);
            oFile.CloseFile();
            return true;
        }
        static bool Exists(const std::wstring&  strFileName)
        {
    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            FILE* pFile = NULL;
            if ( 0 != _wfopen_s( &pFile, strFileName.c_str(), L"rb"))
                return false;
    #else
            BYTE* pUtf8 = NULL;
            LONG lLen = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(strFileName.c_str(), strFileName.length(), pUtf8, lLen, false);
            FILE* pFile = fopen((char*)pUtf8, "rb");
            delete [] pUtf8;
    #endif
            if (NULL != pFile)
            {
                fclose(pFile);
                return true;
            }
            else
                return false;
        }
        static bool Copy(const std::wstring&  strSrc, const std::wstring&  strDst)
        {
            if (strSrc == strDst)
                return true;

            std::ifstream src;
            std::ofstream dst;

            int nLenBuffer = 1024 * 1024; // 10
            CFileBinary oFile;
            if (oFile.OpenFile(strSrc))
            {
                int nFileSize = (int)oFile.GetFileSize();
                if (nFileSize < nLenBuffer)
                    nLenBuffer = nFileSize;

                oFile.CloseFile();
            }

            char* pBuffer_in = NULL;
            char* pBuffer_out = NULL;

            if (nLenBuffer > 0)
            {
                pBuffer_in = new char[nLenBuffer];
                pBuffer_out = new char[nLenBuffer];

                src.rdbuf()->pubsetbuf(pBuffer_in, nLenBuffer);
                dst.rdbuf()->pubsetbuf(pBuffer_out, nLenBuffer);
            }

    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            src.open(strSrc.c_str(), std::ios::binary);
            dst.open(strDst.c_str(), std::ios::binary);
    #else
            BYTE* pUtf8Src = NULL;
            LONG lLenSrc = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(strSrc.c_str(), strSrc.length(), pUtf8Src, lLenSrc, false);
            BYTE* pUtf8Dst = NULL;
            LONG lLenDst = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(strDst.c_str(), strDst.length(), pUtf8Dst, lLenDst, false);

            src.open((char*)pUtf8Src, std::ios::binary);
            dst.open((char*)pUtf8Dst, std::ios::binary);

            delete [] pUtf8Src;
            delete [] pUtf8Dst;
    #endif

            bool bRet = false;

            if (src.is_open() && dst.is_open())
            {
                dst << src.rdbuf();
                src.close();
                dst.close();

                bRet = true;
            }
            RELEASEARRAYOBJECTS(pBuffer_in);
            RELEASEARRAYOBJECTS(pBuffer_out);
            return bRet;
        }
        static bool Remove(const std::wstring& strFileName)
        {
    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            int nRes = _wremove(strFileName.c_str());
    #else
            BYTE* pUtf8 = NULL;
            LONG lLen = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(strFileName.c_str(), strFileName.length(), pUtf8, lLen, false);
            int nRes = std::remove((char*)pUtf8);
            delete [] pUtf8;
    #endif
            return 0 == nRes;
        }
        static bool Move(const std::wstring&  strSrc, const std::wstring&  strDst)
        {
            if (strSrc == strDst)
                return true;
            if (Copy(strSrc, strDst))
                if (Remove(strSrc))
                    return true;
            return false;
        }
        static bool Truncate(const std::wstring& sPath, size_t nNewSize)
        {
            bool bIsSuccess = false;

    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            HANDLE hFile = ::CreateFileW( sPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                            NULL, OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL, NULL );
            if ( hFile == INVALID_HANDLE_VALUE )
            {
                return bIsSuccess;
            }

            LARGE_INTEGER Size = { 0 };

            if ( GetFileSizeEx( hFile, &Size ) )
            {
                LARGE_INTEGER Distance = { 0 };
                // Negative values move the pointer backward in the file
                Distance.QuadPart = (LONGLONG)nNewSize - Size.QuadPart;
                bIsSuccess = (SetFilePointerEx(hFile, Distance, NULL, FILE_END) && SetEndOfFile(hFile));
            }

            CloseHandle( hFile );
    #else
            std::string sFileUTF8 = U_TO_UTF8(sPath);
            bIsSuccess = (0 == truncate(sFileUTF8.c_str(), nNewSize));
    #endif
            return bIsSuccess;
        }
        static void SetTempPath(const std::wstring& strTempPath)
        {
            g_overrideTmpPath = strTempPath;
        }

        static std::wstring GetTempPath()
        {
            if (!g_overrideTmpPath.empty())
                return g_overrideTmpPath;

    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            wchar_t pBuffer[MAX_PATH + 1];
            memset(pBuffer, 0, sizeof(wchar_t) * (MAX_PATH + 1));
            ::GetTempPathW(MAX_PATH, pBuffer);

            std::wstring sRet(pBuffer);

            size_t nSeparatorPos = sRet.find_last_of(wchar_t('/'));
            if (std::wstring::npos == nSeparatorPos)
            {
                nSeparatorPos = sRet.find_last_of(wchar_t('\\'));
            }

            if (std::wstring::npos == nSeparatorPos)
                return L"";

            return sRet.substr(0, nSeparatorPos);
    #else
            char *folder = getenv("TEMP");

            if (NULL == folder)
                folder = getenv("TMP");
            if (NULL == folder)
                folder = getenv("TMPDIR");
            if (NULL == folder)
                folder = "/tmp";

            return NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)folder, strlen(folder));
    #endif
        }
        static std::wstring CreateTempFileWithUniqueName(const std::wstring& strFolderPathRoot, const std::wstring& Prefix)
        {
        #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
                wchar_t pBuffer[MAX_PATH + 1];
                ::GetTempFileNameW(strFolderPathRoot.c_str(), Prefix.c_str(), 0, pBuffer);
                std::wstring sRet(pBuffer);
                return sRet;
        #else
                char pcRes[MAX_PATH];
                BYTE* pData = (BYTE*)pcRes;

                std::wstring sPrefix = strFolderPathRoot + L"/" + Prefix + L"_XXXXXX";
                LONG lLen = 0;
                NSFile::CUtf8Converter::GetUtf8StringFromUnicode(sPrefix.c_str(), (LONG)sPrefix.length(), pData, lLen);
                pcRes[lLen] = '\0';

                int res = mkstemp(pcRes);
                if (-1 != res)
                    close(res);

                std::string sRes = pcRes;
                return NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)sRes.c_str(), sRes.length());
        #endif
            }
        static bool OpenTempFile(std::wstring *pwsName, FILE **ppFile, wchar_t *wsMode, wchar_t *wsExt, wchar_t *wsFolder, wchar_t* wsName = NULL)
        {
            // TODO: Реализовать когда wsName != NULL

            std::wstring wsTemp, wsFileName;
            FILE *pTempFile = NULL;
    #if defined(_WIN32) || defined (_WIN64)
            wchar_t *wsTempDir = NULL;
            size_t sz = 0;
            if ( (0 == _wdupenv_s(&wsTempDir, &sz, L"TEMP")) && (wsFolder == NULL))
            {
                wsTemp = std::wstring(wsTempDir, sz-1);
    #else
            char *wsTempDirA;
            if ((wsTempDirA = getenv("TEMP")) && (wsFolder == NULL))
            {
                std::wstring wsTempDir = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)wsTempDirA, strlen(wsTempDirA));
                wsTemp = wsTempDir.c_str();
    #endif
                wsTemp += L"/";
            }
            else if (wsFolder != NULL)
            {
                wsTemp = std::wstring(wsFolder);
                wsTemp += L"/";
            }
            else
            {
                wsTemp = L"";
            }
            wsTemp += L"x";
            int nTime = (int)time(NULL);
            for (int nIndex = 0; nIndex < 1000; ++nIndex)
            {
                wsFileName = wsTemp;
                wsFileName.append(std::to_wstring(nTime + nIndex));

                if (wsExt)
                {
                    wsFileName.append(wsExt);
                }
    #if defined (_WIN32) || defined (_WIN64)
                if ( 0 != _wfopen_s(&pTempFile, wsFileName.c_str(), L"r") )
                {
                    if (0 != _wfopen_s(&pTempFile, wsFileName.c_str(), wsMode))
    #else
                std::string sFileName = U_TO_UTF8(wsFileName);
                if (!(pTempFile = fopen(sFileName.c_str(), "r")))
                {
                    std::wstring strMode(wsMode);
                    std::string sMode = U_TO_UTF8(strMode);
                    if (!(pTempFile = fopen(sFileName.c_str(), sMode.c_str())))
    #endif
                    {
                        return FALSE;
                    }
                    *pwsName = wsFileName;
                    *ppFile = pTempFile;
                    return TRUE;
                }

                fclose(pTempFile);
            }

            return FALSE;
        }
        static FILE* OpenFileNative(const std::wstring& sFileName, const std::wstring& sMode)
        {
    #if defined(_WIN32) || defined(_WIN32_WCE) || defined(_WIN64)
            FILE* pFile = NULL;
            _wfopen_s(&pFile, sFileName.c_str(), sMode.c_str());

            return pFile;
    #else
            BYTE* pUtf8 = NULL;
            LONG lLen = 0;
            CUtf8Converter::GetUtf8StringFromUnicode(sFileName.c_str(), sFileName.length(), pUtf8, lLen, false);

            BYTE* pMode = NULL;
            LONG lLenMode;
            CUtf8Converter::GetUtf8StringFromUnicode(sMode.c_str(), sMode.length(), pMode, lLenMode, false);

            FILE* pFile = fopen((char*)pUtf8, (char*)pMode);

            delete [] pUtf8;
            delete [] pMode;

            return pFile;
    #endif
        }
	};

    #define NS_FILE_MAX_PATH 32768
    std::wstring GetProcessPath()
    {
    #if defined (_WIN64) || defined(_WIN32)
            wchar_t buf [NS_FILE_MAX_PATH];
            GetModuleFileNameW(GetModuleHandle(NULL), buf, NS_FILE_MAX_PATH);
            return std::wstring(buf);
    #endif

    #if defined(__linux__) || defined(_MAC) && !defined(_IOS)
            char buf[NS_FILE_MAX_PATH];
            memset(buf, 0, NS_FILE_MAX_PATH);
            if (readlink ("/proc/self/exe", buf, NS_FILE_MAX_PATH) <= 0)
            {
    #ifdef _MAC
                uint32_t _size = NS_FILE_MAX_PATH;
                _NSGetExecutablePath(buf, &_size);
                std::string sUTF8(buf);
                std::wstring sRet = CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)sUTF8.c_str(), sUTF8.length());
                return sRet;
    #endif
                return L"";
            }

            std::string sUTF8(buf);
            std::wstring sRet = CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)sUTF8.c_str(), sUTF8.length());
            return sRet;
    #endif

            return L"";
    }
    std::wstring GetProcessDirectory()
    {
        std::wstring sPath = GetProcessPath();

        size_t pos1 = sPath.find_last_of(wchar_t('/'));
        size_t pos2 = sPath.find_last_of(wchar_t('\\'));

        size_t pos = std::wstring::npos;
        if (pos1 != std::wstring::npos)
            pos = pos1;

        if (pos2 != std::wstring::npos)
        {
            if (pos == std::wstring::npos)
                pos = pos2;
            else if (pos2 > pos)
                pos = pos2;
        }

        if (pos != std::wstring::npos)
        {
            sPath = sPath.substr(0, pos);
        }
        return sPath;
    }

    // CommonFunctions
    std::wstring GetFileExtention(const std::wstring& sPath)
    {
        std::wstring::size_type nPos = sPath.rfind('.');
        if (nPos != std::wstring::npos)
            return sPath.substr(nPos + 1);
        return sPath;
    }
    std::wstring GetFileName(const std::wstring& sPath)
    {
        std::wstring::size_type nPos1 = sPath.rfind('\\');
        std::wstring::size_type nPos2 = sPath.rfind('/');
        std::wstring::size_type nPos = std::wstring::npos;

        if (nPos1 != std::wstring::npos)
        {
            nPos = nPos1;
            if (nPos2 != std::wstring::npos && nPos2 > nPos)
                nPos = nPos2;
        }
        else
            nPos = nPos2;

        if (nPos == std::wstring::npos)
            return sPath;
        return sPath.substr(nPos + 1);
    }
    std::wstring GetDirectoryName(const std::wstring& sPath)
    {
        std::wstring::size_type nPos1 = sPath.rfind('\\');
        std::wstring::size_type nPos2 = sPath.rfind('/');
        std::wstring::size_type nPos = std::wstring::npos;

        if (nPos1 != std::wstring::npos)
        {
            nPos = nPos1;
            if (nPos2 != std::wstring::npos && nPos2 > nPos)
                nPos = nPos2;
        }
        else
            nPos = nPos2;

        if (nPos == std::wstring::npos)
            return sPath;
        return sPath.substr(0, nPos);
    }

	std::wstring GetDirectoryName(const std::wstring& strFileName)
	{
		std::wstring sRes;
		//_wsplitpath return directory path, including trailing slash.
		//dirname() returns the string up to, but not including, the final '/',
#if defined(_WIN32) || defined (_WIN64)
		wchar_t tDrive[256];
		wchar_t tFolder[256];
		_wsplitpath( strFileName.c_str(), tDrive, tFolder, NULL, NULL );
		sRes.append(tDrive);
		sRes.append(tFolder);
		if(sRes.length() > 0)
			sRes.erase(sRes.length()-1);
#elif __linux__ || MAC
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
		NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strFileName.c_str(), strFileName.length(), pUtf8, lLen, false);
		char* pDirName = dirname((char*)pUtf8);
		sRes = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)pDirName, strlen(pDirName));
		delete [] pUtf8;
#endif
        return sRes;
	}
	std::wstring GetFileName(const std::wstring& strFileName)
	{
		std::wstring sRes;
#if defined(_WIN32) || defined (_WIN64)
		wchar_t tFilename[256];
		wchar_t tExt[256];
		_wsplitpath( strFileName.c_str(), NULL, NULL, tFilename, tExt );
		sRes.append(tFilename);
		sRes.append(tExt);
		return sRes;
#elif __linux__ || MAC
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
		NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strFileName.c_str(), strFileName.length(), pUtf8, lLen, false);
		char* pBaseName = basename((char*)pUtf8);
		sRes = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)pBaseName, strlen(pBaseName));
		delete [] pUtf8;
#endif
		return sRes;
	}
	std::wstring Combine(const std::wstring& strLeft, const std::wstring& strRight)
	{
		std::wstring sRes;
		bool bLeftSlash = false;
		bool bRightSlash = false;
		if(strLeft.length() > 0)
		{
			wchar_t cLeft = strLeft[strLeft.length() - 1];
			bLeftSlash = ('/' == cLeft) || ('\\' == cLeft);
		}
		if(strRight.length() > 0)
		{
			wchar_t cRight = strRight[0];
			bRightSlash = ('/' == cRight) || ('\\' == cRight);
		}
		if(bLeftSlash && bRightSlash)
		{
			sRes = strLeft + strRight.substr(1);
		}
		else if(!bLeftSlash && !bRightSlash)
            sRes = strLeft + L"/" + strRight;
		else
			sRes = strLeft + strRight;
		return sRes;
	}

    void GetFiles2(std::wstring strDirectory, std::vector<std::wstring>& oArray, bool bIsRecursion = false)
    {
#if defined(_WIN32) || defined (_WIN64)
        WIN32_FIND_DATAW oFD;

        std::wstring sSpec = strDirectory + L"\\*.*";
        HANDLE hRes = FindFirstFileW( sSpec.c_str(), &oFD );
        if( INVALID_HANDLE_VALUE == hRes )
            return;
        do
        {
            sSpec = oFD.cFileName;
            if (sSpec != L"." && sSpec != L"..")
            {
                sSpec = strDirectory + L"\\" + sSpec;
                if( !( oFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
                {
                    oArray.push_back(sSpec);
                }
                else if (bIsRecursion)
                {
                    GetFiles2(sSpec, oArray, bIsRecursion);
                }
            }
        } while( FindNextFileW( hRes, &oFD ) );
        FindClose( hRes );
#endif

#ifdef __linux__
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
        DIR *dp;
        struct dirent *dirp;
        if((dp  = opendir((char*)pUtf8)) != NULL)
        {
            while ((dirp = readdir(dp)) != NULL)
            {
                int nType = 0;
                if(DT_REG == dirp->d_type)
                    nType = 2;
                else if (DT_DIR == dirp->d_type)
                    nType = 1;
                else if (DT_UNKNOWN == dirp->d_type)
                {
                     // XFS problem
                     struct stat buff;
                     std::string sTmp = std::string((char*)pUtf8) + "/" + std::string(dirp->d_name);
                     stat(sTmp.c_str(), &buff);
                     if (S_ISREG(buff.st_mode))
                        nType = 2;
                     else if (S_ISDIR(buff.st_mode))
                        nType = 1;
                }

                if (2 == nType)
                {
                    std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                    oArray.push_back(strDirectory + L"/" + sName);
                }

                if (bIsRecursion && (1 == nType))
                {
                    if(dirp->d_name[0] != '.')
                    {
                        std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                        GetFiles2(strDirectory + L"/" + sName, oArray, bIsRecursion);
                    }
                }
            }
            closedir(dp);
        }
        delete [] pUtf8;
#endif
        
#if defined(MAC) || defined (_IOS)
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
        DIR *dp;
        struct dirent *dirp;
        if((dp  = opendir((char*)pUtf8)) != NULL)
        {
            while ((dirp = readdir(dp)) != NULL)
            {
                if(DT_REG == dirp->d_type)
                {
                    std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                    oArray.push_back(strDirectory + L"/" + sName);
                }
                
                if (bIsRecursion && DT_DIR == dirp->d_type)
                {
                    if(dirp->d_name[0] != '.')
                    {
                        std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                        GetFiles2(strDirectory + L"/" + sName, oArray, bIsRecursion);
                    }
                }
            }
            closedir(dp);
        }
        delete [] pUtf8;
        return;
#endif
    }

    std::vector<std::wstring> GetFiles(std::wstring strDirectory, bool bIsRecursion = false)
	{
		std::vector<std::wstring> oArray;
		
		if (!strDirectory.empty())
		{     
			GetFiles2(strDirectory, oArray, bIsRecursion);
		}
		return oArray;
	}

    std::vector<std::wstring> GetDirectories(std::wstring strDirectory)
{
		std::vector<std::wstring> oArray;

#if defined(_WIN32) || defined (_WIN64)
		WIN32_FIND_DATAW oFD; 
		
		std::wstring sSpec = strDirectory + L"\\*";
		HANDLE hRes = FindFirstFileW( sSpec.c_str(), &oFD );
		if( INVALID_HANDLE_VALUE == hRes )
			return oArray;
		do 
		{
			sSpec = oFD.cFileName;
			if (sSpec != L"." && sSpec != L"..")
			{
				sSpec = strDirectory + L"\\" + sSpec;
				if( oFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) 
				{
					oArray.push_back(sSpec);
				}
			}
		} while( FindNextFileW( hRes, &oFD ) );		
		FindClose( hRes );
#elif __linux__
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
		DIR *dp;
		struct dirent *dirp;
		if((dp  = opendir((char*)pUtf8)) != NULL)
		{
			while ((dirp = readdir(dp)) != NULL)
			{
				bool bIsDir = false;
				if (DT_DIR == dirp->d_type)
					bIsDir = true;
				else if (DT_UNKNOWN == dirp->d_type)
				{
					// XFS problem
					struct stat buff;
					std::string sTmp = std::string((char*)pUtf8) + "/" + std::string(dirp->d_name);
					stat(sTmp.c_str(), &buff);
					if (S_ISDIR(buff.st_mode))
						bIsDir = true;
				}
				if(bIsDir)
				{
					if(dirp->d_name[0] != '.')
					{
						std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
						oArray.push_back(strDirectory + L"/" + sName);
					}
				}
			}
			closedir(dp);
		}
		delete [] pUtf8;
#elif MAC
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
        DIR *dp;
        struct dirent *dirp;
        if((dp  = opendir((char*)pUtf8)) != NULL)
        {
            while ((dirp = readdir(dp)) != NULL)
            {
                if(DT_DIR == dirp->d_type)
                {
                    if(dirp->d_name[0] != '.')
                    {
                        std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                        oArray.push_back(strDirectory + L"/" + sName);
                    }
                }
            }
            closedir(dp);
        }
        delete [] pUtf8;
#endif
		return oArray;
	}

    bool Exists(const std::wstring& strDirectory)
{
#if defined(_WIN32) || defined (_WIN64)
		DWORD dwAttrib = ::GetFileAttributesW(strDirectory.c_str());
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && 0 != (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#elif __linux__
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
        bool bRes = is_directory_exist((char*)pUtf8);
        delete [] pUtf8;
        return bRes;
#elif MAC
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
        struct stat st;
        bool bRes = is_directory_exist((char*)pUtf8);
        delete [] pUtf8;
        return bRes;
#endif
        return false;
	}

    bool CreateDirectory(const std::wstring& strDirectory)
    {
		if (Exists(strDirectory) == true)  return true;

#if defined(_WIN32) || defined (_WIN64)
		return FALSE != ::CreateDirectoryW(strDirectory.c_str(), NULL);
#else
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
		struct stat st;
		int nRes = 0;
		if (stat((char*)pUtf8, &st) == -1) {
			nRes = mkdir((char*)pUtf8, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		}
		delete [] pUtf8;
		return 0 == nRes;
#endif
	}
    bool CreateDirectories(const std::wstring& strDirectory)
    {
		if (Exists(strDirectory) == true)  return true;

#if defined(_WIN32) || defined (_WIN64)
		DWORD dwAttrib = ::GetFileAttributesW(strDirectory.c_str());
		if  (dwAttrib != INVALID_FILE_ATTRIBUTES && 0 != (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) return true;

		int codeResult = ERROR_SUCCESS;

		SECURITY_ATTRIBUTES sa={};

		if (strDirectory.find(L"./") == 0)
		{
			std::wstring sDir = NSFile::GetProcessDirectory() + L"/" + strDirectory;
			codeResult = SHCreateDirectoryExW(NULL, sDir.c_str(), &sa);
		}
		else
			codeResult = SHCreateDirectoryExW(NULL, strDirectory.c_str(), &sa);

		bool created = false;
		if (codeResult == ERROR_SUCCESS)
			created = true;

		return created;
#else
		std::string pathUtf8 = U_TO_UTF8(strDirectory);
		return _mkdir (pathUtf8.c_str());
#endif
		return false;
	}
    bool CopyDirectory(const std::wstring& strSrc, const std::wstring& strDst, bool bIsRecursion = true)
    {
        if (!NSDirectory::Exists(strDst))
            NSDirectory::CreateDirectory(strDst);

#ifdef WIN32
        WIN32_FIND_DATAW oFD;

        std::wstring sSpec = strSrc + L"\\*.*";
        HANDLE hRes = FindFirstFileW( sSpec.c_str(), &oFD );
        if( INVALID_HANDLE_VALUE == hRes )
            return false;
        do
        {
            sSpec = oFD.cFileName;
            if (sSpec != L"." && sSpec != L"..")
            {
                if( !( oFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
                {
                    NSFile::CFileBinary::Copy(strSrc + L"/" + sSpec, strDst + L"/" + sSpec);
                }
                else if (bIsRecursion)
                {
                    CopyDirectory(strSrc + L"/" + sSpec, strDst + L"/" + sSpec, bIsRecursion);
                }
            }
        } while( FindNextFileW( hRes, &oFD ) );
        FindClose( hRes );
        return true;
#endif

#ifdef __linux__
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strSrc.c_str(), strSrc.length(), pUtf8, lLen, false);
        DIR *dp;
        struct dirent *dirp;
        if((dp  = opendir((char*)pUtf8)) != NULL)
        {
            while ((dirp = readdir(dp)) != NULL)
            {
                int nType = 0;
                if(DT_REG == dirp->d_type)
                    nType = 2;
                else if (DT_DIR == dirp->d_type)
                    nType = 1;
                else if (DT_UNKNOWN == dirp->d_type)
                {
                     // XFS problem
                     struct stat buff;
                     std::string sTmp = std::string((char*)pUtf8) + "/" + std::string(dirp->d_name);
                     stat(sTmp.c_str(), &buff);
                     if (S_ISREG(buff.st_mode))
                        nType = 2;
                     else if (S_ISDIR(buff.st_mode))
                        nType = 1;
                }

                if (2 == nType)
                {
                    std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                    NSFile::CFileBinary::Copy(strSrc + L"/" + sName, strDst + L"/" + sName);
                }

                if (bIsRecursion && (1 == nType))
                {
                    if(dirp->d_name[0] != '.')
                    {
                        std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                        CopyDirectory(strSrc + L"/" + sName, strDst + L"/" + sName, bIsRecursion);
                    }
                }
            }
            closedir(dp);
        }
        delete [] pUtf8;
        return true;
#endif

#if defined(MAC) || defined (_IOS)
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strSrc.c_str(), strSrc.length(), pUtf8, lLen, false);
        DIR *dp;
        struct dirent *dirp;
        if((dp  = opendir((char*)pUtf8)) != NULL) {
            while ((dirp = readdir(dp)) != NULL) {
                if(DT_REG == dirp->d_type) {
                    std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                    NSFile::CFileBinary::Copy(strSrc + L"/" + sName, strDst + L"/" + sName);
                }

                if (bIsRecursion && DT_DIR == dirp->d_type) {
                    if(dirp->d_name[0] != '.') {
                        std::wstring sName = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)dirp->d_name, strlen(dirp->d_name));
                        CopyDirectory(strSrc + L"/" + sName, strDst + L"/" + sName, bIsRecursion);
                    }
                }
            }
            closedir(dp);
        }
        delete [] pUtf8;
        return true;
#endif

        return false;
    }
    void DeleteDirectory(const std::wstring& strDirectory, bool deleteRoot = true) {
		std::vector<std::wstring> aFiles = GetFiles(strDirectory);
		for(size_t i = 0; i < aFiles.size(); ++i)
		{
			NSFile::CFileBinary::Remove(aFiles[i]);
		}
		std::vector<std::wstring> aDirectories = GetDirectories(strDirectory);
		for(size_t i = 0; i < aDirectories.size(); ++i) {
			DeleteDirectory(aDirectories[i]);
		}
#if defined(_WIN32) || defined (_WIN64)
		if (deleteRoot) RemoveDirectoryW(strDirectory.c_str());
#elif __linux__
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
		rmdir((char*)pUtf8);
		delete [] pUtf8;

		if (deleteRoot = false)CreateDirectory(strDirectory);
#elif MAC
        BYTE* pUtf8 = NULL;
        LONG lLen = 0;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strDirectory.c_str(), strDirectory.length(), pUtf8, lLen, false);
        rmdir((char*)pUtf8);
        delete [] pUtf8;
        
        if (deleteRoot = false)CreateDirectory(strDirectory);
#endif
	}
    std::wstring GetFolderPath(const std::wstring& wsFolderPath) {
		int n1 = (int)wsFolderPath.rfind('\\');
		if (n1 < 0)
		{
			n1 = (int)wsFolderPath.rfind('/');
			if (n1 < 0)
			{
				return L"";
			}
			return wsFolderPath.substr(0, n1);
		}
		return wsFolderPath.substr(0, n1);
	}
    std::wstring CreateTempFileWithUniqueName (const std::wstring & strFolderPathRoot, std::wstring Prefix) {
        return NSFile::CFileBinary::CreateTempFileWithUniqueName(strFolderPathRoot, Prefix);
    }
    std::wstring CreateDirectoryWithUniqueName (const std::wstring & strFolderPathRoot) {
#if defined(_WIN32) || defined (_WIN64)
        UUID uuid;
        RPC_WSTR str_uuid;
        UuidCreate (&uuid);
        UuidToStringW (&uuid, &str_uuid);
                std::wstring pcTemplate = strFolderPathRoot + FILE_SEPARATOR_STR;
        pcTemplate += (wchar_t *) str_uuid;
        RpcStringFreeW (&str_uuid);

        int attemps = 10;
        while (!CreateDirectory(pcTemplate))
        {
            UuidCreate (&uuid);
            UuidToStringW (&uuid, &str_uuid);
            pcTemplate = strFolderPathRoot + FILE_SEPARATOR_STR;
            pcTemplate += (wchar_t *) str_uuid;
            RpcStringFreeW (&str_uuid);
            attemps--;

            if (0 == attemps)
            {
                pcTemplate = L"";
                break;
            }
        }
        return pcTemplate;
#else
        std::string pcTemplate = U_TO_UTF8(strFolderPathRoot) + "/ascXXXXXX";
        char *pcRes = mkdtemp(const_cast <char *> (pcTemplate.c_str()));
        if (NULL == pcRes)
            return L"";

        std::string sRes = pcRes;
        return NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)sRes.c_str(), (LONG)sRes.length());
#endif
    }
    std::wstring GetTempPath() {
        return NSFile::CFileBinary::GetTempPath();
    }

    int GetFilesCount(const std::wstring& path, const bool& recursive) {
        std::vector<std::wstring> arrFiles = NSDirectory::GetFiles(path, recursive);
#if defined(_WIN32) || defined (_WIN64)
        return (int)arrFiles.size();
#endif
        return (int)arrFiles.size() + 1;
        // ???
    }
    bool PathIsDirectory(const std::wstring& pathName) {
        return Exists(pathName);
    }
}

#endif //_BUILD_FILE_CROSSPLATFORM_H_
