#ifndef GATEWAY_H
#define GATEWAY_H

#include <chrono>
#include <map>

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
#include "src/utils/HTTP.hpp"


//using namespace std;

namespace ftx {

class gateway : public std::enable_shared_from_this<gateway>
{
private:
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
    std::shared_ptr<spdlog::logger>  _logs_logger;
    std::shared_ptr<spdlog::logger>  _pingpong_logger;
    std::shared_ptr<spdlog::logger>  _balances_logger;
    std::shared_ptr<spdlog::logger>  _errors_logger;
    //std::shared_ptr<ftx::AsyncRESTClient> ftx;

    // константы ошибок aeron
    const char* BACK_PRESSURED_DESCRIPTION     = "Offer failed due to back pressure.";
    const char* NOT_CONNECTED_DESCRIPTION      = "Offer failed because publisher is not connected to a core.";
    const char* ADMIN_ACTION_DESCRIPTION       = "Offer failed because of an administration action in the system.";
    const char* PUBLICATION_CLOSED_DESCRIPTION = "Offer failed because publication is closed.";
    const char* UNKNOWN_DESCRIPTION            = "Offer failed due to unknkown reason.";

    // глубина посылаемого ядру ордербука
    int         _sended_orderbook_depth;
    // флаг успешного получения конфига
    bool        _config_was_received = false;


    //https://www.geeksforgeeks.org/implementing-multidimensional-map-in-c/
    /* словарь с рынками
     * идем попорядку:
     * содержит массив рынков (рынок является ключом)
     * значением является map, у которого ключ это массивы "ask" и "bid", а значением являются "стаканы"
     */
    map<string, map<string, map<double, double>, std::greater<string>>>                   _markets_map;
    // итераторы для словаря с рынками
    map<string, map<string, map<double, double>, std::greater<string>>>::const_iterator   _market_itr;
    map<string, map<double, double>>::const_iterator                                      _direct_itr;
    map<double, double>::const_iterator                                                   _asks_itr;
    map<double, double>::const_reverse_iterator                                           _bids_itr;

    // содержит дефолтную конфигурацию
    gate_config                _default_config;
    // содержит рабочую конфигурацию
    gate_config                _work_config;
    //
    boost::asio::io_context    _ioc;
    // содержит ошибки
    bss::error                 _error;
    // счетчик принятых из сокета данных
    uint64_t                   _socket_data_counter;
    // переменная для отслеживания отправки метрик
    std::chrono::time_point<std::chrono::system_clock>  _last_metric_ping_time;
    // переменная для отсечки времени отправки ping-а
    std::chrono::time_point<std::chrono::system_clock>  _last_ping_time;
    // содержит предыдущее успешно отправленное сообщение о балансах в ядро
    std::string                _prev_balance_message_core = "none";
    // содержит предыдущее успешно отправленное сообщение о балансах в лог
    std::string                _prev_balance_message_log  = "none";
    // содержит предыдущее успешно отправленное сообщение об ордербуках
    std::string                _prev_orderbook_message    = "none";
    // содержит предыдущее успешно отправленное сообщение о статусе ордера в ядро
    std::string                _prev_order_status_message_core = "none";
    // содержит предыдущее успешно отправленное сообшение о статусе ордера с лог
    std::string                _prev_order_status_message_log  = "none";
    // содержит предыдущее успешно отправленное сообщение с метрикой
    std::string                _prev_metric_message = "none";

    // callback функция принимает ордербуки с биржы через public WebSocket
    void        public_ws_handler(std::string_view message_, void* id_);
    // callback функция принимает команды от ядра
    void        aeron_handler(std::string_view message_);
    // создаёт канал для отправки запроса на получение конфига и канал для приёма конфига от агента
    bool        create_aeron_agent_channels(bss::error& error_);
    // создаёт каналы для работы с ядром
    bool        create_aeron_core_channels(bss::error& error_);
    // создаёт приватный REST сокет
    bool        create_private_REST(bss::error& error_);
    // создаёт приватный WebSocket
    bool        create_private_ws(bss::error& error_);
    // создаёт публичный WebSocket
    bool        create_public_ws(bss::error& error_);
    // запрос на получение полного конфига  ????
    void        get_full_config_request();
    // принимает конфиг от агента
    void        config_from_agent_handler(std::string_view message_);
    void        private_ws_handler(std::string_view message_, void* id_);
    // callback функция результата асинхронного выставления и отмены оредров
    void        place_order_result_handler(std::string_view message_);
    // обрабатывает и логирует ошибку от каналов aeron
    void        processing_error(std::string_view error_source_, const std::string& prev_messsage_, const std::int64_t& error_code_);
    // создает ордер
    void        create_order(std::string_view side_, const std::string& symbol_, const double& price_, const double& quantity_);
    // отменяет ордер по order_id
    //void        cancel_order(const int64_t& order_id);
    void        cancel_order(const std::string& order_id_);
    // отменяет все ордера
    void        cancel_all_orders();
    // отправляет метрики
    void        metric_sender();
    // подготавливаем стакан
    void        orderbook_prepare(const map<string, map<string, map<double, double>, std::greater<string>>>& markets_map_);
    // отправляем стакан
    void        orderbook_sender(std::string_view orderbook_);
    // подготавливаем json order_status
    void        order_status_prepare(std::string_view action_, std::string_view message_, std::string_view place_result_, bool is_error_ = false, std::string error_ = "");
    // отправляем order_status
    void        order_status_sender(std::string_view order_status_);
    // проверяет баланс
    void        check_balances(/*const bool& start_trigger_ = false*/);
    // отправляет баланс
    void        balance_sender(const std::vector<s_balances_state>& balances_vector_);
    // отправляем ошибки
    //void        error_sender(std::string_view message_);
    // получает более подробную информацию об изменении ордера
    std::string get_order_change_description(std::string_view side_, std::string_view status_, const double& filled_size_, const double& remaining_size_);

public:
    // для получения конфига с сервера
    util::HTTPSession http_session;

    // конструктор
    explicit    gateway(const std::string& config_file_path_);
    // отпраляет запрос на получение конфига
    void        send_config_request();
    // возвращает конфиг с сервера
    bool        get_config_from_api(bss::error& error_);
    // проверяет получен ли конфиг
    bool        has_config();
    // загружает конфиг из файла
    bool        load_config_from_file(bss::error& error_) noexcept;
    // загружет конфиг из json
    bool        load_config_from_json(const std::string& message_, bss::error& error_) noexcept;
    // посылает ошибку в консоль и лог
    void        send_error(std::string_view error_);
    // посылает сообщение в консоль и лог
    void        send_message(std::string_view message_);
    //
    void        pool();
    //
    void        pool_from_agent();
    // подготовка к запуску (создает каналы aeron, сокеты и т.д.)
    bool        preparation_for_launch();
    //
    //void    order_sender(const std::vector<SOrder>& orders_vector_);
    // перезапускает публичный WebSocket
    void        restart_public_ws();
    // перезапускает приватный WebSocket
    void        restart_private_ws(const std::string& reason_);
    //void    restart_private_REST();

    //void create_json();
    // получает источник конфига
    std::string get_config_source();
    // создает каналы для агента
    bool        create_agent_channel();

};
}


#endif // GATEWAY_H
