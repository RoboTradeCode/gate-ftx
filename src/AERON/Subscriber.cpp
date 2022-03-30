#include "Subscriber.hpp"

/**
 * Создать экземпляр Subscriber и подключиться к каналу
 *
 * @param handler Функция обратного вызова для обработки каждого сообщения
 * @param channel Канал Aeron
 * @param stream_id Уникальный идентификатор потока в канале. Значение 0 зарезервировано, его использовать нельзя
 * @param fragment_limit Максимальное количество фрагментов в сообщении
 */
Subscriber::Subscriber(std::function<void(std::string_view)> handler, const std::string& channel, int stream_id,
    int fragment_limit)
    : handler(std::move(handler)),
      FRAGMENT_LIMIT(fragment_limit),
      fragment_assembler(
          [&](const aeron::AtomicBuffer& buffer, aeron::util::index_t offset, aeron::util::index_t length,
              const aeron::Header& header)
          { shared_from_this()->fragment_handler(buffer, offset, length, header); })
{
    // Подключение к медиа-драйверу
    // https://github.com/real-logic/aeron/wiki/Cpp-Programming-Guide#aeron
    aeron = aeron::Aeron::connect();

    // Инициализация объекта Subscription, с помощью которого принимаются сообщения
    //
    // Сначала вызывается метод addSubscription. Этот метод неблокирующий — сначала он возвращает результат (уникальный
    // идентификатор регистрации) и только потом пытается добавить Subscription. Чтобы убедиться в том, что операция
    // добавления прошла успешно и получить объект Subscription, нужно дополнитель вызвать метод findSubscription,
    // передав ему идентификатор регистрации
    //
    // Метод findSubscription тоже является неблокирующим и сразу же возвращает результат. Но операция добавления
    // Subscription занимает некоторое время. И если вызвать findSubscription в момент, когда Subscription ещё не
    // добавлен, вернётся nullptr. Поэтому findSubscription вызывается в цикле — до тех пор, пока вместо nullptr не
    // вернётся объект
    //
    // https://github.com/real-logic/aeron/wiki/Cpp-Programming-Guide#subscription
    long id = aeron->addSubscription(channel, stream_id);
    subscription = aeron->findSubscription(id);
    while (!subscription)
    {
        std::this_thread::yield();
        subscription = aeron->findSubscription(id);
    }
}

/**
 * Добавить конечную точку
 *
 * @param destination Канал Aeron
 */
void Subscriber::add_destination(const std::string& destination)
{
    subscription->addDestination(destination);
}

/**
 * Удалить конечную точку
 *
 * @param destination Канал Aeron
 */
void Subscriber::remove_destination(const std::string& destination)
{
    subscription->removeDestination(destination);
}

/**
 * Проверить наличие новых сообщений. Если новые сообщения имеются, они будут получены и переданы в функцию
 * обратного вызова
 *
 * @return Количество полученных фрагментов
 */
int Subscriber::poll()
{
    return subscription->poll(fragment_assembler.handler(), FRAGMENT_LIMIT);
}

/**
 * Функция обратного вызова для обработки каждого сообщения
 */
void Subscriber::fragment_handler(const aeron::AtomicBuffer& buffer, aeron::util::index_t offset,
    aeron::util::index_t length, const aeron::Header& header)
{
    std::string message(reinterpret_cast<const char *>(buffer.buffer()) + offset, static_cast<std::size_t>(length));
    handler(message);
}
