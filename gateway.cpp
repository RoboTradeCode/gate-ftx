#include "gateway.hpp"


using namespace aeron::util;
using namespace aeron;

namespace ftx {

gateway::gateway(const std::string& config_file_path_)
 : _general_logger(spdlog::get("general")),
   _logs_logger(spdlog::get("logs")),
   _pingpong_logger(spdlog::get("pingpong")),
   _balances_logger(spdlog::get("balances")),
   _errors_logger(spdlog::get("errors"))
{
    // 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
    _general_logger->info("Starting...");
    _socket_data_counter = 0;
    // получаем дефолтную конфигурацию
    _default_config = parse_config(config_file_path_);
    bss::error error;
    // создаём канал агента
    if (!create_aeron_agent_channels(error)) {
        error.describe("Ошибка создание каналов aeron агента для получения конфигурации.");
        _general_logger->error(error.to_string());
        error.clear();
    } else {}
}
//--------------------------------------------------------
//
//--------------------------------------------------------
bool gateway::preparation_for_launch() {
    _error.clear();
    // запоминаем время для отправки пинга
    _last_ping_time = std::chrono::system_clock::now();
    // создаём каналы aeron, websocket и rest
    if (create_aeron_core_channels(_error)) {
        if (create_private_REST(_error)) {
            if (create_private_ws(_error)) {
                if (create_public_ws(_error)) {
                    return true;
                } else {
                    _general_logger->error(_error.to_string());
                    _error.clear();
                    return false;
                }
            } else {
                _general_logger->error(_error.to_string());
                _error.clear();
                return false;
            }
        } else {
            _general_logger->error(_error.to_string());
            _error.clear();
            return false;
        }

    } else {
        _general_logger->error(_error.to_string());
        _error.clear();
        return false;
    }
}
//--------------------------------------------------------
// создаёт канал для приёма конфига от агента
//--------------------------------------------------------
bool gateway::create_aeron_agent_channels(bss::error& error_) {
    bool result = true;
    bss::error error;
    try {
        // установка соединения с каналом aeron, в котором будем "ловить" полный конфиг
        _subscriber_agent_channel = std::make_shared<Subscriber>([&](std::string_view message)
        {shared_from_this()->config_from_agent_handler(message);},
                    _default_config.aeron_agent.subscribers.agent.channel,
                    _default_config.aeron_agent.subscribers.agent.stream_id);
    } catch (std::exception& err) {
        // добавляем описание ошибки
        error_.describe(fmt::format("Канал для получения конфига от агента не создан: {}", err.what()));
        result = false;
    }
    try {
        // установка соединения с каналом aeron, в котором будем запрашивать полный конфиг
        _publisher_agent_channel = std::make_shared<Publisher>(_default_config.aeron_agent.publishers.agent.channel,
                                                              _default_config.aeron_agent.publishers.agent.stream_id);
    } catch (std::exception& err) {
        // добавляем описание ошибки
        error_.describe(fmt::format("Канал для запроса на получение конфига не создан: {}", err.what()));
        result = false;
    }
    try {
        // установка соединения с каналом aeron, в который будем посылать логи работы
        _publisher_logs_channel  = std::make_shared<Publisher>(_default_config.aeron_agent.publishers.logs.channel,
                                                              _default_config.aeron_agent.publishers.logs.stream_id);
    } catch (std::exception& err) {
        // добавляем описание ошибки
        error_.describe(fmt::format("Канал для отправки логов агенту не создан: {}", err.what()));
        result = false;
    }
    return result;
}
//---------------------------------------------------------------
// загружает конфиг из файла
//---------------------------------------------------------------
bool gateway::load_config(bss::error& error_) noexcept {

    // создаем парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд (если будет меньше 0x08, то будет ошибка)
    auto &&error_code = parser.allocate(0x1000, 0x12);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        auto load_result = simdjson::padded_string::load("config.json");
        //std::cout << load_result << std::endl;
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
                    //return false;
                }
                // получим рынки, с которыми предстоит работать
                if (auto markets_array{result["data"]["markets"].get_array()}; simdjson::SUCCESS == markets_array.error()){
                    for (auto market : markets_array) {
                        if (auto common_symbol_element{market["common_symbol"].get_c_str()}; simdjson::SUCCESS == common_symbol_element.error()) {
                            //std::cout << common_symbol_element.value() << std::endl;
                            //std::string_view mrk = common_symbol_element.value();
                            _work_config._markets.push_back(common_symbol_element.value());
                        } else {
                            error_.describe("При загрузке конфигурации в теле json не найден объект \"common_symbol\".");
                        }
                    }
                } else {
                    error_.describe("При загрузке конфигурации в теле json не найден объект \"maktets\".");
                }
                // получим часть пути для сокращения полного пути до элементов
                if (auto cfg = result["data"]["configs"]["gate_config"]; simdjson::SUCCESS == cfg.error()) {
                    // получаем имя биржи
                    if (auto name_element{cfg["info"]["exchange"].get_string()}; simdjson::SUCCESS == name_element.error()){
                        _work_config.exchange.name = name_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"name\".");
                    }
                    // получаем instance
                    if (auto instance_element{cfg["info"]["instance"].get_int64()}; simdjson::SUCCESS == instance_element.error()) {
                        _work_config.exchange.instance = instance_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"instance\".");
                    }
                    // получаем node
                    if (auto node_element{cfg["info"]["node"].get_string()}; simdjson::SUCCESS == node_element.error()) {
                        _work_config.exchange.node = node_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"node\".");
                    }
                    // получаем глубину стакана
                    if (auto orderbook_depth_element{cfg["info"]["depth"].get_int64()}; simdjson::SUCCESS == orderbook_depth_element.error()) {
                        _work_config.exchange.orderbook_depth = orderbook_depth_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"depth\".");
                    }
                    // получаем значения ping
                    if (auto ping_delay_element{cfg["info"]["ping_delay"].get_int64()}; simdjson::SUCCESS == ping_delay_element.error()) {
                        _work_config.exchange.ping_delay = ping_delay_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"ping_delay\".");
                    }
                    // получаем ключ
                    if (auto api_key_element{cfg["account"]["api_key"].get_string()}; simdjson::SUCCESS == api_key_element.error()) {
                        _work_config.account.api_key = api_key_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"api_key\".");
                    }
                    // получаем ключ
                    if (auto secret_key_element{cfg["account"]["secret_key"].get_string()}; simdjson::SUCCESS == secret_key_element.error()) {
                        _work_config.account.secret_key = secret_key_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"secret_key\".");
                    }
                    // получаем данные для канала оредбуков
                    if (auto orderbook_channel_element{cfg["aeron"]["publishers"]["orderbooks"]["channel"].get_string()}; simdjson::SUCCESS == orderbook_channel_element.error()) {
                        _work_config.aeron_core.publishers.orderbook.channel = orderbook_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook channel\".");
                    }
                    if (auto orderbook_stream_element{cfg["aeron"]["publishers"]["orderbooks"]["stream_id"].get_int64()}; simdjson::SUCCESS == orderbook_stream_element.error()) {
                        _work_config.aeron_core.publishers.orderbook.stream_id = orderbook_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook stream_id\".");
                    }
                    // получаем данные для канала балансов
                    if (auto balance_channel_element{cfg["aeron"]["publishers"]["balances"]["channel"].get_string()}; simdjson::SUCCESS == balance_channel_element.error()) {
                        _work_config.aeron_core.publishers.balance.channel = balance_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"balance channel\".");
                    }
                    if (auto balance_stream_element{cfg["aeron"]["publishers"]["balances"]["stream_id"].get_int64()}; simdjson::SUCCESS == balance_stream_element.error()) {
                        _work_config.aeron_core.publishers.balance.stream_id = balance_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"balance stream_id\".");
                    }
                    // получаем данные для канала логов
                    if (auto log_channel_element{cfg["aeron"]["publishers"]["logs"]["channel"].get_string()}; simdjson::SUCCESS == log_channel_element.error()) {
                        _work_config.aeron_core.publishers.logs.channel = log_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"log channel\".");
                    }
                    if (auto log_stream_element{cfg["aeron"]["publishers"]["logs"]["stream_id"].get_int64()}; simdjson::SUCCESS == log_stream_element.error()) {
                        _work_config.aeron_core.publishers.logs.stream_id = log_stream_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"log stream_id\".");
                    }
                    // получаем данные для канала статуса ордеров
                    if (auto order_status_channel_element{cfg["aeron"]["publishers"]["order_statuses"]["channel"].get_string()}; simdjson::SUCCESS == order_status_channel_element.error()) {
                        _work_config.aeron_core.publishers.order_status.channel = order_status_channel_element.value();
                    } else {
                        error_.describe("При загрузке конфигурации в теле json не найден оъект \"order_status channel\".");
                    }
                    if (auto order_status_stream_element{cfg["aeron"]["publishers"]["order_statuses"]["stream_id"].get_int64()}; simdjson::SUCCESS == order_status_stream_element.error()) {
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
                error_.describe(fmt::format("Ошибка разбора json фрейма в процессе парсинга конфига. Код ошибки: {}.", result.error()));
            }
        } else {
            error_.describe("Ошибка загрузки файла конфигурации.");
        }
    } else {
        error_.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
    }
    if (not error_)
        return true;
    else
        return false;
}
//---------------------------------------------------------------
//
//---------------------------------------------------------------
bool gateway::create_aeron_core_channels(bss::error& error_)
{
    bool result = true;
    try {
        // установка соединений с каналом aeron
        _balance_channel      = std::make_shared<Publisher>(_work_config.aeron_core.publishers.balance.channel,      _work_config.aeron_core.publishers.balance.stream_id);
    } catch (const std::exception& err) {
        // добавляем описание ошибки
        error_.describe(fmt::format("Канал для отправки балансов не создан: {}", err.what()));
        result = false;
    }
    try {
        _orderbook_channel    = std::make_shared<Publisher>(_work_config.aeron_core.publishers.orderbook.channel,    _work_config.aeron_core.publishers.orderbook.stream_id);
    } catch (const std::exception& err) {
        // добавим описание ошибки
        error_.describe(fmt::format("Канал для отправки ордербуков не создан: {}", err.what()));
        result = false;
    }
    try {
        _log_channel          = std::make_shared<Publisher>(_work_config.aeron_core.publishers.logs.channel,         _work_config.aeron_core.publishers.logs.stream_id);
    } catch (const std::exception& err) {
        error_.describe(fmt::format("Канал для отправки логов не создан: {}", err.what()));
        result = false;
    }
    try {
        _order_status_channel = std::make_shared<Publisher>(_work_config.aeron_core.publishers.order_status.channel, _work_config.aeron_core.publishers.order_status.stream_id);
    }  catch (const std::exception& err) {
        error_.describe(fmt::format("Канал для отправки статуса ордеров не создан: {}", err.what()));
        result = false;
    }
    try {
        _core_channel         = std::make_shared<Subscriber>([&](std::string_view message)
                {shared_from_this()->aeron_handler(message);},
                _work_config.aeron_core.subscribers.core.channel, _work_config.aeron_core.subscribers.core.stream_id);
    } catch (const std::exception& err) {
        error_.describe(fmt::format("Канал для приёма команд не создан: {}", err.what()));
        result = false;
    }
    return result;
}
//---------------------------------------------------------------
// создаёт публичный WS
//---------------------------------------------------------------
bool gateway::create_public_ws(bss::error& error_) {
    try {
        _ftx_ws_public    = std::make_shared<ftx::WSClient>("",
                                                            "",
                                                            _ioc,
                                                            [&](std::string_view message_, void* id_)
                                {shared_from_this()->public_ws_handler(message_, id_);},
                                                            _errors_logger);
        //ftx_ws_public->subscribe_orderbook("ETH/USDT");
        // подписываемся на канал ордербуков
        for (auto market : _work_config._markets) {
            size_t szt = _ftx_ws_public->subscribe_orderbook(market);
            _general_logger->info("Подписываемся на {} в публичном канале. Результат: {}", market, szt);
        }
        _sended_orderbook_depth = _work_config.exchange.orderbook_depth;
        return true;
    } catch (const std::exception& err) {
        error_.describe(fmt::format("Ошибка создания публичного WebSocket канала: {}", err.what()));
        return false;
    }
}
//---------------------------------------------------------------
// создаёт приватный WS
//---------------------------------------------------------------
bool gateway::create_private_ws(bss::error& error_) {
    try {
        _ftx_ws_private   = std::make_shared<ftx::WSClient>(_work_config.account.api_key,
                                                            _work_config.account.secret_key,
                                                            _ioc,
                                                            [&](std::string_view message_, void* id_)
                                {shared_from_this()->private_ws_handler(message_, id_);},
                                                            _errors_logger);
        // авторизуемся в приватном ws
        std::string error;
        // количество попыток
        int try_count = 0;
        size_t login_result = _ftx_ws_private->login(error);
        if(!login_result){
            _general_logger->error("Ending...");
            exit(0);
        }
        // подписываемся на приватный поток order
//        for (auto market : _work_config._markets) {
//            size_t szt = _ftx_ws_private->subscribe_order(market);
//            _general_logger->info("Подписались на {} в приватном WS канале. Результат: {}", market, szt);
//        }
        // похоже надо просто подписываться на канал для всех рынков
        _ftx_ws_private->subscribe_order();
        return true;
    } catch (const std::exception& err) {
        error_.describe(fmt::format("Ошибка создания приватного WebSocket канала: {}", err.what()));
        return false;
    }
}
//---------------------------------------------------------------
// создаёт приватный REST
//---------------------------------------------------------------
bool gateway::create_private_REST(bss::error& error_) {
    _ftx_rest_private = std::make_shared<ftx::RESTClient>(_work_config.account.api_key,
                                                          _work_config.account.secret_key);
    if (!_ftx_rest_private) {
        error_.describe("Ошибка создания приватного REST канала");
        return false;
    } else {
        return true;
    }

}
void gateway::pool() {
    _core_channel->poll();
    _ioc.run_for(std::chrono::microseconds(100));
    // каждые 15 секунд будем дёргать приватный WS и общественный WS
    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _last_ping_time) > 15s) {
        _ftx_ws_private->ping();
        _ftx_ws_public->ping();
        _last_ping_time = std::chrono::system_clock::now();
        // если 5 минут не было ничего кроме понга в приватном канале, то нам надо перегрузить канал
        /*if(ws_control >= 20){
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
        }*/
    }
}
void gateway::pool_from_agent() {
    _subscriber_agent_channel->poll();
}
//---------------------------------------------------------------
// принимает конфиг от агента
//---------------------------------------------------------------
void gateway::config_from_agent_handler(std::string_view message_) {
    // это временная мера
    if (_config_was_received == true)
        return;
    _error.clear();
    //std::cout << "config was received" << message_ << std::endl;
    // создаем парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд (если будет меньше 0x08, то будет ошибка)
    auto &&error_code = parser.allocate(0x1000, 0x10);
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
            // получим рынки, с которыми предстоит работать
            if (auto markets_array{parse_result["data"]["markets"].get_array()}; simdjson::SUCCESS == markets_array.error()){
                for (auto market : markets_array) {
                    if (auto common_symbol_element{market["common_symbol"].get_c_str()}; simdjson::SUCCESS == common_symbol_element.error()) {
                        //std::cout << common_symbol_element.value() << std::endl;
                        //std::string_view mrk = common_symbol_element.value();
                        _work_config._markets.push_back(common_symbol_element.value());
                    } else {
                        _error.describe("При загрузке конфигурации в теле json не найден объект \"common_symbol\".");
                    }
                }
            } else {
                _error.describe("При загрузке конфигурации в теле json не найден объект \"maktets\".");
            }
            // получим часть пути для сокращения полного пути до элементов
            if (auto cfg = parse_result["data"]["configs"]["gate_config"]; simdjson::SUCCESS == cfg.error()) {
                // получаем имя биржи
                if (auto name_element{cfg["info"]["exchange"].get_string()}; simdjson::SUCCESS == name_element.error()){
                    _work_config.exchange.name = name_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"name\".");
                }
                // получаем instance
                if (auto instance_element{cfg["info"]["instance"].get_int64()}; simdjson::SUCCESS == instance_element.error()) {
                    _work_config.exchange.instance = instance_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"instance\".");
                }
                // получаем node
                if (auto node_element{cfg["info"]["node"].get_string()}; simdjson::SUCCESS == node_element.error()) {
                    _work_config.exchange.node = node_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"node\".");
                }
                // получаем глубину стакана
                if (auto orderbook_depth_element{cfg["info"]["depth"].get_int64()}; simdjson::SUCCESS == orderbook_depth_element.error()) {
                    _work_config.exchange.orderbook_depth = orderbook_depth_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"depth\".");
                }
                // получаем значения ping
                if (auto ping_delay_element{cfg["info"]["ping_delay"].get_int64()}; simdjson::SUCCESS == ping_delay_element.error()) {
                    _work_config.exchange.ping_delay = ping_delay_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"ping_delay\".");
                }
                // получаем ключ
                if (auto api_key_element{cfg["account"]["api_key"].get_string()}; simdjson::SUCCESS == api_key_element.error()) {
                    _work_config.account.api_key = api_key_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"api_key\".");
                }
                // получаем ключ
                if (auto secret_key_element{cfg["account"]["secret_key"].get_string()}; simdjson::SUCCESS == secret_key_element.error()) {
                    _work_config.account.secret_key = secret_key_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"secret_key\".");
                }
                // получаем данные для канала оредбуков
                if (auto orderbook_channel_element{cfg["aeron"]["publishers"]["orderbooks"]["channel"].get_string()}; simdjson::SUCCESS == orderbook_channel_element.error()) {
                    _work_config.aeron_core.publishers.orderbook.channel = orderbook_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook channel\".");
                }
                if (auto orderbook_stream_element{cfg["aeron"]["publishers"]["orderbooks"]["stream_id"].get_int64()}; simdjson::SUCCESS == orderbook_stream_element.error()) {
                    _work_config.aeron_core.publishers.orderbook.stream_id = orderbook_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"orderbook stream_id\".");
                }
                // получаем данные для канала балансов
                if (auto balance_channel_element{cfg["aeron"]["publishers"]["balances"]["channel"].get_string()}; simdjson::SUCCESS == balance_channel_element.error()) {
                    _work_config.aeron_core.publishers.balance.channel = balance_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"balance channel\".");
                }
                if (auto balance_stream_element{cfg["aeron"]["publishers"]["balances"]["stream_id"].get_int64()}; simdjson::SUCCESS == balance_stream_element.error()) {
                    _work_config.aeron_core.publishers.balance.stream_id = balance_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"balance stream_id\".");
                }
                // получаем данные для канала логов
                if (auto log_channel_element{cfg["aeron"]["publishers"]["logs"]["channel"].get_string()}; simdjson::SUCCESS == log_channel_element.error()) {
                    _work_config.aeron_core.publishers.logs.channel = log_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"log channel\".");
                }
                if (auto log_stream_element{cfg["aeron"]["publishers"]["logs"]["stream_id"].get_int64()}; simdjson::SUCCESS == log_stream_element.error()) {
                    _work_config.aeron_core.publishers.logs.stream_id = log_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"log stream_id\".");
                }
                // получаем данные для канала статуса ордеров
                if (auto order_status_channel_element{cfg["aeron"]["publishers"]["order_statuses"]["channel"].get_string()}; simdjson::SUCCESS == order_status_channel_element.error()) {
                    _work_config.aeron_core.publishers.order_status.channel = order_status_channel_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"order_status channel\".");
                }
                if (auto order_status_stream_element{cfg["aeron"]["publishers"]["order_statuses"]["stream_id"].get_int64()}; simdjson::SUCCESS == order_status_stream_element.error()) {
                    _work_config.aeron_core.publishers.order_status.stream_id = order_status_stream_element.value();
                } else {
                    _error.describe("При загрузке конфигурации в теле json не найден оъект \"order_status stream_id\".");
                }
                // получаем данные для канала команд
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
                std::cout << _error.to_string() << std::endl;
                //if (!_error) объекта is_new нет, и поэтому _error будет содержать ошибку и мы тогда не сможем установиоть флаг
                    // установим флаг о том что конфиг был получен
                    _config_was_received = true;
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
void gateway::aeron_handler(std::string_view message_) {
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
                            if (exchange_element.value() == _work_config.exchange.name) {
                                std::string_view action;
                                std::string_view side;
                                std::string symbol;
                                int64_t order_id;
                                double price;
                                double amount;
                                if (auto action_element{parse_result["action"].get_string()}; simdjson::SUCCESS == action_element.error()) {
                                    action = action_element.value();
                                } else {}
                                if (auto data_element_array{parse_result["data"].get_array()}; simdjson::SUCCESS == data_element_array.error())
                                {
                                    for (auto data_element : data_element_array) {
                                        // получаем рынок
                                        if (auto symbol_element{data_element["symbol"]}; simdjson::SUCCESS == symbol_element.error()) {
                                            symbol = symbol_element.value();
                                        } else {}
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
                                        if (auto order_id_element{data_element["id"].get_int64()}; simdjson::SUCCESS == order_id_element.error()) {
                                            order_id = order_id_element.value();
                                        }
                                        // выставлен ордер на отмену продажи
                                        if (action.compare("cancel_order") == 0) {
                                            cancel_order(order_id);
                                        }
                                        // выставлен ордер на покупку
                                        else if (action.compare("create_order") == 0) {
                                            create_order(side, symbol, price, amount);
                                        } else if (action.compare("get_balances") == 0){
                                            check_balances();
                                        } else if (action.compare("cancel_all_orders") == 0) {
                                            cancel_all_orders();
                                        } else {
                                            //_error.describe("Не могу распознать action и side в команде от ядра {}.", message_);
                                            _general_logger->error("Не могу распознать action и side в команде от ядра {}.", message_.data());
                                        }
                                    }
                                } else {
                                    if (action.compare("get_balances") == 0) {
                                        check_balances();
                                    } else if (action.compare("cancel_all_orders") == 0) {
                                        cancel_all_orders();
                                    } else {
                                        //_error.describe(fmt::format("Не могу получить массив данных data {}.", message_.data()));
                                        _general_logger->error("Не могу получить массив данных data {}.", message_.data());
                                    }
                                }
                            } else {
                                //_error.describe(fmt::format("В команде неверно указана биржа: {}.", message_.data()));
                                _general_logger->error("В команде неверно указана биржа: {}.", message_.data());
                            }
                        } else {

                        }
                    } else {
                          //_error.describe(fmt::format("В команде неверно указан event: {}.", message_.data()));
                        _general_logger->error("В команде неверно указан event: {}.", message_.data());
                    }
                }
            }
            else
            {
                //_error.describe("Ошибка разбора json фрейма.");
                //_general_logger->error(_error.to_string());
                //_error.clear();
                _general_logger->error("Ошибка разбора json фрейма при получении команды от ядра: {}.", message_.data());
            }
        }
        else
        {
            //_error.describe("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).");
            _general_logger->error("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).");
            //_error.clear();
        }
    }
    catch(simdjson::simdjson_error& err) {
        //_error.describe(err.what() + fmt::format(" (json body: {}).", message_));
        std::string error_description = err.what() + fmt::format(" (json body: {}).", message_);
        _general_logger->error(error_description);
        //_error.clear();
    }
    //std::cout << "------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << "" << std::endl;
}
//---------------------------------------------------------------
// отменяет ордер по id
//---------------------------------------------------------------
void gateway::cancel_order(const int64_t &order_id) {
    _general_logger->info("Получена команда на отмену ордера: order_id = {}.", order_id);
    _error.clear();
    std::string cancel_order_result = _ftx_rest_private->cancel_order(std::to_string(order_id), _error);
    if(_error) {
        _error.describe("Ошибка отмены ордера");
        _general_logger->error("Результат отмены ордера (id): {} {} {}", order_id, cancel_order_result, _error.to_string());
        order_status_prepare("order_cancel", "Order was cancel", cancel_order_result, true, _error.to_string());
        _error.clear();
    } else {
        _general_logger->info("Результат отмены ордера (id): {} {}", order_id, cancel_order_result);
        order_status_prepare("order_cancel", "Order was cancel", cancel_order_result);
    }
}
//---------------------------------------------------------------
// отменяет все ордера
//---------------------------------------------------------------
void gateway::cancel_all_orders() {
    _general_logger->info("Получена команда на отмену всех ордеров");
    _error.clear();
    for (auto market : _work_config._markets) {
//        std::string cancel_all_order_result = _ftx_rest_private->cancel_all_orders(market, _error);
//        if (_error) {
//            _error.describe(fmt::format("Ошибка отмены ордеров для {}", market));
//            _general_logger->error("Результат отмены всех ордеров: {}, ошибка: {}", cancel_all_order_result, _error.to_string());
//            order_status_prepare("cancel_all_orders", "All orders was cancel", cancel_all_order_result, true, _error.to_string());
//            _error.clear();
//        } else {
//            _general_logger->info("Результат отмены всех ордеров для {} {}", market, cancel_all_order_result);
//            order_status_prepare("cancel_all_orders", "All orders was cancel", cancel_all_order_result);
//        }
        std::make_shared<ftx::AsyncRESTClient>(_work_config.account.api_key,
                                               _work_config.account.secret_key,
                                               _ioc,
                                               [&](std::string_view message_)
                 {shared_from_this()->place_order_result_handler(message_);})->cancel_all_orders(market);
    }
}
//---------------------------------------------------------------
// обрабатывает команду на создания ордера
//---------------------------------------------------------------
void gateway::create_order(std::string_view side_, const std::string& symbol_, const double& price_, const double& quantity_) {
    _error.clear();
    if (side_.compare("buy") == 0) {
        _general_logger->info("Ядром выставлен ордер на покупку {} с ценой: {} и объёмом {}", symbol_, price_, quantity_);
        // выставляем ордер (синхронно)
        /*std::string place_order_result = _ftx_rest_private->place_order(symbol_, "buy", price_, quantity_, _error);
        if(_error) {
            _general_logger->error("Ошибка выставления ордера на покупку: {} {}", place_order_result, _error.to_string());
            order_status_prepare("order_created", "Order was created", place_order_result, true, _error.to_string());
            _error.clear();
        } else {
            _general_logger->info("Результат выставления ордера на покупку: {}", place_order_result);
            order_status_prepare("order_created", "Order was created", place_order_result);
        }*/
        // выставляем ордер асинхронно
        std::make_shared<ftx::AsyncRESTClient>(_work_config.account.api_key,
                                               _work_config.account.secret_key,
                                               _ioc,
                                               [&](std::string_view message_)
                 {shared_from_this()->place_order_result_handler(message_);})->place_order(symbol_, "buy", price_, quantity_);
    }
    else if (side_.compare("sell") == 0) {
        _general_logger->info("Ядром выставлен ордер на продажу {} с ценой: {} и объёмом {}", symbol_, price_, quantity_);
        // выставляем ордер (синхронно)
        /*std::string place_order_result = _ftx_rest_private->place_order(symbol_, "sell", price_, quantity_, _error);
        if(_error) {
            _general_logger->error("Ошибка выставления ордера на продажу: {} {}", place_order_result, _error.to_string());
            order_status_prepare("order_created", "Order was created", place_order_result, true, _error.to_string());
            _error.clear();
        } else {
            _general_logger->info("Результат выставления ордера на продажу: {}", place_order_result);
            order_status_prepare("order_created", "Order was created", place_order_result);
        }*/
        std::make_shared<ftx::AsyncRESTClient>(_work_config.account.api_key,
                                               _work_config.account.secret_key,
                                               _ioc,
                                               [&](std::string_view message_)
                 {shared_from_this()->place_order_result_handler(message_);})->place_order(symbol_, "sell", price_, quantity_);
    }
}
//---------------------------------------------------------------
// подготавливаем json order_status
//---------------------------------------------------------------
void gateway::order_status_prepare(std::string_view action_, std::string_view message_, std::string_view place_result, bool is_error, std::string error_) {
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
                    } else {}
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
    order_status_root["exchange"]  = _work_config.exchange.name;
    order_status_root["node"]      = _work_config.exchange.node;
    order_status_root["instance"]  = _work_config.exchange.instance;
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
// отправляет order_status
//---------------------------------------------------------------
void gateway::order_status_sender(std::string_view order_status_) {
    std::int64_t result = _order_status_channel->offer(order_status_.data());
    if (result < 0) {
        processing_error("error: Ошибка отправки информации о статусе ордера в ядро: ", _prev_order_status_message_core, result);
    } else {
        _prev_order_status_message_core = fmt::format("Результат: {}. Сообщение: {}", result, order_status_.data());
    }
    // теперь отравим все это дело в лог
    result = _log_channel->offer(order_status_.data());
    if (result < 0) {
        processing_error("error: Ошибка отправки информации о статусе ордера в лог: ", _prev_order_status_message_log, result);
    } else {
        _logs_logger->info(order_status_.data());
        _prev_order_status_message_log = fmt::format("Результат: {}. Сообщение: {}", result, order_status_.data());
    }
}
//---------------------------------------------------------------
// приватный канал WS (neew refactoring, обработка ошибок и отмена try catch)
//---------------------------------------------------------------
void gateway::private_ws_handler(std::string_view message_, void* id_) {
    if (message_.compare("{\"type\":\"pong\"}") == 0) {
    //if (message_.find("pong") != std::string_view::npos) {
        //std::cout << "pong in private" << std::endl;
        return;
    } else {}
    try {
        std::string_view side;
        std::string_view status;
        double filled;
        double remaining;
        int64_t id;
        //std::cout << "" << message_ << std::endl;
        //_general_logger->info("(message from private_ws_handler): {}", message_);
        // создадим парсер
        simdjson::dom::parser parser;
        // пусть парсер подготовит буфер для своих нужд
        auto &&error_code = parser.allocate(0x1000,0x04);
        // если буфер был успешно выделен
        if (simdjson::SUCCESS == error_code) {
            // разбираем строку
            auto result = parser.parse(message_.data(), message_.size(), false);
            // если данные успешно разобрались
            if (simdjson::SUCCESS == result.error()) {
                if (auto element_type{result["type"].get_string()}; simdjson::SUCCESS == element_type.error()) {
                    if (element_type.value().compare("update") == 0) {
                        if (auto element_side{result["data"]["side"].get_string()}; simdjson::SUCCESS == element_side.error()) {
                            side = element_side.value();
                        } else {}
                        if (auto element_status{result["data"]["status"].get_string()}; simdjson::SUCCESS == element_status.error()) {
                            status = element_status.value();
                        } else {}
                        if (auto element_filled{result["data"]["filledSize"].get_double()}; simdjson::SUCCESS == element_filled.error()) {
                            filled = element_filled.value();
                        } else {}
                        if (auto element_remaining{result["data"]["remainingSize"].get_double()}; simdjson::SUCCESS == element_remaining.error()) {
                            remaining = element_remaining.value();
                        } else {}
                        if (auto element_id{result["data"]["id"].get_int64()}; simdjson::SUCCESS == element_id.error()) {
                            id = element_id.value();
                        } else {}
                        //std::string_view element_side{result["data"]["side"].get_string()};
                        //std::string_view element_status{result["data"]["status"].get_string()};
                        //double element_filled{result["data"]["filledSize"].get_double()};
                        //double element_remaining{result["data"]["remainingSize"].get_double()};
                        //int64_t element_id{result["data"]["id"].get_int64()};
                        std::string description = get_order_change_description(side, status, filled, remaining);
                        _general_logger->info("(ws private) произошли изменения в ордерах: {} object_id = {} id: {} side: {} status: {} filledSize: {} remainingSize: {}",
                                              description, id_, id, side, status, filled, remaining);
                        //std::cout << "" << std::endl;
                        // проверяем и отправляем баланс
                        check_balances();

                    } else {
                        if (element_type.value().compare("subscribed") != 0) {
                            _error.describe(fmt::format("json не содержит поле \"update\". json body: {}", message_));
                            _general_logger->error(_error.to_string());
                            _error.clear();
                        }
                    }
                } else {
                    _error.describe(fmt::format("json не содержит поле \"type\". json body: {}", message_));
                    _general_logger->error(_error.to_string());
                    _error.clear();
                }
            } else {
                _error.describe(fmt::format("Ошибка разбора json фрейма. ((ws private) json body: {}).", message_));
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        } else {
            _error.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). ((ws private) json body: {}).", message_));
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    } catch(simdjson::simdjson_error& err) {
        _error.describe(err.what() + fmt::format(" ((ws private) json body: {}).", message_));
        _general_logger->error(_error.to_string());
        _error.clear();
    }
}
//---------------------------------------------------------------
// получает более подробную информацию об изменении ордера
//---------------------------------------------------------------
std::string gateway::get_order_change_description(std::string_view side_, std::string_view status_, const double& filled_size_, const double& remaining_size_) {
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
void gateway::public_ws_handler(std::string_view message_, void* id_) {
    if (message_.compare("{\"type\":\"pong\"}") == 0) {
        _pingpong_logger->info("pong in public ws.");
        return;
    } else {}

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
                                _markets_map.insert(make_pair(market, map<string, map<double, double>, std::greater<string>>()));
                                _markets_map[market].insert(make_pair("bids", map<double, double>()));
                                _markets_map[market]["bids"].insert(make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения массива bids. json body: {}.", message_));
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
                                _markets_map.insert(make_pair(market, map<string, map<double, double>, std::greater<string>>()));
                                _markets_map[market].insert(make_pair("asks", map<double, double>()));
                                _markets_map[market]["asks"].insert(make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения массива asks. json body: {}.", message_));
                            _general_logger->error(_error.to_string());
                            _error.clear();
                        }
                        // подготовим "стакан" к отправке
                        orderbook_prepare(_markets_map);
                    } else {
                        _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}.", message_));
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
                                    } else {
                                        _markets_map[market]["bids"][bid_pair_value.at(0).get_double()] = bid_pair_value.at(1).get_double();
                                    }
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения массива bids. json body: {}.", message_));
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
                                    } else {
                                        _markets_map[market]["asks"][ask_pair_value.at(0).get_double()] = ask_pair_value.at(1).get_double();
                                    }
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения массива asks. json body: {}.", message_));
                                 _general_logger->error(_error.to_string());
                                _error.clear();
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}.", message_));
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
                                //error_sender(fmt::format("Ошибка в канале orderbook (код: {}, сообщение: \"{}\").", code_element.value(), message_element.value()));
                                send_error(fmt::format("Ошибка в канале orderbook (код: {}, сообщение: \"{}\").", code_element.value(), message_element.value()));
                            }
                        }
                    }
                    // видимо было сообщение 1 типа (subscribed)
                }
                _socket_data_counter++;
                if (std::chrono::duration_cast<std::chrono::seconds>
                        (std::chrono::system_clock::now() - _last_metric_ping_time).count() >= _work_config.exchange.ping_delay) {

                    _last_metric_ping_time = std::chrono::system_clock::now();
                    metric_sender();
                }
            } else {
                _error.describe(fmt::format("Ошибка определения типа сообщения. json body: {}.", message_));
                 _general_logger->error(_error.to_string());
                _error.clear();
            }
        } else {
            _error.describe(fmt::format("Ошибка разбора json фрейма (error: {})", result.error()));
            //error_sender(_error.to_string());
            send_error(_error.to_string());
            _general_logger->error(_error.to_string());
            _error.clear();
        }
    } else {
        _error.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
        _general_logger->error(_error.to_string());
        _error.clear();
    }
}
//---------------------------------------------------------------
// отправляет метрику
//---------------------------------------------------------------
void gateway::metric_sender(){
    JSON metric_root;
    metric_root["event"]     = "info";
    metric_root["exchange"]  = _work_config.exchange.name;
    metric_root["node"]      = _work_config.exchange.node;
    metric_root["instance"]  = _work_config.exchange.instance;
    metric_root["action"]    = "ping";
    metric_root["message"]   = nullptr;
    metric_root["algo"]      = "cross_3t_php";
    metric_root["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    metric_root["date"]      = _socket_data_counter;
    int64_t result = _log_channel->offer(metric_root.dump());
    if (result < 0) {
        processing_error("error: Ошибка отправки метрики в лог: ", _prev_order_status_message_log, result);
    } else {
        //_logs_logger->info(metric_root.dump());
        //_prev_order_status_message_log = fmt::format("Результат: {}. Сообщение: {}", result, metric_root.dump());
    }
}
//---------------------------------------------------------------
// подготавливает ордербук
//---------------------------------------------------------------
void gateway::orderbook_prepare(const map<string, map<string, map<double, double>, std::greater<string>>>& markets_map_) {
    // это надо брать из конфига _work_config.exchange.orderbook_depth
    //const int _sended_size = _work_config.exchange.orderbook_depth;
    //map<string, map<string, map<double, double>, std::greater<string>>>::const_iterator market_itr;
    //map<string, map<double, double>>::const_iterator direct_itr;
    //map<double, double>::const_iterator ptr;
    //map<double, double>::const_reverse_iterator rptr;

    //uint64_t start2 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (_market_itr = markets_map_.begin(); _market_itr != markets_map_.end(); ++_market_itr) {
        JSON orderbook_root;
        orderbook_root["event"]     = "data";
        orderbook_root["exchange"]  = _work_config.exchange.name;
        orderbook_root["node"]      = _work_config.exchange.node;
        orderbook_root["instance"]  = _work_config.exchange.instance;
        orderbook_root["action"]    = "orderbook";
        orderbook_root["message"]   = nullptr;
        orderbook_root["algo"]      = "signal";
        orderbook_root["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        //json_loh j_no_init_list = json_loh::array();
        JSON data;
        // теперь проходим по bids и asks в текущем рынке
        for (_direct_itr = _market_itr->second.begin(); _direct_itr != _market_itr->second.end(); _direct_itr++) {

            if (_direct_itr->first.compare("asks") == 0) {
                JSON asks;
                // получаем итератор на начало map
                auto begin_map = _direct_itr->second.begin();
                // вычисляем на сколько можем сместиться
                auto remain_count = ((_direct_itr->second.size() < _sended_orderbook_depth) ? _direct_itr->second.size() : _sended_orderbook_depth);
                // смещаемся на N
                std::advance(begin_map, _direct_itr->second.size() - remain_count);
                //std::cout << " смещаемся на " << direct_itr->second.size() - remain_count << std::endl;
                for (_asks_itr = begin_map; _asks_itr != _direct_itr->second.end(); _asks_itr++) {
                    //j_no_init_list.push_back(2);
                    asks.push_back({_asks_itr->first, _asks_itr->second});
                }
                data["asks"] = asks;
            } else {
                JSON bids;
                // получаем обратный итератор на начало map
                auto rbegin_map = _direct_itr->second.rbegin();
                // вычисляем сможем ли сдвинуть на N элементов
                auto remain_count = ((_direct_itr->second.size() < _sended_orderbook_depth) ? _direct_itr->second.size() : _sended_orderbook_depth);
                // смещаемся на  (map.size - N)
                std::advance(rbegin_map, (_direct_itr->second.size() - remain_count));
                for (_bids_itr = rbegin_map; _bids_itr != _direct_itr->second.rend(); _bids_itr++) {
                    //j_no_init_list.push_back(2);
                    bids.push_back({_bids_itr->first, _bids_itr->second});
                }
                data["bids"] = bids;
            }
        }
        data["symbol"]    = _market_itr->first;
        data["timestamp"] = 1499280391811876;
        orderbook_root["data"] = data;

        orderbook_sender(orderbook_root.dump());
    }
}
//---------------------------------------------------------------
// отправляет ордербук
//---------------------------------------------------------------
void gateway::orderbook_sender(std::string_view orderbook_) {
    const std::int64_t result = _orderbook_channel->offer(orderbook_.data());
    if (result < 0) {
        processing_error("error: Ошибка отправки информации об ордербуке в ядро: ", _prev_orderbook_message, result);
    } else {
        // запомним предудущее сообщение
        _prev_orderbook_message = fmt::format("Результат: {}. Сообщение: {}", result, orderbook_.data());
    }
    //std::cout << result << " length: " << orderbook_.size() << std::endl;
}
//---------------------------------------------------------------
// callback функция результата выставления оредров
//---------------------------------------------------------------
void gateway::place_order_result_handler(std::string_view message_) {
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
                if (auto element_success{result["success"].get_bool()}; simdjson::SUCCESS == element_success.error()) {
                    // если значения == true, значит ордер выставлен успешно
                    if(element_success.value() == true) {
                        auto res_value = result["result"];
                        _general_logger->info("(order_result_handler) Результат выставления ордера: {}", res_value);
                    } else if(element_success.value() == false) {
                        auto err_value = result["error"];
                        _error.describe(fmt::format("(order_result_handler) Ошибка выставления ордера. Причина: {}).", std::string(err_value.get_c_str())));
                        _general_logger->error(_error.to_string());
                        _error.clear();
                    }
                }
            } else {
                _error.describe(fmt::format("Ошибка разбора json фрейма. ((order_result_handler) json body: {}).", message_));
                _general_logger->error(_error.to_string());
                _error.clear();
            }
        } else {
            _error.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). ((order_result_handler) json body: {}).", message_));
            _general_logger->error(_error.to_string());
            _error.clear();
        }

    } catch(simdjson::simdjson_error& err) {
        _error.describe(err.what() + fmt::format(" ((order_result_handler) json body: {}).", message_));
        _general_logger->error(_error.to_string());
        _error.clear();
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию об открытых ордерах
//---------------------------------------------------------------
/*void gateway::order_sender(const std::vector<SOrder> &orders_vector_)
{
    pt::ptree root;
    pt::ptree children;
    for(auto&& order : orders_vector_)
    {
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
}*/
//---------------------------------------------------------------
// отправляет в ядро информацию о балансе
//---------------------------------------------------------------
void gateway::balance_sender(const std::vector<s_balances_state>& balances_vector) {

    JSON balance_root;
    balance_root["event"]     = "data";
    balance_root["exchange"]  = _work_config.exchange.name;
    balance_root["node"]      = _work_config.exchange.node;
    balance_root["instance"]  = _work_config.exchange.instance;
    balance_root["action"]    = "balances";
    balance_root["message"]   = nullptr;
    balance_root["algo"]      = "3t_php";
    balance_root["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    JSON data;
    for (auto&& balance: balances_vector) {
        JSON asset;
        asset["free"]  = balance.free;
        asset["used"]  = (balance.total - balance.free);
        asset["total"] = balance.total;
        data[balance.coin] = asset;
    }
    balance_root["data"] = data;
    //отправляем json в aeron
    //std::cout << balance_root.dump() << std::endl;
    std::int64_t result = _balance_channel->offer(balance_root.dump());
    if (result < 0) {
        processing_error("error: Ошибка отправки информации о балансе в ядро: ", _prev_balance_message_core, result);
    } else {
        _general_logger->info("send info about balance: {}", balance_root.dump());
        _balances_logger->info("send info about balance: {}", balance_root.dump());
        // запомним предыдущее сообщение
        _prev_balance_message_core = fmt::format("Результат: {}. Сообщение: {}", result, balance_root.dump());
    }
    // теперь отравим все это дело в лог
    result = _log_channel->offer(balance_root.dump());
    if (result < 0) {
        processing_error("error: Ошибка отправки информации о балансах в лог: ", _prev_balance_message_log, result);
    } else {
        _logs_logger->info(balance_root.dump());
        // запомним предыдущее сообщение
        _prev_balance_message_log = fmt::format("Результат: {}. Сообщение: {}", result, balance_root.dump());
    }

}
//---------------------------------------------------------------
// проверяет баланс
//---------------------------------------------------------------
void gateway::check_balances() {
    _error.clear();
    // получаем баланс
    std::vector<s_balances_state> balances_vector = _ftx_rest_private->get_balances(_error);
    // если вектор нулевого размера, значит была какая-то ошибка и баланс мы не получили
    if(balances_vector.empty()){
        if(_error){
            _error.describe("Ошибка получения баланса (get_balances).");
            _general_logger->error(_error.to_string());
            _error.clear();
        } else {}
    }
    else{
        // отправляем баланс в ядро
        balance_sender(balances_vector);
    }
}
//---------------------------------------------------------------
// обрабатывает ошибку
//---------------------------------------------------------------
void gateway::processing_error(std::string_view error_source_, const std::string& prev_messsage_, const std::int64_t& error_code_) {
    //_general_logger->info(error_source_);
    _errors_logger->info("Предыдущее успешно отправленное сообщение: {}.", prev_messsage_);
    _errors_logger->error(error_source_);
    if(error_code_ == BACK_PRESSURED)
    {
        //_general_logger->error(BACK_PRESSURED_DESCRIPTION);
        _errors_logger->error(BACK_PRESSURED_DESCRIPTION);
    }
    else if(error_code_ == NOT_CONNECTED)
    {
        //_general_logger->error(NOT_CONNECTED_DESCRIPTION);
        _errors_logger->error(NOT_CONNECTED_DESCRIPTION);
    }
    else if(error_code_ == ADMIN_ACTION)
    {
        //_general_logger->error(ADMIN_ACTION_DESCRIPTION);
        _errors_logger->error(ADMIN_ACTION_DESCRIPTION);
    }
    else if(error_code_ == PUBLICATION_CLOSED)
    {
        //_general_logger->error(PUBLICATION_CLOSED_DESCRIPTION);
        _errors_logger->error(PUBLICATION_CLOSED_DESCRIPTION);
    }
    else
    {
        //_general_logger->error(UNKNOWN_DESCRIPTION);
        _errors_logger->error(UNKNOWN_DESCRIPTION);
    }
}
//---------------------------------------------------------------
// перезапускает публичный WS
//---------------------------------------------------------------
void gateway::restart_public_ws() {
    _error.clear();
    _ioc.reset();
    _ftx_ws_public.reset();
    // потупим немного
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    create_public_ws(_error);
    _general_logger->info("restart public ws channel.");
}
//---------------------------------------------------------------
// перезапускает приватный WS
//---------------------------------------------------------------
void gateway::restart_private_ws(const std::string& reason_) {
    _error.clear();
    _ioc.reset();
    _ftx_ws_private.reset();
    // потупим немного
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // создаём сокет заново
    create_private_ws(_error);
    //start_trigger = true;
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
//---------------------------------------------------------------
// отправляет ошибки
//---------------------------------------------------------------
/*void gateway::error_sender(std::string_view message_)
{
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << message_;
    _general_logger->info(message_);
}*/
//---------------------------------------------------------------
// проверяет, получен ли конфиг
//---------------------------------------------------------------
bool gateway::has_config() {
    return _config_was_received;
}
//---------------------------------------------------------------
// посылает запрос на получение конфига
//---------------------------------------------------------------
void gateway::send_config_request() {
    get_full_config_request();
}
//---------------------------------------------------------------
// отправляет запрос на получение полного конфига
//---------------------------------------------------------------
void gateway::get_full_config_request() {
    JSON full_cfg_request;
    full_cfg_request["event"]       = "command";
    full_cfg_request["exchange"]    = _work_config.exchange.name;
    full_cfg_request["node"]        = _work_config.exchange.node;
    full_cfg_request["instance"]    = _work_config.exchange.instance;
    full_cfg_request["action"]      = "get_config";
    full_cfg_request["message"]     = nullptr;
    full_cfg_request["algo"]        = nullptr;
    full_cfg_request["timestamp"]   = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    full_cfg_request["data"]        = nullptr;

    std::int64_t result = _publisher_agent_channel->offer(full_cfg_request.dump());
    if (result < 0) {
        processing_error("error: Ошибка отправки запроса получения полного конфига: ", "none", result);
    } else {
        _general_logger->info("Отправлен запрос на получение полного конфига.");
    }
}
//---------------------------------------------------------------
// посылает ошибку в лог и в консоль
//---------------------------------------------------------------
void gateway::send_error(std::string_view error_) {
    _general_logger->error(error_);
    _errors_logger->error(error_);
}
}

