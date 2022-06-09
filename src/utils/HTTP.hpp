//#pragma once
#ifndef HTTP_H
#define HTTP_H


#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

namespace util
{

    class HTTPSession
    {

        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;

    public:
        void configure(std::string uri_, std::string api_key_, std::string api_secret_, std::string subacc_name_);

        http::response<http::string_body> get(const std::string& target_);
        http::response<http::string_body> get_config(const std::string& uri_, const std::string& target_);
        http::response<http::string_body> post(const std::string& target, const std::string& payload_);
        http::response<http::string_body> delete_(const std::string& target_);
        http::response<http::string_body> delete_(const std::string& target_, const std::string& payload_);

    private:
        http::response<http::string_body> request(http::request<http::string_body> req_);

        void authenticate(http::request<http::string_body>& req_);

    private:
        net::io_context _ioc;
        std::string _uri;
        std::string _api_key;
        std::string _api_secret;
        std::string _subacc_name;
    };
}
#endif
