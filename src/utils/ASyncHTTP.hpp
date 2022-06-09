#ifndef ASYNCHTTP_H
#define ASYNCHTTP_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/spawn.hpp>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "Time.hpp"
#include "Encoding.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http  = beast::http;           // from <boost/beast/http.hpp>
namespace net   = boost::asio;            // from <boost/asio.hpp>
namespace ssl   = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using     tcp   = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class AsyncHTTPSession : public std::enable_shared_from_this<AsyncHTTPSession>
{
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    //http::request<http::empty_body> req_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    std::string _host;
    std::string _port;
    std::string _api_key;
    std::string _api_secret;
    std::string _subacc_name;
public:
    AsyncHTTPSession(net::any_io_executor ex_,
            ssl::context& ctx_,
            const std::string& api_key_,
            const std::string& api_secret_,
            const std::string& subacc_name_,
            std::function<void(std::string)> event_handler_);

    ~AsyncHTTPSession();

    std::function<void(std::string/*, void* */)> event_handler;
    void get(const std::string& target);
    void post(const std::string& target, const std::string& payload);
    void delete_(const std::string& target_, const std::string &payload_);
    void delete_(const std::string& target_);
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);
    void on_handshake(beast::error_code ec);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_shutdown(beast::error_code ec);
    void authenticate(http::request<http::string_body>& req);
    static void fail(beast::error_code ec, char const* what);
};

#endif // ASYNCHTTP_H
