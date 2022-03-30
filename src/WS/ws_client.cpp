#include "ws_client.hpp"
#include "../utils/Encoding.hpp"
#include "../utils/Time.hpp"
#include <utility>


namespace encoding = util::_encoding;

namespace ftx {

    WSClient::WSClient(const std::string& api_key, const std::string& api_secret,
                       net::io_context& ioc, const std::function<void(std::string, void*)>& event_handler)
    {
        try{
            m_api_key     = api_key;
            m_api_secret  = api_secret;
            ws = std::make_shared<util::WS>("ftx.com", "443", "/ws", ioc, event_handler);
            if(api_key.empty() && api_secret.empty())
                ws->setChannelName("public channel");
            else
                ws->setChannelName("private channel");
            //ws = std::make_shared<util::WS>("ftx.com", "8443", "/api/markets", ioc, event_handler);
            //ws = std::make_shared<util::WS>("ws.okx.com", "8443", "/ws/v5/public", ioc, event_handler);
            ws->async_read();
            /*ws->configure(m_uri, api_key, api_secret, m_subaccount_name);
            ws->set_on_open_cb([this]() { return this->on_open(); });*/
        }
        catch(std::exception& ex){
            throw ex.what();
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
    size_t WSClient::subscribe_order(const std::string& market)
    {
        return ws->write(json::serialize(json::value{
                                      {"op", "subscribe"},
                                      {"channel", "orders"},
                                      {"market", market}
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
            if (!(m_api_key.empty() || m_api_secret.empty()))
            {
                long ts = util::get_ms_timestamp(util::current_time()).count();
                std::string data = std::to_string(ts) + "websocket_login";
                std::string hmacced = encoding::hmac(std::string(m_api_secret), data, 32);
                std::string sign = encoding::str_to_hex((unsigned char*)hmacced.c_str(), 32);
                return ws->write(json::serialize(json::value{
                                              {"op", "login"},
                                              {"args",
                                              {
                                                  {"key", m_api_key},
                                                  {"sign", sign},
                                                  {"time", ts}
                                              }
                                              }
                                          }));
            }
        }
        catch(std::exception& ex){
            error += ex.what();
            return 0;
        }
    }
}
