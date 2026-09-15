# 创建 GitHub 私有仓库并上传

本目录是本地源码包，尚未创建 GitHub 远程仓库或上传。

本机需先安装 Git 和 GitHub CLI (`gh`)。解压后执行 `bash publish.sh` 即可运行下方完整命令；脚本会先检查依赖并定位到自己的源码目录。在本文件所在目录执行以下命令，并在浏览器登录 `aksjfds`。
命令面向 macOS/Linux 终端或 Windows Git Bash。任何一步失败都会停止，不会覆盖或删除现有远程仓库。

```bash
(
  set -e
  test -f source/src/kernels.c
  if [ -e .git ]; then
    echo "This folder already contains .git; stopped to avoid changing an existing repository." >&2
    exit 1
  fi
  gh auth login --hostname github.com --web --git-protocol https
  test "$(gh api --hostname github.com user --jq .login)" = "aksjfds"
  gh auth setup-git --hostname github.com
  git init -b main
  git config --local user.name "aksjfds"
  git config --local user.email "63064782+aksjfds@users.noreply.github.com"
  git add .
  git -c commit.gpgsign=false commit -m "Initial import of AM5 Native Memory Lab source"
  gh repo create aksjfds/AM5-Native-Memory-Lab --private --source=. --remote=origin --push
  test "$(git rev-parse HEAD)" = "$(git ls-remote origin refs/heads/main | cut -f1)"
  echo "Created private repository and verified main was uploaded."
  gh repo view aksjfds/AM5-Native-Memory-Lab --json url --jq .url
)
```

`gh auth setup-git` 会配置 Git 使用 GitHub CLI 的凭据助手；Git 提交姓名和邮箱只写入本仓库配置，不改全局提交身份。
授权在你的本机完成，不需要把密码或 Token 发到聊天中。
仓库重名、网络或授权出错时，命令会停止；不要用强制推送或删除已有仓库来绕过错误。

官方命令文档：
- https://cli.github.com/manual/gh_repo_create
- https://cli.github.com/manual/gh_auth_login
- https://cli.github.com/manual/gh_auth_setup-git
