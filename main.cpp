#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include "gateway.hpp"
#include "src/utils/mylogger.hpp"

//using namespace std;

void sigint_handler(int);


const char* CONFIG_FILE_PATH        = "default_config.toml";

std::atomic<bool> running(true);

int main() {
    // инициализируем логгера
    init_logger();
    std::shared_ptr<spdlog::logger> restart_error_logger = spdlog::get("restart_errors");
    try {
        // создаем шлюз
        std::shared_ptr<ftx::gateway> _gateway_ = std::make_shared<ftx::gateway>(CONFIG_FILE_PATH);
        _gateway_->initialization();
        signal(SIGINT, sigint_handler);
        // главный цикл
        while(running){
            try{
                _gateway_->pool();
            }
            catch(std::exception &ex){
                // получим описание ошибки
                std::string sourceException = ex.what();
                //BOOST_LOG_SEV(gateway_logger, logging::trivial::info) << "error: " << sourceException;
                // залогируем ошибку
                restart_error_logger->error(sourceException);
                // проверям источник исключения
                if(sourceException == "on_read: public channel")
                {
                    // перезапускаем публичный WS
                    _gateway_->restart_public_ws();
                }
                else if(sourceException == "on_read: private channel"){
                    // перезапускаем приватный WS и укажем причину перезапуска
                    _gateway_->restart_private_ws("by exception");
                }
            }
        }
    }  catch (std::invalid_argument &invlaid_arg) {
        //BOOST_LOG_SEV(gateway_logger, logging::trivial::info) << "error: " << invlaid_arg.what()
        //<< " Настройте файл конфигурации и запустите шлюз заново.";
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
