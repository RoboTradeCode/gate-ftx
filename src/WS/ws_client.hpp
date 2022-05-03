#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include "WS.hpp"
/*#include <../external/json.hpp>*/
#include <string>
#include <vector>


#include <boost/json.hpp>

namespace json = boost::json;

namespace ftx
{

    class WSClient
    {
    public:
        explicit WSClient(const std::string& api_key, const std::string& api_secret, boost::asio::io_context& ioc, const std::function<void(std::string, void*)>& event_handler, const std::shared_ptr<spdlog::logger> &logger);

        size_t subscribe_ticker(const std::string &market);
        size_t subscribe_markets();
        size_t subscribe_orderbook(const std::string& market);

        size_t subscribe_order(const std::string& market);
        size_t ping();
        size_t login(std::string& error);

        std::shared_ptr<util::WS> ws;

    private:
        std::string m_api_key;
        std::string m_api_secret;
        //const std::string m_subaccount_name = "";
    };
}
#endif
