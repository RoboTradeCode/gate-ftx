#include <utility>

#include "ws_client.hpp"
#include "../utils/Encoding.hpp"
#include "../utils/Time.hpp"


namespace encoding = util::_encoding;

namespace ftx {

    WSClient::WSClient(const std::string& api_key,
                       const std::string& api_secret,
                       const std::string& subacc_name_,
                       net::io_context& ioc,
                       const std::function<void(std::string/*, void**/)>& event_handler,
                       const std::shared_ptr<spdlog::logger> &logger)
    {
        try{
            _api_key         = api_key;
            _api_secret      = api_secret;
            _subaccount_name = subacc_name_;
            ws = std::make_shared<util::WS>("ftx.com", "443", "/ws", ioc, event_handler, logger);
            if(api_key.empty() && api_secret.empty())
                ws->set_channel_name("public channel");
            else
                ws->set_channel_name("private channel");
            ws->async_read();
        }
        catch(std::exception& ex){
            //throw ex.what();
            std::string exc = ex.what();
            std::cout << exc << std::endl;
            throw std::runtime_error(exc);
        }
    }
    size_t WSClient::subscribe_ticker(const std::string& market)
    {
        return ws->write(json::serialize(json::value{
                                      {"op", "subscribe"},
                                      {"channel", "ticker"},
                                      {"market", market}
                                  }));
    }
    size_t WSClient::subscribe_markets()
    {
        return ws->write(json::serialize(json::value{
                                      {"op", "subscribe"},
                                      {"channel", "markets"}
                                  }));
    }
    size_t WSClient::subscribe_orderbook(const std::string& market)
    {
        return ws->write(json::serialize(json::value{
                                      {"op", "subscribe"},
                                      {"channel", "orderbook"},
                                      {"market", market}
                                  }));
    }
    size_t WSClient::subscribe_order(/*const std::string& market*/)
    {
        return ws->write(json::serialize(json::value{
                                      {"op", "subscribe"},
                                      {"channel", "orders"}/*,
                                      {"market", market}*/
                                  }));
    }
    size_t WSClient::ping()
    {
        return ws->write(json::serialize(json::value{
                                      {"op", "ping"}
                                  }));
    }
    size_t WSClient::login(std::string& error)
    {
        try{
            if (!(_api_key.empty() || _api_secret.empty()))
            {
                long ts = util::get_ms_timestamp(util::current_time()).count();
                std::string data = std::to_string(ts) + "websocket_login";
                std::string hmacced = encoding::hmac(std::string(_api_secret), data, 32);
                std::string sign = encoding::str_to_hex((unsigned char*)hmacced.c_str(), 32);
                if (_subaccount_name.empty()) {
                    return ws->write(json::serialize(json::value{
                                                  {"op", "login"},
                                                  {"args",
                                                  {
                                                      {"key", _api_key},
                                                      {"sign", sign},
                                                      {"time", ts}
                                                  }
                                                  }
                                              }));
                } else {
                    return ws->write(json::serialize(json::value{
                                                  {"op", "login"},
                                                  {"args",
                                                  {
                                                      {"key", _api_key},
                                                      {"sign", sign},
                                                      {"time", ts},
                                                      {"subaccount", _subaccount_name}
                                                  }
                                                  }
                                              }));
                }

            }
        }
        catch(std::exception& ex){
            error += ex.what();
            return 0;
        }
        return 0;
    }
}
