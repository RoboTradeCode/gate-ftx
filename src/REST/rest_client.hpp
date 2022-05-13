//#pragma once
#ifndef REST_CLIENT_H
#define REST_CLIENT_H

#include "../utils/HTTP.hpp"
#include "../utils/json.hpp"
#include "../utils/simdjson.hpp"
#include <string>
#include "../utils/decl_header.hpp"
#include "../utils/error.hpp"

using json_loh = nlohmann::json;

namespace ftx {

class RESTClient
{
  public:
    RESTClient(const std::string api_key, const std::string api_secret);

    //SCurrencyCharacteristics list_markets(const std::string market, bss::error& error_);

    std::vector<s_order> get_open_orders(const std::string market, bss::error& error_);
    //json_loh get_orders_history(const std::string market);

    /*json_loh place_order(const std::string market,
                     const std::string side,
                     const std::string clientId,
                     double price,
                     double size,
                     bool ioc = false,
                     bool post_only = false,
                     bool reduce_only = false);*/

    std::string place_order(const std::string market,
                            const std::string side,
                            //const std::string price,
                            const double price_,
                            //const std::string size,
                            const double size_,
                            bss::error& error_,
                            bool ioc = false,
                            bool post_only = false,
                            bool reduce_only = false);

    // Market order overload
    /*json_loh place_order(const std::string market,
                     const std::string side,
                     double size,
                     bool ioc = false,
                     bool post_only = false,
                     bool reduce_only = false);*/

    std::string cancel_all_orders(const std::string market, bss::error& error_);
    std::string cancel_order(const std::string& order_id, bss::error& error_);
    //json_loh cancel_order_by_client_id(const std::string client_id);

    std::vector<s_balances_state> get_balances(bss::error& error_);

  private:
    util::HTTPSession http_client;
    const std::string uri = "ftx.com";
    const std::string subaccount_name = "";
};

}
#endif
