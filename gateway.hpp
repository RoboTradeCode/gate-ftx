#ifndef GATEWAY_H
#define GATEWAY_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include "src/AERON/Publisher.hpp"
#include "src/AERON/Subscriber.hpp"

#include "src/WS/ws_client.hpp"
#include "src/REST/rest_client.hpp"
#include "src/REST/async_rest_client.hpp"
#include "src/config/config.hpp"
#include "src/utils/error.hpp"
#include <chrono>
#include <map>

using namespace std;
using dec_float = boost::multiprecision::cpp_dec_float_50;

class gateway : public std::enable_shared_from_this<gateway>
{
    // каналы AERON
    std::shared_ptr<Publisher>      orderbook_channel;
    std::shared_ptr<Publisher>      balance_channel;
    std::shared_ptr<Publisher>      log_channel;
    std::shared_ptr<Subscriber>     core_channel;

    // каналы FTX
    std::shared_ptr<ftx::WSClient>   ftx_ws_public;
    std::shared_ptr<ftx::WSClient>   ftx_ws_private;
    std::shared_ptr<ftx::RESTClient> ftx_rest_private;
    //std::shared_ptr<ftx::AsyncRESTClient> ftx;

    //------- для отладки зависания ---------------------
    int                             ws_control = 0;
    bool                            start_trigger = false;
    //---------------------------------------------------
    std::map<double, double, std::greater<double>>   _bids_map;
    std::map<double, double>          _asks_map;
    core_config                     config;
    boost::asio::io_context         ioc;
    bss::error                      _error;
    STicker                         prev_ticker;
    SCurrencyCharacteristics        curr_characters;
    std::filesystem::path           _path;
    std::chrono::time_point<std::chrono::system_clock>  last_ping_time;

    void        public_ws_handler(std::string_view message_, void* id_);
    void        aeron_handler(std::string_view message_);
    void        private_ws_handler(std::string_view message_, void* id_);
    // callback функция результата выставления оредров
    void        place_order_result_handler(std::string_view message_);
    // обрабатывает ордер на покупку
    void        buy_order(std::string_view price_, std::string_view quantity_);
    // обрабатывает отмену ордера на покупку
    void        cancel_buy_order();
    // обрабатывает ордер на продажу
    void        sell_order(std::string_view price_, std::string_view quantity_);
    // обрабатывает отмену ордера на продажу
    void        cancel_sell_order();
    // отправляем ошибки
    void        error_sender(std::string_view message_);
    // получает более подробную информацию об изменении ордера
    std::string get_order_change_description(std::string_view side, std::string_view status_, const double& filled_size_, const double& remaining_size_);

    void        get_precision(SCurrencyCharacteristics& curr_characteristic_);
    int         get_precision(double value_);
    std::string set_size_precision(std::string value_);
    std::string set_price_precision(std::string value_);
public:
    explicit gateway(const std::string& config_file_path_);

    bool    create_aeron_channel();
    //
    void    create_public_ws();
    //
    void    create_private_ws();
    void    create_private_REST();
    //
    void    pool();
    //
    void    ticker_sender(const STicker& best_ticker_, void* id_);
    // проверяет баланс
    void    check_balance(/*const bool& start_trigger_ = false*/);
    //
    void    balance_sender(const std::vector<SBState>& balances_vector_);
    //
    void    order_sender(const std::vector<SOrder>& orders_vector_);
    //
    void    processing_error(std::string_view message_, const std::int64_t& error_code_);
    //
    void    restart_public_ws();
    //
    void    restart_private_ws(const std::string& reason_);
    //void    restart_private_REST();

};

#endif // GATEWAY_H
