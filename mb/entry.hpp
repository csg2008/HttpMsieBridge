#ifndef _MINIBLINK_CXX_EXAMPLE_ENTRY_H
#define _MINIBLINK_CXX_EXAMPLE_ENTRY_H


#include <map>
#include <string>
#include "bind.h"
#include "CMiniblink.h"
#include "wke.h"

using namespace HttpBridge;

//std::map<std::string, Window*> g_mb;

int mb_open(const wchar_t *url);

#endif  // _MINIBLINK_CXX_EXAMPLE_ENTRY_H