#!/usr/bin/env bash
# Run from any location; publish only this extracted source directory.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
for tool in git gh; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'Missing required command: %s. Install Git and GitHub CLI first.
' "$tool" >&2
    exit 1
  fi
done
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
