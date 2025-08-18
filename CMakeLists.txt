cmake_minimum_required(VERSION 3.18)
project(kvstore LANGUAGES CXX)

# Build settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_definitions(ASIO_STANDALONE)
option(BUILD_TESTS "Build unit tests" OFF)

# ---- Dependencies ----
find_package(Protobuf REQUIRED)             # protoc + Protobuf::libprotobuf
find_package(OpenSSL REQUIRED)              # OpenSSL::Crypto
find_package(fmt REQUIRED)                  # fmt::fmt
find_package(spdlog REQUIRED)               # spdlog::spdlog
find_package(nlohmann_json REQUIRED)        # nlohmann_json::nlohmann_json
find_package(Threads REQUIRED)

# gRPC: Ubuntu 22.04 lacks gRPCConfig.cmake; use pkg-config instead.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)     # provides GRPC_INCLUDE_DIRS / GRPC_LIBRARIES
find_program(GRPC_CPP_PLUGIN NAMES grpc_cpp_plugin REQUIRED)

# NuRaft (installed by Dockerfile to /usr/local)
find_library(NURAFT_LIB nuraft REQUIRED)
find_path(NURAFT_INCLUDE_DIR libnuraft/nuraft.hxx REQUIRED)

# RocksDB (no rocksdb.pc on Jammy → manual find)
find_library(ROCKSDB_LIBRARY NAMES rocksdb REQUIRED)
find_path(ROCKSDB_INCLUDE_DIR NAMES rocksdb/db.h REQUIRED)
if(NOT ROCKSDB_LIBRARY OR NOT ROCKSDB_INCLUDE_DIR)
  message(FATAL_ERROR "RocksDB not found. Install librocksdb-dev.")
endif()

# ---- Protobuf / gRPC codegen ----
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/proto)
file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto")

set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_SRCS "")
set(PROTO_HDRS "")
foreach(P ${PROTO_FILES})
  get_filename_component(F ${P} NAME_WE)
  add_custom_command(
    OUTPUT  ${GEN_DIR}/${F}.pb.cc      ${GEN_DIR}/${F}.pb.h
            ${GEN_DIR}/${F}.grpc.pb.cc ${GEN_DIR}/${F}.grpc.pb.h
    COMMAND protoc -I ${PROTO_DIR}
            --cpp_out=${GEN_DIR}
            --grpc_out=${GEN_DIR}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${P}
    DEPENDS ${P}
    VERBATIM
  )
  list(APPEND PROTO_SRCS ${GEN_DIR}/${F}.pb.cc ${GEN_DIR}/${F}.grpc.pb.cc)
  list(APPEND PROTO_HDRS ${GEN_DIR}/${F}.pb.h  ${GEN_DIR}/${F}.grpc.pb.h)
endforeach()

add_library(kv_proto ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(kv_proto PUBLIC ${GEN_DIR} ${GRPC_INCLUDE_DIRS})
target_link_libraries(kv_proto PUBLIC ${GRPC_LIBRARIES} protobuf::libprotobuf)

# ---- Core library ----
file(GLOB_RECURSE KV_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/core/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/raft/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/storage/*.cpp"
)

add_library(kvcore ${KV_CORE_SOURCES})
target_include_directories(kvcore
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${GEN_DIR}
    ${NURAFT_INCLUDE_DIR}
    ${ROCKSDB_INCLUDE_DIR}
    ${GRPC_INCLUDE_DIRS}
)
target_link_libraries(kvcore
  PUBLIC
    ${ROCKSDB_LIBRARY}
    kv_proto
    ${GRPC_LIBRARIES}
    protobuf::libprotobuf
    ${NURAFT_LIB}
    OpenSSL::Crypto
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Threads::Threads
    z zstd bz2 lz4 snappy         # common RocksDB codecs
)

# ---- Binaries ----
add_executable(kv_server
  ${CMAKE_SOURCE_DIR}/src/server/KVServer.cpp
  ${CMAKE_SOURCE_DIR}/src/server/main.cpp
)
target_link_libraries(kv_server PRIVATE
  kvcore
  grpc++_reflection
)

add_executable(kv_client ${CMAKE_SOURCE_DIR}/src/client/KVClient.cpp)
target_link_libraries(kv_client PRIVATE kvcore)

add_executable(benchmark ${CMAKE_SOURCE_DIR}/src/client/benchmark.cpp)
target_link_libraries(benchmark PRIVATE kvcore)

# ---- Tests (optional) ----
if(BUILD_TESTS)
  enable_testing()
  # add_subdirectory(tests)
endif()
