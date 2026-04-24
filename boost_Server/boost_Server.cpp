// boost_Server.cpp: local Boost.Beast HTTP server entry point.

#include "Auth.h"
#include "StaticFiles.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstdlib>
#include <cctype>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace {

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }

    return escaped;
}

std::string json_response_body(const AuthResult& result) {
    return std::string("{\"ok\":") + (result.ok ? "true" : "false")
        + ",\"message\":\"" + json_escape(result.message) + "\"}";
}

std::string json_response_body(const AuthResult& result, const std::string& token, const std::string& username) {
    return std::string("{\"ok\":") + (result.ok ? "true" : "false")
        + ",\"message\":\"" + json_escape(result.message) + "\""
        + ",\"token\":\"" + json_escape(token) + "\""
        + ",\"username\":\"" + json_escape(username) + "\"}";
}

std::string comments_response_body(const std::vector<CommentItem>& comments) {
    std::string body = "{\"ok\":true,\"comments\":[";
    for (std::size_t index = 0; index < comments.size(); ++index) {
        const auto& comment = comments[index];
        if (index > 0) {
            body += ",";
        }
        body += "{\"id\":" + std::to_string(comment.id)
            + ",\"article_id\":\"" + json_escape(comment.article_id) + "\""
            + ",\"username\":\"" + json_escape(comment.username) + "\""
            + ",\"content\":\"" + json_escape(comment.content) + "\""
            + ",\"created_at\":\"" + json_escape(comment.created_at) + "\"}";
    }
    body += "]}";
    return body;
}

std::string generate_session_token(const std::string& username) {
    static unsigned long long counter = 0;
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return md5_hex(username + ":" + std::to_string(ticks) + ":" + std::to_string(++counter));
}

int hex_value(const char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

std::string url_decode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            decoded += ' ';
            continue;
        }

        if (value[index] == '%' && index + 2 < value.size()) {
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded += static_cast<char>((high << 4) | low);
                index += 2;
                continue;
            }
        }

        decoded += value[index];
    }

    return decoded;
}

std::unordered_map<std::string, std::string> parse_form_body(const std::string& body) {
    std::unordered_map<std::string, std::string> fields;
    std::size_t start = 0;

    while (start <= body.size()) {
        const std::size_t end = body.find('&', start);
        const std::string pair = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::size_t separator = pair.find('=');
        if (separator != std::string::npos) {
            fields[url_decode(pair.substr(0, separator))] = url_decode(pair.substr(separator + 1));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return fields;
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

}

// Handles an HTTP request and loads a static frontend file into the response.
void handle_http_request(http::request<http::string_body>&& req, http::response<http::string_body>&& res) {
    static AuthStore auth_store("users.db");
    static const AuthResult auth_init = auth_store.initialize();
    static std::unordered_map<std::string, std::string> sessions;

    const std::string target = std::string(req.target());

    if (starts_with(target, "/api/")) {
        AuthResult result{ false, "接口不存在" };
        http::status status = http::status::not_found;
        std::string response_body;

        if (!auth_init.ok) {
            result = { false, "数据库初始化失败: " + auth_init.message };
            status = http::status::internal_server_error;
        }
        else if (req.method() != http::verb::post) {
            result = { false, "请使用 POST 请求" };
            status = http::status::method_not_allowed;
        }
        else {
            const auto fields = parse_form_body(req.body());
            const auto username = fields.count("username") ? fields.at("username") : "";
            const auto password = fields.count("password") ? fields.at("password") : "";
            const auto token = fields.count("token") ? fields.at("token") : "";
            const auto article_id = fields.count("article_id") ? fields.at("article_id") : "1";
            const auto content = fields.count("content") ? fields.at("content") : "";

            if (starts_with(target, "/api/register")) {
                result = auth_store.register_user(username, password);
                status = result.ok ? http::status::ok : http::status::bad_request;
            }
            else if (starts_with(target, "/api/login")) {
                result = auth_store.login_user(username, password);
                status = result.ok ? http::status::ok : http::status::unauthorized;
                if (result.ok) {
                    const std::string session_token = generate_session_token(username);
                    sessions[session_token] = username;
                    response_body = json_response_body(result, session_token, username);
                }
            }
            else if (starts_with(target, "/api/comments/list")) {
                const auto session = sessions.find(token);
                if (session == sessions.end()) {
                    result = { false, "请先登录后查看评论" };
                    status = http::status::unauthorized;
                }
                else {
                    response_body = comments_response_body(auth_store.list_comments(article_id));
                    status = http::status::ok;
                }
            }
            else if (starts_with(target, "/api/comments/add")) {
                const auto session = sessions.find(token);
                if (session == sessions.end()) {
                    result = { false, "请先登录后发表评论" };
                    status = http::status::unauthorized;
                }
                else {
                    result = auth_store.add_comment(article_id, session->second, content);
                    status = result.ok ? http::status::ok : http::status::bad_request;
                }
            }
        }

        if (response_body.empty()) {
            response_body = json_response_body(result);
        }

        res = { status, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json; charset=utf-8");
        res.body() = response_body;
        res.prepare_payload();
        return;
    }

    const auto file = static_files::load_for_target(std::string(req.target()));

    res = { static_cast<http::status>(file.status), req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, file.content_type);
    res.body() = file.body;
    res.prepare_payload();
}

// HTTP server session.
class session : public std::enable_shared_from_this<session> {
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;

public:
    explicit session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        // Read the request and process it asynchronously.
        http::async_read(
            socket_,
            buffer_,
            request_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

private:
    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            return;
        }

        // Write the generated page back to the client.
        handle_http_request(std::move(request_), std::move(response_));
        http::async_write(
            socket_,
            response_,
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            return;
        }

        // Close the connection.
        beast::error_code ec_close;
        socket_.shutdown(tcp::socket::shutdown_both, ec_close);
    }
};

// HTTP server wrapper.
class server {
    tcp::acceptor acceptor_;
    tcp::socket socket_;

public:
    server(net::io_context& ioc, tcp::endpoint endpoint)
        : acceptor_(ioc), socket_(ioc) {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
        do_accept();
    }

    // Accept a new client connection.
    void do_accept() {
        acceptor_.async_accept(
            socket_,
            beast::bind_front_handler(
                &server::on_accept,
                this));
    }

private:
    void on_accept(beast::error_code ec) {
        if (ec) {
            return;
        }

        std::make_shared<session>(std::move(socket_))->start();
        do_accept();
    }
};

int main() {
    try {
        auto const address = net::ip::make_address("0.0.0.0");
        unsigned short port = static_cast<unsigned short>(std::atoi("18080"));

        net::io_context ioc{ 1 };

        server s{ ioc, tcp::endpoint{address, port} };

        ioc.run();
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
