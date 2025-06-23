# High-Performance C++ Key-Value Store

A lightweight, high-performance key-value database written in modern C++20.

* **RocksDB** – fast, persistent storage  
* **NuRaft** – Raft consensus for strong replication  
* **gRPC** – clean language-agnostic API (put / get / delete / scan)  
* **Consistent-Hash Ring** – simple horizontal sharding  
* **Docker Compose** – spin up a three-node cluster with one command  

---

## Directory Layout

```text
CMakeLists.txt           – CMake build script
Dockerfile               – multi-stage image (build + runtime)
docker-compose.yml       – local 3-node demo cluster
include/                 – public headers
src/                     – implementation files
proto/                   – kvstore.proto, raft.proto
tests/                   – GoogleTest unit tests
scripts/                 – helper scripts (launch.sh, load_test.sh)
```

---

## Quick Start (Docker Compose)

> **Prerequisites:** Docker 20.10 + and Docker Compose v2.

```bash
# build the image and start a 3-node cluster
docker compose up --build
```

| Node | Raft ID | Host Port |
|------|---------|-----------|
| kv1  | 1       | 50051     |
| kv2  | 2       | 50052     |
| kv3  | 3       | 50053     |

### Sanity Check with `grpcurl`

```bash
# write
grpcurl -plaintext \
  -d '{"key":"foo","value":"bar"}' \
  localhost:50051 kv.KeyValue/Put

# read (linearizable – hits the leader)
grpcurl -plaintext \
  -d '{"key":"foo"}' \
  localhost:50051 kv.KeyValue/Get
```

Expected output:

```json
{
  "found": true,
  "value": "bar"
}
```

---

## Building from Source (optional)

**Requires:** CMake ≥ 3.23, a C++20 compiler (GCC 12 + / Clang 15 +), plus:

* RocksDB  
* Protobuf + gRPC development headers  
* spdlog, fmt, nlohmann-json, Google Test  

```bash
mkdir -B build
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build -j                 # server + client + tests
ctest --test-dir build                 # run unit tests
```

---

## gRPC API Snapshot

```proto
service KeyValue {
  rpc Put             (PutRequest)       returns (PutResponse);
  rpc Delete          (DeleteRequest)    returns (DeleteResponse);
  rpc Get             (GetRequest)       returns (GetResponse); // leader-only
  rpc GetLinearizable (GetRequest)       returns (GetResponse); // any replica
  rpc GetStale        (GetRequest)       returns (GetResponse); // may be stale
  rpc ScanRange       (ScanRangeRequest) returns (stream KeyValuePair);
  rpc ScanPrefix      (PrefixRequest)    returns (stream KeyValuePair);
}
```

See **`proto/kvstore.proto`** for full message definitions.

---

## Contributing

1. Fork the repository.  
2. Create a feature branch: `git checkout -b feature/my-change`.  
3. Run `clang-format` (or `make format`) before committing.  
4. Open a pull request.

---

## Roadmap

* Cluster join/leave with automatic re-sharding  
* TLS for gRPC and Raft channels  
* Performance benchmarks against Redis and RocksDB-native  
* CI pipeline (GitHub Actions) with sanitizers and static analysis  
* Kubernetes Helm chart for production deployment  
