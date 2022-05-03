#include "WS.hpp"

#include <memory>
#include <iostream>

//logger_type error_logger(keywords::channel = "error");

namespace util
{


    WS::WS(std::string host, const std::string& port, const std::string& target,
           net::io_context& ioc, std::function<void(std::string, void*)> event_handler, const std::shared_ptr<spdlog::logger> &logger)
        :event_handler(std::move(event_handler))
    {
        _errors_logger = logger;
        ssl::context ssl_ctx{ssl::context::tls_client};
        //ssl::context ssl_ctx{ssl::context::tlsv12_client};
        // какая-то не понятная дичь взятая вот от сюда - https://github.com/djarek/certify/blob/2d719a9ad79ce1a61684278a30196527e412a0b6/examples/get_page.cpp#L77
        ssl_ctx.set_verify_mode(ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert);
        ssl_ctx.set_default_verify_paths();
        // tag::ctx_setup_source[]
        //boost::certify::enable_native_https_server_verification(ssl_ctx);

        ws = std::make_shared<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(ioc, ssl_ctx);

        tcp::resolver resolver{ioc};
        auto results = resolver.resolve(host, port);
        auto ep = beast::get_lowest_layer(*ws).connect(results);

        if(!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.data()))
        {
            boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error(ec);
        }
        host += ':' + std::to_string(ep.port());

        ws->next_layer().handshake(ssl::stream_base::client);

        beast::get_lowest_layer(*ws).expires_never();
        ws->set_option(websocket::stream_base::stream_base::timeout{
                           std::chrono::seconds(30),
                           std::chrono::seconds(30),
                           true
                       });
        //ws->handshake(host, "/ws");
        ws->handshake(host, target);
     }

    size_t WS::write(const std::string& message)
    {
        return ws->write(net::buffer(message));
    }
    void WS::async_read()
    {
        ws->async_read(buffer, beast::bind_front_handler(&WS::on_read, shared_from_this()));
    }
    void WS::on_read(beast::error_code ec, std::size_t bytes_transgerred)
    {
        boost::ignore_unused(bytes_transgerred);
        if(ec)
        {
            //if(ec.message().compare("stream truncated") == 0)
            /*if(ec == boost::asio::ssl::error::stream_truncated)
            {
                //BOOST_LOG_SEV(orders_logger, logging::trivial::info) << cancel_result;
                //std::cout << "!!!! STREAM WAS TRUNCATED !!!" << "what: " << ec.what()<< std::endl;
                BOOST_LOG_SEV(error_logger, logging::trivial::info) << "what: " << ec.what()
                                                                    << " message: " << ec.message()
                                                                    << " bytes: " << bytes_transgerred
                                                                    << " channel " << channelName;
            }
            else if(ec.message().compare("Operation canceled") == 0)
            {
                //std::cout << "!!!! OPERATION CANCELED !!!" << "what: " << ec.what()<< std::endl;
                BOOST_LOG_SEV(error_logger, logging::trivial::info) << "what: " << ec.what()
                                                                    << " message: " << ec.message()
                                                                    << " bytes: " << bytes_transgerred
                                                                    << " channel " << channelName;
            }
            else
            {
                BOOST_LOG_SEV(error_logger, logging::trivial::info) << "what: " << ec.what()
                                                                    << " message: " << ec.message()
                                                                    << " bytes: " << bytes_transgerred
                                                                    << " channel " << channelName;;
                throw std::runtime_error("on_read: " + ec.message() + "what: " + ec.what());
            }
            return;*/
            /*
             * ещё была такая
             * ERROR: handshake: Connection reset by peer [system:104], но где непонятно. Приложение просто завершило
             * работу
             */
            /*BOOST_LOG_SEV(error_logger, logging::trivial::info) << "what: " << ec.what()
                                                                << " message: " << ec.message()
                                                                << " bytes: " << bytes_transgerred
                                                                << " channel " << channelName;*/
            _errors_logger->error("Wat: {}, message {}, channel {}.", ec.what(), ec.message(), channelName);

            throw std::runtime_error("on_read: " + channelName);
        }


        event_handler(beast::buffers_to_string(buffer.data()), this);
        buffer.clear();
        ws->async_read(buffer, beast::bind_front_handler(&WS::on_read, shared_from_this()));
    }

  }
