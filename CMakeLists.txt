cmake_minimum_required(VERSION 3.16)
project(PythonREPLServer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json REQUIRED)

# Add GraphQL parser library
add_subdirectory(third_party/libgraphqlparser)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/python_executor.cpp
    src/http_server.cpp
    src/graphql_handler.cpp
)

# Header files
set(HEADERS
    include/python_executor.h
    include/http_server.h
    include/graphql_handler.h
)

# Create executable
add_executable(python_repl_server ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(python_repl_server
    Boost::system
    nlohmann_json::nlohmann_json
    graphqlparser
    pthread
)

# Compiler flags
target_compile_options(python_repl_server PRIVATE
    -Wall
    -Wextra
    -O2
)

# Install target
install(TARGETS python_repl_server DESTINATION bin)