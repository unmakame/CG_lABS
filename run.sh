#!/bin/zsh
set -e
CG_DIR="$(cd "$(dirname "$0")" && pwd)"
case "${1:-1}" in
  1|2|3) exec zsh "$CG_DIR/lab${1:-1}/run.sh" ;;
  *) echo "Usage: zsh run.sh 1|2|3"; exit 1 ;;
esac
