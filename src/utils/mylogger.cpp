#include "mylogger.hpp"

/**
 * Инициализация логирования
 */
void init_logger()
{
    // Параметры для логирования в файлы
    int max_size = 1048576 * 2;  // 2 MiB
    int max_files = 5;

    // Логирование биржевых стаканов
    auto orderbooks = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "orderbooks",
        "logs/orderbooks.log",
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

    // Логирование ping pong
    auto ping_pong = spdlog::rotating_logger_mt<spdlog::async_factory>(
        "pingpong",
        "logs/pingpong.log",
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

    // Логирование ордеров
    std::vector<spdlog::sink_ptr> orders_sinks;
    auto orders_file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/general.log", max_size, max_files);
    auto orders_stdout = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    orders_sinks.push_back(orders_file);
    orders_sinks.push_back(orders_stdout);
    auto orders = std::make_shared<spdlog::logger>("general", begin(orders_sinks), end(orders_sinks));
    spdlog::register_logger(orders);

    // Политика сброса буфера
    spdlog::flush_every(std::chrono::seconds(5));
}
