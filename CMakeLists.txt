cmake_minimum_required(VERSION 3.5)

project(ftxGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_AERON /home/kirill/aeron)

find_package(Boost 1.78.0 REQUIRED COMPONENTS log_setup log json)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt)

include_directories(${LIB_AERON}/aeron-client/src/main/cpp)
# for fmt library
include_directories(/usr/local/include)


add_executable(
    ftxGateway
    main.cpp
    gateway.cpp
    gateway.hpp
    src/AERON/Publisher.cpp
    src/AERON/Publisher.hpp
    src/AERON/Subscriber.cpp
    src/AERON/Subscriber.hpp
    src/config/config.cpp
    src/config/config.hpp
    src/utils/logging.cpp
    src/utils/logging.hpp
    src/utils/Encoding.cpp
    src/utils/Encoding.hpp
    src/utils/simdjson.cpp
    src/utils/simdjson.hpp
    src/utils/json.hpp
    src/utils/HTTP.cpp
    src/utils/HTTP.hpp
    src/utils/ASyncHTTP.cpp
    src/utils/ASyncHTTP.hpp
    src/utils/decl_header.hpp
    src/utils/Time.hpp
    src/utils/error.hpp
    src/REST/rest_client.cpp
    src/REST/rest_client.hpp
    src/REST/async_rest_client.cpp
    src/REST/async_rest_client.hpp
    src/WS/WS.cpp
    src/WS/WS.hpp
    src/WS/ws_client.cpp
    src/WS/ws_client.hpp
    )
target_link_libraries(
    ftxGateway
    Threads::Threads
    OpenSSL::SSL
    Boost::log_setup
    Boost::log
    Boost::json
    fmt::fmt
    ${LIB_AERON}/cppbuild/Release/lib/libaeron_client.a
    )
