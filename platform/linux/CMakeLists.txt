cmake_minimum_required(VERSION 3.16)
project(pdqminer_linux C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization: match ESP32 approach — Os for SHA256, O2 for the rest
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Source root
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# PDQminer Linux binary
add_executable(pdqminer
    # Linux platform layer
    ${PLATFORM_DIR}/main.c
    ${PLATFORM_DIR}/linux_hal.c
    ${PLATFORM_DIR}/linux_config.c
    ${PLATFORM_DIR}/linux_wifi.c
    ${PLATFORM_DIR}/linux_display.c
    ${PLATFORM_DIR}/linux_mining.c

    # Shared core sources (portable)
    ${SRC_DIR}/core/sha256_engine.c
    ${SRC_DIR}/stratum/stratum_client.c
    ${SRC_DIR}/api/device_api.c
)

target_include_directories(pdqminer PRIVATE
    ${SRC_DIR}
)

target_compile_definitions(pdqminer PRIVATE
    PDQ_HEADLESS=1
    PDQ_LINUX=1
    _GNU_SOURCE
)

# pthread
find_package(Threads REQUIRED)
target_link_libraries(pdqminer PRIVATE Threads::Threads)

# Compiler warnings
target_compile_options(pdqminer PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# Install target
install(TARGETS pdqminer DESTINATION bin)
