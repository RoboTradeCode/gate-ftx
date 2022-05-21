//#pragma once
#ifndef TEST_ASYNC_REST_CLIENT_H
#define TEST_ASYNC_REST_CLIENT_H


#include <boost/beast/core.hpp>
#include <string>
#include <functional>

#include "../utils/ASyncHTTP.hpp"
#include "../utils/json.hpp"

using JSON = nlohmann::json;
//using JSON     = nlohmann::ordered_json;
namespace net = boost::asio;
namespace ftx {

class AsyncRESTClient
{
  public:
    AsyncRESTClient(const std::string api_key_, const std::string api_secret_, net::io_context& ioc_, const std::function<void(std::string)>& event_handler_);

    void get_balances();

    // выставляет ордер
    void place_order(const std::string market_,
                     const std::string side_,
                     const double& price_,
                     const double& size_);
    // отменяет все ордера
    void cancel_all_orders(const std::string market_);
    // отменяет ордер по идентфикатору
    void cancel_order(const std::string& order_id_);

  private:
    std::shared_ptr<AsyncHTTPSession> _async_http_client;
    const std::string uri = "ftx.com";
};

}
#endif
