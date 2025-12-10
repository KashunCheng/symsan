FROM  jchen1192/symsan-rl:latest

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

WORKDIR /work

# Install base dependencies and development tools
# RUN apt-get update && apt-get install -y \
    # && apt clean && rm -rf /var/lib/apt/lists/*

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

RUN apt update &&apt install python3-pip python3-grpcio python3-grpc-tools  -y && \
     apt clean && rm -rf /var/lib/apt/lists/*
# Switch to vscode user
USER $USERNAME

WORKDIR /work/symsan
