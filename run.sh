pushd example_rl/complex

SPDLOG_LEVEL=trace /workspaces/symsan/build/bin/RLDriver \
    --config rl.conf 

popd