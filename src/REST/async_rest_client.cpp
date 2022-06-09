#include "async_rest_client.hpp"
#include <iostream>
#include <sstream>

namespace ftx
{

    AsyncRESTClient::AsyncRESTClient(const std::string api_key_,
                                     const std::string api_secret_,
                                     const std::string subaccount_name_,
                                     net::io_context& ioc_,
                                     const std::function<void(std::string)>& event_handler_) {

        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12_client};

        ctx.set_default_verify_paths();

        // Verify the remote server's certificate
        ctx.set_verify_mode(ssl::verify_peer);
        _async_http_client = std::make_shared<AsyncHTTPSession>(net::make_strand(ioc_), ctx, api_key_, api_secret_, subaccount_name_, event_handler_);
    }
    void AsyncRESTClient::get_balances() {
        _async_http_client->get("/api/wallet/balances");
    }
    //-----------------------------------------------------------------------
    // выставляет ордер
    //-----------------------------------------------------------------------
    void AsyncRESTClient::place_order(const std::string market_, const std::string side_, const std::string& type_, const double& price_, const double& size_) {
        if (type_ == "limit") {
            JSON payload = {{"market", market_},
                        {"side", side_},
                        {"price", price_},
                        {"type", "limit"},
                        {"size", size_},
                        {"ioc", false},
                        {"postOnly", false},
                        {"reduceOnly", false}};
            _async_http_client->post("/api/orders", payload.dump());
        } else {
            JSON payload = {{"market", market_},
                        {"side", side_},
                        {"price", NULL},
                        {"type", "market"},
                        {"size", size_},
                        {"ioc", false},
                        {"postOnly", false},
                        {"reduceOnly", false}};
            _async_http_client->post("/api/orders", payload.dump());
        }

    }
    //-----------------------------------------------------------------------
    // выставляет ордер
    //-----------------------------------------------------------------------
    void AsyncRESTClient::place_order(const std::string& client_id_, const std::string market_, const std::string side_, const std::string& type_, const double& price_, const double& size_) {
        if (type_ == "limit") {
            JSON payload = {{"market", market_},
                        {"side", side_},
                        {"price", price_},
                        {"type", "limit"},
                        {"size", size_},
                        {"ioc", false},
                        {"postOnly", false},
                        {"reduceOnly", false},
                        {"clientId", client_id_}};
            _async_http_client->post("/api/orders", payload.dump());
        } else {
            JSON payload = {{"market", market_},
                        {"side", side_},
                        {"price", NULL},
                        {"type", "market"},
                        {"size", size_},
                        {"ioc", false},
                        {"postOnly", false},
                        {"reduceOnly", false},
                        {"clientId", client_id_}};
            _async_http_client->post("/api/orders", payload.dump());
        }

    }
    //-----------------------------------------------------------------------
    // отменяет все ордера
    //-----------------------------------------------------------------------
    void AsyncRESTClient::cancel_all_orders(const std::string market_) {
        JSON payload = {{"market", market_}};
        _async_http_client->delete_("/api/orders", payload.dump());
    }
    //-----------------------------------------------------------------------
    // отменяет ордер по идентфикатору
    //-----------------------------------------------------------------------
    void AsyncRESTClient::cancel_order(const std::string &order_id_) {
        _async_http_client->delete_("/api/orders/" + order_id_);
    }
    //-----------------------------------------------------------------------
    // получает статус ордера по идентификатору
    //-----------------------------------------------------------------------
    void AsyncRESTClient::get_order_status(const std::string& order_id_) {
        _async_http_client->get("/api/orders/" + order_id_);
    }
    //-----------------------------------------------------------------------
    // получает статус ордера по клиентскому идентификатору
    //-----------------------------------------------------------------------
    void AsyncRESTClient::get_order_status_by_client_id(const std::string &client_id_) {
        _async_http_client->get("/api/orders/by_client_id/" + client_id_);
    }
}
