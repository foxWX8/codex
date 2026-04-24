#pragma once

#include <string>

namespace static_files {

struct FileResponse {
    unsigned status;
    std::string content_type;
    std::string body;
};

FileResponse load_for_target(const std::string& request_target);

}
