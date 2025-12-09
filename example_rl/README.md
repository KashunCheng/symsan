# RL driver demo

This folder shows how to run `RLDriver` against sample targets. Two variants
are provided:

- `dummy` – a very small example copied from `example/`
- `complex` – a multi-branch program (only `if` statements, no loops/switches)

Files:
- `dummy` / `dummy.c` – simple target and source
- `complex` / `complex.c` – multi-branch target and source
- `dummy_input.bin` – seed input
- `complex_input.bin` – seed input for the complex target
- `dummy.linecov.msgpack` – line coverage map for the target
- `complex.linecov.msgpack` – coverage map for the complex target
- `rl.conf` – config consumed by `RLDriver`
- `rl_complex.conf` – config for the complex target
- `build_dummy.sh`, `build_complex.sh` – helper scripts to rebuild the targets

Build (from repo root):
```bash
cd example_rl
./build_dummy.sh
./build_complex.sh
```

Run the dummy target:
```bash
cd /work/symsan/build
./bin/RLDriver --config ../example_rl/rl.conf
```

Run the complex target (different port/output dir to avoid clobbering dummy):
```bash
cd /work/symsan/build
./bin/RLDriver --config ../example_rl/rl_complex.conf
```

In another shell, query it via the Python client (from repo root):
```bash
. .venv/bin/activate  # if using the local venv
python rl_client.py --endpoint 127.0.0.1:50051 read-lines
python rl_client.py --endpoint 127.0.0.1:50051 trace \
  --line-to-reach 0x<line_id> --branch <line_id>:true
```

Outputs will be written to `example_rl/out/`.

For the complex sample, adjust the endpoint/output directory:
```bash
python rl_client.py --endpoint 127.0.0.1:50052 read-lines
python rl_client.py --endpoint 127.0.0.1:50052 trace \
  --line-to-reach 0x<line_id> --branch <line_id>:true
```
Results are written to `example_rl/out_complex/`.
