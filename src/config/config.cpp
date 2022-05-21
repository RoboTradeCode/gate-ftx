#include "config.hpp"
#include <filesystem>


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
