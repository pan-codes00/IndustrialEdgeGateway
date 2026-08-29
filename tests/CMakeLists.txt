cmake_minimum_required(VERSION 3.16)

project(
        IndustrialEdgeGateway
        VERSION 0.1.0
        LANGUAGES CXX
    )

include(CTest)        

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(
    gateway_server
    src/server.cpp
        )

add_executable(
    device_simulator
    src/client.cpp
        )
add_executable(
    gateway_query
    tools/gateway_query.cpp
        )
#创建库
add_library(
    gateway_common
    STATIC
    src/common/socket_utils.cpp
        )
add_library(
    gateway_protocol
    STATIC
    src/protocol/device_protocol.cpp
        )
add_library(
    gateway_core
    STATIC
    src/gateway/device_registry.cpp
        )
add_library(
    gateway_storage
    STATIC
    src/storage/sqlite_storage.cpp
        )
add_library(
    gateway_config
    STATIC
    src/config/gateway_config.cpp
        )

#设置头文件目录
target_include_directories(
    gateway_common
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_include_directories(
    gateway_protocol
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_include_directories(
    gateway_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_include_directories(
    gateway_storage
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_include_directories(
    gateway_config
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

#开启编译警告
target_compile_options(
    gateway_server
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    device_simulator
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    gateway_common
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    gateway_protocol
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    gateway_core
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    gateway_storage
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    gateway_config
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )
target_compile_options(
    gateway_query
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )

#日志模块
add_library(
    gateway_logging
    STATIC
    src/logging/logger.cpp
        )
target_include_directories(
    gateway_logging
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_compile_options(
    gateway_logging
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
        )

#链接依赖库
#两个连接公共库
#服务端
target_link_libraries(
    gateway_server
    PRIVATE
    gateway_common
    gateway_core
    gateway_protocol
    gateway_storage
    gateway_config
    gateway_logging
    Threads::Threads
    )
#客户端
target_link_libraries(
    device_simulator
    PRIVATE
    gateway_common
    Threads::Threads
    )
target_link_libraries(
    gateway_core
    PUBLIC
    gateway_protocol
    Threads::Threads
    )
target_link_libraries(
    gateway_storage
    PUBLIC
    gateway_protocol
    SQLite::SQLite3
    Threads::Threads
    )
target_link_libraries(
    gateway_query
    PRIVATE
    gateway_storage
    )

if(BUILD_TESTING)
    add_executable(
    protocol_tests
    tests/protocol_tests.cpp
)
    target_compile_options(
        protocol_tests
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    target_link_libraries(
        protocol_tests
        PRIVATE
        gateway_protocol
    )
    add_test(
        NAME protocol_tests
            COMMAND protocol_tests
    )
endif()


if(BUILD_TESTING)
    add_executable(
    device_registry_tests
    tests/device_registry_tests.cpp
)
    target_compile_options(
        device_registry_tests
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    target_link_libraries(
        device_registry_tests
        PRIVATE
        gateway_core
    )
    add_test(
        NAME device_registry_tests
        COMMAND device_registry_tests
    )
endif()

if(BUILD_TESTING)
    add_executable(
        sqlite_storage_tests
        tests/sqlite_storage_tests.cpp
    )

    target_compile_options(
        sqlite_storage_tests
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_link_libraries(
        sqlite_storage_tests
        PRIVATE
        gateway_storage
    )

    add_test(
        NAME sqlite_storage_tests
        COMMAND sqlite_storage_tests
    )
endif()


