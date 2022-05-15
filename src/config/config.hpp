#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H


#include <string>
#include <vector>
#include "../toml++/toml.h"

/*extern const char* DEFAULT_EXCHANGE_NAME;
extern const char* DEFAULT_SUBSCRIBER_CHANNEL;
extern const char* DEFAULT_PUBLISHER_CHANNEL;
extern const int DEFAULT_CORE_STREAM_ID;
extern const int DEFAULT_BALANCE_STREAM_ID;
extern const int DEFAULT_TICKER_STREAM_ID;
extern const int DEFAULT_LOG_STREAM_ID;
extern const int DEFAULT_BUFFER_SIZE;*/

// Структура конфигурации ядра
struct gate_config
{
    // список рынков
    std::vector<std::string> _markets;

    std::string source;
    std::string config_uri;
    std::string config_target;
    // биржа
    struct exchange
    {
        // имя биржы
        std::string name;
        int         instance;
        std::string node;
        // глубина стакана
        int orderbook_depth;
        int         ping_delay;
    } exchange;
    // ключи
    struct account
    {
        std::string api_key;
        std::string secret_key;
    }account;

    struct aeron_core
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
            struct order_status{
                std::string channel;
                int stream_id;
            }order_status;
        }publishers;
        struct {
            struct core{
                std::string channel;
                int stream_id;
            } core;
        }subscribers;
    } aeron_core;

    struct aeron_agent{
        struct {
            struct {
                std::string channel;
                int stream_id;
            } agent;
            struct {
                std::string channel;
                int stream_id;
            } logs;
        } publishers;
        struct {
            struct {
                std::string channel;
                int stream_id;
            } agent;
        } subscribers;
    } aeron_agent;

};

/**
 * Преобразует файл конфигурации в структуру, понятную ядру
 *
 * @param file_path Путь к файлу конфигурации в формате TOML
 * @return Конфигурация ядра
 */
gate_config parse_config(const std::string&);


#endif  // GATEWAY_CONFIG_H
