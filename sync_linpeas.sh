#!/bin/bash
# sync_linpeas.sh - 每半个月从官方 PEASS-ng 仓库同步最新 linpeas.sh
# 用法: ./sync_linpeas.sh [--dry-run]

set -euo pipefail

REPO="carlospolop/PEASS-ng"
TARGET_DIR="$(cd "$(dirname "$0")" && pwd)"
DRY_RUN="${1:-}"

echo "[$(date '+%Y-%m-%d %H:%M:%S')] 开始同步 linpeas 到官方最新 release ..."

# 获取最新 release tag
LATEST_TAG=$(gh release list --repo "$REPO" --limit 1 --json tagName --jq '.[0].tagName')
echo "  最新 release: $LATEST_TAG"

# 下载 linpeas.sh (从 release assets 或直接从 repo 获取)
LINPEAS_URL="https://raw.githubusercontent.com/carlospolop/PEASS-ng/$LATEST_TAG/linpeas/linpeas.sh"

if [ -n "$DRY_RUN" ]; then
    echo "  [DRY-RUN] 将下载: $LINPEAS_URL"
    echo "  [DRY-RUN] 目标文件: $TARGET_DIR/linpeas.sh"
    exit 0
fi

# 备份旧版本
if [ -f "$TARGET_DIR/linpeas.sh" ]; then
    BACKUP="$TARGET_DIR/linpeas.sh.bak.$(date '+%Y%m%d_%H%M%S')"
    cp "$TARGET_DIR/linpeas.sh" "$BACKUP"
    echo "  已备份旧版本: $BACKUP"
fi

# 下载新版本
echo "  正在下载 $LINPEAS_URL ..."
curl -sL -o "$TARGET_DIR/linpeas.sh.tmp" "$LINPEAS_URL"

# 验证下载成功（非空且以 shebang 开头）
if [ -s "$TARGET_DIR/linpeas.sh.tmp" ] && head -1 "$TARGET_DIR/linpeas.sh.tmp" | grep -q '^#!/'; then
    mv "$TARGET_DIR/linpeas.sh.tmp" "$TARGET_DIR/linpeas.sh"
    chmod +x "$TARGET_DIR/linpeas.sh"
    echo "  ✅ 同步完成: linpeas.sh 已更新为 $LATEST_TAG"
else
    rm -f "$TARGET_DIR/linpeas.sh.tmp"
    echo "  ❌ 下载失败，保留旧版本"
    exit 1
fi
