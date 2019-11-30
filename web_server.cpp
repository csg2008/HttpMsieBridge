// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "defines.h"
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <thread>
#include <map>

#include "3rd/ieproxy.h"
#include "3rd/httplib.h"
#include "3rd/jsonrpc.h"
#include "misc/file_utils.h"
#include "web_server.h"
#include "misc/logger.h"
#include "main.h"
#include "settings.h"
#include "misc/string_utils.h"
#include "msie/browser_window.h"
#include "misc/version.h"
#include "mb/mb.hpp"

using namespace httplib;

extern HWND g_hwnd;

httplib::Server svr;

std::string dump_headers(const Headers &headers) {
  std::string s;
  char buf[BUFSIZ];

  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const auto &x = *it;
    snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

std::string log(const Request &req, const Response &res) {
  std::string s;
  char buf[BUFSIZ];

  s += "================================\n";

  snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
           req.version.c_str(), req.path.c_str());
  s += buf;

  std::string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto &x = *it;
    snprintf(buf, sizeof(buf), "%c%s=%s",
             (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
             x.second.c_str());
    query += buf;
  }
  snprintf(buf, sizeof(buf), "%s\n", query.c_str());
  s += buf;

  s += dump_headers(req.headers);

  s += "--------------------------------\n";

  snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
  s += buf;
  s += dump_headers(res.headers);
  s += "\n";

  if (!res.body.empty()) { s += res.body; }

  s += "\n";

  return s;
}

// get browse
BrowserWindow* getBrowse(const Request &req, Response &res) {
    BrowserWindow* browser = 0;
    std::string pid = req.get_form_value("pid");

    if ("" != pid) {
        browser = GetBrowserWindow(pid);
        if (!browser) {
            LOG_WARNING << "router(): could not fetch BrowserWindow";

            res.set_content("could not fetch BrowserWindow", "text/plain");
        }
    }

    return browser;
}

