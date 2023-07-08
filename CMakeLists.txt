cmake_minimum_required(VERSION 3.18)

project(website LANGUAGES CXX VERSION 0.1.0)

# Figure out git hash, if any
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    RESULT_VARIABLE GIT_COMMAND_SUCCESS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

if (GIT_COMMAND_SUCCESS EQUAL 0)
    set(WEBSITE_FULL_VERSION "${PROJECT_VERSION}.${GIT_COMMIT_HASH}")
else()
    set(WEBSITE_FULL_VERSION "${PROJECT_VERSION}")
endif()

# Create configure file to store CMake build parameter
configure_file("${PROJECT_SOURCE_DIR}/include/website_config.hpp.config"
               "${PROJECT_BINARY_DIR}/website_config.hpp")

# Uncomment to enable sanitizer (will need commenting out static standard libs below)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

include(FetchContent)

set(BUILD_SHARED_LIBS "No")

FetchContent_Declare(lexy
                     GIT_REPOSITORY https://github.com/foonathan/lexy.git
                     GIT_TAG        v2022.12.1)
FetchContent_MakeAvailable(lexy)

FetchContent_Declare(json
                     GIT_REPOSITORY https://github.com/nlohmann/json.git
                     GIT_TAG        v3.11.2)
FetchContent_MakeAvailable(json)

FetchContent_Declare(asio
                     GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
                     GIT_TAG        asio-1-28-0)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR "${PROJECT_BINARY_DIR}/_deps/asio-src/asio/include")
set(CROW_INSTALL "No")

FetchContent_Declare(crow
                     GIT_REPOSITORY https://github.com/cschreib/Crow.git
                     GIT_TAG        5e97a6b968ae72310d8d1b3ba12b6647b19aaac9)
FetchContent_MakeAvailable(crow)

add_executable(website
    ${PROJECT_SOURCE_DIR}/src/markdown.cpp
    ${PROJECT_SOURCE_DIR}/src/html.cpp
    ${PROJECT_SOURCE_DIR}/src/utility.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp)

target_compile_features(website PRIVATE cxx_std_23)
target_include_directories(website PRIVATE
    ${MD4C_INCLUDE_DIR}
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(website PRIVATE "-Wno-psabi")
endif()

target_link_libraries(website PRIVATE
    nlohmann_json::nlohmann_json
    Crow::Crow
    foonathan::lexy)

target_link_libraries(website PRIVATE
    -static -static-libgcc -static-libstdc++)
