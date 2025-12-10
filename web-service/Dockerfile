# Build steps:
#   1) Build the symsan builder image from repo root: docker build -t symsan-builder .
#   2) Build this image using the builder artifacts:
#      docker build -t symsan-web-service -f web-service/Dockerfile . --build-arg BUILDER_IMAGE=symsan-builder

ARG BUILDER_IMAGE=jchen1192/symsan-rl:latest
FROM ${BUILDER_IMAGE} AS symsan-builder
WORKDIR /work/symsan

FROM ${BUILDER_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    FGTEST_PATH=/appdata/bin/fgtest \
    FGTEST_BIN_DIR=/appdata/bin \
    PATH="/opt/venv/bin:/appdata/bin:${PATH}"

WORKDIR /app

# Install cloudflared
RUN curl -L https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -o /usr/local/bin/cloudflared && \
    chmod +x /usr/local/bin/cloudflared

COPY web-service/requirements.txt .
RUN python3 -m venv /opt/venv && \
    /opt/venv/bin/pip install --no-cache-dir -r requirements.txt

RUN mkdir -p /appdata/bin
COPY --from=symsan-builder /work/symsan/build/bin/fgtest /appdata/bin/fgtest
COPY --from=symsan-builder /work/symsan/examples/dummy /appdata/bin/dummy
COPY --from=symsan-builder /work/symsan/examples/xor /appdata/bin/xor
COPY --from=symsan-builder /work/symsan/examples/control_temp /appdata/bin/control_temp
COPY --from=symsan-builder /work/symsan/examples/*ctwm_index.json /appdata/bin/

COPY web-service/app.py web-service/fgtest_wrapper.py ./
COPY web-service/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Ensure runtime directories exist
RUN mkdir -p uploads results /root/.cloudflared

EXPOSE 8000
ENTRYPOINT ["/entrypoint.sh"]
