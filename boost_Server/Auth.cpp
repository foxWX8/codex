#include "Auth.h"

#include "third_party/sqlite/sqlite-amalgamation-3530000/sqlite3.h"

#include <windows.h>
#include <bcrypt.h>

#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace {

bool valid_username(const std::string& username) {
    if (username.size() < 3 || username.size() > 32) {
        return false;
    }

    for (const char ch : username) {
        const bool is_letter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        const bool is_digit = ch >= '0' && ch <= '9';
        if (!is_letter && !is_digit && ch != '_' && ch != '-') {
            return false;
        }
    }

    return true;
}

AuthResult sqlite_error(sqlite3* db) {
    return { false, sqlite3_errmsg(db) };
}

}

std::string md5_hex(const std::string& input) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD hash_size = 0;
    DWORD data_size = 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0) < 0) {
        return {};
    }

    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &data_size, 0) < 0
        || BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &data_size, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::vector<unsigned char> hash_object(object_size);
    std::vector<unsigned char> digest(hash_size);

    if (BCryptCreateHash(algorithm, &hash, hash_object.data(), object_size, nullptr, 0, 0) < 0
        || BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()), 0) < 0
        || BCryptFinishHash(hash, digest.data(), hash_size, 0) < 0) {
        if (hash) {
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        out << std::setw(2) << static_cast<int>(byte);
    }
    return out.str();
}

AuthStore::AuthStore(std::string db_path) : db_path_(std::move(db_path)) {}

AuthStore::~AuthStore() {
    if (db_) {
        sqlite3_close(db_);
    }
}

AuthResult AuthStore::initialize() {
    if (db_) {
        return { true, "ready" };
    }

    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        return sqlite_error(db_);
    }

    const char* create_users_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL UNIQUE,"
        "password_md5 TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");";
    const char* create_comments_sql =
        "CREATE TABLE IF NOT EXISTS comments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "article_id TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");";

    char* error_message = nullptr;
    if (sqlite3_exec(db_, create_users_sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
        std::string message = error_message ? error_message : "failed to create users table";
        sqlite3_free(error_message);
        return { false, message };
    }
    if (sqlite3_exec(db_, create_comments_sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
        std::string message = error_message ? error_message : "failed to create comments table";
        sqlite3_free(error_message);
        return { false, message };
    }

    return { true, "ready" };
}

AuthResult AuthStore::register_user(const std::string& username, const std::string& password) {
    if (!valid_username(username)) {
        return { false, "账号只能包含 3-32 位字母、数字、下划线或短横线" };
    }
    if (password.size() < 4 || password.size() > 64) {
        return { false, "密码长度需要在 4-64 位之间" };
    }

    const std::string password_hash = md5_hex(password);
    if (password_hash.empty()) {
        return { false, "密码加密失败" };
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT INTO users (username, password_md5) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return sqlite_error(db_);
    }

    sqlite3_bind_text(statement, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);

    if (rc == SQLITE_CONSTRAINT) {
        return { false, "账号已存在" };
    }
    if (rc != SQLITE_DONE) {
        return sqlite_error(db_);
    }

    return { true, "注册成功，可以登录了" };
}

AuthResult AuthStore::login_user(const std::string& username, const std::string& password) {
    if (!valid_username(username) || password.empty()) {
        return { false, "账号或密码不正确" };
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT password_md5 FROM users WHERE username = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return sqlite_error(db_);
    }

    sqlite3_bind_text(statement, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(statement);

    if (rc != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return { false, "账号或密码不正确" };
    }

    const unsigned char* stored = sqlite3_column_text(statement, 0);
    const std::string stored_hash = stored ? reinterpret_cast<const char*>(stored) : "";
    sqlite3_finalize(statement);

    if (stored_hash != md5_hex(password)) {
        return { false, "账号或密码不正确" };
    }

    return { true, "登录成功" };
}

AuthResult AuthStore::add_comment(const std::string& article_id, const std::string& username, const std::string& content) {
    if (article_id.empty() || article_id.size() > 32) {
        return { false, "文章不存在" };
    }
    if (!valid_username(username)) {
        return { false, "请先登录" };
    }
    if (content.empty() || content.size() > 500) {
        return { false, "评论内容需要在 1-500 字之间" };
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT INTO comments (article_id, username, content) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return sqlite_error(db_);
    }

    sqlite3_bind_text(statement, 1, article_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, content.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);

    if (rc != SQLITE_DONE) {
        return sqlite_error(db_);
    }

    return { true, "评论已发布" };
}

std::vector<CommentItem> AuthStore::list_comments(const std::string& article_id) {
    std::vector<CommentItem> comments;
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "SELECT id, article_id, username, content, created_at "
        "FROM comments WHERE article_id = ? ORDER BY id DESC LIMIT 50;";

    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return comments;
    }

    sqlite3_bind_text(statement, 1, article_id.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        CommentItem item{};
        item.id = sqlite3_column_int(statement, 0);
        const unsigned char* article = sqlite3_column_text(statement, 1);
        const unsigned char* user = sqlite3_column_text(statement, 2);
        const unsigned char* text = sqlite3_column_text(statement, 3);
        const unsigned char* created_at = sqlite3_column_text(statement, 4);
        item.article_id = article ? reinterpret_cast<const char*>(article) : "";
        item.username = user ? reinterpret_cast<const char*>(user) : "";
        item.content = text ? reinterpret_cast<const char*>(text) : "";
        item.created_at = created_at ? reinterpret_cast<const char*>(created_at) : "";
        comments.push_back(std::move(item));
    }

    sqlite3_finalize(statement);
    return comments;
}
