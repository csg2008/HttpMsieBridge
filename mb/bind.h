#pragma once
#include <functional>
#include <string>
#include <vector>
#include "wke.h"

namespace HttpBridge {

class JsValueShim {
public:
    JsValueShim(jsExecState es, int index)
        : es_(es), value_(jsArg(es, index)) {}
    operator bool() const { return jsToBoolean(es_, value_); }
    operator int() const { return jsToInt(es_, value_); }
    operator std::string() const { return jsToTempString(es_, value_); }
    operator std::wstring() const { return jsToTempStringW(es_, value_); }

private:
    jsExecState es_;
    jsValue value_;
};

void bind(const char *name, std::function<void()> callback);
void bind(const char *name, std::function<void(JsValueShim)> callback);
void bind(
    const char *name,
    std::function<void(JsValueShim, JsValueShim)> callback);
void bind(
    const char *name,
    std::function<void(JsValueShim, JsValueShim, JsValueShim)> callback);
void bind(
    const char *name,
    std::function<void(
        JsValueShim, JsValueShim, JsValueShim, JsValueShim)> callback);

}  // namespace HttpBridge
