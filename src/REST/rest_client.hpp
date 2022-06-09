//#pragma once
#ifndef REST_CLIENT_H
#define REST_CLIENT_H

#include "../utils/HTTP.hpp"
#include "../utils/json.hpp"
#include "../utils/simdjson.hpp"
#include <string>
#include "../utils/decl_header.hpp"
#include "../utils/error.hpp"

using JSON = nlohmann::json;

namespace ftx {

class RESTClient
{
  public:
    RESTClient(const std::string api_key_, const std::string api_secret_, const std::string subaccount_name_);

    // получает открытые ордера
    std::vector<s_order> get_open_orders(const std::string market_, bss::error& error_);
    // создет ордер
    std::string place_order(const std::string market_,
                            const std::string side_,
                            //const std::string price,
                            const double price_,
                            //const std::string size,
                            const double size_,
                            bss::error& error_,
                            bool ioc_ = false,
                            bool post_only_ = false,
                            bool reduce_only_ = false);

    // отменяет все ордера
    std::string cancel_all_orders(const std::string market_, bss::error& error_);
    // отменяет ордер по идентификатору
    std::string cancel_order(const std::string& order_id_, bss::error& error_);
    // получает балансы
    std::map<std::string, s_balances_state> get_balances(bss::error& error_);

  private:
    util::HTTPSession _http_client;
    const std::string _uri = "ftx.com";
};

}
#endif
