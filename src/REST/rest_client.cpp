#include "rest_client.hpp"
#include <iostream>
#include <sstream>
namespace ftx {

    RESTClient::RESTClient(const std::string api_key_, const std::string api_secret_)
    {
        _http_client.configure(_uri, api_key_, api_secret_);
    }

    //----------------------------------------------------------
    // получает открытые ордера
    //----------------------------------------------------------
    std::vector<s_order> RESTClient::get_open_orders(const std::string market_, bss::error& error_)
    {

        std::vector<s_order> ordersVector;
        http::response<http::string_body> response;
        try{
            response = _http_client.get("orders?market=" + market_);

            //std::cout << "открытые ордера: " << response.body() << std::endl;
            simdjson::dom::parser parser;
            auto &&error_code = parser.allocate(0x1000,0x04);
            if(simdjson::SUCCESS == error_code)
            {
                auto result = parser.parse(response.body().c_str(), response.body().size(), false);
                if(simdjson::SUCCESS == result.error())
                {
                    auto element_success{result["success"].get_bool()};
                    if(element_success.value() == true)
                    {
                        auto result_array = result["result"];
                        for(simdjson::dom::object order_unit : result_array)
                        {
                            s_order order;
                            order.createdAt = order_unit["createdAt"].value();
                            order.id        = order_unit["id"].value();
                            order.price     = order_unit["price"].value();
                            order.side      = order_unit["side"].value();
                            order.status    = order_unit["status"].value();
                            order.type      = order_unit["type"].value();
                            //order.clientId  = order_unit["clientId"].value();
                            ordersVector.push_back(order);
                        }
                    }
                    else
                    {
                        //error += "json фрейм содержит \"result\" со значением false (get_open_orders)";
                        //error += " (json: " + response.body() + ").";
                        error_.describe(fmt::format("json фрейм содержит \"result\" со значением false. (json body: {}).",
                                        response.body().c_str()));
                    }
                }
                else
                {
                    //error += "Ошибка разбора json фрейма (get_open_orders).";
                    //error += " (json: " + response.body() + ").";
                    error_.describe(fmt::format("Ошибка разбора json фрейма. (json body: {}).",
                                                response.body().c_str()));
                }
            }
            else
            {
                //error += "Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)) (get_open_orders).";
                //error += " (json: " + response.body() + ").";
                error_.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). (json body: {})).",
                                response.body().c_str()));
            }
        }
        catch(simdjson::simdjson_error& err){
            //error += err.what();
            //error += " (json: " + response.body() + ") (get_open_orders).";
            error_.describe(err.what() + fmt::format("(json body: {}).", response.body().c_str()));
        }
        catch(std::exception& ex){
            //error += "error в rest private channel (get_open_orders): ";
            //error += ex.what();
            //throw std::runtime_error("on_read: private REST");
            error_.describe(fmt::format("error в rest private channel (get_open_orders): {}", ex.what()));
        }
        return ordersVector;
    }
    //----------------------------------------------------------
    // создает ордер
    //----------------------------------------------------------
    std::string RESTClient::place_order(const std::string market_,
                                        const std::string side_,
                                        const double price_,
                                        const double size_,
                                        bss::error& error_,
                                        bool ioc_,
                                        bool post_only_,
                                        bool reduce_only_)
    {
        json_loh payload = {{"market",      market_},
                            {"side",        side_},
                            {"price",       price_},
                            {"type",        "limit"},
                            {"size",        size_},
                            {"ioc",         ioc_},
                            {"postOnly",    post_only_},
                            {"reduceOnly",  reduce_only_}};

        std::string result_response;
        http::response<http::string_body> response;
        try{
            response = _http_client.post("orders", payload.dump());
            result_response = response.body();
        }
        catch(simdjson::simdjson_error& err){
            //error += err.what();
            //error += " (json: " + response.body() + ") (place_order).";
            error_.describe(err.what() + fmt::format(" (json body: {}).", response.body().c_str()));
        }
        catch(std::exception& ex){
            //error += "error в rest private channel (place_order): ";
            //error += ex.what();
            //throw std::runtime_error("on_read: private REST");
            error_.describe(fmt::format("error в rest private channel (place_order): {}", ex.what()));
        }
        return result_response;
    }

    //----------------------------------------------------------
    // отменяет все ордера
    //----------------------------------------------------------
    std::string RESTClient::cancel_all_orders(const std::string market_, bss::error& error_)
    {
        std::string result_string = "";
        http::response<http::string_body> response;
        json_loh payload = {{"market", market_}};
        try {
            response = _http_client.delete_("orders", payload.dump());
            simdjson::dom::parser parser;
            auto &&error_code = parser.allocate(0x1000,0x04);
            if(simdjson::SUCCESS == error_code) {
                auto result = parser.parse(response.body().c_str(), response.body().size(), false);
                if(simdjson::SUCCESS == result.error()) {
                    auto element_success{result["success"].get_bool()};
                    // если success == true, то вернем результат
                    if(element_success.value() == true) {
                        result_string = result["result"].get_c_str();
                    } else {
                        error_.describe(fmt::format("json фрейм содержит \"result\" со значением false. (json body: {}).",
                                                    response.body().c_str()));
                        result_string = result["error"].get_c_str();
                    }
                } else {
                    error_.describe(fmt::format("Ошибка разбора json фрейма. (json body: {}).",
                                                response.body().c_str()));
                }
            } else {
                error_.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). (json body: {}).",
                                            response.body().c_str()));
            }
        }
        catch(simdjson::simdjson_error& err){
            error_.describe(err.what() + fmt::format("(json body: {}).", response.body().c_str()));
        }
        catch(std::exception& ex){
            error_.describe(fmt::format("error в rest private channel: {}", ex.what()));
        }
        return result_string;
    }
    //----------------------------------------------------------
    // отменяет ордер по идентификатору
    //----------------------------------------------------------
    std::string RESTClient::cancel_order(const std::string& order_id_, bss::error& error_)
    {

        std::string result_string = "";
        http::response<http::string_body> response;
        try{

            response = _http_client.delete_("orders/" + order_id_);

            simdjson::dom::parser parser;
            auto &&error_code = parser.allocate(0x1000,0x04);
            if(simdjson::SUCCESS == error_code)
            {
                auto result = parser.parse(response.body().c_str(), response.body().size(), false);
                if(simdjson::SUCCESS == result.error())
                {
                    auto element_success{result["success"].get_bool()};
                    if(element_success.value() == true)
                    {
                        result_string = result["result"].get_c_str();
                    }
                    else
                    {
                        //error += "json фрейм содержит \"result\" со значением false (cancel_order)";
                        //error += " (json: " + response.body() + ").";
                        error_.describe(fmt::format("json фрейм содержит \"result\" со значением false. (json body: {}).",
                                                    response.body().c_str()));
                    }
                }
                else
                {
                    //error += "Ошибка разбора json фрейма.";
                    //error += " (json: " + response.body() + ").";
                    error_.describe(fmt::format("Ошибка разбора json фрейма. (json body: {}).",
                                                response.body().c_str()));
                }
            }
            else
            {
                //error += "Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).";
                //error += " (json: " + response.body() + ").";
                error_.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). (json body: {}).",
                                            response.body().c_str()));
            }
        }
        catch(simdjson::simdjson_error& err){
            //error += err.what();
            //error += " (json: " + response.body() + ") (cancel_order).";
            error_.describe(err.what() + fmt::format("(json body: {}).", response.body().c_str()));
        }
        catch(std::exception& ex){
            //error += "error в rest private channel (cancel_order): ";
            //error += ex.what();
            //throw std::runtime_error("on_read: private REST");
            error_.describe(fmt::format("error в rest private channel: {}", ex.what()));
        }
        return result_string;
    }
    //----------------------------------------------------------
    // получает балансы
    //----------------------------------------------------------
    std::vector<s_balances_state> RESTClient::get_balances(bss::error& error_)
    {
        std::vector<s_balances_state> balancesVector;
        http::response<http::string_body> response;
        try
        {
            response = _http_client.get("wallet/balances");
            //response = http_client.get("wallet/all_balances");
            //try
            //{
                std::cout<<"get_balances: " << response.body().c_str()<<std::endl;
                simdjson::dom::parser parser;
                auto &&error_code = parser.allocate(0x1000,0x04);
                if(simdjson::SUCCESS == error_code)
                {
                    auto result = parser.parse(response.body().c_str(), response.body().size(), false);
                    if(simdjson::SUCCESS == result.error())
                    {
                        auto element_success{result["success"].get_bool()};
                        if(element_success.value() == true)
                        {
                            auto arr = result["result"];
                            //std::cout<<arr<<std::endl;
                            /*for(simdjson::dom::object obj:arr)
                            {
                                s_balances_state balanceState;
                                //std::cout<<obj["coin"]<<" "<<obj["free"]<< " "<<obj["total"]<< ""<< obj["usdValue"]<<std::endl;
                                balanceState.coin     = obj["coin"].value();
                                balanceState.free     = obj["free"].value();
                                balanceState.total    = obj["total"].value();
                                balanceState.usdValue = obj["usdValue"].value();
                                balancesVector.push_back(balanceState);
                            }*/
                            if (auto result_array{result["result"].get_array()}; simdjson::SUCCESS == result_array.error()) {
                                for (auto balance : result_array) {
                                    s_balances_state balanceState;
                                    if (auto coin_element{balance["coin"].get_string()}; simdjson::SUCCESS == coin_element.error()) {
                                        balanceState.coin = coin_element.value();
                                    }
                                    if (auto free_element{balance["free"].get_double()}; simdjson::SUCCESS == free_element.error()) {
                                        balanceState.free = free_element.value();
                                    }
                                    if (auto total_element{balance["total"].get_double()}; simdjson::SUCCESS == total_element.error()) {
                                        balanceState.total = total_element.value();
                                    }
                                    if (auto usdValue_element{balance["usdValue"].get_double()}; simdjson::SUCCESS == usdValue_element.error()) {
                                        balanceState.usdValue = usdValue_element.value();
                                    }
                                    balancesVector.push_back(balanceState);
                                }

                            }
                        }
                        else
                        {
                            //error += "json фрейм содержит \"result\" со значением false (get_balances)";
                            //error += " (json: " + response.body() + ").";

                            error_.describe(fmt::format("json фрейм содержит \"result\" со значением false. (json body: {}).",
                                            response.body().c_str()));
                        }
                    }
                    else
                    {
                        //error += "Ошибка разбора json фрейма.";
                        //error += " (json: " + response.body() + ").";
                        error_.describe(fmt::format("Ошибка разбора json фрейма. (json body: {}).",
                                                    response.body().c_str()));
                    }
                }
                else
                {
                    //error += "Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).";
                    //error += " (json: " + response.body() + ").";
                    error_.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). (json body: {})).",
                                    response.body().c_str()));
                }
        }
        catch(simdjson::simdjson_error& err){
            //error += err.what();
            //error += " (json: " + response.body() + ") (get_balances).";
            error_.describe(err.what() + fmt::format("(json body: {}).", response.body().c_str()));
        }
        catch(std::exception& ex){
            //error += "error в rest private channel (get_balances): ";
            //error += ex.what();
            //throw std::runtime_error("on_read: private REST");
            error_.describe(fmt::format("error в rest private channel (get_balances): {}", ex.what()));
        }
        return balancesVector;
    }
}
