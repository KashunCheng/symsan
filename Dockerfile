FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

WORKDIR /work

RUN apt-get update
RUN apt-get install -y cmake llvm-14 clang-14 libclang-14-dev libc++-14-dev libc++abi-14-dev libunwind-14-dev \
    python3-minimal python-is-python3 zlib1g-dev git joe libprotobuf-dev libz3-dev libgoogle-perftools-dev libboost-container-dev python3-dev libspdlog-dev && \
    git clone --depth=1 --branch=v4.31c https://github.com/AFLplusplus/AFLplusplus /work/aflpp && \
    cd /work/aflpp && make PERFORMANCE=1 LLVM_CONFIG=llvm-config-14 NO_NYX=1 source-only -j $(nproc) && make install && \
    cd /work && git clone https://github.com/msgpack/msgpack-c.git && \
    cd msgpack-c && \
    git checkout cpp-7.0.0 && \
    CC=clang-14 CXX=clang++-14 cmake . && \
    cmake --build . --target install && \
    cd /work && \
    git clone --recurse-submodules -b v1.76.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc && \
    cd grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    CC=clang-14 CXX=clang++-14 cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_CXX_STANDARD=17 \
      ../.. && \
    make -j $(nproc) && \
    make install

RUN apt install python3-pip -y && pip install grpcio grpcio-tools --break-system-packages && apt clean

COPY . /work/symsan

RUN cd /work/symsan/ && mkdir -p build && \
    cd build && CC=clang-14 CXX=clang++-14 cmake -DCMAKE_INSTALL_PREFIX=. -DAFLPP_PATH=/work/aflpp ../ -DCMAKE_BUILD_TYPE=Debug && \
    make -j $(nproc) && make install

ENV KO_CC=clang-14
ENV KO_CXX=clang++-14
ENV KO_USE_FASTGEN=1
