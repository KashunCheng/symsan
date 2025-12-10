pushd example_rl/control_temp
./build.sh
SPDLOG_LEVEL=trace ../../build/bin/RLDriver \
    --config rl.conf 

popd