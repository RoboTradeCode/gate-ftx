#include "async_rest_client.hpp"
#include <iostream>
#include <sstream>



namespace ftx
{

    AsyncRESTClient::AsyncRESTClient(const std::string api_key, const std::string api_secret,
                                     net::io_context& ioc, const std::function<void(std::string)>& event_handler)

    {

        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12_client};

        ctx.set_default_verify_paths();

        // Verify the remote server's certificate
        ctx.set_verify_mode(ssl::verify_peer);
        async_http_client = std::make_shared<AsyncHTTPSession>(net::make_strand(ioc), ctx, api_key, api_secret, event_handler);
    }
    void AsyncRESTClient::get_balances()
    {
        async_http_client->get("/api/wallet/balances");
    }
    void AsyncRESTClient::place_order(const std::string market, const std::string side, const std::string price, const std::string size)
    {
        json_loh payload = {{"market", market},
                    {"side", side},
                    {"price", price},
                    {"type", "limit"},
                    {"size", size},
                    {"ioc", false},
                    {"postOnly", false},
                    {"reduceOnly", false}};
        async_http_client->post("/api/orders", payload.dump());
    }
}
