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
//using fast_json = simdjson::simdjson_result<dom::element>;

namespace ftx {

class RESTClient
{
  public:
    RESTClient(const std::string api_key, const std::string api_secret);

    SCurrencyCharacteristics list_markets(const std::string market, bss::error& error_);

    std::vector<SOrder> get_open_orders(const std::string market, bss::error& error_);
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
                            const std::string price,
                            const std::string size,
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

    std::string cancel_all_orders(const std::string market);
    std::string cancel_order(const std::string& order_id, bss::error& error_);
    //json_loh cancel_order_by_client_id(const std::string client_id);

    std::vector<SBState> get_balances(bss::error& error_);

  private:
    util::HTTPSession http_client;
    const std::string uri = "ftx.com";
    //const std::string api_key = "LKqLwrHgxiueUj8jR7WrkwoRa68BZg7_33YXVxuQ";
    //const std::string api_secret = "UjXvP5_76jFvXl_uEhL28G52t9EeL2GwD884uVFV";
    const std::string subaccount_name = "";
};

}
#endif
