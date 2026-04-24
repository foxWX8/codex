#pragma once

#include <string>
#include <vector>

struct AuthResult {
    bool ok;
    std::string message;
};

struct CommentItem {
    int id;
    std::string article_id;
    std::string username;
    std::string content;
    std::string created_at;
};

class AuthStore {
public:
    explicit AuthStore(std::string db_path);
    ~AuthStore();

    AuthStore(const AuthStore&) = delete;
    AuthStore& operator=(const AuthStore&) = delete;

    AuthResult initialize();
    AuthResult register_user(const std::string& username, const std::string& password);
    AuthResult login_user(const std::string& username, const std::string& password);
    AuthResult add_comment(const std::string& article_id, const std::string& username, const std::string& content);
    std::vector<CommentItem> list_comments(const std::string& article_id);

private:
    std::string db_path_;
    struct sqlite3* db_ = nullptr;
};

std::string md5_hex(const std::string& input);
