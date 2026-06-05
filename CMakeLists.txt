cmake_minimum_required(VERSION 3.16)
project(XenforoCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(XENFORO_BUILD_EXAMPLES "Build the example programs" ON)

# --- Dependencies ----------------------------------------------------------
include(FetchContent)

# libcurl (HTTP). Prefer a system / vcpkg install; otherwise build it from
# source via FetchContent so the project builds out of the box.
#   Fast path (Windows):  vcpkg install curl nlohmann-json
#   and configure with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    message(STATUS "System libcurl not found -- fetching and building it from source")
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)          # only HTTP/HTTPS
    set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
    if(WIN32)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)   # native Windows TLS
    endif()
    FetchContent_Declare(
        curl
        URL https://github.com/curl/curl/releases/download/curl-8_11_1/curl-8.11.1.tar.gz
    )
    FetchContent_MakeAvailable(curl)
endif()

# nlohmann/json: prefer an installed package, otherwise fetch it.
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# --- Library ---------------------------------------------------------------
add_library(xenforo
    src/HttpClient.cpp
    src/Client.cpp
)
add_library(xenforo::xenforo ALIAS xenforo)

target_include_directories(xenforo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(xenforo
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
)

# --- Examples --------------------------------------------------------------
if(XENFORO_BUILD_EXAMPLES)
    add_executable(login_example examples/login_example.cpp)
    target_link_libraries(login_example PRIVATE xenforo::xenforo)
endif()
