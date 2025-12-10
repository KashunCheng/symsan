pushd example_rl/complex
./build.sh
SPDLOG_LEVEL=trace /../../build/bin/RLDriver \
    --config rl.conf 

popd