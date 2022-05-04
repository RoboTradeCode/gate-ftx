#include "rest_client.hpp"
#include <iostream>
#include <sstream>
namespace ftx {

    RESTClient::RESTClient(const std::string api_key, const std::string api_secret)
    {
        http_client.configure(uri, api_key, api_secret, subaccount_name);
    }

    SCurrencyCharacteristics RESTClient::list_markets(const std::string market, bss::error& error_)
    {
        SCurrencyCharacteristics returnCharacteristics;
        auto response = http_client.get("markets");
        //std::cout<<response.body()<<std::endl;
        //return json::parse(response.body());
        try
        {
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
                        for(simdjson::dom::object characteristic : result_array)
                        {
                            std::string element_success{characteristic["name"].get_c_str()};
                            //std::cout << element_success << std::endl;
                            if(element_success.compare("BTC/USDT") == 0)
                            //if(characteristic["name"].value() == "BTC/USDT")
                            {
                                //std::cout << characteristic["priceIncrement"].get_double() << std::endl;
                                //std::cout << characteristic["sizeIncrement"].get_double() << std::endl;
                                returnCharacteristics.priceIncrement = characteristic["priceIncrement"].get_double();
                                returnCharacteristics.sizeIncrement  = characteristic["sizeIncrement"].get_double();
                            }
                        }
                    }
                    else
                    {
                        //error += "json фрейм содержит \"result\" со значением false";
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
        catch(simdjson::simdjson_error& err)
        {
            //error += err.what();
            //error += " (json: " + response.body() + ").";
            error_.describe(err.what() + fmt::format("(json body: {}).", response.body().c_str()));
        }
        catch(std::exception& ex){
            error_.describe(fmt::format("error в rest private channel: {}", ex.what()));
        }
        return returnCharacteristics;
    }

    std::vector<SOrder> RESTClient::get_open_orders(const std::string market, bss::error& error_)
    {

        std::vector<SOrder> ordersVector;
        http::response<http::string_body> response;
        try{
            response = http_client.get("orders?market=" + market);

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
                            SOrder order;
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
    /*json_loh RESTClient::get_orders_history(const std::string market)
    {
        auto response = http_client.get("orders/history?market=" + market);
        //std::cout<<response.body()<<std::endl;
        return json_loh::parse(response.body());
    }*/

    /*json_loh RESTClient::place_order(const std::string market,
                             const std::string side,
                             const std::string clientId,
                             double price,
                             double size,
                             bool ioc,
                             bool post_only,
                             bool reduce_only)
    {
        json_loh payload = {{"market", market},
                    {"side", side},
                    {"price", price},
                    {"type", "limit"},
                    {"size", size},
                    {"ioc", ioc},
                    {"postOnly", post_only},
                    {"reduceOnly", reduce_only},
                    {"clientId", clientId}};
        auto response = http_client.post("orders", payload.dump());
        //std::cout<<response.body()<<std::endl;
        return json_loh::parse(response.body());
    }*/
    /*json_loh*/
    std::string RESTClient::place_order(const std::string market, const std::string side, const double price, const double size, bss::error& error_, bool ioc, bool post_only, bool reduce_only)
    {
        json_loh payload = {{"market", market},
                    {"side", side},
                    {"price", price},
                    {"type", "limit"},
                    {"size", size},
                    {"ioc", ioc},
                    {"postOnly", post_only},
                    {"reduceOnly", reduce_only}};
        //auto response = http_client.post("orders", payload.dump());
        //std::cout<<response.body()<<std::endl;
        //return json_loh::parse(response.body());
        //return response.body();

        std::string result_response;
        http::response<http::string_body> response;
        try{
            response = http_client.post("orders", payload.dump());
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

/*json_loh RESTClient::place_order(const std::string market,
                             const std::string side,
                             double size,
                             bool ioc,
                             bool post_only,
                             bool reduce_only)
{
    json_loh payload = {{"market", market},
                    {"side", side},
                    {"price", NULL},
                    {"type", "market"},
                    {"size", size},
                    {"ioc", ioc},
                    {"postOnly", post_only},
                    {"reduceOnly", reduce_only}};
    auto response = http_client.post("orders", payload.dump());
    return json_loh::parse(response.body());
}*/
    std::string RESTClient::cancel_all_orders(const std::string market)
    {
        json_loh payload = {{"market", market}};
        auto response = http_client.delete_("orders", payload.dump());
        //return json_loh::parse(response.body());
        return response.body();
    }
    std::string RESTClient::cancel_order(const std::string& order_id, bss::error& error_)
    {

        std::string result_string = "";
        http::response<http::string_body> response;
        try{

            response = http_client.delete_("orders/" + order_id);

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
    /*json_loh RESTClient::cancel_order_by_client_id(const std::string client_id)
    {
        auto response = http_client.delete_("orders/by_client_id/" + client_id);
        std::cout<<response.body()<<std::endl;
        return json_loh::parse(response.body());
    }*/
    /*
    json RESTClient::get_fills()
    {
        auto response = http_client.get("fills");
        return json::parse(response.body());
    }*/

    /*json_loh RESTClient::get_coins()
    {
        auto response = http_client.get("wallet/coins");
        std::cout<<response.body()<<std::endl;
        return json_loh::parse(response.body());
    }*/
    std::vector<SBState> RESTClient::get_balances(/*std::string*/bss::error& error_)
    {
        std::vector<SBState> balancesVector;
        http::response<http::string_body> response;
        try
        {
            response = http_client.get("wallet/balances");
            //response = http_client.get("wallet/all_balances");
            //try
            //{
                //std::cout<<response.body().c_str()<<std::endl;
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
                            for(simdjson::dom::object obj:arr)
                            {
                                SBState balanceState;
                                //std::cout<<obj["coin"]<<" "<<obj["free"]<< " "<<obj["total"]<< ""<< obj["usdValue"]<<std::endl;
                                balanceState.coin     = obj["coin"].value();
                                balanceState.free     = obj["free"].value();
                                balanceState.total    = obj["total"].value();
                                balanceState.usdValue = obj["usdValue"].value();
    //                            balanceState.coin     = "USDT";
    //                            balanceState.free     = 238.36f;
                                balancesVector.push_back(balanceState);
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
