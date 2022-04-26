#include "gateway.hpp"
//#include "src/spdlog/spdlog.h"

namespace pt = boost::property_tree;
using namespace aeron::util;
using namespace aeron;
/*
logger_type ftxgateway_logger(keywords::channel = "ftxGateway");
logger_type orderbook_logger(keywords::channel = "ftxOrderBook");
logger_type ping_pong_logger(keywords::channel = "ftxPingPong");
using boost::multiprecision::cpp_dec_float_50;
*/
namespace ftx {
gateway::gateway(const std::string& config_file_path_)
 : _general_logger(spdlog::get("general")),
   _ping_pong_logger(spdlog::get("pingpong")),
   _orderbook_logger(spdlog::get("orderbooks"))
{

    // установим имя файла, при появлении которого необходимо отправить баланс
    _path = "balance";

    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Starting";
    _general_logger->info("Starting...");
    // получаем дефолтную конфигурацию конфигурацию
    _default_config = parse_config(config_file_path_);
    bss::error error;
    // создаём канал агента
  /*  if (!create_agent_channel(error)) {
        error.describe("Ошибка создание канала получения конфигурации.");
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << error.to_string();
        _general_logger->error(error.to_string());
        error.clear();
    } else {}
    get_full_config_request();
*/
    load_config(error);
    // запоминаем время для отправки пинга
    _last_ping_time = std::chrono::system_clock::now();
    // создаём каналы aeron, websocket и rest
    create_aeron_channel();
    create_private_REST();
    create_private_ws();
    create_public_ws();

    // проверяем и отправляем баланс
    check_balance();
    // получаем характеристики валютной пары
/*    curr_characters = ftx_rest_private->list_markets("BTC/USDT", _error);
    if(_error){
        _error.describe("Ошибка получения характеристик валютной пары (list_markets)");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _error.clear();
    }

    get_precision(curr_characters);

    // получаем открытые ордера
    std::vector<SOrder> orders_vector = ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(orders_vector.size() != 0)
    {
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "есть открытые ордера";
        // отправляем ордера в ядро
        order_sender(orders_vector);
        // и потом отменяем их
        std::string cancel_result = ftx_rest_private->cancel_all_orders("BTC/USDT");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << cancel_result;
    }
    else
    {
        if(_error){
            _error.describe("Ошибка получения информации об открытых оредерах (get_open_orders).");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << _error.to_string();
            _error.clear();
        }
        else{
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет открытых ордеров.";
        }
    }*/
}
//--------------------------------------------------------
// инициализация
//--------------------------------------------------------
void gateway::initialization()
{
    // ждем получения конфига 30 секунд (эта хрень временно)
    int waiter_count = 0;
    while (waiter_count < 300) {
        if (_subscriber_agent_channel)
            _subscriber_agent_channel->poll();
        ioc.run_for(std::chrono::microseconds(100));
        std::this_thread::sleep_for(100ms);
        ++waiter_count;
    }
    // если конфиг не был получен
    if (_config_was_received == false) {
        // то загрузим его из файла
    }
}
//--------------------------------------------------------
// создаёт канал для приёма конфига от агента
//--------------------------------------------------------
bool gateway::create_agent_channel(bss::error& error_)
{
    bool result = true;
    bss::error error;
    try {
        // установка соединения с каналом aeron, в котором будем "ловить" полный конфиг
        _subscriber_agent_channel = std::make_shared<Subscriber>([&](std::string_view message)
        {shared_from_this()->config_from_agent_handler(message);},
                    _default_config.aeron_agent.subscribers.agent.channel,
                    _default_config.aeron_agent.subscribers.agent.stream_id);

        // установка соединения с каналом aeron, в котором будем запрашивать полный конфиг
        _publisher_agent_channel = std::make_shared<Publisher>(_default_config.aeron_agent.publishers.agent.channel,
                                                              _default_config.aeron_agent.publishers.agent.stream_id);
        // установка соединения с каналом aeron, в который будем посылать логи работы
        _publisher_logs_channel  = std::make_shared<Publisher>(_default_config.aeron_agent.publishers.logs.channel,
                                                              _default_config.aeron_agent.publishers.logs.stream_id);
    } catch (std::exception& err) {
        // добавляем описание ошибки
        error_.describe(err.what());
        return false;
    }

    /*if (load_config(error); not error) {
        std::cout << "Конфигурация прочитана" << std::endl;
    } else {
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << error.to_string();
        _general_logger->error(error.to_string());
        error.clear();
    }*/
    return result;
}
//---------------------------------------------------------------
// загружает конфиг из файла
//---------------------------------------------------------------
void gateway::load_config(bss::error& error_) noexcept
{
    // создаем парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд (если будет меньше 0x08, то будет ошибка)
    auto &&error_code = parser.allocate(0x1000, 0x08);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        auto load_result = simdjson::padded_string::load("config.json");
        // если файл загрузили
        if (simdjson::SUCCESS == load_result.error()) {
            auto result = parser.parse(load_result.value().data(), load_result.value().size(), false);
            if (simdjson::SUCCESS == result.error()) {
                if (auto is_new_element{result["is_new"].get_bool()}; simdjson::SUCCESS == is_new_element.error()) {
                    if (is_new_element.value() == true) {
                        std::cout << "Конфигурация обновилась." << std::endl;
                    }
                } else {
                    error_.describe("При загрузке конфигурации в теле json не найден оъект is_new.");
                    // скорее всего дальше незачем парсить
                    return ;
                }
                // получим часть пути для сокращения полного пути до элементов
                if (auto cfg = result["data"]["gate_config"]; simdjson::SUCCESS == cfg.error()) {
                    // получаем все необходимые элементы
                    if (auto name_element{cfg["exchange"]["name"].get_string()}; simdjson::SUCCESS == name_element.error()){
                        _work_config.exchange.name = name_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"name\".");
                    }
                    if (auto instance_element{cfg["exchange"]["instance"].get_int64()}; simdjson::SUCCESS == instance_element.error()) {
                        _work_config.exchange.instance = instance_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"instance\".");
                    }
                    if (auto api_key_element{cfg["account"]["api_key"].get_string()}; simdjson::SUCCESS == api_key_element.error()) {
                        _work_config.account.api_key = api_key_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"api_key\".");
                    }
                    if (auto secret_key_element{cfg["account"]["secret_key"].get_string()}; simdjson::SUCCESS == secret_key_element.error()) {
                        _work_config.account.secret_key = secret_key_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"secret_key\".");
                    }
                    if (auto orderbook_channel_element{cfg["aeron"]["publishers"]["orderbook"]["channel"].get_string()}; simdjson::SUCCESS == orderbook_channel_element.error()) {
                        _work_config.aeron_core.publishers.orderbook.channel = orderbook_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook channel\".");
                    }
                    if (auto orderbook_stream_element{cfg["aeron"]["publishers"]["orderbook"]["stream_id"].get_int64()}; simdjson::SUCCESS == orderbook_stream_element.error()) {
                        _work_config.aeron_core.publishers.orderbook.stream_id = orderbook_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook stream_id\".");
                    }
                    if (auto balance_channel_element{cfg["aeron"]["publishers"]["balance"]["channel"].get_string()}; simdjson::SUCCESS == balance_channel_element.error()) {
                        _work_config.aeron_core.publishers.balance.channel = balance_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"balance channel\".");
                    }
                    if (auto balance_stream_element{cfg["aeron"]["publishers"]["balance"]["stream_id"].get_int64()}; simdjson::SUCCESS == balance_stream_element.error()) {
                        _work_config.aeron_core.publishers.balance.stream_id = balance_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"balance stream_id\".");
                    }
                    if (auto log_channel_element{cfg["aeron"]["publishers"]["log"]["channel"].get_string()}; simdjson::SUCCESS == log_channel_element.error()) {
                        _work_config.aeron_core.publishers.logs.channel = log_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"log channel\".");
                    }
                    if (auto log_stream_element{cfg["aeron"]["publishers"]["log"]["stream_id"].get_int64()}; simdjson::SUCCESS == log_stream_element.error()) {
                        _work_config.aeron_core.publishers.logs.stream_id = log_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"log stream_id\".");
                    }
                    // получаем данные для канала статуса ордеров
                    if (auto order_status_channel_element{cfg["aeron"]["publishers"]["order_status"]["channel"].get_string()}; simdjson::SUCCESS == order_status_channel_element.error()) {
                        _work_config.aeron_core.publishers.order_status.channel = order_status_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"order_status channel\".");
                    }
                    if (auto order_status_stream_element{cfg["aeron"]["publishers"]["order_status"]["stream_id"].get_int64()}; simdjson::SUCCESS == order_status_stream_element.error()) {
                        _work_config.aeron_core.publishers.order_status.stream_id = order_status_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"order_status stream_id\".");
                    }
                    // получаем данные для канала команд
                    if (auto core_channel_element{cfg["aeron"]["subscribers"]["core"]["channel"].get_string()}; simdjson::SUCCESS == core_channel_element.error()) {
                        _work_config.aeron_core.subscribers.core.channel = core_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"core channel\".");
                    }
                    if (auto core_stream_element{cfg["aeron"]["subscribers"]["core"]["stream_id"].get_int64()}; simdjson::SUCCESS == core_stream_element.error()) {
                        _work_config.aeron_core.subscribers.core.stream_id = core_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"core stream_id\".");
                    }
                } else {
                    error_.describe("При загрузке конфигурации в теле json не найден оъект [\"data\"][\"gate_config\"].");
                }
            } else {
                error_.describe(fmt::format("Ошибка разбора json фрейма {}.", result.error()));
            }
        } else {
            error_.describe("Ошибка загрузки файла конфигурации.");
        }
    } else {
        error_.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
    }
}
//---------------------------------------------------------------
//
//---------------------------------------------------------------
bool gateway::create_aeron_channel()
{
    bool result = true;
    try{
        // установка соединений с каналом aeron
        _balance_channel      = std::make_shared<Publisher>(_work_config.aeron_core.publishers.balance.channel,      _work_config.aeron_core.publishers.balance.stream_id);
        _orderbook_channel    = std::make_shared<Publisher>(_work_config.aeron_core.publishers.orderbook.channel,    _work_config.aeron_core.publishers.orderbook.stream_id);
        _log_channel          = std::make_shared<Publisher>(_work_config.aeron_core.publishers.logs.channel,         _work_config.aeron_core.publishers.logs.stream_id);
        _order_status_channel = std::make_shared<Publisher>(_work_config.aeron_core.publishers.order_status.channel, _work_config.aeron_core.publishers.order_status.stream_id);
        _core_channel         = std::make_shared<Subscriber>([&](std::string_view message)
                {shared_from_this()->aeron_handler(message);},
                _work_config.aeron_core.subscribers.core.channel, _work_config.aeron_core.subscribers.core.stream_id);
    }
    catch(std::exception& err){
        std::cout << err.what() << std::endl;
        result = false;
    }
    return result;
}
//---------------------------------------------------------------
// создаёт публичный WS
//---------------------------------------------------------------
void gateway::create_public_ws()
{
    _ftx_ws_public    = std::make_shared<ftx::WSClient>("", "", ioc, [&](std::string_view message_, void* id_)
                            {shared_from_this()->public_ws_handler(message_, id_);});
    // подписываемся на публичный поток ticker
    //ftx_ws_public->subscribe_ticker("BTC/USDT");
    //ftx_ws_public->subscribe_ticker("ETH/USDT");
    // подписываемся на канал ордербуков
    _ftx_ws_public->subscribe_orderbook("BTC/USDT");
    //ftx_ws_public->subscribe_orderbook("ETH/USDT");
}
//---------------------------------------------------------------
// создаёт приватный WS
//---------------------------------------------------------------
void gateway::create_private_ws()
{
    _ftx_ws_private   = std::make_shared<ftx::WSClient>(_work_config.account.api_key, _work_config.account.secret_key, ioc, [&](std::string_view message_, void* id_)
                            {shared_from_this()->private_ws_handler(message_, id_);});
    // авторизуемся в приватном ws
    std::string error;
    // количество попыток
    int try_count = 0;
    // флаг успешного подключения
    bool loggon = false;
    //ftx_ws_private->login(error);
    while(true){
        // пытаемся логиниться
        size_t login_result = _ftx_ws_private->login(error);
        if(login_result != 0){
            // успешно залогинились
            loggon = true;
            //_general_logger->info("loggon true, try count = {}", try_count);
            // выходим
            break;
        }
        else{
            // пытаемся еще залогиниться
            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "loggon false " << "try count = " << try_count;
            _general_logger->error("loggon false, try count = {}", try_count);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            ++try_count;
            if(try_count == 3)
                break;
        }
    }
    if(!loggon){
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ending...";
        _general_logger->error("Ending...");
        exit(0);
    }
    // подписываемся на приватный поток order
    _ftx_ws_private->subscribe_order("BTC/USDT");
    //size_t subscribe_result = ftx_ws_private->subscribe_order("BTC/USDT");
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "subscribe_order with result: " << subscribe_result;

}
//---------------------------------------------------------------
// создаёт приватный REST
//---------------------------------------------------------------
void gateway::create_private_REST()
{
    _ftx_rest_private = std::make_shared<ftx::RESTClient>(_work_config.account.api_key,
                                                          _work_config.account.secret_key);
}
void gateway::pool()
{
    _core_channel->poll();
   // _subscriber_agent_channel->poll();
    ioc.run_for(std::chrono::microseconds(100));
    // каждые 15 секунд будем дёргать приватный REST
    /*if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _last_ping_time) > 15s){
        size_t ping_result = _ftx_ws_private->ping();
        //BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << "ping with result: " << ping_result;
        _last_ping_time = std::chrono::system_clock::now();
        // если 5 минут не было ничего кроме понга в приватном канале, то нам надо перегрузить канал
        if(ws_control >= 20){
            // перезапускаем приватный WS по-таймауту
            //restart_private_ws("by timeout");

        }
        // эта проверка сделана для проверки работоспособности шлюза, в случае возникновения ошибок
        if(std::filesystem::exists(_path)){
            // появился файл balance: отправим баланс
            check_balance();
            //BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << "send balance by demand";
            _general_logger->info("send balance by demand.");
            // удалим файл
            std::filesystem::remove(_path);
        }
    }*/
}
//---------------------------------------------------------------
// принимает конфиг от агента
//---------------------------------------------------------------
void gateway::config_from_agent_handler(std::string_view message_)
{
    // это временная мера
    if (_config_was_received == true)
        return;
    _error.clear();
    std::cout << "config was received" << message_ << std::endl;
    // создаем парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд (если будет меньше 0x08, то будет ошибка)
    auto &&error_code = parser.allocate(0x1000, 0x12);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        // разбираем пришедшие данные
        auto parse_result = parser.parse(message_.data(), message_.size(), false);
        if (simdjson::SUCCESS == parse_result.error()) {
            if (auto is_new_element{parse_result["is_new"].get_bool()}; simdjson::SUCCESS == is_new_element.error()) {
                if (is_new_element.value() == true) {
                    std::cout << "Конфигурация обновилась." << std::endl;
                }
            } else {
                _error.describe("При загрузке конфигурации в теле json не найден оъект is_new.");
                // скорее всего дальше незачем парсить
                //return ;
            }
            // получим часть пути для сокращения полного пути до элементов
            if (auto cfg = parse_result["data"]["configs"]["gate_config"]; simdjson::SUCCESS == cfg.error()) {
                // получаем все необходимые элементы
                if (auto name_element{cfg["exchange"]["name"].get_string()}; simdjson::SUCCESS == name_element.error()){
                    _work_config.exchange.name = name_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"name\".");
                }
                if (auto instance_element{cfg["exchange"]["instance"].get_int64()}; simdjson::SUCCESS == instance_element.error()) {
                    _work_config.exchange.instance = instance_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"instance\".");
                }
                if (auto api_key_element{cfg["account"]["api_key"].get_string()}; simdjson::SUCCESS == api_key_element.error()) {
                    _work_config.account.api_key = api_key_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"api_key\".");
                }
                if (auto secret_key_element{cfg["account"]["secret_key"].get_string()}; simdjson::SUCCESS == secret_key_element.error()) {
                    _work_config.account.secret_key = secret_key_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"secret_key\".");
                }
                if (auto orderbook_channel_element{cfg["aeron"]["publishers"]["orderbook"]["channel"].get_string()}; simdjson::SUCCESS == orderbook_channel_element.error()) {
                    _work_config.aeron_core.publishers.orderbook.channel = orderbook_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook channel\".");
                }
                if (auto orderbook_stream_element{cfg["aeron"]["publishers"]["orderbook"]["stream_id"].get_int64()}; simdjson::SUCCESS == orderbook_stream_element.error()) {
                    _work_config.aeron_core.publishers.orderbook.stream_id = orderbook_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook stream_id\".");
                }
                if (auto balance_channel_element{cfg["aeron"]["publishers"]["balance"]["channel"].get_string()}; simdjson::SUCCESS == balance_channel_element.error()) {
                    _work_config.aeron_core.publishers.balance.channel = balance_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"balance channel\".");
                }
                if (auto balance_stream_element{cfg["aeron"]["publishers"]["balance"]["stream_id"].get_int64()}; simdjson::SUCCESS == balance_stream_element.error()) {
                    _work_config.aeron_core.publishers.balance.stream_id = balance_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"balance stream_id\".");
                }
                if (auto log_channel_element{cfg["aeron"]["publishers"]["log"]["channel"].get_string()}; simdjson::SUCCESS == log_channel_element.error()) {
                    _work_config.aeron_core.publishers.logs.channel = log_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"log channel\".");
                }
                if (auto log_stream_element{cfg["aeron"]["publishers"]["log"]["stream_id"].get_int64()}; simdjson::SUCCESS == log_stream_element.error()) {
                    _work_config.aeron_core.publishers.logs.stream_id = log_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"log stream_id\".");
                }
                if (auto core_channel_element{cfg["aeron"]["subscribers"]["core"]["channel"].get_string()}; simdjson::SUCCESS == core_channel_element.error()) {
                    _work_config.aeron_core.subscribers.core.channel = core_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"core channel\".");
                }
                if (auto core_stream_element{cfg["aeron"]["subscribers"]["core"]["stream_id"].get_int64()}; simdjson::SUCCESS == core_stream_element.error()) {
                    _work_config.aeron_core.subscribers.core.stream_id = core_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"core stream_id\".");
                }
                /*if (not _error)
                    // установим флаг о том что конфиг был получен
                    _config_was_received = true;*/
            } else {
                _error.describe("При загрузке конфигурации в теле json не найден оъект [\"data\"][\"gate_config\"].");
            }
        } else {
            _error.describe(fmt::format("Ошибка разбора json фрейма {}.", parse_result.error()));
        }
    }
}
//---------------------------------------------------------------
// обрабатывает сообщения от ядра
//---------------------------------------------------------------
void gateway::aeron_handler(std::string_view message_)
{
    _error.clear();
    _general_logger->info("***************************************");
    _general_logger->info("(message from core): {}", message_);
    _general_logger->info("***************************************");
    try
    {
        // создаем парсер
        simdjson::dom::parser parser;
        // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд
        auto &&error_code = parser.allocate(0x1000,0x04);
        // если буфер успешно выделен
        if (simdjson::SUCCESS == error_code) {
            // разбираем пришедшие данные
            auto parse_result = parser.parse(message_.data(), message_.size(), false);
            if (simdjson::SUCCESS == parse_result.error()) {
                // проверяем значение события
                if (auto event_element{parse_result["event"].get_string()}; simdjson::SUCCESS == event_element.error()) {
                    if (event_element.value() == "command") {
                        // проверяем биржу
                        if (auto exchange_element{parse_result["exchange"].get_string()}; simdjson::SUCCESS == exchange_element.error()) {
                            if (exchange_element.value() == "ftx") {
                                std::string_view action;
                                std::string_view side;
                                double price;
                                double amount;
                                if (auto action_element{parse_result["action"].get_string()}; simdjson::SUCCESS == action_element.error()) {
                                    action = action_element.value();
                                } else {}
                                if (auto data_element_array{parse_result["data"].get_array()}; simdjson::SUCCESS == data_element_array.error())
                                {
                                    for (auto data_element : data_element_array) {
                                        // получаем вид сделки
                                        if (auto side_element{data_element["side"].get_string()}; simdjson::SUCCESS == side_element.error()) {
                                            side = side_element.value();
                                        } else {}
                                        if (auto price_element{data_element["price"].get_double()}; simdjson::SUCCESS == price_element.error()) {
                                            price = price_element.value();
                                        } else {}
                                        if (auto amount_element{data_element["amount"].get_double()}; simdjson::SUCCESS == amount_element.error()) {
                                            amount = amount_element.value();
                                        } else {}
                                        // выставлен ордер на отмену продажи
                                        if (action.compare("cancel_order") == 0 && side.compare("sell") == 0) {
                                            cancel_sell_order();
                                         }
                                        // выставлен ордер на продажу
                                        else if (action.compare("create_order") == 0 && side.compare("sell") == 0) {
                                            sell_order(price, amount);
                                        }
                                        // выставлен ордер на отмену покупки
                                        else if (action.compare("cancel_order") == 0 && side.compare("buy") == 0) {
                                            cancel_buy_order();
                                        }
                                        // выставлен ордер на покупку
                                        else if (action.compare("create_order") == 0 && side.compare("buy") == 0) {
                                            buy_order(price, amount);
                                        } else {
                                            _error.describe("Не могу распознать action и side в команде.");
                                        }
                                    }
                                } else {
                                    _error.describe(fmt::format("Не могу получить массив данных data {}.", message_.data()));
                                }
                            } else {
                                _error.describe(fmt::format("В команде неверно указана биржа: {}.", message_.data()));
                            }
                        } else {

                        }
                    } else {
                          _error.describe(fmt::format("В команде неверно указан event: {}.", message_.data()));
                    }
                }
            }
            else
            {
                _error.describe("Ошибка разбора json фрейма.");
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        }
        else
        {
            _error.describe("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).");
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    }
    catch(simdjson::simdjson_error& err)
    {
        _error.describe(err.what() + fmt::format(" (json body: {}).", message_));
        _general_logger->error(_error.to_string());
        _error.clear();
    }
    //std::cout << "------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << "" << std::endl;
}
/*void gateway::aeron_handler(std::string_view message_)
{
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "(message from core): " << message_;
    _general_logger->info("(message from core): {}", message_);
    try
    {
        simdjson::dom::parser parser;
        auto &&error_code = parser.allocate(0x1000,0x04);
        if(simdjson::SUCCESS == error_code)
        {
            auto result = parser.parse(message_.data(), message_.size(), false);
            if(simdjson::SUCCESS == result.error())
            {
                std::string_view a, S, s, t, p, q;
                a = result.at_key("a").get_string();
                S = result.at_key("S").get_string();
                s = result.at_key("s").get_string();
                if(a.compare("+") == 0)
                {
                    t = result.at_key("t").get_string();
                    p = result.at_key("p").get_string();
                    q = result.at_key("q").get_string();
                }
                // выставлен ордер на отмену продажи
                if(a.compare("-") == 0  && s.compare("SELL") == 0)
                {
                    cancel_sell_order();
                }
                // выставлен ордер на продажу
                else if(a.compare("+") == 0  && s.compare("SELL") == 0)
                {
                    sell_order(p, q);
                }
                // выставлен ордер на отмену покупки
                else if(a.compare("-") == 0  && s.compare("BUY") == 0)
                {
                    cancel_buy_order();
                }
                // выставлен ордер на покупку
                else if(a.compare("+") == 0  && s.compare("BUY") == 0)
                {
                    buy_order(p, q);
                }
            }
            else
            {
                _error.describe("Ошибка разбора json фрейма.");
                //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        }
        else
        {
            _error.describe("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).");
            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    }
    catch(simdjson::simdjson_error& err)
    {
        _error.describe(err.what() + fmt::format(" (json body: {}).", message_));
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _general_logger->error(_error.to_string());
        _error.clear();
    }
    std::cout << "------------------------------------------------------------------------------------------------" << std::endl;
}*/
//---------------------------------------------------------------
// обрабатывает отмену ордера на продажу ("-" && "sell")
//---------------------------------------------------------------
void gateway::cancel_sell_order()
{
    _general_logger->info("Ядром выставлен ордер на отмену продажи, проверим открытые ордера.");
    // получаем открытые ордера
    _error.clear();
    std::vector<SOrder> open_orders_list = _ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на продажу
            if(order.side == "sell") {
                _general_logger->info("Есть открытый ордер на продажу {} {}", open_orders_list.at(0).clientId, _error.to_string());
                // отменяем ордер по идентификатору
                std::string cancel_order_result = _ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if(_error) {
                    _error.describe("Ошибка отмены ордера");
                    _general_logger->error("Результат отмены ордера (id): {} {} {}", order.id, cancel_order_result, _error.to_string());
                    order_status_prepare("order_cancel", "Order was cancel", cancel_order_result, true, _error.to_string());
                    _error.clear();
                } else {
                    _general_logger->info("Результат отмены ордера (id): {} {}", order.id, cancel_order_result);
                    order_status_prepare("order_cancel", "Order was cancel", cancel_order_result);
                }
            } else if (order.side == "buy") {
                _general_logger->info("Есть открытый ордер, но на покупку.");
            }
        }
    } else {
        if(_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            _general_logger->error(_error.to_string());
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            _general_logger->info("Нет выставленных ордеров на продажу (отменять нечего).");
        }
    }
}
//---------------------------------------------------------------
// обрабатывает отмену ордера на покупку ("-" && "buy")
//---------------------------------------------------------------
void gateway::cancel_buy_order()
{
    _general_logger->info("Ядром выставлен ордер на отмену покупки, проверим открытые ордера.");
    // получаем открытые ордера
    _error.clear();
    std::vector<SOrder> open_orders_list = _ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на покупку
            if(order.side == "buy") {
                _general_logger->info("Есть открытый ордер на покупку {} {}.", open_orders_list.at(0).clientId, _error.to_string());
                // отменяем ордер по идентификатору
                std::string cancel_order_result = _ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if(_error) {
                    _error.describe("Ошибка отмены ордера");
                    _general_logger->error("Результат отмены ордера (id): {} {} {}", order.id, cancel_order_result, _error.to_string());
                    order_status_prepare("order_cancel", "Order was cancel", cancel_order_result, true, _error.to_string());
                    _error.clear();
                } else {
                    _general_logger->info("Результат отмены ордера (id): {} {}", order.id, cancel_order_result);
                    order_status_prepare("order_cancel", "Order was cancel", cancel_order_result, true, _error.to_string());
                }
            } else if (order.side == "sell") {
                _general_logger->info("Есть открытый ордер, но на продажу.");
            }
        }
    } else {
        if(_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            _general_logger->error(_error.to_string());
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            _general_logger->info("Нет выставленных ордеров на покупку (отменять нечего).");
        }
    }
}
//---------------------------------------------------------------
// обрабатывает ордер на продажу ("+" && "sell")
//---------------------------------------------------------------
//void gateway::sell_order(std::string_view price_, std::string_view quantity_)
void gateway::sell_order(const double& price_, const double& quantity_)
{
    _general_logger->info("Ядром выставлен ордер на продажу, проверим открытые ордера.");
    _error.clear();
    // получаем открытые ордера
    std::vector<SOrder> open_orders_list = _ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на продажу
            if (order.side == "sell") {
                _general_logger->info("Есть открытый ордер на продажу {} {}.", open_orders_list.at(0).clientId, _error.to_string());
                // отменяем ордер по идентификатору
                std::string result = _ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if (_error) {
                    _error.describe("Ошибка отмены ордера");
                    _general_logger->error("Результат отмены ордера (id): {} {} {}.", order.id, result, _error.to_string());
                    _error.clear();
                } else {
                    _general_logger->info("Результат отмены ордера (id): {} {}.", order.id, result);
                }
            } else if(order.side == "buy") {
                _general_logger->info("Есть открытый ордер, но на покупку.");
            }
        }
    } else {
        if(_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            _general_logger->error(_error.to_string());
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            _general_logger->info("Нет выставленных ордеров на продажу (отменять нечего).");
        }
    }
    _general_logger->info("Выставлен ордер на продажу с ценой: {} и объёмом {}", price_, quantity_);
    /*std::string price = set_price_precision(std::string(price_));
    std::string size  = set_size_precision(std::string(quantity_));
    _general_logger->info("После корректировки: {} {}", price, size);*/

    // выставляем ордер (синхронно)
    std::string place_order_result = _ftx_rest_private->place_order("BTC/USDT", "sell", price_, quantity_, _error);
    if(_error) {
        _error.describe("Ошибка выставления ордера.");
        _general_logger->error("Ошибка выставления ордера на продажу: {} {}", place_order_result, _error.to_string());
        order_status_prepare("order_created", "Order was created", place_order_result, true, _error.to_string());
        _error.clear();
    } else {
        _general_logger->info("Результат выставления ордера на продажу: {}", place_order_result);
        order_status_prepare("order_created", "Order was created", place_order_result);
    }

    /*std::make_shared<ftx::AsyncRESTClient>(config.account.api_key,
                                           config.account.secret_key,
                                           ioc,
                                           [&](std::string_view message_)
             {shared_from_this()->place_order_result_handler(message_);})->place_order("BTC/USDT", "sell", price, size);*/
}
//---------------------------------------------------------------
// обрабатывает ордер на покупку ("+" && "buy")
//---------------------------------------------------------------
//void gateway::buy_order(std::string_view price_, std::string_view quantity_)
void gateway::buy_order(const double& price_, const double& quantity_)
{
    _general_logger->info("Ядром выставлен ордер на покупку, проверим открытые ордера.");
    // получаем открытые ордера
    _error.clear();
    std::vector<SOrder> open_orders_list = _ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на продажу
            if(order.side == "buy")
            {
                _general_logger->info("Есть открытый ордер на покупку {} {}", open_orders_list.at(0).clientId, _error.to_string());
                // отменяем ордер по идентификатору
                std::string result = _ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if(_error) {
                    _error.describe("Ошибка отмены ордера");
                    _general_logger->error("Результат отмены ордера (id): {} {} {}", order.id, result, _error.to_string());
                    _error.clear();
                } else {
                    _general_logger->info("Результат отмены ордера (id): {} {}", order.id, result);
                }
            } else if(order.side == "sell") {
                 _general_logger->info("Есть открытый ордер, но на покупку.");
            }
        }
    } else {
        if (_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            _general_logger->error(_error.to_string());
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            _general_logger->info("Нет выставленных ордеров на покупку (отменять нечего).");
        }
    }
    _general_logger->info("Выставлен ордер на покупку с ценой: {} и объёмом {}", price_, quantity_);
 /*   std::string price = set_price_precision(std::string(price_));
    std::string size  = set_size_precision(std::string(quantity_));
    _general_logger->info("После корректировки: {} {}", price, size);*/

    // выставляем ордер (синхронно)
    std::string place_order_result = _ftx_rest_private->place_order("BTC/USDT", "buy", price_, quantity_, _error);
    if(_error) {
        _general_logger->error("Ошибка выставления ордера на покупку: {} {}", place_order_result, _error.to_string());
        order_status_prepare("order_created", "Order was created", place_order_result, true, _error.to_string());
        _error.clear();
    } else {
        _general_logger->info("Результат выставления ордера на покупку: {}", place_order_result);
        order_status_prepare("order_created", "Order was created", place_order_result);
    }
    /*std::make_shared<ftx::AsyncRESTClient>(config.account.api_key,
                                           config.account.secret_key,
                                           ioc,
                                           [&](std::string_view message_)
             {shared_from_this()->place_order_result_handler(message_);})->place_order("BTC/USDT", "buy", price, size);*/
}
//---------------------------------------------------------------
// подготавливаем json order_status
//---------------------------------------------------------------
void gateway::order_status_prepare(std::string_view action_, std::string_view message_, std::string_view place_result, bool is_error, std::string error_)
{
    std::string_view event{"data"};
    std::string_view id;
    std::string_view status;
    std::string_view symbol;
    std::string_view type;
    std::string_view side;
    double price;
    double amount;
    double filled;
    // обработаем результат place_result
    // создадим парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд
    auto &&error_code = parser.allocate(0x2000, 0x05);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        // разбираем результат размещения ордера
        auto parse_result = parser.parse(place_result.data(), place_result.size(), false);
        // если данные успешно разобрались
        if (simdjson::SUCCESS == parse_result.error()) {
            // получаем значение поля success
            if (auto success_element{parse_result["success"].get_bool()}; simdjson::SUCCESS == success_element.error()) {
                if (success_element.value() == true) {
                    if (auto result_element{parse_result["result"]["id"].get_int64()}; simdjson::SUCCESS == result_element.error()) {
                        id = std::to_string(result_element.value());
                    }
                    if (auto status_element{parse_result["result"]["status"].get_string()}; simdjson::SUCCESS == status_element.error()) {
                        status = status_element.value();
                    }
                    if (auto symbol_element{parse_result["result"]["market"].get_string()}; simdjson::SUCCESS == symbol_element.error()) {
                        symbol = symbol_element.value();
                    }
                    if (auto type_element{parse_result["result"]["type"].get_string()}; simdjson::SUCCESS == type_element.error()) {
                        type = type_element.value();
                    }
                    if (auto side_element{parse_result["result"]["side"].get_string()}; simdjson::SUCCESS == side_element.error()) {
                        side = side_element.value();
                    }
                    if (auto price_element{parse_result["result"]["price"].get_double()}; simdjson::SUCCESS == price_element.error()) {
                        price = price_element.value();
                    }
                    if (auto amount_element{parse_result["result"]["size"].get_double()}; simdjson::SUCCESS == amount_element.error()) {
                        amount = amount_element.value();
                    }
                    if (auto filled_element{parse_result["result"]["filledSize"].get_double()}; simdjson::SUCCESS == filled_element.error()) {
                        filled = filled_element.value();
                    }
                } else if (success_element.value() == false) {
                    // если результат отрицательный, то отправим это сообщение ядру
                    if (auto element_error{parse_result["error"].get_string()}; simdjson::SUCCESS == element_error.error()) {
                        // переоределим сообщения
                        event    = "error";
                        //action_  = "order_not_created";
                        message_ = element_error.value();
                    }
                }
            }
        }
    }
    if (is_error == true) {
        event    = "error";
        message_ = error_;
    }
    JSON order_status_root;
    order_status_root["event"]     = event;
    order_status_root["exchange"]  = "ftx";
    order_status_root["node"]      = "gate";
    order_status_root["instance"]  = "1";
    order_status_root["action"]    = action_;
    order_status_root["message"]   = message_;
    order_status_root["algo"]      = "3t_php";
    order_status_root["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    JSON data;
    {
        data["id"]        = id;
        data["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        data["status"]    = status;
        data["symbol"]    = symbol;
        data["type"]      = type;
        data["side"]      = side;
        data["price"]     = price;
        data["amount"]    = amount;
        data["filled"]    = filled;
    }
    // потом заполнить это
    order_status_root["data"] = data;
    // отправляем информацию в ядро
    order_status_sender(order_status_root.dump());
}
//---------------------------------------------------------------
// отправляем order_status
//---------------------------------------------------------------
void gateway::order_status_sender(std::string_view order_status_)
{

    const std::int64_t result = _order_status_channel->offer(order_status_.data());
    if(result < 0)
    {
        processing_error("error: Ошибка отправки информации о статусе ордера в ядро: ", result);
    }
}
//---------------------------------------------------------------
// приватный канал WS (neew refactoring, обработка ошибок и отмена try catch)
//---------------------------------------------------------------
void gateway::private_ws_handler(std::string_view message_, void* id_)
{
    //return;
    if(message_.compare("{\"type\": \"pong\"}") == 0){
        // ответ должны получать каждые 30 секунд, увеличим счётчик
        if(start_trigger == true)
            ++ws_control;
//        BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << "pong: "            << message_      <<
//                                                                   " start_trigger = " << start_trigger <<
//                                                                   " ws_control = "    << ws_control    <<
//                                                                   " object_id = "     << id_;
        _ping_pong_logger->info("pong: {} start_trigger = {} ws_control = {} object_id = {}", message_, start_trigger, ws_control, id_);
        return;
    }
    try
    {
        //std::cout << "" << message_ << std::endl;
        //_general_logger->info("(message from private_ws_handler): {}", message_);
        // создадим парсер
        simdjson::dom::parser parser;
        // пусть парсер подготовит буфер для своих нужд
        auto &&error_code = parser.allocate(0x1000,0x04);
        // если буфер был успешно выделен
        if(simdjson::SUCCESS == error_code){
            // разбираем строку
            auto result = parser.parse(message_.data(), message_.size(), false);
            // если данные успешно разобрались
            if(simdjson::SUCCESS == result.error()){
                if(auto element_type{result["type"].get_string()}; simdjson::SUCCESS == element_type.error()){
                    if(element_type.value().compare("update") == 0){
                        std::string_view element_side{result["data"]["side"].get_string()};
                        std::string_view element_status{result["data"]["status"].get_string()};
                        double element_filled{result["data"]["filledSize"].get_double()};
                        double element_remaining{result["data"]["remainingSize"].get_double()};
                        int64_t element_id{result["data"]["id"].get_int64()};
                        std::string description = get_order_change_description(element_side, element_status, element_filled, element_remaining);
//                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info)
//                                << "(ws private) произошли изменения в ордерах : "
//                                << description
//                                << " object_id  = " << id_
//                                << " id: " << element_id
//                                << " side: " << element_side
//                                << " status: " << element_status
//                                << " filledSize: " << element_filled
//                                << " remainingSize: " << element_remaining;
                        _general_logger->info("(ws private) произошли изменения в ордерах: {} object_id = {} id: {} side: {} status: {} filledSize: {} remainingSize: {}",
                                              description, id_, element_id, element_side, element_status, element_filled, element_remaining);
                        std::cout << "" << std::endl;
                        //buy closed 0 0 -

                        /*std::vector<SBState> balances_vector = ftx_rest_private->get_balances(_error);
                        // если вектор нулевого размера, значит была какая-то ошибка
                        if(balances_vector.empty())
                        {
                            if(_error){
                                _error.describe("Ошибка получения баланса (get_balances).");
                                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _error.clear();
                            }
                            // взводим триггер
                            start_trigger = true;
                            BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << " start_trigger = " << start_trigger;
                        }
                        else{
                            balance_sender(balances_vector);
                        }*/
                        // проверяем и отправляем баланс
                        check_balance();
                        // сбросим счётчик
                        ws_control = 0;
                        // сбросим триггер
                        start_trigger = false;
//                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "start_trigger set to false !!! " <<
//                                                                                    "object_id = " << id_;
                        _general_logger->info("start_trigger set to false !!! object_id = {}", id_);
                    }
                    else{
                        if(element_type.value().compare("subscribed") != 0){
                            _error.describe(fmt::format("json не содержит поле \"update\". json body: {}", message_));
                            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _general_logger->error(_error.to_string());
                            _error.clear();
                        }
                    }
                }
                else{
                    _error.describe(fmt::format("json не содержить поле \"type\". json body: {}", message_));
                    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                    _general_logger->error(_error.to_string());
                    _error.clear();
                }
            }
            else
            {
                //error += "Ошибка разбора json фрейма.";
                //error += " ((ws private) json: " + std::string(message_) + ").";
                _error.describe(fmt::format("Ошибка разбора json фрейма. ((ws private) json body: {}).", message_));
                //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        }
        else
        {
            //error += "Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).";
            //error += " ((ws private) json: " + std::string(message_) + ").";
            _error.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). ((ws private) json body: {}).", message_));
            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    }
    catch(simdjson::simdjson_error& err)
    {
        //error += err.what();
        //error += " ((ws private) json: " + std::string(message_) + ").";
        _error.describe(err.what() + fmt::format(" ((ws private) json body: {}).", message_));
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
        _general_logger->error(_error.to_string());
        _error.clear();
    }
}
//---------------------------------------------------------------
// получает более подробную информацию об изменении ордера
//---------------------------------------------------------------
std::string gateway::get_order_change_description(std::string_view side_, std::string_view status_, const double& filled_size_, const double& remaining_size_)
{
    std::string result_message;

    if (0 ==side_.compare("buy")) {
        if (0 == status_.compare("new")) {
            // выставлен новый ордер
            result_message = " Создан ордер на покупку. ";
        } else if (0 == status_.compare("closed")) {
            // ордер был выполнен или отменен
            if ((0 == filled_size_) && (0 == remaining_size_)) {
                //result_message = " Покупка выполнена. ";
                result_message = " Покупка отменена. ";
            } else {
                //result_message = " Покупка отменена. ";
                result_message = " Покупка выполнена. ";
            }
        } else {
            result_message = " Неопределенный статус в ордере buy. ";
        }
    } else if (0 == side_.compare("sell")) {
        if (0 == status_.compare("new")) {
            // выставлен новый ордер
            result_message = " Создан ордер на продажу. ";
        } else if (0 == status_.compare("closed")) {
            // ордер выполнен или отменен
            if ((0 == filled_size_) && (0 == remaining_size_)) {
                //result_message = " Продажа выполнена. ";
                result_message = " Продажа отменена. ";
            } else {
                //result_message = " Продажа отменена. ";
                result_message = " Продажа выполнена. ";
            }
        }
    } else {
        result_message = " Неопределенная операция.";
    }
    return result_message;
}
//---------------------------------------------------------------
// публичный канал WS
//---------------------------------------------------------------
void gateway::public_ws_handler(std::string_view message_, void* id_)
{
    //создадим парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд
    auto &&error_code = parser.allocate(0x2000, 0x05);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        // разбираем пришедшее сообщение
        //std::cout << message_<< std::endl;
        auto result = parser.parse(message_.data(), message_.size(), false);
        // если данные успешно разобрались
        if (simdjson::SUCCESS == result.error()) {
            // получаем значение поля type, чтобы понять тип сообщения (нам нужны сообщения второго типа (см. примеры выше))
            if (auto type_element{result["type"].get_string()}; simdjson::SUCCESS == type_element.error()) {
                // после подписки получаем snapshot ("type": "partial"), содержащий 100 лучших ордеров
                if (type_element.value().compare("partial") == 0) {
                    // очистим стакан на всякий случай
                    //_bids_map.clear();
                    //_asks_map.clear();
                    // получаем рынок, с которого пришло сообщение (получаем его в std::string, потому что будем изменять строку)
                    if (auto market_element{result["market"].get_c_str()}; simdjson::SUCCESS == market_element.error()) {
                        std::string market = market_element.value();
                        // заменим '/' (так приходит с биржи) на '-' (так требует ядро)
                        /*std::replace(market.begin(), market.end(),'/','-');*/
                        //std::cout << "-----------------   bids   ----------------------------"<< std::endl;
                        // получаем bid
                        if (auto bids_array_element{result["data"]["bids"].get_array() }; simdjson::SUCCESS == bids_array_element.error()) {
                            //std::cout << bids_array_element << std::endl;
                            // получим итератор для быстрой вставки
                            //auto insert_it (std::end(_bids_map));
                            for(auto bid_pair_value : bids_array_element) {
                                //insert_it = _bids_map.insert(insert_it, std::make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));
                                ////_bids_map.insert(std::make_pair(market, std::map<double, double, std::greater<double>>()));
                                ////_bids_map[market].insert(std::make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));

                                ////_bids_map.insert(make_pair(market, map<string, map<double, double, greater<double>>>()));
                                ////_bids_map[market].insert(make_pair("bids", map<double, double, greater<double>>()));
                                ////_bids_map[market]["bids"].insert(make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));
                                _markets_map.insert(make_pair(market, map<string, map<double, double>, std::greater<string>>()));
                                _markets_map[market].insert(make_pair("bids", map<double, double>()));
                                _markets_map[market]["bids"].insert(make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения массива bids. json body: {}.", message_));
                            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _general_logger->error(_error.to_string());
                            _error.clear();
                        }
                        //std::cout << "-----------------   asks  ----------------------------"<< std::endl;
                        // получаем ask
                        if (auto asks_array_element{result["data"]["asks"].get_array() }; simdjson::SUCCESS == asks_array_element.error()) {
                            //std::cout << asks_array_element << std::endl;
                            // получим итератор для быстрой вставки
                            //auto insert_it (std::end(_asks_map));
                            for(auto ask_pair_value : asks_array_element) {
                                //insert_it = _asks_map.insert(insert_it, std::make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));
                                ////_asks_map.insert(std::make_pair(market, std::map<double, double>()));
                                ////_asks_map[market].insert(std::make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));

                                ////_asks_map.insert(make_pair(market, map<string, map<double, double>>()));
                                ////_asks_map[market].insert(make_pair("asks", map<double, double>()));
                                ////_asks_map[market]["bids"].insert(make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));
                                _markets_map.insert(make_pair(market, map<string, map<double, double>, std::greater<string>>()));
                                _markets_map[market].insert(make_pair("asks", map<double, double>()));
                                _markets_map[market]["asks"].insert(make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения массива asks. json body: {}.", message_));
                            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _general_logger->error(_error.to_string());
                            _error.clear();
                        }
                        // подготовим "стакан" к отправке
                        orderbook_prepare(_markets_map);
                    } else {
                        _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}.", message_));
                        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                        _general_logger->error(_error.to_string());
                        _error.clear();
                    }
                } else {

                    if (type_element.value().compare("update") == 0) {
                        // получаем рынок
                        if (auto market_element{result["market"].get_c_str()}; simdjson::SUCCESS == market_element.error()) {
                            std::string market = market_element.value();
                            // заменим '/' (так приходит с биржи) на '-' (так требует ядро)
                            /*std::replace(market.begin(), market.end(),'/','-');*/
                            // получаем bid
                            if (auto bids_array_element{result["data"]["bids"].get_array() }; simdjson::SUCCESS == bids_array_element.error()) {
                                //std::cout << bids_array_element << std::endl;
                                for(auto bid_pair_value : bids_array_element) {
                                    //std::cout << bid_pair_value.at(0).get_double() << "  " << bid_pair_value.at(1).get_double();
                                    // если объем равен нулю, то удаляем такой ключ
                                    if (0.0 == bid_pair_value.at(1).get_double()) {
                                        // для начала найдем его
                                        auto finded_key = _markets_map[market]["bids"].find(bid_pair_value.at(0).get_double());
                                        if (finded_key != _markets_map[market]["bids"].end()) {
                                            _markets_map[market]["bids"].erase(finded_key);
                                            //std::cout << "удалили ключ из bids_array: " << bid_pair_value.at(0).get_double() << std::endl;
                                        } else {
                                            std::cout << "ключ не найден в bids_array: " << bid_pair_value.at(0).get_double() << std::endl;
                                        }
//                                        auto find_iterator = _bids_map.find(bid_pair_value.at(0).get_double());
//                                        if (find_iterator != _bids_map.end()) {
//                                            _bids_map.erase(find_iterator);
//                                            //std::cout << "удалили ключ из bids_array: " << bid_pair_value.at(0).get_double() << std::endl;
//                                            //std::cout << "размер bids_array: " << _bids_map.size() << std::endl;
//                                        } else {
//                                            ///std::cout << "ключ не найден в bids_array: " << bid_pair_value.at(0).get_double() << std::endl;
//                                        }
                                    } else {
//                                        _bids_map[bid_pair_value.at(0).get_double()] = bid_pair_value.at(1).get_double();
                                        _markets_map[market]["bids"][bid_pair_value.at(0).get_double()] = bid_pair_value.at(1).get_double();
                                    }
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения массива bids. json body: {}.", message_));
                                //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _general_logger->error(_error.to_string());
                                _error.clear();
                            }
                            // получаем ask
                            if (auto asks_array_element{result["data"]["asks"].get_array() }; simdjson::SUCCESS == asks_array_element.error()) {
                                //std::cout << asks_array_element << std::endl;
                                for(auto ask_pair_value : asks_array_element) {
                                    //std::cout << ask_pair_value.at(0).get_double() << "  " << ask_pair_value.at(1).get_double();
                                    // если объем равен нулю, то удаляем такой ключ
                                    if (0.0 == ask_pair_value.at(1).get_double()) {

//                                        // для начала найдем его
                                        auto finded_key = _markets_map[market]["asks"].find(ask_pair_value.at(0).get_double());
                                        if (finded_key != _markets_map[market]["asks"].end()) {
                                            _markets_map[market]["asks"].erase(finded_key);
                                            //std::cout << "удалили ключ из asks_array: " << ask_pair_value.at(0).get_double() << std::endl;
                                        } else {
                                            std::cout << "ключ не найден в asks_array: " << ask_pair_value.at(0).get_double() << std::endl;
                                        }
//                                        auto find_iterator = _asks_map.find(ask_pair_value.at(0).get_double());
//                                        if (find_iterator != _asks_map.end()) {
//                                            _asks_map.erase(find_iterator);
//                                            //std::cout << "удалили ключ из asks_array: " << ask_pair_value.at(0).get_double() << std::endl;
//                                            //std::cout << "размер asks_array: " << _bids_map.size() << std::endl;
//                                        } else {
//                                            //std::cout << "ключ не найден в asks_array: " << ask_pair_value.at(0).get_double() << std::endl;
//                                        }
                                    } else {
//                                        _asks_map[ask_pair_value.at(0).get_double()] = ask_pair_value.at(1).get_double();
                                        _markets_map[market]["asks"][ask_pair_value.at(0).get_double()] = ask_pair_value.at(1).get_double();
                                    }
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения массива asks. json body: {}.", message_));
                                //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _general_logger->error(_error.to_string());
                                _error.clear();
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}.", message_));
                            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _general_logger->error(_error.to_string());
                            _error.clear();
                        }
                        // подготовим "стакан" к отправке
                        orderbook_prepare(_markets_map);

                    } else if (type_element.value().compare("error") == 0) {
                        // получаем код ошибки
                        if (auto code_element{result["code"].get_int64()}; simdjson::SUCCESS == code_element.error()) {
                            //std::string code = code_element.value();
                            // получаем описание ошибки
                            if (auto message_element{result["msg"].get_c_str()}; simdjson::SUCCESS == message_element.error()) {
                                // отправими ошибку
                                error_sender(fmt::format("Ошибка в канале orderbook (код: {}, сообщение: \"{}\").", code_element.value(), message_element.value()));
                            }
                        }
                    }
                    // видимо было сообщение 1 типа (subscribed)
                }
            } else {
                _error.describe(fmt::format("Ошибка определения типа сообщения. json body: {}.", message_));
                //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        } else {
            _error.describe(fmt::format("Ошибка разбора json фрейма (error: {})", result.error()));
            error_sender(_error.to_string());
            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    } else {
        _error.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _general_logger->error(_error.to_string());
        _error.clear();
    }
}
/*void gateway::public_ws_handler(std::string_view message_, void* id_)
{
    // примеры поступающих сообщений:
    // Тип 1. После subscribe на канал
    // {"type": "subscribed", "channel": "ticker", "market": "ETH/USDT"}
    // Тип 2. В дальнейшем работе
    // {"channel": "ticker", "market": "ETH/USDT", "type": "update", "data": {"bid": 3146.8, "ask": 3146.9, "bidSize": 15.233, "askSize": 16.39, "last": 3146.8, "time": 1648380244.282335}}
    //
    std::cout << message_ << std::endl;
    //создадим парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд
    auto &&error_code = parser.allocate(0x1000, 0x04);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        // разбираем пришедшее сообщение
        auto result = parser.parse(message_.data(), message_.size(), false);
        // если данные успешно разобрались
        if (simdjson::SUCCESS == result.error()) {
            // получаем значение поля type, чтобы понять тип сообщения (нам нужны сообщения второго типа (см. примеры выше))
            if (auto type_element{result["type"].get_string()}; simdjson::SUCCESS == type_element.error()) {
                if (type_element.value().compare("update") == 0) {
                    // получаем рынок, с которого пришло сообщение (получаем его в std::string, потому что будем изменять строку)
                    if (auto market_element{result["market"].get_c_str()}; simdjson::SUCCESS == market_element.error()) {
                        std::string market = market_element.value();
                        // заменим '/' (так приходит с биржи) на '-' (так требует ядро)
                        std::replace(market.begin(), market.end(),'/','-');
                        // получаем bid
                        if (auto bid_element{result["data"]["bid"].get_double()}; simdjson::SUCCESS == bid_element.error()) {
                            // получаем ask
                            if (auto ask_element{result["data"]["ask"].get_double()}; simdjson::SUCCESS == ask_element.error()) {
                                // получаем bidSize
                                if (auto bid_size_element{result["data"]["bidSize"].get_double()}; simdjson::SUCCESS == bid_size_element.error()) {
                                    // получаем askSize
                                    if (auto ask_size_element{result["data"]["askSize"].get_double()}; simdjson::SUCCESS == ask_size_element.error()) {
                                        STicker ticker{market,
                                                    std::to_string(bid_element.value()),
                                                    std::to_string(bid_size_element.value()),
                                                    std::to_string(ask_element.value()),
                                                    std::to_string(ask_size_element.value())};
                                        // отправляем "стакан" только в том случае, если значения отличны от предыдущих
                                        if (ticker.b.compare(prev_ticker.b) != ticker.B.compare(prev_ticker.B)!= ticker.a.compare(prev_ticker.a)!= ticker.A.compare(prev_ticker.A))
                                                ticker_sender(ticker, id_);
                                        prev_ticker = ticker;
                                    } else {
                                        _error.describe(fmt::format("Ошибка получения объема (askSize) лучшей заявки на продажу. json body: {}", message_));
                                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                        _error.clear();
                                    }
                                } else {
                                    _error.describe(fmt::format("Ошибка получения объема (bidSize) лучшей заявки на покупку. json body: {}", message_));
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                    _error.clear();
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения цены (ask) лучшей заявки на продажу. json body: {}", message_));
                                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _error.clear();
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения цены (bid) лучшей заявки на покупку. json body: {}", message_));
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                        }
                    } else {
                        _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}", message_));
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                        _error.clear();
                    }
                } else {
                    // видимо было сообщение 1 типа
                }
            } else {
                _error.describe(fmt::format("Ошибка определения типа сообщения. json body: {}", message_));
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _error.clear();
            }
        } else {
            _error.describe("Ошибка разбора json фрейма.");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
        }
    } else {
        _error.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _error.clear();
    }
}*/
//---------------------------------------------------------------
// подготавливает ордербук
//---------------------------------------------------------------
void gateway::orderbook_prepare(const map<string, map<string, map<double, double>, std::greater<string>>>& markets_map_)
{
    // это надо брать из конфига
    const int sended_size = 34;
    map<string, map<string, map<double, double>, std::greater<string>>>::const_iterator market_itr;
    map<string, map<double, double>>::const_iterator direct_itr;
    map<double, double>::const_iterator ptr;
    map<double, double>::const_reverse_iterator rptr;

    //std::cout << "---------------------------------------------------------------------" << std::endl;
//    uint64_t start1 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
//    // проходим сначала по рынкам
//    for (market_itr = markets_map_.begin(); market_itr != markets_map_.end(); market_itr++) {
//        pt::ptree orderbook_root;
//        orderbook_root.put("event",     "data");
//        orderbook_root.put("exchange",  "ftx");
//        orderbook_root.put("node",      "gate");
//        orderbook_root.put("instance",  1);
//        orderbook_root.put("action",    "orderbook");
//        orderbook_root.put("message",   NULL);
//        orderbook_root.put("algo",      "signal");
//        orderbook_root.put("timestamp", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
//        pt::ptree data;
//        // теперь проходим по bids и asks в текущем рынке
//        for (direct_itr = market_itr->second.begin(); direct_itr != market_itr->second.end(); direct_itr++) {
//            /* обработка массивов ask и bid будет отличаться, так как порядок сортировки в map одинаковый (по возрастанию), а
//             * в реальности для bid нужна сортировка в другом порядке.
//             *      Поэтому для ask (продать надо как можно дороже):
//             * если надо отправлять N значений, то в ask смещаем указатель так, чтобы осталось последних N и формируем json из них.
//             *      Для bid (купить надо как можно дешевле):
//             * есди надо отправить N значений, то в bid берем первые N и формируем из них json, только в обратном порядке
//             */
//            if (direct_itr->first.compare("asks") == 0) {
//                // получаем итератор на начало map
//                auto begin_map = direct_itr->second.begin();
//                // вычисляем на сколько можем сместиться
//                auto remain_count = ((direct_itr->second.size() < sended_size) ? direct_itr->second.size() : sended_size);
//                // смещаемся на N
//                std::advance(begin_map, direct_itr->second.size() - remain_count);
//                //std::cout << " смещаемся на " << direct_itr->second.size() - remain_count << std::endl;

//                pt::ptree asks_node;
//                for (ptr = begin_map; ptr != direct_itr->second.end(); ptr++) {
//                    pt::ptree cell;
//                    pt::ptree price;
//                    price.put_value<double>(ptr->first);
//                    cell.push_back(std::make_pair("", price));

//                    pt::ptree size;
//                    size.put_value<double>(ptr->second);
//                    cell.push_back(std::make_pair("", size));

//                    asks_node.push_back(std::make_pair("", cell));
//                }
//                data.add_child(direct_itr->first, asks_node);
//            }
//            else {
//                // получаем обратный итератор на начало map
//                auto rbegin_map = direct_itr->second.rbegin();
//                // вычисляем сможем ли сдвинуть на N элементов
//                auto remain_count = ((direct_itr->second.size() < sended_size) ? direct_itr->second.size() : sended_size);
//                // смещаемся на  (map.size - N)
//                std::advance(rbegin_map, (direct_itr->second.size() - remain_count));
//                pt::ptree bids_node;
//                for (rptr = rbegin_map; rptr != direct_itr->second.rend(); rptr++) {
//                    pt::ptree cell;
//                    pt::ptree price;
//                    price.put_value<double>(rptr->first);
//                    cell.push_back(std::make_pair("", price));

//                    pt::ptree size;
//                    size.put_value<double>(rptr->second);
//                    cell.push_back(std::make_pair("", size));

//                    bids_node.push_back(std::make_pair("", cell));
//                }
//                data.add_child(direct_itr->first, bids_node);
//            }

//        }
//        data.put("symbol", market_itr->first);
//        data.put("timestamp", 1499280391811876);
//        orderbook_root.add_child("data", data);
//        std::stringstream json_for_core;
//        pt::write_json(json_for_core, orderbook_root, false);
//        orderbook_sender(json_for_core.str());
//    }
//    uint64_t stop1 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
//    std::cout << stop1-start1 << std::endl;
    //--------------------------------------------------------
    uint64_t start2 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (market_itr = markets_map_.begin(); market_itr != markets_map_.end(); ++market_itr) {
        JSON orderbook_root;
        orderbook_root["event"]     = "data";
        orderbook_root["exchange"]  = "ftx";
        orderbook_root["node"]      = "gate";
        orderbook_root["instance"]  = "1";
        orderbook_root["action"]    = "orderbook";
        orderbook_root["message"]   = nullptr;
        orderbook_root["algo"]      = "signal";
        orderbook_root["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        //json_loh j_no_init_list = json_loh::array();
        JSON data;
        // теперь проходим по bids и asks в текущем рынке
        for (direct_itr = market_itr->second.begin(); direct_itr != market_itr->second.end(); direct_itr++) {

            if (direct_itr->first.compare("asks") == 0) {
                JSON asks;
                // получаем итератор на начало map
                auto begin_map = direct_itr->second.begin();
                // вычисляем на сколько можем сместиться
                auto remain_count = ((direct_itr->second.size() < sended_size) ? direct_itr->second.size() : sended_size);
                // смещаемся на N
                std::advance(begin_map, direct_itr->second.size() - remain_count);
                //std::cout << " смещаемся на " << direct_itr->second.size() - remain_count << std::endl;
                for (ptr = begin_map; ptr != direct_itr->second.end(); ptr++) {
                    //j_no_init_list.push_back(2);
                    asks.push_back({ptr->first, ptr->second});
                }
                data["asks"] = asks;
            } else {
                JSON bids;
                // получаем обратный итератор на начало map
                auto rbegin_map = direct_itr->second.rbegin();
                // вычисляем сможем ли сдвинуть на N элементов
                auto remain_count = ((direct_itr->second.size() < sended_size) ? direct_itr->second.size() : sended_size);
                // смещаемся на  (map.size - N)
                std::advance(rbegin_map, (direct_itr->second.size() - remain_count));
                for (rptr = rbegin_map; rptr != direct_itr->second.rend(); rptr++) {
                    //j_no_init_list.push_back(2);
                    bids.push_back({rptr->first, rptr->second});
                }
                data["bids"] = bids;
            }
        }
        data["symbol"]    = market_itr->first;
        data["timestamp"] = 1499280391811876;
        orderbook_root["data"] = data;

        orderbook_sender(orderbook_root.dump());
    }
//    uint64_t stop2 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
//    std::cout << stop2-start2 << std::endl;
}
//---------------------------------------------------------------
// отправляет ордербук
//---------------------------------------------------------------
void gateway::orderbook_sender(std::string_view orderbook_)
{
    //std::cout << orderbook_ << std::endl;
    //std::cout << orderbook_.size() << std::endl;
    const std::int64_t result = _orderbook_channel->offer(orderbook_.data());
    if(result < 0)
    {
        processing_error("error: Ошибка отправки информации об ордербуке в ядро: ", result);
    }
}
//---------------------------------------------------------------
// callback функция результата выставления оредров
//---------------------------------------------------------------
void gateway::place_order_result_handler(std::string_view message_)
{
    //std::cout << message_ << std::endl;
    try {
        // создадим парсер
        simdjson::dom::parser parser;
        // пусть парсер подготовит буфер для своих нужд
        auto &&error_code = parser.allocate(0x1000,0x04);
        // если буфер был успешно выделен
        if(simdjson::SUCCESS == error_code){
            // разбираем строку
            auto result = parser.parse(message_.data(), message_.size(), false);
            // если данные успешно разобрались
            if(simdjson::SUCCESS == result.error()){
                // получаем значение поля success
                auto element_success{result["success"].get_bool()};
                // если значения == true, значит ордер выставлен успешно
                if(element_success.value() == true){
                    auto res_value = result["result"];
//                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "(order_result_handler) Результат выставления ордера: " <<
//                                                                                res_value;
                    _general_logger->info("(order_result_handler) Результат выставления ордера: {}", res_value);
                } else if(element_success.value() == false){
                    auto err_value = result["error"];
                    //std::cout << "Ошибка выставления ордера. Причина: " << err_value.get_c_str() << std::endl;
                    _error.describe(fmt::format("(order_result_handler) Ошибка выставления ордера. Причина: {}).", std::string(err_value.get_c_str())));
                    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                    _general_logger->error(_error.to_string());
                    _error.clear();
                }
            } else{
                _error.describe(fmt::format("Ошибка разбора json фрейма. ((order_result_handler) json body: {}).", message_));
                //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        } else{
            _error.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). ((order_result_handler) json body: {}).", message_));
            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
            _general_logger->error(_error.to_string());
            _error.clear();
        }

    } catch(simdjson::simdjson_error& err){
        _error.describe(err.what() + fmt::format(" ((order_result_handler) json body: {}).", message_));
        //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
        _general_logger->error(_error.to_string());
        _error.clear();
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию о лучших ценовых предложениях
//---------------------------------------------------------------
void gateway::ticker_sender(const STicker& best_ticker_, void* id_)
{
    pt::ptree root_ticker;
    root_ticker.put("exchange", "ftx");
    //root_ticker.put("s", "BTC-USDT");
    root_ticker.put("s", best_ticker_.s);
    root_ticker.put("b", best_ticker_.b);
    root_ticker.put("B", best_ticker_.B);
    root_ticker.put("a", best_ticker_.a);
    root_ticker.put("A", best_ticker_.A);
    std::stringstream jsonForCore;
    pt::write_json(jsonForCore, root_ticker, false);
    // отправляем json в aeron
    std::string message = jsonForCore.str();
 /*   const std::int64_t result = orderbook_channel->offer(message);
    if(result < 0){
        processing_error("error: Ошибка отправки информации о лучших ценовых предложениях в ядро: ", result);
    }
    else*/{
//        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) <<
//                 //"send info about ticker: " << jsonForCore.str();
//                 fmt::format("send info about ticker ({}): ", id_) << jsonForCore.str();
        _orderbook_logger->info("send info about ticker ({}): {}", id_, jsonForCore.str());
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию об открытых ордерах
//---------------------------------------------------------------
void gateway::order_sender(const std::vector<SOrder> &orders_vector_)
{
    pt::ptree root;
    pt::ptree children;
    for(auto&& order : orders_vector_)
    {
        /*pt::ptree order_node;
        order_node.put("id",     order.id);
        order_node.put("side",   order.side);
        order_node.put("type",   order.type);
        order_node.put("price",  order.price);
        order_node.put("status", order.status);
        order_node.put("clientId",       order.clientId);*/
        pt::ptree order_node;
        order_node.put("t", "000000");
        order_node.put("T", "i");
        order_node.put("p", "g");
        order_node.put("n", "ftx");
        order_node.put("c", 0);
        order_node.put("e", "order");
        children.push_back(std::make_pair("", order_node));
    }
    root.add_child("O", children);
    std::stringstream jsonForCore;
    pt::write_json(jsonForCore, root);
    // отправляем json в ядро
    std::string message = jsonForCore.str();
    //std::cout << "order sender: " << message <<std::endl;
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Отправка информации об открытых ордерах: " << message;
    _general_logger->info("Отправка информации об открытых ордерах: {}", message);
    const std::int64_t result = _log_channel->offer(message);
    if(result < 0){
        processing_error("error: Ошибка отправки информации об открытых ордерах в ядро: ", result);
    }
    else{
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "send info about ticker: " << jsonForCore.str();
        _orderbook_logger->info("send info about ticker: {}", jsonForCore.str());
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию о балансе
//---------------------------------------------------------------
void gateway::balance_sender(const std::vector<SBState>& balances_vector)
{
    // вспомогательная строка для визуального контроля баланса в консоли/логе
    /*std::string help_string;
    // формируем json в определенном формате
    pt::ptree root;
    pt::ptree children;
    for (auto&& balanceState: balances_vector)
    {
        pt::ptree balance_node;
        balance_node.put("exchange", "ftx");
        balance_node.put("a", balanceState.coin);
        balance_node.put("f", std::to_string(balanceState.free));
        children.push_back(std::make_pair("", balance_node));
        help_string += std::to_string(balanceState.total);
        help_string += " ";
    }
    root.add_child("B", children);
    std::stringstream jsonForCore;
    pt::write_json(jsonForCore, root);
    //отправляем json в aeron
    const std::int64_t result = _balance_channel->offer(jsonForCore.str());
    if(result < 0)
    {
        processing_error("error: Ошибка отправки информации о балансе в ядро: ", result);
    }
    else
    {
//        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "send info about balance: " <<
//                                                                 jsonForCore.str()<<
//                                                                 " total: " <<
//                                                                 help_string;
        _general_logger->info("send info about balance: {} total: {}", jsonForCore.str(), help_string);
    }*/
    JSON balance_root;
    balance_root["event"]     = "data";
    balance_root["exchange"]  = "ftx";
    balance_root["node"]      = "gate";
    balance_root["instance"]  = "1";
    balance_root["action"]    = "balances";
    balance_root["message"]   = nullptr;
    balance_root["algo"]      = "3t_php";
    balance_root["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    JSON data;
    for (auto&& balance: balances_vector) {
        JSON asset;
        asset["free"]  = balance.free;
        asset["used"]  = balance.usdValue;
        asset["total"] = balance.total;
        data[balance.coin] = asset;
    }
    balance_root["data"] = data;
    //отправляем json в aeron
    //std::cout << balance_root.dump() << std::endl;
    const std::int64_t result = _balance_channel->offer(balance_root.dump());
    if(result < 0)
    {
        processing_error("error: Ошибка отправки информации о балансе в ядро: ", result);
    }
    else
    {
//        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "send info about balance: " <<
//                                                                 jsonForCore.str()<<
//                                                                 " total: " <<
//                                                                 help_string;
        _general_logger->info("send info about balance: {}", balance_root.dump());
        // вставим пустую строку для наглядности
        std::cout << "" << std::endl;
    }
}
//---------------------------------------------------------------
// обрабатывает ошибку
//---------------------------------------------------------------
void gateway::processing_error(std::string_view message_, const std::int64_t& error_code_)
{
    //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << message_;
    _general_logger->info(message_);
    if(error_code_ == BACK_PRESSURED)
    {
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed due to back pressure.";
        _general_logger->error("Offer failed due to back pressure.");
    }
    else if(error_code_ == NOT_CONNECTED)
    {
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed because publisher is not conntected to a core.";
        _general_logger->error("Offer failed because publisher is not conntected to a core.");
    }
    else if(error_code_ == ADMIN_ACTION)
    {
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed because of an administration action in the system.";
        _general_logger->error("Offer failed because of an administration action in the system.");
    }
    else if(error_code_ == PUBLICATION_CLOSED)
    {
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed because publication is closed.";
        _general_logger->error("Offer failed because publication is closed.");
    }
    else
    {
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed due to unknkown reason.";
        _general_logger->error("Offer failed due to unknkown reason.");
    }
}
//---------------------------------------------------------------
// перезапускает публичный WS
//---------------------------------------------------------------
void gateway::restart_public_ws()
{
    ioc.reset();
    _ftx_ws_public.reset();
    create_public_ws();
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "restart public ws channel";
    _general_logger->info("restart public ws channel.");
}
//---------------------------------------------------------------
// перезапускает приватный WS
//---------------------------------------------------------------
void gateway::restart_private_ws(const std::string& reason_)
{
    ioc.reset();
    _ftx_ws_private.reset();
    // потупим немного
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    // создаём сокет заново
    create_private_ws();
    start_trigger = true;
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "restart private ws channel: " << reason_;
    _general_logger->info("restart private ws channel: {}", reason_);
}
//---------------------------------------------------------------
// перезапускает приватный REST
//---------------------------------------------------------------
/*void Gateway::restart_private_REST()
{
    ioc.reset();
    ftx_rest_private.reset();
    create_private_REST();
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "restart private REST channel";
}*/
void gateway::get_precision(SCurrencyCharacteristics& curr_characteristic_)
{
    curr_characteristic_.pricePrecision = get_precision(curr_characteristic_.priceIncrement);
    curr_characteristic_.sizePrecision  = get_precision(curr_characteristic_.sizeIncrement);
}
int gateway::get_precision(double value)
{
    int precision = 0;
    std::stringstream ss;
    ss << value;
    std::string CheckString = ss.str();
    size_t dotPos = CheckString.find('.');
    if(dotPos != CheckString.npos)
    {
        precision = CheckString.size() - 1 - dotPos;
    }
    return precision;
}
std::string gateway::set_size_precision(std::string value)
{
    // зададим значение по умолчанию
    std::string result = value;
    size_t dotPos = value.find('.');
    if(dotPos != value.npos)
    {
        value.erase(dotPos + (curr_characters.sizePrecision + 1));
        result = value;
    }
    return result;
}
std::string gateway::set_price_precision(std::string value)
{
    // зададим значение по умолчанию
    std::string result = value;
    size_t dotPos = value.find('.');
    if(dotPos != value.npos)
    {
        value.erase(dotPos + (curr_characters.pricePrecision));
        result = value;
    }
    return result;
}
//---------------------------------------------------------------
// проверяет баланс
//---------------------------------------------------------------
void gateway::check_balance(/*const bool &start_trigger_*/)
{
    // получаем баланс
    std::vector<SBState> balances_vector = _ftx_rest_private->get_balances(_error);
    // если вектор нулевого размера, значит была какая-то ошибка и баланс мы не получили
    if(balances_vector.empty()){
        if(_error){
            _error.describe("Ошибка получения баланса (get_balances).");
            //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    }
    else{
        // отправляем баланс в ядро
        balance_sender(balances_vector);
    }
}
//---------------------------------------------------------------
// отправляет ошибки
//---------------------------------------------------------------
void gateway::error_sender(std::string_view message_)
{
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << message_;
    _general_logger->info(message_);
}
//---------------------------------------------------------------
// // запрос на получение полного конфига
//---------------------------------------------------------------
void gateway::get_full_config_request()
{
    JSON full_cfg_request;
    full_cfg_request["event"]       = "command";
    full_cfg_request["exchange"]    = "ftx";
    full_cfg_request["node"]        = "gate";
    full_cfg_request["instance"]    = "1";
    full_cfg_request["action"]      = "get_config";
    full_cfg_request["message"]     = nullptr;
    full_cfg_request["algo"]        = nullptr;
    full_cfg_request["timestamp"]   = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    full_cfg_request["data"]        = nullptr;

    const std::int64_t result = _publisher_agent_channel->offer(full_cfg_request.dump());
    if (result < 0) {
        processing_error("error: Ошибка отправки запроса получения полного конфига: ", result);
    } else {
        //BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "send info about ticker: " << full_cfg_request.dump();
    }
}
}

