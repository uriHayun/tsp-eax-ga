cmake_minimum_required(VERSION 3.24)
project(TSP-EAX-GA VERSION 1.00 LANGUAGES C CXX)  # C needed for vendoring whereami++.hpp - C++ wrapper of whereami.h
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2)

set(BOOST_INCLUDE_LIBRARIES algorithm)
set(BOOST_ENABLE_CMAKE ON)

FetchContent_Declare(boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    USES_TERMINAL_DOWNLOAD TRUE)

set(BUILD_CURL_EXE OFF)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(HTTP_ONLY ON)
set(CURL_USE_LIBPSL OFF)

if (WIN32)
    set(CURL_USE_SCHANNEL ON)
else ()
    set(CURL_USE_OPENSSL ON)
endif ()

FetchContent_Declare(curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_10_1
    GIT_SHALLOW    TRUE)

FetchContent_Declare(dotenv
    GIT_REPOSITORY https://github.com/Ayush272002/dotenv.git
    GIT_TAG        v1.0.2)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)

FetchContent_MakeAvailable(asio boost curl dotenv json)

add_executable(tsp_solver
    src/main.cpp src/crossover.cpp
    external/whereami/whereami.c external/whereami/whereami++.cpp)

target_include_directories(tsp_solver PRIVATE
    include
    external/whereami
    ${asio_SOURCE_DIR}/asio/include)

set_source_files_properties(src/main.cpp PROPERTIES
    COMPILE_DEFINITIONS ASIO_STANDALONE)

target_link_libraries(tsp_solver PRIVATE
    nlohmann_json::nlohmann_json
    Boost::algorithm
    CURL::libcurl
    dotenv)

if (WIN32)
    # ws2_32: link the system libraries' socket functions which are unlinked on default on Windows
    # mswsock: Microsoft's Winsock extensions - used by Asio
    target_link_libraries(tsp_solver PRIVATE ws2_32 mswsock)
endif ()