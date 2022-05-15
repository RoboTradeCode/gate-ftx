#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include "gateway.hpp"
#include "src/utils/mylogger.hpp"


void sigint_handler(int);


const char* CONFIG_FILE_PATH        = "default_config.toml";

std::atomic<bool> running(true);

int main() {
    // инициализируем логгера
    init_logger();
    std::shared_ptr<spdlog::logger> restart_error_logger = spdlog::get("restart_errors");
    try {
        // создаем шлюз
        std::shared_ptr<ftx::gateway> gateway = std::make_shared<ftx::gateway>(CONFIG_FILE_PATH);
        bss::error error;
        // получаем источник получения конфига
        std::string source = gateway->get_config_source();
        // если источник конфига - файл
        if (source == "file") {
            if(std::filesystem::exists("config.json")) {
                if (gateway->load_config_from_file(error)) {
                    gateway->send_message("Конфигурация загружена из файла.");
                } else {
                    gateway->send_error(error.to_string());
                    std::this_thread::sleep_for(1s);
                    return EXIT_FAILURE;
                }
            } else {
                error.describe("В параметрах конфигурации указан режим работы с файлом, но файла не существует");
                gateway->send_error(error.to_string());
                std::this_thread::sleep_for(1s);
                return EXIT_FAILURE;
            }
        } else if (source == "agent") {
            if (gateway->create_agent_channel()) {
                gateway->send_config_request();
                // получаем конфиг (ожидаем 5 секунд, если получаем быстрее, то выходим из цикла)
                int try_count = 0;
                while( try_count < 5) {
                    gateway->pool_from_agent();
                    if (gateway->has_config())
                        break;
                    else
                    std::this_thread::sleep_for(1s);
                    ++try_count;
                }
                // если конфиг не был получен, то работать нет смысла
                if (!gateway->has_config()) {
                    gateway->send_error("Файл конфигурации от агента не получен.");
                    std::this_thread::sleep_for(1s);
                    return EXIT_FAILURE;
                } else {
                    gateway->send_message("Конфигурация получена от агента.");
                }
            } else {
                gateway->send_error("Ошибка создания aeron канала для агента.");
                std::this_thread::sleep_for(1s);
                return EXIT_FAILURE;
            }
        } else if (source == "api") {
            if (gateway->get_config_from_api(error)) {
                // если конфиг не был получен, то работать нет смысла
                if (!gateway->has_config()) {
                    gateway->send_error("Файл конфигурации от агента не получен.");
                    std::this_thread::sleep_for(1s);
                    return EXIT_FAILURE;
                } else {
                    gateway->send_message("Конфигурация получена с сервера.");
                }
            } else {
                gateway->send_error(error.to_string());
                std::this_thread::sleep_for(1s);
                return EXIT_FAILURE;
            }
        } else {
            gateway->send_error("Файл конфигурации содержит неизвестный источник получения конфига.");
            std::this_thread::sleep_for(1s);
            return EXIT_FAILURE;
        }


        // !!!!!!!!!!!!!  на время тестирования  !!!!!!!!!!!!
        // если такой файл есть, то загрузим конфиг из него
        /*if(std::filesystem::exists("config.json")) {
            if (!gateway->load_config(error))
                gateway->send_error(error.to_string());
        } else {
            // если файла нет, то работаем через агента
            gateway->send_config_request();
            // получаем конфиг (ожидаем 5 секунд, если получаем быстрее, то выходим из цикла)
            int try_count = 0;
            while( try_count < 5) {
                gateway->pool_from_agent();
                if (gateway->has_config())
                    break;
                else
                std::this_thread::sleep_for(1s);
                ++try_count;
            }
            // если конфиг не был получен, то работать нет смысла
            if (!gateway->has_config()) {
                gateway->send_error("Файл конфигурации не получен.");
                std::this_thread::sleep_for(1s);
                return EXIT_FAILURE;
            }
        }*/

        // продолжаем подготовку к запуску
        if (not gateway->preparation_for_launch()) {
            return EXIT_FAILURE;
        }
        signal(SIGINT, sigint_handler);
        // главный цикл
        while(running){
            try{
                gateway->pool();
            }
            catch(std::exception &ex){
                // получим описание ошибки
                std::string sourceException = ex.what();
                // залогируем ошибку
                restart_error_logger->error(sourceException);
                // проверям источник исключения
                if(sourceException == "on_read: public channel")
                {
                    // перезапускаем публичный WS
                    gateway->restart_public_ws();
                }
                else if(sourceException == "on_read: private channel"){
                    // перезапускаем приватный WS и укажем причину перезапуска
                    gateway->restart_private_ws("by exception");
                }
            }
        }
    }  catch (std::invalid_argument &invlaid_arg) {
        restart_error_logger->info("{} Настройте файл конфигурации и запустите шлюз заново.", invlaid_arg.what());
        std::this_thread::sleep_for(1s);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void sigint_handler(int)
{
    running = false;
}
