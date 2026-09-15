# AM5 Native Memory Lab

[![Build and Release](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml/badge.svg)](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml)

Windows x64 原生内存实测与主动访问模式诊断程序。核心为 C11/AVX2，使用真实内存访问和依赖指针追逐，离线 HTML 用于展示结果。

## 下载与运行

在 [Releases](https://github.com/aksjfds/AM5-Native-Memory-Lab/releases) 下载 `AM5MemoryLab-Windows-x64.zip`，完整解压后运行 `START.cmd`。运行需要支持 AVX2 的 Windows x64 电脑，不需要安装 Python、编译器或额外驱动。`QUICK.cmd` 为快速测试，`SELFTEST.cmd` 仅运行内核自检。

程序不会修改 BIOS、电压或系统防护。`profile.ini` 只记录参数注释，不读取 BIOS，也不生成或影响实测成绩。程序未签名；不要为运行它关闭系统防护。

## 自动编译与 Release

工作流：`.github/workflows/build-release.yml`，名称 **Build and Release**。

| 触发方式 | 行为 |
|---|---|
| 推送到 `main` | 编译、验证，并发布独立的 `ci-运行编号-尝试编号` 预发布版 |
| 向 `main` 提交 Pull Request | 编译和验证，仅保存 Actions artifacts，不发布 Release |
| 推送 `v` 加数字开头的标签，如 `v2.0.0` | 编译、验证，使用该标签发布正式 Release |
| 推送含 `-` 的版本标签，如 `v2.0.0-rc.1` | 使用该标签发布预发布版，不覆盖 Latest 正式版本 |
| 在 Actions 页面手动 Run workflow | `main` 发布 CI 预发布版；版本标签按标签规则发布；其他分支只构建 |

构建沿用现有 `source/build_builds.py`：Ubuntu 24.04 安装 Clang/LLD 18，生成 Linux 验证程序和 Windows x64 EXE。Linux 执行 56 项内核自检和小工作集端到端测试，随后打包 Windows 程序。

Windows Server 2022 runner 下载**同一份 ZIP**，校验 SHA-256、解压、启动 EXE，执行 56 项内核自检和小工作集测试（包含有空格和中文的输出路径）。`scripts/verify_smoke.py` 校验完成状态、错误数、计时计算和 JSON/CSV/HTML 输出，不对云主机跑分高低设门槛。

只有 Linux 和 Windows 两个检查均成功，独立发布任务才上传以下附件：

- `AM5MemoryLab-Windows-x64.zip`：EXE、启动脚本、配置示例、中文说明和构建信息。
- `BUILD_INFO.txt`：完整源码提交号、编译器版本、EXE 校验值和 Actions 运行地址。
- `SHA256SUMS.txt`：ZIP 与构建信息文件的 SHA-256。

Actions 同时保留发布候选包和两端测试日志 14 天。Release 附件不受该 artifact 保留期限制。同名 Release 已存在时停止发布，不删除标签、不覆盖原有附件、不强制推送。普通 `main` 构建为预发布版，不抢占 Latest 正式版本。

不需要额外配置 PAT 或 Secret；构建任务只有 `contents: read`，发布任务使用 GitHub 自动提供的 `GITHUB_TOKEN` 和 `contents: write`。仓库须允许 GitHub Actions 运行及该权限；不要将工作流改为执行不可信 PR 的 `pull_request_target`。

正式版本示例（在准备发布的本地提交上执行）：

```sh
git tag v2.0.0
git push origin v2.0.0
```

## 源码与本地构建

- `source/src/`：C11 测试内核、Win32/Linux 平台层、结果生成和自检。
- `source/report/template.html`：离线 HTML/JavaScript 报告。
- `source/build_builds.py`、`source/winshim/`：交叉构建脚本和 Windows ABI 接口声明。
- `scripts/package_release.py`：核对 PE32+ x64 结构、打包运行文件并生成校验清单。
- `scripts/verify_smoke.py`：跨平台核对本轮新生成的烟雾测试结果，不使用历史跑分。
- `source/tests/`、`VALIDATION.md`：最初开发版本的历史验证记录，不是后续 CI 的状态报告。
- `SOURCE_SHA256SUMS.txt`：原始文件提取时的校验记录，不是 Release 附件校验清单。

本地 Linux x86-64 构建需要 Python 3、Clang/LLVM、`lld-link` 和 POSIX threads：

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

产物位于 `source/build/`。自动构建没有改动内存测试算法或 BIOS 参数。

## 验证边界

查看对应提交的 Actions 运行结果确认该版本是否通过 CI，不能仅凭仓库存在工作流就认定测试成功。历史文档中的“Windows 尚未验证”描述的是首次打包时的状态；后续 CI 的 Windows 自检及端到端结果单独保存在对应运行中。

云主机烟雾测试只检查功能、基本兼容性和输出一致性，不是用户的 9700X 性能实测，不证明内存超频稳定，也不证明所有 Windows 环境均兼容。程序没有集成 IBS/UMC 计数器、物理地址到 DRAM Bank/Row 映射或 BIOS 自动写入；诊断提供访问行为和候选参数组，不声称测出每项 BIOS 时序的独立贡献。
