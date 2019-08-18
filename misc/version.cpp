// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// getDiskSn token from: https://blog.csdn.net/lpwstr/article/details/78935969
// Website: http://code.google.com/p/phpdesktop/

#include <tchar.h>
#include <Windows.h>
#include <assert.h>
#include "logger.h"
#include "version.h"
#include "string_utils.h"

#pragma comment(lib, "Version")

char* flipAndCodeBytes(const char * str, int pos, int flip, char * buf) {
    int i;
    int j = 0;
    int k = 0;

    buf[0] = '\0';
    if (pos <= 0)
        return buf;

    if (!j) {
        char p = 0;

        // First try to gather all characters representing hex digits only.
        j = 1;
        k = 0;
        buf[k] = 0;
        for (i = pos; j && str[i] != '\0'; ++i) {
            char c = tolower(str[i]);

            if (isspace(c))
                c = '0';

            ++p;
            buf[k] <<= 4;

            if (c >= '0' && c <= '9')
                buf[k] |= (unsigned char)(c - '0');
            else if (c >= 'a' && c <= 'f')
                buf[k] |= (unsigned char)(c - 'a' + 10);
            else {
                j = 0;
                break;
            }

            if (p == 2) {
                if (buf[k] != '\0' && !isprint(buf[k])) {
                    j = 0;
                    break;
                }
                ++k;
                p = 0;
                buf[k] = 0;
            }

        }
    }

    if (!j) {
        // There are non-digit characters, gather them as is.
        j = 1;
        k = 0;
        for (i = pos; j && str[i] != '\0'; ++i) {
            char c = str[i];

            if (!isprint(c)) {
                j = 0;
                break;
            }

            buf[k++] = c;
        }
    }

    if (!j) {
        // The characters are not there or are not printable.
        k = 0;
    }

    buf[k] = '\0';

    if (flip)
        // Flip adjacent characters
        for (j = 0; j < k; j += 2) {
            char t = buf[j];
            buf[j] = buf[j + 1];
            buf[j + 1] = t;
        }

    // Trim any beginning and end space
    i = j = -1;
    for (k = 0; buf[k] != '\0'; ++k) {
        if (!isspace(buf[k])) {
            if (i < 0)
                i = k;
            j = k;
        }
    }

    if ((i >= 0) && (j >= 0)) {
        for (k = i; (k <= j) && (buf[k] != '\0'); ++k)
            buf[k - i] = buf[k];
        buf[k - i] = '\0';
    }

    return buf;
}

size_t GetHDSerial(PCHAR pszIDBuff, int nBuffLen, int nDriveID) {
    HANDLE hPhysicalDrive = INVALID_HANDLE_VALUE;
    size_t ulSerialLen = 0;
    __try {
        //  Try to get a handle to PhysicalDrive IOCTL, report failure
        //  and exit if can't.
        TCHAR szDriveName[32];
        wsprintf(szDriveName, TEXT("\\\\.\\PhysicalDrive%d"), nDriveID);

        //  Windows NT, Windows 2000, Windows XP - admin rights not required
        hPhysicalDrive = CreateFile(szDriveName, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, NULL);
        if (hPhysicalDrive == INVALID_HANDLE_VALUE) {
            __leave;
        }
        STORAGE_PROPERTY_QUERY query;
        DWORD cbBytesReturned = 0;
        static char local_buffer[10000];

        memset((void *)&query, 0, sizeof(query));
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        memset(local_buffer, 0, sizeof(local_buffer));

        if (DeviceIoControl(hPhysicalDrive, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &local_buffer[0], sizeof(local_buffer), &cbBytesReturned, NULL)) {
            STORAGE_DEVICE_DESCRIPTOR * descrip = (STORAGE_DEVICE_DESCRIPTOR *)& local_buffer;
            char serialNumber[1000];

            flipAndCodeBytes(local_buffer, descrip->SerialNumberOffset, 1, serialNumber);

            if (isalnum(serialNumber[0])) {
                size_t ulSerialLenTemp = strnlen(serialNumber, nBuffLen - 1);
                memcpy(pszIDBuff, serialNumber, ulSerialLenTemp);
                pszIDBuff[ulSerialLenTemp] = NULL;
                ulSerialLen = ulSerialLenTemp;
                __leave;
            }

        }
    }
    __finally {
        if (hPhysicalDrive != INVALID_HANDLE_VALUE) {
            CloseHandle(hPhysicalDrive);
        }
        return ulSerialLen;
    }
}

void GetAllHDSerialNumber() {
    const int MAX_IDE_DRIVES = 16;
    static char szBuff[0x100];
    for (int nDriveNum = 0; nDriveNum < MAX_IDE_DRIVES; nDriveNum++) {
        size_t ulLen = GetHDSerial(szBuff, sizeof(szBuff), nDriveNum);
        if (ulLen > 0) {
            LOG_DEBUG << "disk: " << nDriveNum << " sn: " << std::string(szBuff);
        }
    }
}

std::string GetModuleVersion() {
    TCHAR versionFile[MAX_PATH];
    if (!GetModuleFileName(NULL, versionFile, MAX_PATH))
        return "";
    DWORD versionHandle = NULL;
    DWORD versionSize = GetFileVersionInfoSize(versionFile, &versionHandle);
    if (versionSize == NULL)
        return "";
    LPSTR versionData = new char[versionSize];
    if (GetFileVersionInfo(versionFile, versionHandle, versionSize, versionData)) {
        UINT querySize = 0;
        LPBYTE queryBuffer = NULL;
        if (VerQueryValue(versionData, L"\\", (VOID**)&queryBuffer, &querySize)) {
            if (querySize) {
                VS_FIXEDFILEINFO *versionInfo = (VS_FIXEDFILEINFO*)queryBuffer;
                if (versionInfo->dwSignature == 0xfeef04bd)
                {
                    int major = HIWORD(versionInfo->dwFileVersionMS);
                    int minor = LOWORD(versionInfo->dwFileVersionMS);
                    int build = versionInfo->dwFileVersionLS;
                    delete[] versionData;
                    return IntToString(major).append(".").append(IntToString(minor));
                }
            }
        }
    }
    delete[] versionData;
    return "";
}