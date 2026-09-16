# =============================================================================
# ЭТАП 1: Сборка (Builder)
# =============================================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libsodium-dev \
    libssl-dev \
    zlib1g-dev \
    uuid-dev \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Собираем jsoncpp из исходников
RUN git clone https://github.com/open-source-parsers/jsoncpp.git && \
    cd jsoncpp && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DBUILD_SHARED_LIBS=ON \
             -DJSONCPP_WITH_TESTS=OFF \
             -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF && \
    make -j$(nproc) && \
    make install

# Собираем Drogon из исходников
RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install

RUN ldconfig

COPY . .

RUN mkdir -p build_app && cd build_app \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
    && make -j$(nproc)


# =============================================================================
# ЭТАП 2: Финальный легкий образ (Runtime)
# =============================================================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    zlib1g \
    libuuid1 \
    libsodium23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /usr/local/lib/libdrogon.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libjsoncpp.so* /usr/local/lib/

RUN ldconfig

COPY --from=builder /app/build_app/petty_plusik ./
COPY --from=builder /app/config.json ./

RUN ln -s /app/config.json /config.json

EXPOSE 8080

# ВАЖНО: Указываем путь к конфигу явно
CMD ["./petty_plusik", "-c", "/app/config.json"]