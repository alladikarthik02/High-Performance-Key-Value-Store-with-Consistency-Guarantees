########################################
# 1️⃣  Builder image                   #
########################################
FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

# base toolchain + ninja
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential git ca-certificates wget gpg curl pkg-config ninja-build \
    && rm -rf /var/lib/apt/lists/*

# latest CMake
RUN wget -qO - https://apt.kitware.com/keys/kitware-archive-latest.asc | \
    gpg --dearmor -o /usr/share/keyrings/kitware.gpg && \
    echo "deb [signed-by=/usr/share/keyrings/kitware.gpg] https://apt.kitware.com/ubuntu/ jammy main" \
    > /etc/apt/sources.list.d/kitware.list && \
    apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*

# build-time libs
RUN apt-get update && apt-get install -y --no-install-recommends \
    librocksdb-dev protobuf-compiler libprotobuf-dev \
    libssl-dev zlib1g-dev libfmt-dev libspdlog-dev nlohmann-json3-dev libasio-dev \
    && rm -rf /var/lib/apt/lists/*

# ── 4. gRPC v1.63.0  (bundled Protobuf + Abseil) ────────────────────
ENV GRPC_VERSION=v1.63.0
RUN git clone --depth 1 --recurse-submodules -b ${GRPC_VERSION} \
    https://github.com/grpc/grpc.git /tmp/grpc && \
    cmake -S /tmp/grpc -B /tmp/grpc/build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DgRPC_BUILD_TESTS=OFF \
    -DgRPC_ABSL_PROVIDER=module \
    -DgRPC_PROTOBUF_PROVIDER=module \
    -DgRPC_SSL_PROVIDER=package \
    -DgRPC_ZLIB_PROVIDER=package \
    -DgRPC_INSTALL=ON && \
    cmake --build /tmp/grpc/build --target install -j$(nproc) && \
    rm -rf /tmp/grpc

# RocksDB 8.11.3 (2 cores to fit 2 GB Docker Desktop memory)
ENV ROCKSDB_VERSION=v8.11.3
RUN git clone --depth 1 -b $ROCKSDB_VERSION https://github.com/facebook/rocksdb.git /tmp/rocksdb && \
    cmake -S /tmp/rocksdb -B /tmp/rocksdb/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=OFF -DWITH_TOOLS=OFF \
    -DWITH_BENCHMARK_TOOLS=OFF -DWITH_GFLAGS=OFF -DPORTABLE=1 && \
    cmake --build /tmp/rocksdb/build -j2 && \
    cmake --install /tmp/rocksdb/build && rm -rf /tmp/rocksdb

# NuRaft
RUN git clone --depth 1 https://github.com/eBay/NuRaft.git /tmp/nuraft && \
    cmake -S /tmp/nuraft -B /tmp/nuraft/build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build /tmp/nuraft/build --target install -j$(nproc) && \
    rm -rf /tmp/nuraft

# project
WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF && \
    cmake --build build -j$(nproc)

########################################
# 2️⃣  Runtime image                   #
########################################
FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    librocksdb-dev libprotobuf23 libssl3 zlib1g libfmt8 libspdlog1 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local /usr/local
COPY --from=builder /src/build/kv_server  /usr/local/bin/
COPY --from=builder /src/build/kv_client  /usr/local/bin/

RUN useradd -m kvuser
USER kvuser
WORKDIR /home/kvuser

EXPOSE 50051            
ENTRYPOINT ["/usr/local/bin/kv_server"]
