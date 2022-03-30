//#pragma once
#ifndef TEST_ASYNC_REST_CLIENT_H
#define TEST_ASYNC_REST_CLIENT_H

#include <boost/beast/core.hpp>
#include "../utils/ASyncHTTP.hpp"
//#include "../utils/session.h"
#include "../utils/json.hpp"
//#include "../utils/simdjson.h"
#include <string>
#include <functional>
//#include "../utils/decl_header.h"
//#include <logging.h>

using json_loh = nlohmann::json;
//using fast_json = simdjson::simdjson_result<dom::element>;
namespace net = boost::asio;
namespace ftx {

class AsyncRESTClient
{
  public:
    AsyncRESTClient(const std::string api_key, const std::string api_secret, net::io_context& ioc, const std::function<void(std::string)>& event_handler);

    void get_balances();

    void place_order(const std::string market,
                     const std::string side,
                     const std::string price,
                     const std::string size);

  private:
    std::shared_ptr<AsyncHTTPSession> async_http_client;
    const std::string uri = "ftx.com";
    //net::io_context _ioc;
    //const std::string api_key = "LKqLwrHgxiueUj8jR7WrkwoRa68BZg7_33YXVxuQ";
    //const std::string api_secret = "UjXvP5_76jFvXl_uEhL28G52t9EeL2GwD884uVFV";
};

}
#endif
