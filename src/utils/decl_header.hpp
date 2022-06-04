#ifndef DECL_HEADER_H
#define DECL_HEADER_H

#include <string>
#include "../spdlog/spdlog.h"


using namespace std;
// описывает состояние баланса
struct s_balances_state{
    // ассет
    std::string coin;
    // общее количество средств
    double total;
    // свободные средства
    double free;
    // среднее количество средств в USD
    double usdValue;
};

struct s_order{
    // время создания оредра
    std::string createdAt;
    // идентификатор ордера (присваивается биржей)
    int64_t    id;
    // цена
    double      price;
    // покупка или продажа
    std::string side;
    // статуч ордера (new, open или close)
    std::string status;
    // тип ордера (рыночный/лимитный)
    std::string type;
    // клиентский идентификатор (присваивает шлюз, не может повторяться)
    //std::string clientId;
    std::string symbol;
    // количество
    double amount;
    //
    double filled;
    //
    double remaining;
};
struct order_status {
    // подробное описание что произошло с ордеро
    std::string description;
    // может принимать одно из следующих значений: create_order, cancel_order, order_status
    std::string action;
    // сообщение (не регламентировано)
    std::string message;
};
//


#endif // DECL_HEADER_H
