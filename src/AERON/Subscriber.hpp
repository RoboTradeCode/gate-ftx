#ifndef AERONUTILS_SUBSCRIBER_H
#define AERONUTILS_SUBSCRIBER_H


#include <Aeron.h>
#include <FragmentAssembler.h>

/**
 * Класс для приёма сообщений с помощью протокола Aeron
 */
class Subscriber : public std::enable_shared_from_this<Subscriber>
{
    // Медиа-драйвер
    std::shared_ptr<aeron::Aeron> aeron;

    // Объект, с помощью которого принимаются сообщения
    std::shared_ptr<aeron::Subscription> subscription;

    // Пользовательская функция обратного вызова для обработки каждого сообщения
    std::function<void (std::string_view)> handler;

    // Максимальное количество фрагментов в сообщении
    int FRAGMENT_LIMIT;

    // Сборщик сообщения из фрагментов
    aeron::FragmentAssembler fragment_assembler;

    /**
     * Функция обратного вызова для обработки каждого сообщения
     */
    void fragment_handler(const aeron::AtomicBuffer& buffer, aeron::util::index_t offset, aeron::util::index_t length,
        const aeron::Header& header);

public:
    /**
     * Создать экземпляр Subscriber и подключиться к каналу
     *
     * @param handler Функция обратного вызова для обработки каждого сообщения
     * @param channel Канал Aeron
     * @param stream_id Уникальный идентификатор потока в канале. Значение 0 зарезервировано, его использовать нельзя
     * @param fragment_limit Максимальное количество фрагментов в сообщении
     */
    explicit Subscriber(std::function<void(std::string_view)> handler,
        const std::string& channel = "aeron:udp?control-mode=manual", int stream_id = 1001, int fragment_limit = 10);

    /**
     * Добавить конечную точку
     *
     * @param destination Канал Aeron
     */
    void add_destination(const std::string& destination);

    /**
     * Удалить конечную точку
     *
     * @param destination Канал Aeron
     */
    void remove_destination(const std::string& destination);

    /**
     * Проверить наличие новых сообщений. Если новые сообщения имеются, они будут получены и переданы в функцию
     * обратного вызова
     *
     * @return Количество полученных фрагментов
     */
    int poll();
};


#endif  // AERONUTILS_SUBSCRIBER_H
