cmake_minimum_required(VERSION 3.23)
project(kvstore LANGUAGES CXX)

# ───────────── Global options ─────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(METRICS     "Build with Prometheus metrics collection" OFF)
option(BUILD_TESTS "Build unit tests"                         ON)

# ───────────── Dependencies ───────────────
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC     CONFIG REQUIRED)          # gRPC::grpc++, grpc_cpp_plugin
find_package(RocksDB          REQUIRED)
find_package(NuRaft           REQUIRED)
find_package(OpenSSL          REQUIRED)
find_package(fmt CONFIG       REQUIRED)
find_package(spdlog CONFIG    REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads          REQUIRED)

if(BUILD_TESTS)
  find_package(GTest CONFIG REQUIRED)
endif()

# ───────────── Locate every .proto file ─────────────
file(GLOB_RECURSE PROTO_FILES "${CMAKE_SOURCE_DIR}/*.proto")
if(NOT PROTO_FILES)
  message(FATAL_ERROR "∅  No .proto files found in ${CMAKE_SOURCE_DIR}")
endif()

# ───────────── Code-generation (protobuf + gRPC) ─────────────
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

protobuf_generate(
  LANGUAGE cpp
  OUT_VAR PROTO_SRCS                       # → list saved in PROTO_SRCS
  PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
  PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
  FILES ${PROTO_FILES}
)

add_library(kv_proto STATIC ${PROTO_SRCS})
# Generated files replicate the source-tree sub-folders under ${CMAKE_CURRENT_BINARY_DIR}
target_include_directories(kv_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/include)   # if your protos live under include/
target_link_libraries(kv_proto
    PUBLIC protobuf::libprotobuf gRPC::grpc++)

# ───────────── Core library ─────────────
file(GLOB_RECURSE CORE_SRC
     CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/src/core/*.cpp"
     "${CMAKE_SOURCE_DIR}/src/storage/*.cpp"
     "${CMAKE_SOURCE_DIR}/src/raft/*.cpp"
     "${CMAKE_SOURCE_DIR}/include/**/*.hpp")

add_library(kvcore STATIC ${CORE_SRC})
target_include_directories(kvcore PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(kvcore
    PUBLIC
      kv_proto
      RocksDB::rocksdb
      NuRaft::nuraft
      fmt::fmt
      spdlog::spdlog
      Threads::Threads
      OpenSSL::Crypto
      nlohmann_json::nlohmann_json)
target_compile_definitions(kvcore
    PRIVATE $<$<BOOL:${METRICS}>:ENABLE_METRICS>)

# ───────────── Binaries ─────────────
add_executable(kv_server  src/server/KVServer.cpp  src/server/main.cpp)
add_executable(kv_client  src/client/KVClient.cpp)
add_executable(benchmark  src/client/benchmark.cpp)

target_link_libraries(kv_server  PRIVATE kvcore)
target_link_libraries(kv_client  PRIVATE kvcore)
target_link_libraries(benchmark  PRIVATE kvcore)

# ───────────── Unit tests ─────────────
if(BUILD_TESTS)
  enable_testing()
  file(GLOB_RECURSE TEST_SRC "${CMAKE_SOURCE_DIR}/tests/*.cpp")
  add_executable(kv_tests ${TEST_SRC})
  target_link_libraries(kv_tests PRIVATE kvcore GTest::gtest_main GTest::gtest)
  add_test(NAME all COMMAND kv_tests)
endif()

# ───────────── Formatting helper ─────────────
file(GLOB_RECURSE ALL_CXX_SOURCE
     CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/include/**/*.hpp"
     "${CMAKE_SOURCE_DIR}/src/**/*.cpp")
add_custom_target(format COMMAND clang-format -i ${ALL_CXX_SOURCE})

# ───────────── Install ─────────────
install(TARGETS kv_server kv_client benchmark
        RUNTIME DESTINATION bin)
