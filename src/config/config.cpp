#include "config.hpp"
#include <filesystem>

/*const char* DEFAULT_EXCHANGE_NAME      = "unknown";
const char* DEFAULT_SUBSCRIBER_CHANNEL = "aeron:ipc";
const char* DEFAULT_PUBLISHER_CHANNEL  = "aeron:ipc";
const int DEFAULT_CORE_STREAM_ID       = 103;
const int DEFAULT_BALANCE_STREAM_ID    = 101;
const int DEFAULT_TICKER_STREAM_ID     = 100;
const int DEFAULT_LOG_STREAM_ID        = 102;
const int DEFAULT_BUFFER_SIZE          = 1400;*/

gate_config parse_config(const std::string& config_file_path_){
    // проверим существование конфигурационного файла
    std::filesystem::path path(config_file_path_);
    if(not std::filesystem::exists(path) && not std::filesystem::is_regular_file(path)){
        throw std::invalid_argument("File " + config_file_path_ + " doesn't exists.");
    }
    // Инициализация структуры и корня файла конфигурации
    gate_config config;
    toml::table tbl = toml::parse_file(config_file_path_);

    // Сокращения для удобства доступа
    /*toml::node_view exchange = tbl["exchange"];
    toml::node_view account  = tbl["account"];
    toml::node_view aeron    = tbl["aeron"];
    toml::node_view balance  = aeron["publishers"]["balance"];
    toml::node_view orderbook= aeron["publishers"]["orderbook"];
    toml::node_view logs     = aeron["publishers"]["logs"];
    toml::node_view core     = aeron["subscribers"]["core"];

    // Заполнение exchange
    config.exchange.name = exchange["name"].value_or(DEFAULT_EXCHANGE_NAME);

    // Заполнение настроек аккаунта
    config.account.api_key    = account["api_key"].value_or("");
    config.account.secret_key = account["secret_key"].value_or("");

    // Заполнение balance
    config.aeron.publishers.balance.channel   = balance[0].value_or("");
    config.aeron.publishers.balance.stream_id = balance[1].value_or(0);

    // Заполнение тикеров
    config.aeron.publishers.orderbook.channel   = orderbook[0].value_or("");
    config.aeron.publishers.orderbook.stream_id = orderbook[1].value_or(0);

    // Заполнение логов
    config.aeron.publishers.logs.channel   = logs[0].value_or("");
    config.aeron.publishers.logs.stream_id = logs[1].value_or(0);

    // Заполнение ядра
    config.aeron.subscribers.core.channel   = core[0].value_or("");
    config.aeron.subscribers.core.stream_id = core[1].value_or(0);*/

    toml::node_view gate         = tbl["gate"];
    toml::node_view configurator = tbl["configuration"];
    toml::node_view aeron        = tbl["aeron"];
    toml::node_view agent_s      = aeron["subscribers"]["agent"];
    toml::node_view logs         = aeron["publishers"]["logs"];
    toml::node_view agent_p      = aeron["publishers"]["agent"];

    // получаем имя и инстанс
    config.exchange.name     = gate["exchange_name"].value_or("ftx");
    config.exchange.instance = gate["instance_name"].value_or(0);

    config.source            = configurator["source"].value_or("unknown");

    config.config_uri        = configurator["api"][0].value_or("unknown");
    config.config_target     = configurator["api"][1].value_or("unknown");

    // канал агента
    config.aeron_agent.subscribers.agent.channel   = agent_s[0].value_or("");
    config.aeron_agent.subscribers.agent.stream_id = agent_s[1].value_or(0);

    // канал логов
    config.aeron_agent.publishers.logs.channel    = logs[0].value_or("");
    config.aeron_agent.publishers.logs.stream_id  = logs[1].value_or(0);

    // канал агента
    config.aeron_agent.publishers.agent.channel    = agent_p[0].value_or("");
    config.aeron_agent.publishers.agent.stream_id  = agent_p[1].value_or(0);

    return config;
}
