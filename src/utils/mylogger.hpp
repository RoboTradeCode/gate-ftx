#ifndef GATEWAY_LOGGIER_H
#define GATEWAY_LOGGIER_H


#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

/**
 * Инициализация логирования
 */
void init_logger();


#endif  // GATEWAY_LOGGIER_H
