FROM debian:bookworm-slim AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ src/
COPY platform/linux/ platform/linux/

WORKDIR /build/platform/linux
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j"$(nproc)"

# ---

FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r pdq && useradd -r -g pdq -d /home/pdq -m pdq

COPY --from=builder /build/platform/linux/build/pdqminer /usr/local/bin/pdqminer

USER pdq
WORKDIR /home/pdq

ENV PDQ_POOL_HOST=pool.nerdminers.org
ENV PDQ_POOL_PORT=3333
ENV PDQ_WALLET=""
ENV PDQ_WORKER=pdqlinux
ENV PDQ_THREADS=2
ENV PDQ_DIFFICULTY=1.0

ENTRYPOINT ["/usr/local/bin/pdqminer"]
