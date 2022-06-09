#include <memory>
#include <iostream>

#include "WS.hpp"
namespace util
{


    WS::WS(std::string host_,
           const std::string& port_,
           const std::string& target_,
           net::io_context& ioc_,
           std::function<void(std::string/*, void**/)> event_handler_,
           const std::shared_ptr<spdlog::logger> &logger_)
        :_event_handler(std::move(event_handler_))
    {
        _errors_logger = logger_;
        ssl::context ssl_ctx{ssl::context::tls_client};
        //ssl::context ssl_ctx{ssl::context::tlsv12_client};
        // какая-то не понятная дичь взятая вот от сюда - https://github.com/djarek/certify/blob/2d719a9ad79ce1a61684278a30196527e412a0b6/examples/get_page.cpp#L77
        ssl_ctx.set_verify_mode(ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert);
        ssl_ctx.set_default_verify_paths();
        // tag::ctx_setup_source[]
        //boost::certify::enable_native_https_server_verification(ssl_ctx);

        _ws = std::make_shared<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(ioc_, ssl_ctx);

        tcp::resolver resolver{ioc_};
        auto results = resolver.resolve(host_, port_);
        auto ep = beast::get_lowest_layer(*_ws).connect(results);

        if(!SSL_set_tlsext_host_name(_ws->next_layer().native_handle(), host_.data()))
        {
            boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error(ec);
        }
        host_ += ':' + std::to_string(ep.port());

        _ws->next_layer().handshake(ssl::stream_base::client);

        beast::get_lowest_layer(*_ws).expires_never();
        _ws->set_option(websocket::stream_base::stream_base::timeout{
                           std::chrono::seconds(30),
                           std::chrono::seconds(30),
                           true
                       });
        //ws->handshake(host, "/ws");
        _ws->handshake(host_, target_);
     }

    size_t WS::write(const std::string& message)
    {
        return _ws->write(net::buffer(message));
    }
    void WS::async_read()
    {
        _ws->async_read(_buffer, beast::bind_front_handler(&WS::on_read, shared_from_this()));
    }
    void WS::on_read(beast::error_code ec, std::size_t bytes_transgerred)
    {
        boost::ignore_unused(bytes_transgerred);
        if(ec)
        {
            _errors_logger->error("Wat: {}, message {}, channel {}.", ec.what(), ec.message(), _channel_name);

            throw std::runtime_error("on_read: " + _channel_name);
        }
        _event_handler(beast::buffers_to_string(_buffer.data())/*, this*/);
        _buffer.clear();
        _ws->async_read(_buffer, beast::bind_front_handler(&WS::on_read, shared_from_this()));
    }

  }
