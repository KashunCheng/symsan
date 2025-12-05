#!/usr/bin/env bash
set -euo pipefail

# Build the web-service image along with its required builder image.
#
# Environment variables:
#   BUILDER_IMAGE      - Name for the builder image (default: symsan-builder)
#   WEB_IMAGE          - Name for the web-service image (default: symsan-web-service)
#   NO_CACHE           - Disable cache for all builds (set to any non-empty value)
#   NO_CACHE_WEB       - Disable cache only for web-service image
#   SKIP_BUILDER       - Skip building the builder image (use existing)
#   USE_DEFAULT_BUILDER - Use default docker builder instead of buildx (default: 1)
#                        Set to empty to use buildx (may have issues with local images)
#   BUILDX_BUILDER     - Use a specific buildx builder (implies buildx mode)
#   BUILDX_PROGRESS    - Progress output type (default: auto)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WEB_DIR="${ROOT_DIR}/web-service"

BUILDER_IMAGE="${BUILDER_IMAGE:-symsan-builder}"
WEB_IMAGE="${WEB_IMAGE:-symsan-web-service}"
BUILDX_BUILDER="${BUILDX_BUILDER:-}"
BUILDX_PROGRESS="${BUILDX_PROGRESS:-auto}"
NO_CACHE="${NO_CACHE:-}"
NO_CACHE_WEB="${NO_CACHE_WEB:-}"
SKIP_BUILDER="${SKIP_BUILDER:-}"
# Use default docker builder to access local images (not a container-based buildx builder)
USE_DEFAULT_BUILDER="${USE_DEFAULT_BUILDER:-1}"

buildx() {
  local no_cache_flag="$1"
  shift
  local args=(build)
  
  # Only use buildx if explicitly using a custom builder
  if [[ -n "${BUILDX_BUILDER}" ]]; then
    args=(buildx build --builder "${BUILDX_BUILDER}")
  elif [[ -z "${USE_DEFAULT_BUILDER}" ]]; then
    args=(buildx build)
  fi
  
  args+=(--progress="${BUILDX_PROGRESS}")
  
  if [[ -n "${no_cache_flag}" ]]; then
    args+=(--no-cache)
  fi
  docker "${args[@]}" "$@"
}

if [[ -z "${SKIP_BUILDER}" ]]; then
  echo "==> Building builder image: ${BUILDER_IMAGE}"
  buildx "${NO_CACHE}" \
    -t "${BUILDER_IMAGE}" \
    "${ROOT_DIR}"
else
  echo "==> Skipping builder image (SKIP_BUILDER is set)"
fi

echo "==> Building web-service image: ${WEB_IMAGE}"
# Use NO_CACHE_WEB if set, otherwise fall back to NO_CACHE
web_no_cache="${NO_CACHE_WEB:-${NO_CACHE}}"
buildx "${web_no_cache}" \
  -t "${WEB_IMAGE}" \
  -f "${WEB_DIR}/Dockerfile" \
  --build-arg "BUILDER_IMAGE=${BUILDER_IMAGE}" \
  "${ROOT_DIR}"

echo "==> Done. Images built:"
docker images "${WEB_IMAGE}"
docker images "${BUILDER_IMAGE}"
