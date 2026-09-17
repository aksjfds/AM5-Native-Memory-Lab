# Source build notes

Current engine: `AM5-Native-2.2.0-COPY-AVX2`; results schema: `AM5Native/4`.

The build script regenerates `source/src/report_template.h` from `source/report/template.html`, builds a Linux validation binary, and cross-compiles the native Windows x64 executable with Clang/LLD.

Required on Linux: Python 3, Clang, LLD/lld-link, pthreads, standard C library headers.

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

Expected self-test count: **60**.

The Windows target is freestanding at compile time and linked against explicitly generated import libraries for the Windows APIs and CRT symbols used by the program. `kernels.c` is compiled with AVX2 enabled; startup and CPU detection remain baseline x64.

`profile.ini` is currently only a temporary parameter snapshot. It is not a hardware readback mechanism and does not control the benchmark score or Copy bottleneck ranking.
