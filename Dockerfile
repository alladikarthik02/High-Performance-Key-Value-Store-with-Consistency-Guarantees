# syntax=docker/dockerfile:1
########################################
# 1) Builder
########################################
FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

# Toolchain & basics
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential git ca-certificates wget curl pkg-config ninja-build gpg \
    && rm -rf /var/lib/apt/lists/*

# Newer CMake (Kitware)
RUN wget -qO - https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor -o /usr/share/keyrings/kitware.gpg && \
    echo "deb [signed-by=/usr/share/keyrings/kitware.gpg] https://apt.kitware.com/ubuntu/ jammy main" \
    > /etc/apt/sources.list.d/kitware.list && \
    apt-get update && apt-get install -y --no-install-recommends cmake && \
    rm -rf /var/lib/apt/lists/*

# Build deps (ADD codec -dev packages here)
RUN apt-get update && apt-get install -y --no-install-recommends \
    librocksdb-dev libspdlog-dev libfmt-dev libssl-dev zlib1g-dev \
    libprotobuf-dev protobuf-compiler protobuf-compiler-grpc libgrpc++-dev \
    nlohmann-json3-dev libasio-dev \
    libzstd-dev libbz2-dev liblz4-dev libsnappy-dev \
    && rm -rf /var/lib/apt/lists/*

# NuRaft (pin + explicit targets to avoid duplicate rules)
ARG NURAFT_TAG=v1.3.0
WORKDIR /opt
RUN git clone --depth 1 --branch ${NURAFT_TAG} https://github.com/eBay/NuRaft.git nuraft && \
    cmake -S nuraft -B nuraft/build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build nuraft/build --target shared_lib static_lib -j"$(nproc)" && \
    cmake --install nuraft/build

# Project
WORKDIR /src
COPY . .
RUN rm -rf third_party/nuraft || true

# Build project (tests off in container)
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF && \
    cmake --build build --target all -j"$(nproc)"

########################################
# 2) Runtime
########################################
FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive

# runtime libs (keep reflection .so available)
RUN apt-get update && apt-get install -y --no-install-recommends \
    librocksdb-dev libsnappy1v5 libzstd1 liblz4-1 libbz2-1.0 zlib1g \
    libgrpc++1 libgrpc++-dev libprotobuf23 \
    libssl3 libfmt8 libspdlog1 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# NuRaft runtime
COPY --from=builder /usr/local/ /usr/local/
RUN ldconfig

# Copy app binaries as root (so perms are fine)
COPY --from=builder /src/build/kv_server /usr/local/bin/
COPY --from=builder /src/build/kv_client /usr/local/bin/

# Create a writable data dir and switch to non-root
RUN useradd -m kvuser && mkdir -p /data && chown -R kvuser:kvuser /data
USER kvuser
WORKDIR /data
VOLUME ["/data"]

EXPOSE 50051
# Single, final entrypoint that sets a safe default
ENTRYPOINT ["/usr/local/bin/kv_server","--id","1","--listen","0.0.0.0:50051","--data","/data"]
