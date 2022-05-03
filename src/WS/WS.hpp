#pragma once
#include <string>
//#include <json.hpp>
/*#include <../external/json.hpp>*/
#include <functional>
/*#include <../websocketpp/client.hpp>
#include <../websocketpp/config/asio_client.hpp>

using json = nlohmann::json;*/

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

//#include "../utils/logging.hpp"
#include "../utils/mylogger.hpp"

namespace net = boost::asio;        // from <boost/asio.hpp>
namespace ssl = net::ssl;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace websocket = beast::websocket;
namespace http = beast::http;       // from <boost/beast/http.hpp>

using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

namespace util
{

    class WS : public std::enable_shared_from_this<WS>
    {
        public:
        std::shared_ptr<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>> ws;
        boost::beast::flat_buffer buffer;
        std::function<void(std::string, void*)> event_handler;
        void on_read(boost::beast::error_code ec, std::size_t bytes_transgerred);
        WS(std::string host, const std::string& port, const std::string& target,
           boost::asio::io_context& ioc, std::function<void(std::string, void*)> event_handler, const std::shared_ptr<spdlog::logger> &logger);
        size_t write(const std::string& message);
        void async_read();

        void setChannelName(std::string value){channelName = value;}
        std::string getChannelName(){return channelName;}

    private:
        std::string channelName;
        uint64_t    _id;
        std::shared_ptr<spdlog::logger>  _errors_logger;
    };
}
