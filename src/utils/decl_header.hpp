#ifndef DECL_HEADER_H
#define DECL_HEADER_H

#include <string>
#include <fmt/format.h>

using namespace std;
// описывает состояние баланса
struct SBState{
    // ассет
    std::string coin;
    // общее количество средств
    double total;
    // свободные средства
    double free;
    // среднее количество средств в USD
    double usdValue;
};

struct STicker{
    // тикерный символ
    std::string s;
    // bid, цена лучшей заявки на покупку
    std::string b;
    // объём лучшей заявки на покупку
    std::string B;
    // ask, цена лучшей заявки на продажу
    std::string a;
    // объём лучшей заявки на продажу
    std::string A;
};
struct SOrder{
    // время создания оредра
    std::string createdAt;
    // идентификатор ордера (присваивается биржей)
    uint64_t    id;
    // цена
    double      price;
    // покупка или продажа
    std::string side;
    // статуч ордера (new, open или close)
    std::string status;
    // тип ордера (рыночный/лимитный)
    std::string type;
    // клиентский идентификатор (присваивает шлюз, не может повторяться)
    std::string clientId;
};

struct SCurrencyCharacteristics
{
    // шаг объёма
    double priceIncrement;
    // шаг цены
    double sizeIncrement;
    int    pricePrecision;
    int    sizePrecision;
};

#endif // DECL_HEADER_H
