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

set(LOVE_VERSION 11.5)
set(LOVE_URL https://github.com/love2d/love/releases/download/${LOVE_VERSION})

if (WIN32)
    FetchContent_Declare(love URL ${LOVE_URL}/love-${LOVE_VERSION}-win64.zip)
elseif (APPLE)
    FetchContent_Declare(love URL ${LOVE_URL}/love-${LOVE_VERSION}-macos.zip)
else ()
    FetchContent_Declare(love
        URL ${LOVE_URL}/love-${LOVE_VERSION}-x86_64.AppImage
        DOWNLOAD_NO_EXTRACT TRUE)
endif ()

FetchContent_MakeAvailable(asio boost curl dotenv json love)

add_executable(tsp_solver
    src/main.cpp src/crossover.cpp
    external/whereami/whereami.c external/whereami/whereami++.cpp)

# Copies LÖVE next to the executable so the C++ backend can launch it
if (WIN32)
    add_custom_target(copy_love ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${love_SOURCE_DIR} $<TARGET_FILE_DIR:tsp_solver>)
elseif (NOT APPLE)
    add_custom_target(copy_love ALL
        COMMAND ${CMAKE_COMMAND} -E copy
                ${love_SOURCE_DIR}/love-${LOVE_VERSION}-x86_64.AppImage
                $<TARGET_FILE_DIR:tsp_solver>/love
        COMMAND chmod +x $<TARGET_FILE_DIR:tsp_solver>/love)
endif ()

if (TARGET copy_love)
    add_dependencies(tsp_solver copy_love)
endif ()

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

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(tsp_solver PRIVATE stdc++exp)
endif ()

# Copies Lua frontend files next to the executable so the C++ backend can launch it
add_custom_target(copy_frontend ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/frontend
            $<TARGET_FILE_DIR:tsp_solver>/src/frontend
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/external/json-lua/json.lua
            $<TARGET_FILE_DIR:tsp_solver>/src/frontend/json.lua)
add_dependencies(tsp_solver copy_frontend)