int httptest(int port) {
    if (!svr.is_valid()) {
        printf("server has an error...\n");
        return -1;
    }

    svr.Get("/", [=](const Request & /*req*/, Response &res) {
        res.set_redirect("/hi");
    });

    svr.Get("/hi", [](const Request & /*req*/, Response &res) {
        res.set_content("Hello World!\n", "text/plain");
    });

    svr.Get("/slow", [](const Request & /*req*/, Response &res) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        res.set_content("Slow...\n", "text/plain");
    });

    svr.Get("/dump", [](const Request &req, Response &res) {
        res.set_content(dump_headers(req.headers), "text/plain");
    });

    svr.Get("/stop", [&](const Request & /*req*/, Response & /*res*/) {
        svr.stop();
    });

    svr.Post("/api/v1/pages", [](const Request &req, Response &res) {
        std::map<std::string, std::string> list = GetBrowserList();

        res.set_content(nlohmann::json(list).dump().c_str(), "text/plain");
    });

    svr.Post("/api/v1/notify", [](const Request &req, Response &res) {
        bool ret = false;
        std::string snd = trim(req.get_form_value("snd"));
        std::string msg = trim(req.get_form_value("msg"));

        if (msg.empty()) {
            res.set_content("param msg is empty", "text/plain");
        } else {
            int sid = snd.empty() ? 0 : std::stoi(snd);
            ret = ShowTrayTip(g_hwnd, msg, sid);

            res.set_content(ret ? "ok" : "failed", "text/plain");
        }
    });

    svr.Post("/api/v1/proxy", [](const Request &req, Response &res) {
        bool ret = false;
        std::string server = trim(req.get_form_value("server"));
        bool enable = "yes" == LowerString(trim(req.get_form_value("enable")));

        ProxyConfig config;
        if (server.empty()) {
            std::map<std::string, std::string> cfg;
            getProxyConfig(&config);

            cfg["auto"] = config.auto_detect_settings ? "yes" : "no";
            cfg["url"] = WideToUtf8(config.auto_config_url);
            cfg["server"] = WideToUtf8(config.proxy_server);
            cfg["bypass"] = WideToUtf8(config.bypass_domain_list);

            res.set_content(nlohmann::json(cfg).dump().c_str(), "text/plain");
        } else {
            config.proxy_server = Utf8ToWide(server);
            setProxyConfig(&config);
            refreshProxySettings();

            res.set_content(ret ? "ok" : "failed", "text/plain");
        }
    });

    svr.Post("/api/v1/open", [](const Request &req, Response &res) {
        std::string url = trim(req.get_form_value("url"));

        if (url.empty()) {
            url = "https://www.baidu.com";
        }

        int flag = mb_open(ConvertW(url.c_str()));

        res.set_content(0 == flag ? "ok" : "failed", "text/plain");
    });

    svr.Post("/api/v1/url", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        std::string url = trim(req.get_form_value("url"));

        if (!browser) {
            return ;
        }

        if (url.empty()) {
            _bstr_t url;
            browser -> GetURL(url.GetAddress());

            res.set_content((char*) url, "text/plain");
        } else {
            std::string data = trim(req.get_form_value("data"));
            std::string header = trim(req.get_form_value("header"));
            bool isPost = "post" == LowerString(trim(req.get_form_value("method")));
            bool ret = browser -> Navigate(isPost, url, header, data);

            res.set_content(ret ? "ok" : "failed", "text/plain");
        }
    });

    svr.Post("/api/v1/trigger", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        std::string id = trim(req.get_form_value("id"));

        if (!browser) {
           return ;
        }

        bool ret = browser -> ClickElementByID(Utf8ToWide(id));

        res.set_content(ret ? "ok" : "failed", "text/plain");
    });

    svr.Post("/api/v1/exec", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        std::wstring seperator = Utf8ToWide("|");
        std::string func = trim(req.get_form_value("func"));
        std::string args = trim(req.get_form_value("args"));
        std::vector<std::wstring> param = Split(Utf8ToWide(args), seperator);

        if (!browser) {
           return ;
        }

        bool ret = browser -> ExecJsFun(Utf8ToWide(func), param);

        res.set_content(ret ? "ok" : "failed", "text/plain");
    });

    svr.Post("/api/v1/attr", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        std::string id = trim(req.get_form_value("id"));
        std::string val = trim(req.get_form_value("val"));
        std::string attr = trim(req.get_form_value("attr"));

        if (!browser) {
           return ;
        }

        if (attr.empty()) {
            attr = "value";
        }

        if (val.empty()) {
            _variant_t varResult;
            bool ret = browser -> GetElementAttrByID(Utf8ToWide(id), Utf8ToWide(attr), &varResult);

            res.set_content(ret ? wchar_to_char(varResult.bstrVal) : "", "text/plain");
        } else {
            bool ret = browser -> SetElementAttrByID(Utf8ToWide(id), Utf8ToWide(attr), Utf8ToWide(val));

            res.set_content(ret ? "ok" : "failed", "text/plain");
        }
    });

    svr.Post("/api/v1/inject", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        _bstr_t where = _bstr_t("beforeEnd");
        _bstr_t html = _bstr_t(req.get_form_value("html").c_str());

        if (!browser) {
           return ;
        }

        bool ret = browser -> Inject(where.GetAddress(), html.GetAddress());

        res.set_content(ret ? "ok" : "failed", "text/plain");
    });

    svr.Post("/api/v1/title", [](const Request &req, Response &res) {
        _bstr_t title;
        BrowserWindow* browser = getBrowse(req, res);

        if (!browser) {
           return ;
        }

        browser -> GetTitle(title.GetAddress());
        
        res.set_content((char*) title, "text/plain");
    });

    svr.Post("/api/v1/html", [](const Request &req, Response &res) {
        _bstr_t html;
        BrowserWindow* browser = getBrowse(req, res);

        if (!browser) {
           return ;
        }

        browser -> GetHtml(html.GetAddress());
        
        res.set_content((char*) html, "text/plain");
    });

    svr.Post("/api/v1/variable", [](const Request &req, Response &res) {
        res.set_content("coding...", "text/plain");
    });

    svr.Post("/api/v1/cookie", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        std::string url = trim(req.get_form_value("url"));
        std::string key = trim(req.get_form_value("key"));

        if (!browser) {
           return ;
        }

        if ("" != url && "" != key) {
            TCHAR szActualCookie [10240];
            bool flag = browser -> GetHttpOnlyCookie(ConvertW(url.c_str()), ConvertW(key.c_str()), szActualCookie);

            if (flag) {
                std::string data = TCHAR2STRING(szActualCookie);

                res.set_content(data.c_str(), "text/plain");
            } else {
                res.set_content("", "text/plain");
            }
        } else {
            _bstr_t cookie;
            if (browser -> GetCookies(cookie.GetAddress()) && cookie.length() > 0) {
                res.set_content((char*) cookie, "text/plain");
            } else {
                res.set_content("", "text/plain");
            }
        }
    });

    svr.Post("/api/v1/refresh", [](const Request &req, Response &res) {
        BrowserWindow* browser = getBrowse(req, res);
        std::string level = trim(req.get_form_value("level"));
        int nLevel = level.empty() ? 0 : std::stoi(level);

        if (!browser) {
           return ;
        }

        if (0 != nLevel && 1 != nLevel && 3 != nLevel) {
            nLevel = 1;
        }

        browser -> Refresh2(nLevel);
        
        res.set_content("ok", "text/plain");
    });

    svr.set_error_handler([](const Request & /*req*/, Response &res) {
        const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    svr.set_logger([](const Request &req, const Response &res) {
        printf("%s", log(req, res).c_str());
    });

    svr.listen("0.0.0.0", port);

	return 0;
}

bool StartWebServer() {
    LOG_INFO << "Starting web server, Built on: " << __DATE__;
    LOG_INFO << "CPU Core " << std::thread::hardware_concurrency();


    nlohmann::json* appSettings = GetApplicationSettings();

    // 404_handler
    std::string _404_handler = (*appSettings)["integration"]["404_handler"];

    // Index files from settings.
    std::size_t cnt;
    std::string indexFiles;
    const nlohmann::json indexFilesArray = (*appSettings)["integration"]["index"];

    cnt = indexFilesArray.size();
    for (std::size_t i = 0; i < cnt; i++) {
        std::string file = indexFilesArray[i];
        if (!file.empty()) {
            if (indexFiles.length())
                indexFiles.append(",");
            indexFiles.append(file);
        }
    }
    if (indexFiles.empty())
        indexFiles = "index.html,index.php";
    LOG_INFO << "Index files: " << indexFiles;


    // Hide files patterns.
    std::string hide_files_patterns = "";
    const nlohmann::json hide_files = (*appSettings)["integration"]["hides"];

    cnt = hide_files.size();
    for (std::size_t i = 0; i < cnt; i++) {
        std::string pattern = hide_files[i];
        if (!pattern.empty()) {
            if (hide_files_patterns.length())
                hide_files_patterns.append("|");
            hide_files_patterns.append("**/").append(pattern).append("$");
        }
    }
    LOG_INFO << "Hide files patterns: " << hide_files_patterns;

    // Ip address and port. If port was set to 0, then real port
    // will be known only after the webserver was started.
    int portInt = 0;
    if ((*appSettings)["integration"]["listen"].is_string()) {
        portInt = (int) atoi((*appSettings)["integration"]["listen"].get<std::string>().c_str());
    } else if ((*appSettings)["integration"]["listen"].is_number()) {
        portInt = (*appSettings)["integration"]["listen"];
    }
    if (0 == portInt) {
        portInt = 8080;
    }

    LOG_INFO << "Starting web server on port " << portInt;

    std::thread th(httptest, portInt);
    th.detach();

    return true;
}

void StopWebServer() {
    LOG_INFO << "Stopping web server";

    if (svr.is_running()) {
        svr.stop();
    }
}