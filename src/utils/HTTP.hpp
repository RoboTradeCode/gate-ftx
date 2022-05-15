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
        void configure(std::string _uri, std::string _api_key, std::string _api_secret, std::string _subaccount_name);

        http::response<http::string_body> get(const std::string& _target);
        http::response<http::string_body> get_config(const std::string& _uri, const std::string& _target);
        http::response<http::string_body> post(const std::string& _target, const std::string& _payload);
        http::response<http::string_body> delete_(const std::string& _target);
        http::response<http::string_body> delete_(const std::string& _target, const std::string& _payload);

    private:
        http::response<http::string_body> request(http::request<http::string_body> req);

        void authenticate(http::request<http::string_body>& req);

    private:
        net::io_context ioc;
        std::string uri;
        std::string api_key;
        std::string api_secret;
        std::string subaccount_name;
    };
}
#endif
