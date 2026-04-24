#include "StaticFiles.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace static_files {
namespace {

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string content_type_for(const std::string& path) {
    if (ends_with(path, ".html")) {
        return "text/html; charset=utf-8";
    }
    if (ends_with(path, ".css")) {
        return "text/css; charset=utf-8";
    }
    if (ends_with(path, ".js")) {
        return "application/javascript; charset=utf-8";
    }
    if (ends_with(path, ".svg")) {
        return "image/svg+xml";
    }
    if (ends_with(path, ".png")) {
        return "image/png";
    }
    if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) {
        return "image/jpeg";
    }

    return "application/octet-stream";
}

std::string clean_target(std::string target) {
    const auto query_start = target.find('?');
    if (query_start != std::string::npos) {
        target = target.substr(0, query_start);
    }

    const auto hash_start = target.find('#');
    if (hash_start != std::string::npos) {
        target = target.substr(0, hash_start);
    }

    if (target.empty() || target == "/") {
        return "index.html";
    }

    while (!target.empty() && target.front() == '/') {
        target.erase(target.begin());
    }

    return target;
}

bool is_safe_relative_path(const std::string& path) {
    return !path.empty()
        && path.find("..") == std::string::npos
        && path.find('\\') == std::string::npos
        && path.front() != '/';
}

FileResponse not_found() {
    return {
        404,
        "text/html; charset=utf-8",
        "<!doctype html><html lang=\"zh-CN\"><meta charset=\"utf-8\"><title>404</title>"
        "<body style=\"font-family:sans-serif;padding:40px\"><h1>404</h1><p>页面不存在。</p></body></html>"
    };
}

}

FileResponse load_for_target(const std::string& request_target) {
    const std::string relative_path = clean_target(request_target);
    if (!is_safe_relative_path(relative_path)) {
        return not_found();
    }

    // Support common working directories: solution root, project root, and Debug output folders.
    const std::vector<std::string> web_roots = {
        "web/",
        "boost_Server/web/",
        "../web/",
        "../boost_Server/web/",
        "../../web/",
        "../../boost_Server/web/"
    };

    for (const auto& root : web_roots) {
        const std::string file_path = root + relative_path;
        std::string body = read_file(file_path);
        if (!body.empty()) {
            return { 200, content_type_for(file_path), std::move(body) };
        }
    }

    return not_found();
}

}
