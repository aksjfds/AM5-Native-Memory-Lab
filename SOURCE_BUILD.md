# Source build notes

Current engine: `AM5-Native-2.2.2-COPY-GROUPS-AVX2`; results schema: `AM5Native/4`; diagnostic scope: `parameter_group_only`.

The build script regenerates `source/src/report_template.h` from `source/report/template.html`, builds a Linux validation binary, and cross-compiles the native Windows x64 executable with Clang/LLD.

Required on Linux: Python 3, Clang, LLD/lld-link, pthreads, standard C library headers.

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

Expected self-test count: **65**.

Normal Linux and Windows compilation uses `-Wall -Wextra` (plus `-Wpedantic` where applicable) with `-Werror`; warning regressions therefore fail CI.

GitHub Actions additionally builds a Linux `-O1 -g` validation binary with AddressSanitizer and UndefinedBehaviorSanitizer, then runs both self-test and smoke mode before packaging the Windows executable. The sanitizer binary is a CI-only artifact and is not shipped to end users.

The Windows target is freestanding at compile time and linked against explicitly generated import libraries for the Windows APIs and CRT symbols used by the program. `kernels.c` is compiled with AVX2 enabled; startup and CPU detection remain baseline x64.

`profile.ini` is currently only a temporary parameter snapshot. It is not a hardware readback mechanism and does not control the benchmark score or Copy bottleneck ranking. Oversized (>64 KiB) or failed reads are rejected rather than silently truncated.

v2.2.2 deliberately limits diagnosis to parameter groups. The serialized `parameter_group` field is derived from the diagnostic path ID and no longer represents a selected individual timing. Concrete timing values may still be shown as snapshot/context data, but the analyzer does not rank members within a group.
