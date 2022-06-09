#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include "iostream"
#include <string>
#include <vector>

#include <boost/json.hpp>

#include "WS.hpp"

namespace json = boost::json;

namespace ftx
{

    class WSClient
    {
    public:
        explicit WSClient(const std::string& api_key,
                          const std::string& api_secret,
                          const std::string& subacc_name_,
                          boost::asio::io_context& ioc,
                          const std::function<void(std::string/*, void**/)>& event_handler,
                          const std::shared_ptr<spdlog::logger> &logger);

        size_t subscribe_ticker(const std::string &market);
        size_t subscribe_markets();
        size_t subscribe_orderbook(const std::string& market);
        size_t subscribe_order();
        size_t ping();
        size_t login(std::string& error);

        std::shared_ptr<util::WS> ws;

    private:
        std::string _api_key;
        std::string _api_secret;
        std::string _subaccount_name;
    };
}
#endif
