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

//#include "src/spdlog/spdlog.h"
#include <chrono>
#include <map>

using namespace std;
using dec_float = boost::multiprecision::cpp_dec_float_50;

namespace ftx {

class gateway : public std::enable_shared_from_this<gateway>
{
    // каналы AERON для агента
    std::shared_ptr<Subscriber>     _subscriber_agent_channel;
    std::shared_ptr<Publisher>      _publisher_agent_channel;
    std::shared_ptr<Publisher>      _publisher_logs_channel;

    // каналы AERON для ядра
    std::shared_ptr<Publisher>      _orderbook_channel;
    std::shared_ptr<Publisher>      _balance_channel;
    std::shared_ptr<Publisher>      _order_status_channel;
    std::shared_ptr<Subscriber>     _core_channel;

    // канал AERON для лог сервера
    std::shared_ptr<Publisher>      _log_channel;

    // каналы FTX
    std::shared_ptr<ftx::WSClient>   _ftx_ws_public;
    std::shared_ptr<ftx::WSClient>   _ftx_ws_private;
    std::shared_ptr<ftx::RESTClient> _ftx_rest_private;

    // логгеры
    std::shared_ptr<spdlog::logger>  _general_logger;
    std::shared_ptr<spdlog::logger>  _ping_pong_logger;
    std::shared_ptr<spdlog::logger>  _orderbook_logger;
    std::shared_ptr<spdlog::logger>  _balances_logger;
    std::shared_ptr<spdlog::logger>  _errors_logger;
    //std::shared_ptr<ftx::AsyncRESTClient> ftx;

    const char* BACK_PRESSURED_DESCRIPTION     = "Offer failed due to back pressure.";
    const char* NOT_CONNECTED_DESCRIPTION      = "Offer failed because publisher is not conntected to a core.";
    const char* ADMIN_ACTION_DESCRIPTION       = "Offer failed because of an administration action in the system.";
    const char* PUBLICATION_CLOSED_DESCRIPTION = "Offer failed because publication is closed.";
    const char* UNKNOWN_DESCRIPTION            = "Offer failed due to unknkown reason.";

    bool        _config_was_received = false;
    //------- для отладки  ---------------------
    int                             ws_control = 0;
    bool                            start_trigger = false;
    //---------------------------------------------------
    //https://www.geeksforgeeks.org/implementing-multidimensional-map-in-c/
    //std::map<double, double, std::greater<double>>  _bids_map;
    //std::map<std::string, std::map<double, double, std::greater<double>>>          _bids_map;
    //std::map<std::string, std::map<double, double>>          _asks_map;

    /* идем попорядку:
     * содержит массив рынков (рынок является ключом)
     * значением является map, у которого ключ это массивы "ask" и "bid", а значением являются "стаканы"
     *
     */
    map<string, map<string, map<double, double>, std::greater<string>>>                   _markets_map;

    // содержит дефолтную конфигурацию
    gate_config                     _default_config;
    // содержит рабочую конфигурацию
    gate_config                     _work_config;
    boost::asio::io_context         ioc;
    bss::error                      _error;
    STicker                         prev_ticker;
    SCurrencyCharacteristics        curr_characters;
    std::filesystem::path           _path;
    // переменная для отсечки времени отправки ping-а
    std::chrono::time_point<std::chrono::system_clock>  _last_ping_time;

    void        public_ws_handler(std::string_view message_, void* id_);
    void        aeron_handler(std::string_view message_);
    // создаёт канал для приёма конфига от агента
    bool        create_agent_channel(bss::error& error_);
    // запрос на получение полного конфига  ????
    void        get_full_config_request();
    // принимает конфиг от агента
    void        config_from_agent_handler(std::string_view message_);
    void        private_ws_handler(std::string_view message_, void* id_);
    // callback функция результата выставления оредров
    void        place_order_result_handler(std::string_view message_);
    // обрабатывает и логирует ошибку от каналов aeron
    void        processing_error(std::string_view error_source_, const std::int64_t& error_code_);
    // обрабатывает ордер на покупку
    //void        buy_order(std::string_view price_, std::string_view quantity_);
    void        buy_order(const std::string& symbol, const double& price, const double& quantity);
    // обрабатывает отмену ордера на покупку
    void        cancel_buy_order(const int64_t& order_id);
    // обрабатывает ордер на продажу
    //void        sell_order(std::string_view price_, std::string_view quantity_);
    void        sell_order(const std::string& symbol, const double& price_, const double& quantity_);
    // обрабатывает отмену ордера на продажу
    void        cancel_sell_order(const int64_t& order_id);
    // обрабатывает отмену ордера по order_id
    void        cancel_order(const int64_t& order_id);
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

    void    initialization();
    // отпраляет запрос на получение конфига
    void    send_config_request();
    // проверяет получен ли конфиг
    bool    has_config();
    // загружает конфиг из файла
    bool    load_config(bss::error& error_) noexcept;
    // посылает ошибку в консоль и лог
    void    send_error(std::string_view error_);
    //
    bool    create_aeron_channel();
    //
    void    create_public_ws();
    //
    void    create_private_ws();
    void    create_private_REST();
    //
    void    pool();
    //
    void    pool_from_agent();
    // подготовка к запуску (создает каналы aeron, сокеты и т.д.)
    bool    prepare();
    // подготавливаем стакан
    void    orderbook_prepare(const map<string, map<string, map<double, double>, std::greater<string>>>& markets_map_);
    // отправляем стакан
    void    orderbook_sender(std::string_view orderbook_);
    // подготавливаем json order_status
    void    order_status_prepare(std::string_view action_, std::string_view message_, std::string_view place_result, bool is_error = false, std::string error_ = "");
    // отправляем order_status
    void    order_status_sender(std::string_view order_status_);
    // проверяет баланс
    void    check_balance(/*const bool& start_trigger_ = false*/);
    //
    void    balance_sender(const std::vector<SBState>& balances_vector_);
    //
    void    order_sender(const std::vector<SOrder>& orders_vector_);
    //
    void    restart_public_ws();
    //
    void    restart_private_ws(const std::string& reason_);
    //void    restart_private_REST();

    void create_json();

};
}


#endif // GATEWAY_H
