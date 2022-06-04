#include "mylogger.hpp"

/**
 * Инициализация логирования
 */
void init_logger()
{
    // Параметры для логирования в файлы
    int max_size = 1048576 * 20;  // 20 MiB
    int max_files = 10;

    // Логирование ping-pong-а
    auto pingpong = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "pingpong",
        "logs/pingpong.log",
        max_size,
        max_files
    );

    // Логирование баланса
    auto balances = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "balances",
        "logs/balances.log",
        max_size,
        max_files
    );

    // Логирование ошибок
    auto errors = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "errors",
        "logs/errors.log",
        max_size,
        max_files
    );

    // Логирование сообщений, отправляемых на лог сервер
    auto logs = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "logs",
        "logs/logs.log",
        max_size,
        max_files
    );

    // Логирование ордеров
    auto orders = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "orders",
        "logs/orders.log",
        max_size,
        max_files
    );

    // Логирование перезапуска
    std::vector<spdlog::sink_ptr> errors_sinks;
    auto restart_errors_file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/restart_errors.log", max_size, max_files);
    auto restart_errors_stdout = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    errors_sinks.push_back(restart_errors_file);
    errors_sinks.push_back(restart_errors_stdout);
    auto restart_errors = std::make_shared<spdlog::logger>("restart_errors", begin(errors_sinks), end(errors_sinks));
    spdlog::register_logger(restart_errors);

    // Логирование основных действий
    std::vector<spdlog::sink_ptr> general_sinks;
    auto general_file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/general.log", max_size, max_files);
    auto general_stdout = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    general_sinks.push_back(general_file);
    general_sinks.push_back(general_stdout);
    auto general = std::make_shared<spdlog::logger>("general", begin(general_sinks), end(general_sinks));
    spdlog::register_logger(general);

    // Политика сброса буфера
    spdlog::flush_every(std::chrono::seconds(5));
}
