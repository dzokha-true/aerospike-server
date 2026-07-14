# EC528: build asd from THIS source tree (submodules already initialized in
# the repo; no network fetch of server source) and run it with a
# vector-configured namespace. Native arch (arm64 on the dev host; the same
# file builds x86_64 images on x86 hosts).
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential git autoconf automake libtool pkg-config cmake \
    python3 zlib1g-dev libssl-dev ca-certificates \
    gcc-11-plugin-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN make -j"$(nproc)" && ls target/Linux-*/bin/asd

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gettext-base libssl3 zlib1g \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /etc/aerospike /var/log/aerospike /opt/aerospike/data \
       /opt/aerospike/usr/udf/lua /opt/aerospike/smd

COPY --from=builder /build/target/Linux-*/bin/asd /usr/bin/asd
COPY docker/aerospike-vector.conf /etc/aerospike/aerospike-vector.conf.template
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Vector namespace config knobs (see docker/README.md).
ENV VECTOR_DIM=64 \
    VECTOR_TYPE=float \
    VECTOR_METRIC=l2 \
    AEROSPIKE_VECTOR_SIMD=auto \
    NODE_ID= \
    MESH_SEED=

EXPOSE 3000 3001 3002

ENTRYPOINT ["/entrypoint.sh"]
