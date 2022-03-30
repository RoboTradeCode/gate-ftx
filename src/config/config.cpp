#include "config.hpp"
#include <filesystem>

const char* DEFAULT_EXCHANGE_NAME      = "unknown";
const char* DEFAULT_SUBSCRIBER_CHANNEL = "aeron:ipc";
const char* DEFAULT_PUBLISHER_CHANNEL  = "aeron:ipc";
const int DEFAULT_CORE_STREAM_ID       = 103;
const int DEFAULT_BALANCE_STREAM_ID    = 101;
const int DEFAULT_TICKER_STREAM_ID     = 100;
const int DEFAULT_LOG_STREAM_ID        = 102;
const int DEFAULT_BUFFER_SIZE          = 1400;

core_config parse_config(const std::string& config_file_path_){
    // проверим существование конфигурационного файла
    std::filesystem::path path(config_file_path_);
    if(not std::filesystem::exists(path) && not std::filesystem::is_regular_file(path)){
        throw std::invalid_argument("File " + config_file_path_ + "doesn't exists");
    }
    // Инициализация структуры и корня файла конфигурации
    core_config config;
    toml::table tbl = toml::parse_file(config_file_path_);

    // Сокращения для удобства доступа
    toml::node_view exchange = tbl["exchange"];
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
    /*config.aeron.balance.channel     = balance["channel"].value_or(DEFAULT_PUBLISHER_CHANNEL);
    config.aeron.balance.stream_id   = balance["stream_id"].value_or(DEFAULT_BALANCE_STREAM_ID);
    config.aeron.balance.buffer_size = balance["buffer_size"].value_or(DEFAULT_BUFFER_SIZE);*/
    config.aeron.publishers.balance.channel   = balance[0].value_or("");
    config.aeron.publishers.balance.stream_id = balance[1].value_or(0);

    // Заполнение тикеров
    /*config.aeron.ticker.channel      = ticker["channel"].value_or(DEFAULT_PUBLISHER_CHANNEL);
    config.aeron.ticker.stream_id    = ticker["stream_id"].value_or(DEFAULT_TICKER_STREAM_ID);
    config.aeron.ticker.buffer_size  = ticker["buffer_size"].value_or(DEFAULT_BUFFER_SIZE);*/
    config.aeron.publishers.orderbook.channel   = orderbook[0].value_or("");
    config.aeron.publishers.orderbook.stream_id = orderbook[1].value_or(0);

    // Заполнение логов
    /*config.aeron.log.channel         = log["channel"].value_or(DEFAULT_PUBLISHER_CHANNEL);
    config.aeron.log.stream_id       = log["stream_id"].value_or(DEFAULT_LOG_STREAM_ID);
    config.aeron.log.buffer_size     = log["buffer_size"].value_or(DEFAULT_BUFFER_SIZE);*/
    config.aeron.publishers.logs.channel   = logs[0].value_or("");
    config.aeron.publishers.logs.stream_id = logs[1].value_or(0);

    // Заполнение ядра
    /*config.aeron.core.channel        = core["channel"] .value_or(DEFAULT_SUBSCRIBER_CHANNEL);
    config.aeron.core.stream_id      = core["stream_id"].value_or(DEFAULT_CORE_STREAM_ID);
    config.aeron.core.buffer_size    = core["buffer_size"].value_or(DEFAULT_BUFFER_SIZE);*/
    config.aeron.subscribers.core.channel   = core[0].value_or("");
    config.aeron.subscribers.core.stream_id = core[1].value_or(0);

    return config;
}
