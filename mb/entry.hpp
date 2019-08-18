#include <windows.h>
#include <map>
#include <string>
#include "bind.h"
#include "window.h"
#include "wke.h"

using namespace miniblink_cxx_example;

std::map<std::string, Window*> g_mb;

int mb_open(const wchar_t *url) {
    if (!wkeInitialize()) {
        return 1;
    }

    Window window(WKE_WINDOW_TYPE_POPUP, NULL, 800, 600);
    bind("add", [&window](int a, int b) {
        window.call("setValue", a + b);
    });
    //window.load(L"app:///index.html");
    window.load(url);
    window.set_quit_on_close();
    window.show();

    return 0;
}
