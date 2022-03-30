#include "gateway.hpp"

namespace pt = boost::property_tree;
using namespace aeron::util;
using namespace aeron;

logger_type ftxgateway_logger(keywords::channel = "ftxGateway");
logger_type orderbook_logger(keywords::channel = "ftxOrderBook");
logger_type ping_pong_logger(keywords::channel = "ftxPingPong");
using boost::multiprecision::cpp_dec_float_50;
gateway::gateway(const std::string& config_file_path_)
{
    // установим имя файла, при появлении которого необходимо отправить баланс
    _path = "balance";

    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Starting...";
    // получаем конфигурацию
    config = parse_config(config_file_path_);
    // запоминаем время для отправки пинга
    last_ping_time = std::chrono::system_clock::now();
    // создаём каналы aeron, websocket и rest
 /*   create_aeron_channel();
    create_private_REST();*/
    create_private_ws();
    create_public_ws();

    // проверяем и отправляем баланс
/*    check_balance();
    // получаем характеристики валютной пары
    curr_characters = ftx_rest_private->list_markets("BTC/USDT", _error);
    if(_error){
        _error.describe("Ошибка получения характеристик валютной пары (list_markets)");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _error.clear();
    }

    get_precision(curr_characters);

    // получаем открытые ордера
    std::vector<SOrder> orders_vector = ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(orders_vector.size() != 0)
    {
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "есть открытые ордера";
        // отправляем ордера в ядро
        order_sender(orders_vector);
        // и потом отменяем их
        std::string cancel_result = ftx_rest_private->cancel_all_orders("BTC/USDT");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << cancel_result;
    }
    else
    {
        if(_error){
            _error.describe("Ошибка получения информации об открытых оредерах (get_open_orders).");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << _error.to_string();
            _error.clear();
        }
        else{
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет открытых ордеров.";
        }
    }*/
}

