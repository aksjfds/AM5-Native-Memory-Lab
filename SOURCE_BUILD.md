# 源码与构建

终端用户直接运行已附带的 AM5MemoryLab.exe，不需要执行构建。

源代码为独立的 C11/AVX2 测试内核和 Win32 平台层，HTML 只是离线展示实际结果。没有把旧的 AIDA64 手工录入统计工具伪装成原生实测。

开发复现环境：Linux x86-64、Python 3、Clang/LLVM 17（含 lld-link 和 llvm-objdump）、POSIX threads。

```
cd source
python build_builds.py
./build/am5lab-linux --self-test
```

脚本先把 report/template.html 嵌入 C 头文件，再构建 Linux 验证程序和 Windows x86-64 EXE。Windows 构建使用显式、最小化 Win32 ABI 声明和系统 DLL 导入库，不需要在构建环境安装 Microsoft SDK；Windows 应用本身仍是真正的原生 EXE。

重要文件：

- src/platform.c / src/win_min.h：计时、拓扑、绑核、页面分配、文件、WHEA、程序启动。
- src/kernels.c：AVX2 读写与依赖指针链，所有性能都由这些真实访问生成。
- src/bench.c：线程同步、计时、负载矩阵、方向切换扫描、短窗口尾延迟、逐样本保存。
- src/diagnostic.c：本轮差分证据、稳健噪声、回归拟合与参数组优先级。
- src/selftest.c：60 项运行时内核/数学自检。
- src/report.c / report/template.html：结果序列化、当前运行的主动诊断与中文报告。
- src/main.c：入口、运行选项和流程。
- winshim/：交叉编译时使用的最小系统 C 接口声明，不是随包运行库。
- tests/：开发环境验证脚本和记录。验证脚本中的样本数以所附 5-CPU Linux 测试为准，不是任意机器都固定 180。

当前 GitHub Actions 会在 Windows runner 上对同一发布候选 EXE 执行 60 项自检和 smoke workflow；根目录 `VALIDATION.md` 保留的是最初开发包的历史验证记录，不代表当前 CI 状态。
