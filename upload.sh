#!/bin/bash
# VocalChain 一键上传脚本
# 使用方法: ./upload.sh <GitHub用户名> [Personal Access Token]

set -e

GITHUB_USER="${1:-vocalchain-dev}"
TOKEN="${2:-}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== VocalChain 上传脚本 ===${NC}"
echo ""

# 检查是否在正确的目录
if [ ! -d "VC-EQ" ] || [ ! -d "VC-Comp" ]; then
    echo -e "${RED}错误: 请在 VocalChain 目录中运行此脚本${NC}"
    exit 1
fi

# 初始化 Git（如果尚未初始化）
if [ ! -d ".git" ]; then
    echo -e "${YELLOW}初始化 Git 仓库...${NC}"
    git init
    git config user.email "vocalchain@example.com"
    git config user.name "$GITHUB_USER"
fi

# 添加所有文件
echo -e "${YELLOW}添加文件到 Git...${NC}"
git add .

# 提交
echo -e "${YELLOW}提交代码...${NC}"
git commit -m "feat: Initial commit - VC-EQ and VC-Comp VST3 plugins with CI/CD

- Add VC-EQ parametric equalizer plugin
- Add VC-Comp compressor plugin  
- Configure GitHub Actions for cross-platform build
- Support macOS (VST3+AU), Windows (VST3), Linux (VST3)"

# 设置远程仓库
REMOTE_URL="https://github.com/$GITHUB_USER/VocalChain.git"

if [ -n "$TOKEN" ]; then
    REMOTE_URL="https://$TOKEN@github.com/$GITHUB_USER/VocalChain.git"
    echo -e "${GREEN}使用 Token 认证${NC}"
fi

git remote add origin "$REMOTE_URL" 2>/dev/null || git remote set-url origin "$REMOTE_URL"

# 推送
echo -e "${YELLOW}推送到 GitHub...${NC}"
git push -u origin main --force

echo ""
echo -e "${GREEN}=== 完成！ ===${NC}"
echo ""
echo "下一步："
echo "1. 访问 https://github.com/$GITHUB_USER/VocalChain"
echo "2. 如果提示创建仓库，选择 'Create repository'"
echo "3. 等待 GitHub Actions 自动构建"
echo "4. 在 Actions 页面下载构建产物"
