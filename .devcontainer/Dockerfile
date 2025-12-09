FROM ubuntu:noble

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

WORKDIR /work
RUN apt-get update && apt-get install -y cmake llvm-14 clang-14 libclang-14-dev libc++-14-dev libc++abi-14-dev libunwind-14-dev \
    python3-minimal python-is-python3 zlib1g-dev git joe libprotobuf-dev libz3-dev libgoogle-perftools-dev libboost-container-dev python3-dev libspdlog-dev \
    && apt clean && rm -rf /var/lib/apt/lists/*
RUN git clone --depth=1 --branch=v4.31c https://github.com/AFLplusplus/AFLplusplus /work/aflpp && \
    cd /work/aflpp && make PERFORMANCE=1 LLVM_CONFIG=llvm-config-14 NO_NYX=1 source-only -j `nproc` && make install
RUN cd /work && git clone https://github.com/msgpack/msgpack-c.git && \
    cd msgpack-c && \
    git checkout cpp-7.0.0 && \
    CC=clang-14 CXX=clang++-14 cmake . && \
    cmake --build . --target install
 RUN cd /work && \
    git clone --recurse-submodules -b v1.76.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc && \
    cd grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    CC=clang-14 CXX=clang++-14 cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_CXX_STANDARD=17 \
      ../.. && \
    make -j `nproc` && \
    make install
# Install base dependencies and development tools
# RUN apt-get update && apt-get install -y \
    # && apt clean && rm -rf /var/lib/apt/lists/*


# RUN git clone --depth=1 --branch=v4.31c https://github.com/AFLplusplus/AFLplusplus /work/aflpp && \
#     cd /work/aflpp && \
#     make PERFORMANCE=1 LLVM_CONFIG=llvm-config-14 NO_NYX=1 source-only -j `nproc` && \
#     make install && \
#     cd /work && rm -rf /work/aflpp/.git
# COPY . /work/symsan
# RUN cd /work/symsan/ && mkdir -p build && \
#     cd build && CC=clang-14 CXX=clang++-14 cmake -DCMAKE_INSTALL_PREFIX=. -DAFLPP_PATH=/work/aflpp ../  && \
#     make -j `nproc` && make install

ENV KO_CC=clang-14
ENV KO_CXX=clang++-14
ENV KO_USE_FASTGEN=1

# Create non-root user vscode
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN (groupadd --gid $USER_GID $USERNAME 2>/dev/null || true) \
    && (useradd --uid $USER_UID --gid $USER_GID -m $USERNAME 2>/dev/null || usermod -l $USERNAME ubuntu) \
    && mkdir -p /etc/sudoers.d \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

RUN apt update &&apt install python3-pip python3-grpcio python3-grpc-tools  -y
# Switch to vscode user
USER $USERNAME

WORKDIR /workspaces/symsan
