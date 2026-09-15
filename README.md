# AM5 Native Memory Lab

Windows x64 原生内存实测与主动访问模式诊断程序的源码仓库。

本目录从原始 `AM5_Native_Memory_Lab_Windows_x64.zip` 提取；`source/` 中的源码、构建脚本、报告模板、测试脚本和历史开发测试记录保持原字节内容。
本源码包不包含预编译的 `AM5MemoryLab.exe`，也没有重新执行原包中的历史验证测试。

## 内容

- `source/src/`：C11 测试内核、Win32/Linux 平台层、结果生成和自检代码。
- `source/report/template.html`：离线 HTML/JavaScript 报告。
- `source/build_builds.py`：Linux 验证程序及 Windows x64 EXE 的交叉构建脚本。
- `source/winshim/`：Windows 交叉编译接口声明。
- `source/tests/`：原始开发测试脚本、日志和样本。
- `profile.ini`：配置注释示例，不读取或修改 BIOS，不用于生成实测分数。
- `SOURCE_BUILD.md`：原始构建说明。
- `VALIDATION.md`：原包记录的验证范围及未完成事项。
- `README_zh-CN.md`：原始完整分发包说明；其中“附带 EXE”指原分发包，而非这个源码仓库。
- `SOURCE_SHA256SUMS.txt`：本次提取的原始文件校验清单。
- `PUBLISH.md`：在本机新建私有 GitHub 仓库并提交源码的命令。

## 构建

原构建脚本面向 Linux x86-64 开发环境，需要 Python 3、Clang/LLVM（原记录为 17）、`lld-link` 和 POSIX threads：

```sh
cd source
python3 build_builds.py
./build/am5lab-linux --self-test
```

构建产物位于 `source/build/`。在 Windows 上使用根目录 `.cmd` 启动文件时，应将构建所得的 `AM5MemoryLab.exe` 放到根目录。

## 边界与验证状态

程序执行真实 AVX2 内存访问和依赖指针追逐；报告提供本次访问模式的对照与候选参数组，不读取每个 BIOS 时序的独立硬件贡献。
没有集成 IBS/UMC 计数器、物理地址到 DRAM Bank/Row 映射或 BIOS 自动写入。

原包记录了 Linux 下的开发验证，但 Windows 可执行文件尚未在 Windows 或目标 9700X 上实际运行验证。参见 `VALIDATION.md`；内核自检不等于内存超频稳定性测试。

本次仅整理源码仓库，没有新设开源许可证，也没有发布编译产物或启用 CI 工作流。上传命令默认创建私有仓库。
