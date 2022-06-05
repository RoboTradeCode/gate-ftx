#include "ASyncHTTP.hpp"

AsyncHTTPSession::AsyncHTTPSession(net::any_io_executor ex,
                 ssl::context& ctx,
                 const std::string& api_key,
                 const std::string& api_secret,
                 std::function<void(std::string)> event_handler)
: resolver_(ex)
, stream_(ex, ctx)
, api_key_(api_key)
, api_secret_(api_secret)
, event_handler(std::move(event_handler))
{
    //std::cout << "session is create" << std::endl;
    host_ = "ftx.com";
    port_ = "443";
}

AsyncHTTPSession::~AsyncHTTPSession() {
    //std::cout << "session is destroyed" << std::endl;
}
void AsyncHTTPSession::get(const std::string& target) {
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if(! SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
    {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
    }

    // Set up an HTTP GET request message
    req_.method(http::verb::get);
    req_.target(target.c_str());
    req_.set(http::field::host, host_);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Look up the domain name
    resolver_.async_resolve(host_, port_, beast::bind_front_handler(&AsyncHTTPSession::on_resolve, shared_from_this()));
}
void AsyncHTTPSession::post(const std::string &target, const std::string &payload) {
    if(! SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
    {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
    }

    // Set up an HTTP POST request message
    req_.method(http::verb::post);
    req_.body() = payload;
    req_.prepare_payload();
    req_.target(target.c_str());
    req_.set(http::field::host, host_);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Look up the domain name
    resolver_.async_resolve(host_, port_, beast::bind_front_handler(&AsyncHTTPSession::on_resolve, shared_from_this()));
}
void AsyncHTTPSession::delete_(const std::string& target, const std::string &payload) {
    if(! SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
    {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
    }
    // Set up an HTTP POST request message
    req_.method(http::verb::delete_);
    req_.body() = payload;
    req_.prepare_payload();
    req_.target(target.c_str());
    req_.set(http::field::host, host_);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Look up the domain name
    resolver_.async_resolve(host_, port_, beast::bind_front_handler(&AsyncHTTPSession::on_resolve, shared_from_this()));
}
void AsyncHTTPSession::delete_(const std::string &target_) {
    if(! SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
    {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
    }
    // Set up an HTTP POST request message
    req_.method(http::verb::delete_);
    req_.target(target_.c_str());
    req_.set(http::field::host, host_);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Look up the domain name
    resolver_.async_resolve(host_, port_, beast::bind_front_handler(&AsyncHTTPSession::on_resolve, shared_from_this()));
}
void AsyncHTTPSession::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if(ec)
        return ;

    // Set a timeout on the operation
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    beast::get_lowest_layer(stream_).async_connect(results, beast::bind_front_handler(&AsyncHTTPSession::on_connect,shared_from_this()));
}
void AsyncHTTPSession::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    if(ec)
        return ;

    // Perform the SSL handshake
    stream_.async_handshake(ssl::stream_base::client, beast::bind_front_handler(&AsyncHTTPSession::on_handshake, shared_from_this()));
}
void AsyncHTTPSession::on_handshake(beast::error_code ec) {
    if(ec)
        return ;

    // Set a timeout on the operation
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
    authenticate(req_);
    // Send the HTTP request to the remote host
    http::async_write(stream_, req_, beast::bind_front_handler(&AsyncHTTPSession::on_write, shared_from_this()));
}
void AsyncHTTPSession::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return ;

    // Receive the HTTP response
    http::async_read(stream_, buffer_, res_, beast::bind_front_handler(&AsyncHTTPSession::on_read, shared_from_this()));
}
void AsyncHTTPSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return ;

    // Write the message to standard out
    //std::cout << res_.body() << std::endl;
    event_handler(res_.body());

    // Set a timeout on the operation
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Gracefully close the stream
    stream_.async_shutdown(beast::bind_front_handler(&AsyncHTTPSession::on_shutdown, shared_from_this()));
}
void AsyncHTTPSession::on_shutdown(beast::error_code ec) {
    if(ec == net::error::eof){
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};

    }
    if(ec)
        return ;
        // If we get here then the connection is closed gracefully
}
void AsyncHTTPSession::authenticate(http::request<http::string_body>& req) {
    std::string method(req.method_string());
    std::string body(req.body());
    std::string path(req.target());

    long ts = util::get_ms_timestamp(std::chrono::system_clock::now()).count();
    std::string data = std::to_string(ts) + method + path;
    if (!body.empty()) {
        data += body;
    }
    std::string hmacced = util::_encoding::hmac(std::string(api_secret_), data, 32);
    std::string sign = util::_encoding::str_to_hex((unsigned char*)hmacced.c_str(), 32);

    req.set("FTX-KEY", api_key_);
    req.set("FTX-TS", std::to_string(ts));
    req.set("FTX-SIGN", sign);
    req.set("FTX-SUBACCOUNT", "SecondAcc");

}
void AsyncHTTPSession::fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

