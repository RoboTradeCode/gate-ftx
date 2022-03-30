#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H


#include <string>
#include <vector>
#include "../toml++/toml.h"

extern const char* DEFAULT_EXCHANGE_NAME;
extern const char* DEFAULT_SUBSCRIBER_CHANNEL;
extern const char* DEFAULT_PUBLISHER_CHANNEL;
extern const int DEFAULT_CORE_STREAM_ID;
extern const int DEFAULT_BALANCE_STREAM_ID;
extern const int DEFAULT_TICKER_STREAM_ID;
extern const int DEFAULT_LOG_STREAM_ID;
extern const int DEFAULT_BUFFER_SIZE;

// Структура конфигурации ядра
struct core_config
{
    struct exchange
    {
        std::string name;
    } exchange;
    struct account
    {
        std::string api_key;
        std::string secret_key;
    }account;

    struct aeron
    {
        struct {
            struct balance{
                std::string channel;
                int stream_id;
            } balance;
            struct orderbook{
                std::string channel;
                int stream_id;
            } orderbook;
            struct logs{
                std::string channel;
                int stream_id;
            } logs;
        }publishers;
        struct {
            struct core{
                std::string channel;
                int stream_id;
            } core;
        }subscribers;
    } aeron;
};

/**
 * Преобразует файл конфигурации в структуру, понятную ядру
 *
 * @param file_path Путь к файлу конфигурации в формате TOML
 * @return Конфигурация ядра
 */
core_config parse_config(const std::string&);


#endif  // GATEWAY_CONFIG_H
