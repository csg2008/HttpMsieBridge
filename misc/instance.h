// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include "defines.h"

class Instance {
  protected:
	DWORD lastError;
	HANDLE mutex;
  public:
    Instance() {}
	void Initialize(TCHAR* mutexname) {
		mutex = CreateMutex(NULL, FALSE, mutexname);
		lastError = GetLastError();
	}
	~Instance() {
		if (mutex) {
			CloseHandle(mutex);
			mutex = NULL;
		}
	}
	bool IsRunning() {
		return (lastError == ERROR_ALREADY_EXISTS);
	}
};