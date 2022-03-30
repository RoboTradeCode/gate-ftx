#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include "gateway.hpp"

using namespace std;

void sigint_handler(int);


const char* CONFIG_FILE_PATH        = "config.toml";

std::atomic<bool> running(true);

logger_type gateway_logger(keywords::channel = "ftxGateway");
logger_type ticker_logger(keywords::channel = "ftxOrderBook");

int main() {
    // инициализируем BOOST.log
    init_logging();
    // создаем шлюх
    std::shared_ptr<gateway> _gateway_ = std::make_shared<gateway>(CONFIG_FILE_PATH);
    signal(SIGINT, sigint_handler);
    // главный цикл
    while(running){
        try{
            _gateway_->pool();
        }
        catch(std::exception &ex){
            std::string sourceException = ex.what();
            BOOST_LOG_SEV(gateway_logger, logging::trivial::info) << "error: " << sourceException;
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
    return 0;
}

void sigint_handler(int)
{
    running = false;
}
