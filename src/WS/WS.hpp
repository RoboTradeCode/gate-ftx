#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <string>
#include <functional>

#include "../utils/mylogger.hpp"

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;

using tcp = net::ip::tcp;

namespace util
{

    class WS : public std::enable_shared_from_this<WS>
    {
    public:
        std::shared_ptr<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>> _ws;
        boost::beast::flat_buffer               _buffer;
        std::function<void(std::string/*, void**/)> _event_handler;

        WS(std::string host_,
           const std::string& port_,
           const std::string& target_,
           boost::asio::io_context& ioc_,
           std::function<void(std::string/*, void**/)> event_handler_,
           const std::shared_ptr<spdlog::logger> &logger_);
        void    on_read(boost::beast::error_code ec_, std::size_t bytes_transgerred_);
        size_t  write(const std::string& message_);
        void    async_read();

        void    set_channel_name(std::string value_){_channel_name = value_;}
        std::string get_channel_name(){return _channel_name;}

    private:
        std::string _channel_name;
        uint64_t    _id;
        std::shared_ptr<spdlog::logger>  _errors_logger;
    };
}
