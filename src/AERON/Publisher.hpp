#ifndef AERONUTILS_PUBLISHER_H
#define AERONUTILS_PUBLISHER_H


#include <Aeron.h>

/**
 * Класс для отправки сообщений с помощью протокола Aeron
 */
class Publisher
{
    // Медиа-драйвер
    std::shared_ptr<aeron::Aeron> aeron;

    // Объект, с помощью которого отправляются сообщения.
    std::shared_ptr<aeron::Publication> publication;

    // Буфер, которым инициализируется AtomicBuffer. Напрямую не используется
    std::vector<std::uint8_t> buffer;

    // Буфер, который Aeron использует для отправки сообщения
    aeron::concurrent::AtomicBuffer src_buffer;

public:
    /**
     * Создать экземпляр Publisher и подключиться к каналу
     *
     * @param channel Канал Aeron. В общем случае для указания канала используется URI
     * @param stream_id Уникальный идентификатор потока в канале. Значение 0 зарезервировано, его использовать нельзя
     * @param buffer_size Размер буфера, используемого для отправки и приема сообщений
     */
    explicit Publisher(const std::string& channel, int stream_id = 1001, int buffer_size = 1400);

    /**
     * Отправить сообщение
     *
     * @param message Сообщение для отправки
     */
    int64_t offer(const std::string& message);
};


#endif  // AERONUTILS_PUBLISHER_H
