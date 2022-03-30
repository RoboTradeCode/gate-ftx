#include "logging.hpp"

BOOST_LOG_ATTRIBUTE_KEYWORD(a_severity, "Severity", logging::trivial::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(a_channel, "Channel", std::string)


void init_logging()
{
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        keywords::filter = a_channel == "ftxGateway",
        keywords::format = "[%TimeStamp%]: %Message%",
        keywords::auto_flush = true
    );
    /*logging::add_console_log(
        std::cout,
        keywords::filter = a_channel == "ftxOrderBook",
        keywords::format = "[%TimeStamp%]: %Message%",
        keywords::auto_flush = true
    );*/

    logging::add_file_log(
        keywords::filter = a_channel == "ftxGateway",
        keywords::file_name = "logs/ftxGateway_%N.log",
        keywords::format = "[%TimeStamp%]: %Message%",
        keywords::open_mode = std::ios_base::app,
        keywords::auto_flush = true,
        keywords::rotation_size = 10 * 1024 * 1024
    );
    logging::add_file_log(
        keywords::filter = a_channel == "ftxOrderBook",
        keywords::file_name = "logs/ftxOrderBook_%N.log",
        keywords::format = "[%TimeStamp%]: %Message%",
        keywords::open_mode = std::ios_base::app,
        keywords::auto_flush = true,
        keywords::rotation_size = 10 * 1024 * 1024
    );
    logging::add_file_log(
        keywords::filter = a_channel == "ftxPingPong",
        keywords::file_name = "logs/ftxPingPong_%N.log",
        keywords::format = "[%TimeStamp%]: %Message%",
        keywords::open_mode = std::ios_base::app,
        keywords::auto_flush = true,
        keywords::rotation_size = 10 * 1024 * 1024
    );
    logging::add_file_log(
        keywords::filter = a_channel == "error",
        keywords::file_name = "logs/error.log",
        keywords::format = "[%TimeStamp%]: %Message%",
        keywords::open_mode = std::ios_base::app,
        keywords::auto_flush = true,
        keywords::rotation_size = 10 * 1024 * 1024
    );
}