bool gateway::create_aeron_channel()
{
    bool result = true;
    try{
        // установка соединений с каналом aeron
        balance_channel    = std::make_shared<Publisher>(config.aeron.publishers.balance.channel, config.aeron.publishers.balance.stream_id);
        orderbook_channel  = std::make_shared<Publisher>(config.aeron.publishers.orderbook.channel, config.aeron.publishers.orderbook.stream_id);
        log_channel        = std::make_shared<Publisher>(config.aeron.publishers.logs.channel, config.aeron.publishers.logs.stream_id);
        core_channel       = std::make_shared<Subscriber>([&](std::string_view message)
                {shared_from_this()->aeron_handler(message);},
                config.aeron.subscribers.core.channel, config.aeron.subscribers.core.stream_id);
    }
    catch(std::exception& err){
        std::cout << err.what() << std::endl;
        result = false;
    }
    return result;
}
//---------------------------------------------------------------
// создаёт публичный WS
//---------------------------------------------------------------
void gateway::create_public_ws()
{
    ftx_ws_public    = std::make_shared<ftx::WSClient>("", "", ioc, [&](std::string_view message_, void* id_)
                            {shared_from_this()->public_ws_handler(message_, id_);});
    // подписываемся на публичный поток ticker
    //ftx_ws_public->subscribe_ticker("BTC/USDT");
    //ftx_ws_public->subscribe_ticker("ETH/USDT");
    ftx_ws_public->subscribe_orderbook("BTC/USDT");
    //ftx_ws_public->subscribe_orderbook("ETH/USDT");
}
//---------------------------------------------------------------
// создаёт приватный WS
//---------------------------------------------------------------
void gateway::create_private_ws()
{
    ftx_ws_private   = std::make_shared<ftx::WSClient>(config.account.api_key, config.account.secret_key, ioc, [&](std::string_view message_, void* id_)
                            {shared_from_this()->private_ws_handler(message_, id_);});
    // авторизуемся в приватном ws
    std::string error;
    // количество попыток
    int try_count = 0;
    // флаг успешного подключения
    bool loggon = false;
    //ftx_ws_private->login(error);
    while(true){
        // пытаемся логиниться
        size_t login_result = ftx_ws_private->login(error);
        if(login_result != 0){
            // успешно залогинились
            loggon = true;
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "loggon true " << "try count = " << try_count;
            // выходим
            break;
        }
        else{
            // пытаемся еще залогиниться
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "loggon false " << "try count = " << try_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            ++try_count;
            if(try_count == 3)
                break;
        }
    }
    if(!loggon){
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ending...";
        exit(0);
    }
    // подписываемся на приватный поток order
    ftx_ws_private->subscribe_order("BTC/USDT");
    //size_t subscribe_result = ftx_ws_private->subscribe_order("BTC/USDT");
    //BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "subscribe_order with result: " << subscribe_result;

}
//---------------------------------------------------------------
// создаёт приватный REST
//---------------------------------------------------------------
void gateway::create_private_REST()
{
    ftx_rest_private = std::make_shared<ftx::RESTClient>(config.account.api_key, config.account.secret_key);
}
void gateway::pool()
{
 /*   core_channel->poll();*/
    ioc.run_for(std::chrono::microseconds(100));
    // каждые 15 секунд будем дёргать приватный REST
    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last_ping_time) > 15s){
        size_t ping_result = ftx_ws_private->ping();
        BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << "ping with result: " << ping_result;
        last_ping_time = std::chrono::system_clock::now();
        // если 5 минут не было ничего кроме понга в приватном канале, то нам надо перегрузить канал
        if(ws_control >= 20){
            // перезапускаем приватный WS по-таймауту
            //restart_private_ws("by timeout");

        }
        // эта проверка сделана для проверки работоспособности шлюза, в случае возникновения ошибок
        if(std::filesystem::exists(_path)){
            // появился файл balance: отправим баланс
            check_balance();
            BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << "send balance by demand";
            // удалим файл
            std::filesystem::remove(_path);
        }
    }
}
//---------------------------------------------------------------
// обрабатывает сообщения от ядра
//---------------------------------------------------------------
void gateway::aeron_handler(std::string_view message_)
{
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "(message from core): " << message_;
    try
    {
        simdjson::dom::parser parser;
        auto &&error_code = parser.allocate(0x1000,0x04);
        if(simdjson::SUCCESS == error_code)
        {
            auto result = parser.parse(message_.data(), message_.size(), false);
            if(simdjson::SUCCESS == result.error())
            {
                std::string_view a, S, s, t, p, q;
                a = result.at_key("a").get_string();
                S = result.at_key("S").get_string();
                s = result.at_key("s").get_string();
                if(a.compare("+") == 0)
                {
                    t = result.at_key("t").get_string();
                    p = result.at_key("p").get_string();
                    q = result.at_key("q").get_string();
                }
                // выставлен ордер на отмену продажи
                if(a.compare("-") == 0  && s.compare("SELL") == 0)
                {
                    cancel_sell_order();
                    /*
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на отмену продажи";
                    // получаем открытые ордера
                    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
                    if(open_orders_list.size() != 0)
                    {
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер  на отмену продажи " <<
                                                                                 open_orders_list.at(0).clientId << " " <<
                                                                                 _error.to_string();
                        _error.clear();
                        // ордера есть, проверим какие
                        for (auto &&order : open_orders_list)
                        {
                            // есть открытый ордер на продажу
                            if(order.side == "sell")
                            {
                                // отменяем ордер по идентификатору
                                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                                if(_error){
                                    _error.describe("Ошибка отмены ордера");
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                          order.id << " " <<
                                                                                          result   << " " <<
                                                                                          _error.to_string();
                                    _error.clear();
                                }
                                else{
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                          order.id << " " <<
                                                                                          result;
                                }
                            }
                        }
                    }
                    else
                    {
                        if(_error){
                            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
                            // дальнейшие действия чреваты
                            // перед выходом проверяем и отправляем баланс
                            check_balance();
                            return;
                        }
                        else{
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на отмену продажы (отменять нечего)";
                        }
                    }*/
                }
                // выставлен ордер на продажу
                else if(a.compare("+") == 0  && s.compare("SELL") == 0)
                {
                    sell_order(p, q);
                    /*BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на продажу";
                    // получаем открытые ордера
                    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
                    if(open_orders_list.size() != 0)
                    {
                        //std::cout << "Есть открытый ордер на продажу" << std::endl;
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер на продажу "<<
                                                                                 open_orders_list.at(0).clientId << " " <<
                                                                                 _error.to_string();
                        _error.clear();
                        // ордера есть, проверим какие
                        for (auto &&order : open_orders_list)
                        {
                            // есть открытый ордер на продажу
                            if(order.side == "sell")
                            {
                                // отменяем ордер по идентификатору
                                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                                if(_error){
                                    _error.describe("Ошибка отмены ордера");
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                             order.id << " " <<
                                                                                             result   << " " <<
                                                                                             _error.to_string();
                                    _error.clear();
                                }
                                else{
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                             order.id << " " <<
                                                                                             result;
                                }
                            }
                        }
                    }
                    else
                    {
                        if(_error){
                            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
                            // дальнейшие действия чреваты
                            // перед выходом проверяем и отправляем баланс
                            check_balance();
                            return;
                        }
                        else{
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на продажу (отменять нечего)";
                        }
                    }
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Выставляем ордер с ценой: " << p << " объёмом: " << q;
                    std::string price = set_price_precision(std::string(p));
                    std::string size  = set_size_precision(std::string(q));
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "После корректировки: " << price << " " << size;
                    //ftx_rest_private->place_order("BTC/USDT", "sell", 50000.0, 0.0001);

//                    std::string place_order_result = ftx_rest_private->place_order("BTC/USDT", "sell", price, size, _error);
//                    if(_error){
//                        _error.describe("Ошибка выставления ордера.");
//                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат выставления ордера: " <<
//                                                                                 place_order_result <<
//                                                                                 _error.to_string();
//                        _error.clear();
//                    }
//                    else{
//                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат выставления ордера: " <<
//                                                                                 place_order_result;
//                    }

                    std::make_shared<ftx::AsyncRESTClient>(config.account.api_key,
                                                           config.account.secret_key,
                                                           ioc,
                                                           [&](std::string_view message_)
                             {shared_from_this()->place_order_result_handler(message_);})->place_order("BTC/USDT", "sell", price, size);
                    */
                }
                // выставлен ордер на отмену покупки
                else if(a.compare("-") == 0  && s.compare("BUY") == 0)
                {
                    cancel_buy_order();
                    /*BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на отмену покупки";
                    // получаем открытые ордера
                    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
                    if(open_orders_list.size() != 0)
                    {
                        //std::cout << "Есть открытый ордер на отмену покупки" << std::endl;
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер на отмену покупки " <<
                                                                                 open_orders_list.at(0).clientId <<
                                                                                 _error.to_string();
                        _error.clear();
                        // ордера есть, проверим какие
                        for (auto &&order : open_orders_list)
                        {
                            // есть открытый ордер на продажу
                            if(order.side == "buy")
                            {
                                // отменяем ордер по идентификатору
                                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                                if(_error){
                                    _error.describe("Ошибка отмены ордера");
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                             order.id << " " <<
                                                                                             result   << " " <<
                                                                                             _error.to_string();
                                    _error.clear();
                                }
                                else{
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                             order.id << " " <<
                                                                                             result;
                                }
                            }
                        }
                    }
                    else
                    {
                        if(_error){
                            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
                            // дальнейшие действия чреваты
                            // перед выходом проверяем и отправляем баланс
                            check_balance();
                            return;
                        }
                        else{
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на отмену покупки (отменять нечего)";
                        }
                    }*/
                }
                // выставлен ордер на покупку
                else if(a.compare("+") == 0  && s.compare("BUY") == 0)
                {
                    buy_order(p, q);
                    /*BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на покупку";
                    // получаем открытые ордера
                    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
                    if(open_orders_list.size() != 0)
                    {
                        //std::cout << "Есть открытый ордер на покупку" << std::endl;
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер на покупку" <<
                                                                                 open_orders_list.at(0).clientId <<
                                                                                 _error.to_string();
                        _error.clear();
                        // ордера есть, проверим какие
                        for (auto &&order : open_orders_list)
                        {
                            // есть открытый ордер на продажу
                            if(order.side == "buy")
                            {
                                // отменяем ордер по идентификатору
                                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                                if(_error){
                                    _error.describe("Ошибка отмены ордера");
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                             order.id << " " <<
                                                                                             result   << " " <<
                                                                                             _error.to_string();
                                }
                                else{
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                                             order.id << " " <<
                                                                                             result;
                                }
                            }
                        }
                    }
                    else
                    {
                        if(_error){
                            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
                            // дальнейшие действия чреваты
                            // перед выходом проверяем и отправляем баланс
                            check_balance();
                            return;
                        }
                        else{
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на покупку (отменять нечего)" ;
                        }
                    }
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Выставляем ордер на покупку с ценой: " << p << " и объёмом: " << q ;
                    std::string price = set_price_precision(std::string(p));
                    std::string size  = set_size_precision(std::string(q));
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "После корректировки: " << price << " " << size ;

//                    std::string place_order_result = ftx_rest_private->place_order("BTC/USDT", "buy", price, size, _error);
//                    if(_error){
//                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат выставления ордера: " <<
//                                                                                 place_order_result <<
//                                                                                 _error.to_string();
//                        _error.clear();
//                    }
//                    else{
//                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат выставления ордера: " <<
//                                                                                 place_order_result;
//                    }
                    std::make_shared<ftx::AsyncRESTClient>(config.account.api_key,
                                                           config.account.secret_key,
                                                           ioc,
                                                           [&](std::string_view message_)
                             {shared_from_this()->place_order_result_handler(message_);})->place_order("BTC/USDT", "buy", price, size);
                    */
                }
            }
            else
            {
                _error.describe("Ошибка разбора json фрейма.");
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _error.clear();
            }
        }
        else
        {
            _error.describe("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
        }
    }
    catch(simdjson::simdjson_error& err)
    {
        _error.describe(err.what() + fmt::format(" (json body: {}).", message_));
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _error.clear();
    }
}
//---------------------------------------------------------------
// обрабатывает отмену ордера на продажу ("-" && "sell")
//---------------------------------------------------------------
void gateway::cancel_sell_order()
{
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на отмену продажи, проверим открытые ордера";
    // получаем открытые ордера
    _error.clear();
    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на продажу
            if(order.side == "sell") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер  на продажу " <<
                                                                         open_orders_list.at(0).clientId << " " <<
                                                                         _error.to_string();
                // отменяем ордер по идентификатору
                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if(_error) {
                    _error.describe("Ошибка отмены ордера");
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                          order.id << " " <<
                                                                          result   << " " <<
                                                                          _error.to_string();
                    _error.clear();
                } else {
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                          order.id << " " <<
                                                                          result;
                }
            } else if (order.side == "buy") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер, но на покупку ";
            }
        }
    } else {
        if(_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на продажу (отменять нечего)";
        }
    }
}
//---------------------------------------------------------------
// обрабатывает отмену ордера на покупку ("-" && "buy")
//---------------------------------------------------------------
void gateway::cancel_buy_order()
{
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на отмену покупки, проверим открытые ордера";
    // получаем открытые ордера
    _error.clear();
    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на покупку
            if(order.side == "buy") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер на покупку " <<
                                                                        open_orders_list.at(0).clientId <<
                                                                        _error.to_string();
                // отменяем ордер по идентификатору
                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if(_error) {
                    _error.describe("Ошибка отмены ордера");
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                             order.id << " " <<
                                                                             result   << " " <<
                                                                             _error.to_string();
                    _error.clear();
                } else {
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                             order.id << " " <<
                                                                             result;
                }
            } else if (order.side == "sell") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер, но на продажу";
            }
        }
    } else {
        if(_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на покупку (отменять нечего)";
        }
    }
}
//---------------------------------------------------------------
// обрабатывает ордер на продажу ("+" && "sell")
//---------------------------------------------------------------
void gateway::sell_order(std::string_view price_, std::string_view quantity_)
{
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на продажу, проверим открытые ордера";
    _error.clear();
    // получаем открытые ордера
    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на продажу
            if (order.side == "sell") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер на продажу "<<
                                                                         open_orders_list.at(0).clientId << " " <<
                                                                         _error.to_string();
                // отменяем ордер по идентификатору
                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if (_error) {
                    _error.describe("Ошибка отмены ордера");
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                             order.id << " " <<
                                                                             result   << " " <<
                                                                             _error.to_string();
                    _error.clear();
                } else {
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                             order.id << " " <<
                                                                             result;
                }
            } else if(order.side == "buy") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер, но на покупку ";
            }
        }
    } else {
        if(_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на продажу (отменять нечего)";
        }
    }
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Выставляем ордер на продажу с ценой: " << price_ << " и объёмом: " << quantity_;
    std::string price = set_price_precision(std::string(price_));
    std::string size  = set_size_precision(std::string(quantity_));
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "После корректировки: " << price << " " << size;

    // выставляем ордер (синхронно)
    std::string place_order_result = ftx_rest_private->place_order("BTC/USDT", "sell", price, size, _error);
    if(_error) {
        _error.describe("Ошибка выставления ордера.");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: Ошибка выставления ордера на продажу: " <<
                                                                                 place_order_result <<
                                                                                 _error.to_string();
        _error.clear();
    } else {
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат выставления ордера на продажу: " << place_order_result;
    }

    /*std::make_shared<ftx::AsyncRESTClient>(config.account.api_key,
                                           config.account.secret_key,
                                           ioc,
                                           [&](std::string_view message_)
             {shared_from_this()->place_order_result_handler(message_);})->place_order("BTC/USDT", "sell", price, size);*/
}
//---------------------------------------------------------------
// обрабатывает ордер на покупку ("+" && "buy")
//---------------------------------------------------------------
void gateway::buy_order(std::string_view price_, std::string_view quantity_)
{
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Ядром выставлен ордер на покупку, проверим открытые ордера";
    // получаем открытые ордера
    _error.clear();
    std::vector<SOrder> open_orders_list = ftx_rest_private->get_open_orders("BTC/USDT", _error);
    if(open_orders_list.size() != 0) {
        // ордера есть, проверим какие
        for (auto &&order : open_orders_list) {
            // есть открытый ордер на продажу
            if(order.side == "buy")
            {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер на покупку" <<
                                                                         open_orders_list.at(0).clientId <<
                                                                         _error.to_string();
                // отменяем ордер по идентификатору
                std::string result = ftx_rest_private->cancel_order(std::to_string(order.id), _error);
                if(_error) {
                    _error.describe("Ошибка отмены ордера");
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                             order.id << " " <<
                                                                             result   << " " <<
                                                                             _error.to_string();
                    _error.clear();
                } else {
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат отмены ордера (id): " <<
                                                                             order.id << " " <<
                                                                             result;
                }
            } else if(order.side == "sell") {
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Есть открытый ордер, но на продажу";
            }
        }
    } else {
        if (_error) {
            _error.describe("Ошибка получения информации об открытых ордерах (get_open_orders)");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
            // если была какая-то ошибка при получении списка ордеров, то выходим из функции
            // дальнейшие действия чреваты
            // перед выходом проверяем и отправляем баланс
            check_balance();
            return;
        } else {
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Нет выставленных ордеров на покупку (отменять нечего)" ;
        }
    }
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Выставляем ордер на покупку с ценой: " << price_ << " и объёмом: " << quantity_ ;
    std::string price = set_price_precision(std::string(price_));
    std::string size  = set_size_precision(std::string(quantity_));
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "После корректировки: " << price << " " << size ;

    // выставляем ордер (синхронно)
    std::string place_order_result = ftx_rest_private->place_order("BTC/USDT", "buy", price, size, _error);
    if(_error) {
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: Ошибка выставления ордера на покупку: " <<
                                                                                 place_order_result <<
                                                                                 _error.to_string();
        _error.clear();
    } else {
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Результат выставления ордера на покупку: " << place_order_result;
    }
    /*std::make_shared<ftx::AsyncRESTClient>(config.account.api_key,
                                           config.account.secret_key,
                                           ioc,
                                           [&](std::string_view message_)
             {shared_from_this()->place_order_result_handler(message_);})->place_order("BTC/USDT", "buy", price, size);*/
}
//---------------------------------------------------------------
// приватный канал WS (neew refactoring, обработка ошибок и отмена try catch)
//---------------------------------------------------------------
void gateway::private_ws_handler(std::string_view message_, void* id_)
{

    if(message_.compare("{\"type\": \"pong\"}") == 0){
        // ответ должны получать каждые 30 секунд, увеличим счётчик
        if(start_trigger == true)
            ++ws_control;
        BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << "pong: "            << message_      <<
                                                                   " start_trigger = " << start_trigger <<
                                                                   " ws_control = "    << ws_control    <<
                                                                   " object_id = "     << id_;
        return;
    }
    try
    {
        //std::cout << message_ << std::endl;
        // создадим парсер
        simdjson::dom::parser parser;
        // пусть парсер подготовит буфер для своих нужд
        auto &&error_code = parser.allocate(0x1000,0x04);
        // если буфер был успешно выделен
        if(simdjson::SUCCESS == error_code){
            // разбираем строку
            auto result = parser.parse(message_.data(), message_.size(), false);
            // если данные успешно разобрались
            if(simdjson::SUCCESS == result.error()){
                if(auto element_type{result["type"].get_string()}; simdjson::SUCCESS == element_type.error()){
                    if(element_type.value().compare("update") == 0){
                        std::string_view element_side{result["data"]["side"].get_string()};
                        std::string_view element_status{result["data"]["status"].get_string()};
                        double element_filled{result["data"]["filledSize"].get_double()};
                        double element_remaining{result["data"]["remainingSize"].get_double()};
                        int64_t element_id{result["data"]["id"].get_int64()};
                        std::string description = get_order_change_description(element_side, element_status, element_filled, element_remaining);
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info)
                                << "(ws private) произошли изменения в ордерах : "
                                << description
                                << " object_id  = " << id_
                                << " id: " << element_id
                                << " side: " << element_side
                                << " status: " << element_status
                                << " filledSize: " << element_filled
                                << " remainingSize: " << element_remaining;
                        //buy closed 0 0 -

                        /*std::vector<SBState> balances_vector = ftx_rest_private->get_balances(_error);
                        // если вектор нулевого размера, значит была какая-то ошибка
                        if(balances_vector.empty())
                        {
                            if(_error){
                                _error.describe("Ошибка получения баланса (get_balances).");
                                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _error.clear();
                            }
                            // взводим триггер
                            start_trigger = true;
                            BOOST_LOG_SEV(ping_pong_logger, logging::trivial::info) << " start_trigger = " << start_trigger;
                        }
                        else{
                            balance_sender(balances_vector);
                        }*/
                        // проверяем и отправляем баланс
                        check_balance();
                        // сбросим счётчик
                        ws_control = 0;
                        // сбросим триггер
                        start_trigger = false;
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "start_trigger set to false !!! " <<
                                                                                    "object_id = " << id_;
                    }
                    else{
                        if(element_type.value().compare("subscribed") != 0){
                            _error.describe(fmt::format("json не содержить поле \"update\". json body: {}", message_));
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                        }
                    }
                }
                else{
                    _error.describe(fmt::format("json не содержить поле \"type\". json body: {}", message_));
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                    _error.clear();
                }
            }
            else
            {
                //error += "Ошибка разбора json фрейма.";
                //error += " ((ws private) json: " + std::string(message_) + ").";
                _error.describe(fmt::format("Ошибка разбора json фрейма. ((ws private) json body: {}).", message_));
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _error.clear();
            }
        }
        else
        {
            //error += "Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)).";
            //error += " ((ws private) json: " + std::string(message_) + ").";
            _error.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). ((ws private) json body: {}).", message_));
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
            _error.clear();
        }
    }
    catch(simdjson::simdjson_error& err)
    {
        //error += err.what();
        //error += " ((ws private) json: " + std::string(message_) + ").";
        _error.describe(err.what() + fmt::format(" ((ws private) json body: {}).", message_));
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
        _error.clear();
    }
}
//---------------------------------------------------------------
// получает более подробную информацию об изменении ордера
//---------------------------------------------------------------
std::string gateway::get_order_change_description(std::string_view side_, std::string_view status_, const double& filled_size_, const double& remaining_size_)
{
    std::string result_message;

    if (0 ==side_.compare("buy")) {
        if (0 == status_.compare("new")) {
            // выставлен новый ордер
            result_message = "создан ордер на покупку";
        } else if (0 == status_.compare("closed")) {
            // ордер был выполнен или отменен
            if ((0 == filled_size_) && (0 == remaining_size_)) {
                result_message = "покупка выполнена";
            } else {
                result_message = "покупка отменена";
            }
        } else {
            result_message = "Неопределенный статус в ордере buy";
        }
    } else if (0 == side_.compare("sell")) {
        if (0 == status_.compare("new")) {
            // выставлен новый ордер
            result_message = "создан ордер на продажу";
        } else if (0 == status_.compare("closed")) {
            // ордер выполнен или отменен
            if ((0 == filled_size_) && (0 == remaining_size_)) {
                result_message = "продажа выполнена";
            } else {
                result_message = "продажа отменена";
            }
        }
    } else {
        result_message = "Неопределенная операция.";
    }
    return result_message;
}
//---------------------------------------------------------------
// публичный канал WS
//---------------------------------------------------------------
void gateway::public_ws_handler(std::string_view message_, void* id_)
{
    // примеры поступающих сообщений:
    // Тип 1. После subscribe на канал
    // {"type": "subscribed", "channel": "orderbook", "market": "BTC/USDT"}
    // Тип 2. В дальнейшем работе
    // {"channel": "ticker", "market": "ETH/USDT", "type": "update", "data": {"bid": 3146.8, "ask": 3146.9, "bidSize": 15.233, "askSize": 16.39, "last": 3146.8, "time": 1648380244.282335}}
    //
 /*   std::cout << message_ << std::endl;*/
    //создадим парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд
    auto &&error_code = parser.allocate(0x2000, 0x05);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        // разбираем пришедшее сообщение
  /*      std::cout << message_.size() << std::endl;*/
        auto result = parser.parse(message_.data(), message_.size(), false);
        // если данные успешно разобрались
        if (simdjson::SUCCESS == result.error()) {
            // получаем значение поля type, чтобы понять тип сообщения (нам нужны сообщения второго типа (см. примеры выше))
            if (auto type_element{result["type"].get_string()}; simdjson::SUCCESS == type_element.error()) {
                // после подписки получаем snapshot ("type": "partial"), содержащий 100 лучших ордеров
                if (type_element.value().compare("partial") == 0) {
                    // получаем рынок, с которого пришло сообщение (получаем его в std::string, потому что будем изменять строку)
                    if (auto market_element{result["market"].get_c_str()}; simdjson::SUCCESS == market_element.error()) {
                        std::string market = market_element.value();
                        // заменим '/' (так приходит с биржи) на '-' (так требует ядро)
                        std::replace(market.begin(), market.end(),'/','-');
                        // получаем bid
                        if (auto bids_array_element{result["data"]["bids"].get_array() }; simdjson::SUCCESS == bids_array_element.error()) {
                            //std::cout << bids_array_element << std::endl;
                            for(auto bid_pair_value : bids_array_element) {
                                _bids_map.insert(std::make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения массива bids. json body: {}.", message_));
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                        }
                        // получаем ask
                        if (auto asks_array_element{result["data"]["asks"].get_array() }; simdjson::SUCCESS == asks_array_element.error()) {
                            /*std::cout << asks_array_element << std::endl;*/
                            for(auto ask_pair_value : asks_array_element) {
                                _asks_map.insert(std::make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения массива asks. json body: {}.", message_));
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                        }
                    } else {
                        _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}.", message_));
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                        _error.clear();
                    }
                } else {

                    if (type_element.value().compare("update") == 0) {
                        // получаем рынок
                        if (auto market_element{result["market"].get_c_str()}; simdjson::SUCCESS == market_element.error()) {
                            std::string market = market_element.value();
                            // заменим '/' (так приходит с биржи) на '-' (так требует ядро)
                            std::replace(market.begin(), market.end(),'/','-');
                            // получаем bid
                            if (auto bids_array_element{result["data"]["bids"].get_array() }; simdjson::SUCCESS == bids_array_element.error()) {
                                //std::cout << bids_array_element << std::endl;
                                for(auto bid_pair_value : bids_array_element) {
                                    //_bids_map.insert(std::make_pair(bid_pair_value.at(0).get_double(), bid_pair_value.at(1).get_double()));
                                    /*std::cout << bid_pair_value.at(0).get_double() << "  " << bid_pair_value.at(1).get_double();*/
                                    // если объем равен нулю, то удаляем такой ключ
                                    if (0.0 == bid_pair_value.at(1).get_double()) {
                                        // для начала найдем его
                                        auto find_iterator = _bids_map.find(bid_pair_value.at(0).get_double());
                                        if (find_iterator != _bids_map.end()) {
                                            _bids_map.erase(find_iterator);
                                            /*std::cout << "удалили ключ из bids_array: " << bid_pair_value.at(0).get_double() << std::endl;
                                            std::cout << "размер bids_array: " << _bids_map.size() << std::endl;*/
                                        } else {
                                            /*std::cout << "ключ не найден в bids_array: " << bid_pair_value.at(0).get_double() << std::endl;*/
                                        }
                                    } else {
                                        _bids_map[bid_pair_value.at(0).get_double()] = bid_pair_value.at(1).get_double();
                                    }
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения массива bids. json body: {}.", message_));
                                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _error.clear();
                            }
                            // получаем ask
                            if (auto asks_array_element{result["data"]["asks"].get_array() }; simdjson::SUCCESS == asks_array_element.error()) {
                                /*std::cout << asks_array_element << std::endl;*/
                                for(auto ask_pair_value : asks_array_element) {
                                    //_asks_map.insert(std::make_pair(ask_pair_value.at(0).get_double(), ask_pair_value.at(1).get_double()));
                                    /*std::cout << ask_pair_value.at(0).get_double() << "  " << ask_pair_value.at(1).get_double();*/
                                    // если объем равен нулю, то удаляем такой ключ
                                    if (0.0 == ask_pair_value.at(1).get_double()) {
                                        // для начала найдем его
                                        auto find_iterator = _asks_map.find(ask_pair_value.at(0).get_double());
                                        if (find_iterator != _asks_map.end()) {
                                            _asks_map.erase(find_iterator);
                                            /*std::cout << "удалили ключ из asks_array: " << ask_pair_value.at(0).get_double() << std::endl;
                                            std::cout << "размер asks_array: " << _bids_map.size() << std::endl;*/
                                        } else {
                                            /*std::cout << "ключ не найден в asks_array: " << ask_pair_value.at(0).get_double() << std::endl;*/
                                        }
                                    } else {
                                        _asks_map[ask_pair_value.at(0).get_double()] = ask_pair_value.at(1).get_double();
                                    }
                                    //_asks_map[ask_pair_value.at(0).get_double()] = ask_pair_value.at(1).get_double();
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения массива asks. json body: {}.", message_));
                                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _error.clear();
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}.", message_));
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                        }
                    } else if (type_element.value().compare("error") == 0) {
                        // получаем код ошибки
                        if (auto code_element{result["code"].get_int64()}; simdjson::SUCCESS == code_element.error()) {
                            //std::string code = code_element.value();
                            // получаем описание ошибки
                            if (auto message_element{result["msg"].get_c_str()}; simdjson::SUCCESS == message_element.error()) {
                                // отправими ошибку
                                error_sender(fmt::format("Ошибка в канале orderbook (код: {}, сообщение: \"{}\").", code_element.value(), message_element.value()));
                            }
                        }
                    }
                    // видимо было сообщение 1 типа (subscribed)
                }
            } else {
                _error.describe(fmt::format("Ошибка определения типа сообщения. json body: {}.", message_));
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _error.clear();
            }
        } else {
            _error.describe(fmt::format("Ошибка разбора json фрейма (error: {})", result.error()));
            error_sender(_error.to_string());
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
        }
    } else {
        _error.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _error.clear();
    }
}
/*void gateway::public_ws_handler(std::string_view message_, void* id_)
{
    // примеры поступающих сообщений:
    // Тип 1. После subscribe на канал
    // {"type": "subscribed", "channel": "ticker", "market": "ETH/USDT"}
    // Тип 2. В дальнейшем работе
    // {"channel": "ticker", "market": "ETH/USDT", "type": "update", "data": {"bid": 3146.8, "ask": 3146.9, "bidSize": 15.233, "askSize": 16.39, "last": 3146.8, "time": 1648380244.282335}}
    //
    std::cout << message_ << std::endl;
    //создадим парсер
    simdjson::dom::parser parser;
    // скажем парсеру, чтобы он подготовил буфер для своих внутренних нужд
    auto &&error_code = parser.allocate(0x1000, 0x04);
    // если буфер успешно выделен
    if (simdjson::SUCCESS == error_code) {
        // разбираем пришедшее сообщение
        auto result = parser.parse(message_.data(), message_.size(), false);
        // если данные успешно разобрались
        if (simdjson::SUCCESS == result.error()) {
            // получаем значение поля type, чтобы понять тип сообщения (нам нужны сообщения второго типа (см. примеры выше))
            if (auto type_element{result["type"].get_string()}; simdjson::SUCCESS == type_element.error()) {
                if (type_element.value().compare("update") == 0) {
                    // получаем рынок, с которого пришло сообщение (получаем его в std::string, потому что будем изменять строку)
                    if (auto market_element{result["market"].get_c_str()}; simdjson::SUCCESS == market_element.error()) {
                        std::string market = market_element.value();
                        // заменим '/' (так приходит с биржи) на '-' (так требует ядро)
                        std::replace(market.begin(), market.end(),'/','-');
                        // получаем bid
                        if (auto bid_element{result["data"]["bid"].get_double()}; simdjson::SUCCESS == bid_element.error()) {
                            // получаем ask
                            if (auto ask_element{result["data"]["ask"].get_double()}; simdjson::SUCCESS == ask_element.error()) {
                                // получаем bidSize
                                if (auto bid_size_element{result["data"]["bidSize"].get_double()}; simdjson::SUCCESS == bid_size_element.error()) {
                                    // получаем askSize
                                    if (auto ask_size_element{result["data"]["askSize"].get_double()}; simdjson::SUCCESS == ask_size_element.error()) {
                                        STicker ticker{market,
                                                    std::to_string(bid_element.value()),
                                                    std::to_string(bid_size_element.value()),
                                                    std::to_string(ask_element.value()),
                                                    std::to_string(ask_size_element.value())};
                                        // отправляем "стакан" только в том случае, если значения отличны от предыдущих
                                        if (ticker.b.compare(prev_ticker.b) != ticker.B.compare(prev_ticker.B)!= ticker.a.compare(prev_ticker.a)!= ticker.A.compare(prev_ticker.A))
                                                ticker_sender(ticker, id_);
                                        prev_ticker = ticker;
                                    } else {
                                        _error.describe(fmt::format("Ошибка получения объема (askSize) лучшей заявки на продажу. json body: {}", message_));
                                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                        _error.clear();
                                    }
                                } else {
                                    _error.describe(fmt::format("Ошибка получения объема (bidSize) лучшей заявки на покупку. json body: {}", message_));
                                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                    _error.clear();
                                }
                            } else {
                                _error.describe(fmt::format("Ошибка получения цены (ask) лучшей заявки на продажу. json body: {}", message_));
                                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                                _error.clear();
                            }
                        } else {
                            _error.describe(fmt::format("Ошибка получения цены (bid) лучшей заявки на покупку. json body: {}", message_));
                            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                            _error.clear();
                        }
                    } else {
                        _error.describe(fmt::format("Ошибка получения тикерного символа. json body: {}", message_));
                        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                        _error.clear();
                    }
                } else {
                    // видимо было сообщение 1 типа
                }
            } else {
                _error.describe(fmt::format("Ошибка определения типа сообщения. json body: {}", message_));
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _error.clear();
            }
        } else {
            _error.describe("Ошибка разбора json фрейма.");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
        }
    } else {
        _error.describe("Ошибка инициализации парсера simdjson (внутренний буфер не выделился).");
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
        _error.clear();
    }
}*/
//---------------------------------------------------------------
// callback функция результата выставления оредров
//---------------------------------------------------------------
void gateway::place_order_result_handler(std::string_view message_)
{
    //std::cout << message_ << std::endl;
    try {
        // создадим парсер
        simdjson::dom::parser parser;
        // пусть парсер подготовит буфер для своих нужд
        auto &&error_code = parser.allocate(0x1000,0x04);
        // если буфер был успешно выделен
        if(simdjson::SUCCESS == error_code){
            // разбираем строку
            auto result = parser.parse(message_.data(), message_.size(), false);
            // если данные успешно разобрались
            if(simdjson::SUCCESS == result.error()){
                // получаем значение поля success
                auto element_success{result["success"].get_bool()};
                // если значения == true, значит ордер выставлен успешно
                if(element_success.value() == true){
                    auto res_value = result["result"];
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "(order_result_handler) Результат выставления ордера: " <<
                                                                                res_value;
                } else if(element_success.value() == false){
                    auto err_value = result["error"];
                    //std::cout << "Ошибка выставления ордера. Причина: " << err_value.get_c_str() << std::endl;
                    _error.describe(fmt::format("(order_result_handler) Ошибка выставления ордера. Причина: {}).", std::string(err_value.get_c_str())));
                    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                    _error.clear();
                }
            } else{
                _error.describe(fmt::format("Ошибка разбора json фрейма. ((order_result_handler) json body: {}).", message_));
                BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
                _error.clear();
            }
        } else{
            _error.describe(fmt::format("Ошибка инициализации парсера(внутренний буфер не выделился (parser.allocate(0x1000,0x04)). ((order_result_handler) json body: {}).", message_));
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
            _error.clear();
        }

    } catch(simdjson::simdjson_error& err){
        _error.describe(err.what() + fmt::format(" ((order_result_handler) json body: {}).", message_));
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error;
        _error.clear();
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию о лучших ценовых предложениях
//---------------------------------------------------------------
void gateway::ticker_sender(const STicker& best_ticker_, void* id_)
{
    pt::ptree root_ticker;
    root_ticker.put("exchange", "ftx");
    //root_ticker.put("s", "BTC-USDT");
    root_ticker.put("s", best_ticker_.s);
    root_ticker.put("b", best_ticker_.b);
    root_ticker.put("B", best_ticker_.B);
    root_ticker.put("a", best_ticker_.a);
    root_ticker.put("A", best_ticker_.A);
    std::stringstream jsonForCore;
    pt::write_json(jsonForCore, root_ticker, false);
    // отправляем json в aeron
    std::string message = jsonForCore.str();
 /*   const std::int64_t result = orderbook_channel->offer(message);
    if(result < 0){
        processing_error("error: Ошибка отправки информации о лучших ценовых предложениях в ядро: ", result);
    }
    else*/{
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) <<
                 //"send info about ticker: " << jsonForCore.str();
                 fmt::format("send info about ticker ({}): ", id_) << jsonForCore.str();
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию об открытых ордерах
//---------------------------------------------------------------
void gateway::order_sender(const std::vector<SOrder> &orders_vector_)
{
    pt::ptree root;
    pt::ptree children;
    for(auto&& order : orders_vector_)
    {
        /*pt::ptree order_node;
        order_node.put("id",     order.id);
        order_node.put("side",   order.side);
        order_node.put("type",   order.type);
        order_node.put("price",  order.price);
        order_node.put("status", order.status);
        order_node.put("clientId",       order.clientId);*/
        pt::ptree order_node;
        order_node.put("t", "000000");
        order_node.put("T", "i");
        order_node.put("p", "g");
        order_node.put("n", "ftx");
        order_node.put("c", 0);
        order_node.put("e", "order");
        children.push_back(std::make_pair("", order_node));
    }
    root.add_child("O", children);
    std::stringstream jsonForCore;
    pt::write_json(jsonForCore, root);
    // отправляем json в ядро
    std::string message = jsonForCore.str();
    //std::cout << "order sender: " << message <<std::endl;
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "Отправка информации об открытых ордерах: " << message;
    const std::int64_t result = log_channel->offer(message);
    if(result < 0){
        processing_error("error: Ошибка отправки информации об открытых ордерах в ядро: ", result);
    }
    else{
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "send info about ticker: " << jsonForCore.str();
    }
}
//---------------------------------------------------------------
// отправляет в ядро информацию о балансе
//---------------------------------------------------------------
void gateway::balance_sender(const std::vector<SBState>& balances_vector)
{
    // вспомогательная строка для визуального контроля баланса в консоли/логе
    std::string help_string;
    // формируем json в определенном формате
    pt::ptree root;
    pt::ptree children;
    for (auto&& balanceState: balances_vector)
    {
        pt::ptree balance_node;
        balance_node.put("exchange", "ftx");
        balance_node.put("a", balanceState.coin);
        balance_node.put("f", std::to_string(balanceState.free));
        children.push_back(std::make_pair("", balance_node));
        help_string += std::to_string(balanceState.total);
        help_string += " ";
    }
    root.add_child("B", children);
    std::stringstream jsonForCore;
    pt::write_json(jsonForCore, root);
    //отправляем json в aeron
    const std::int64_t result = balance_channel->offer(jsonForCore.str());
    if(result < 0)
    {
        processing_error("error: Ошибка отправки информации о балансе в ядро: ", result);
    }
    else
    {
        BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "send info about balance: " <<
                                                                 jsonForCore.str()<<
                                                                 " total: " <<
                                                                 help_string;
    }
}
//---------------------------------------------------------------
// обрабатывает ошибку
//---------------------------------------------------------------
void gateway::processing_error(std::string_view message_, const std::int64_t& error_code_)
{
    BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << message_;
    if(error_code_ == BACK_PRESSURED)
    {
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed due to back pressure.";
    }
    else if(error_code_ == NOT_CONNECTED)
    {
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed because publisher is not conntected to a core.";
    }
    else if(error_code_ == ADMIN_ACTION)
    {
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed because of an administration action in the system.";
    }
    else if(error_code_ == PUBLICATION_CLOSED)
    {
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed because publication is closed.";
    }
    else
    {
        BOOST_LOG_SEV(orderbook_logger, logging::trivial::info) << "Offer failed due to unknkown reason.";
    }
}
//---------------------------------------------------------------
// перезапускает публичный WS
//---------------------------------------------------------------
void gateway::restart_public_ws()
{
    ioc.reset();
    ftx_ws_public.reset();
    create_public_ws();
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "restart public ws channel";
}
//---------------------------------------------------------------
// перезапускает приватный WS
//---------------------------------------------------------------
void gateway::restart_private_ws(const std::string& reason_)
{
    ioc.reset();
    ftx_ws_private.reset();
    // потупим немного
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    // создаём сокет заново
    create_private_ws();
    start_trigger = true;
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "restart private ws channel: " << reason_;
}
//---------------------------------------------------------------
// перезапускает приватный REST
//---------------------------------------------------------------
/*void Gateway::restart_private_REST()
{
    ioc.reset();
    ftx_rest_private.reset();
    create_private_REST();
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "restart private REST channel";
}*/
void gateway::get_precision(SCurrencyCharacteristics& curr_characteristic_)
{
    curr_characteristic_.pricePrecision = get_precision(curr_characteristic_.priceIncrement);
    curr_characteristic_.sizePrecision  = get_precision(curr_characteristic_.sizeIncrement);
}
int gateway::get_precision(double value)
{
    int precision = 0;
    std::stringstream ss;
    ss << value;
    std::string CheckString = ss.str();
    size_t dotPos = CheckString.find('.');
    if(dotPos != CheckString.npos)
    {
        precision = CheckString.size() - 1 - dotPos;
    }
    return precision;
}
std::string gateway::set_size_precision(std::string value)
{
    // зададим значение по умолчанию
    std::string result = value;
    size_t dotPos = value.find('.');
    if(dotPos != value.npos)
    {
        value.erase(dotPos + (curr_characters.sizePrecision + 1));
        result = value;
    }
    return result;
}
std::string gateway::set_price_precision(std::string value)
{
    // зададим значение по умолчанию
    std::string result = value;
    size_t dotPos = value.find('.');
    if(dotPos != value.npos)
    {
        value.erase(dotPos + (curr_characters.pricePrecision));
        result = value;
    }
    return result;
}
//---------------------------------------------------------------
// проверяет баланс
//---------------------------------------------------------------
void gateway::check_balance(/*const bool &start_trigger_*/)
{
    // получаем баланс
    std::vector<SBState> balances_vector = ftx_rest_private->get_balances(_error);
    // если вектор нулевого размера, значит была какая-то ошибка и баланс мы не получили
    if(balances_vector.empty()){
        if(_error){
            _error.describe("Ошибка получения баланса (get_balances).");
            BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << "error: " << _error.to_string();
            _error.clear();
        }
    }
    else{
        // отправляем баланс в ядро
        balance_sender(balances_vector);
    }
}
//---------------------------------------------------------------
// отправляет ошибки
//---------------------------------------------------------------
void gateway::error_sender(std::string_view message_)
{
    BOOST_LOG_SEV(ftxgateway_logger, logging::trivial::info) << message_;
}
