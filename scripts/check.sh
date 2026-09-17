#!/usr/bin/env bash
# 모든 챌린지(버그 코드)를 실행하고, 크래시(SIGSEGV/SIGABRT/SIGBUS) 했는지 요약한다.
# 버그 코드는 "크래시" 하는 것이 정상이며, 그 지점을 gdb 로 추적하는 것이 학습 목표다.
# 옵션: 해설 코드까지 확인하려면  bash scripts/check.sh --with-solutions
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"

if [ ! -d "$BUILD" ]; then
  echo "build/ 가 없습니다. 먼저 'make all' 을 실행하세요." >&2
  exit 1
fi

sig_name() {   # 종료코드 → 신호 이름
  local code="$1"
  if [ "$code" -gt 128 ]; then
    printf 'SIG%s' "$(kill -l $((code - 128)) 2>/dev/null)"
  else
    printf 'exit'
  fi
}

printf "%-30s %-8s %s\n" "CHALLENGE" "EXIT" "결과 (버그는 크래시가 정상)"
printf '%.0s-' {1..72}; echo

crashed=0; total=0
for bin in "$BUILD"/*; do
  [ -f "$bin" ] && [ -x "$bin" ] || continue
  case "$(basename "$bin")" in sol_*) continue;; esac
  name="$(basename "$bin")"
  total=$((total + 1))
  "$bin" >/dev/null 2>&1
  code=$?
  sig="$(sig_name "$code")"
  if [ "$code" -gt 128 ]; then
    verdict="크래시 ($sig) — gdb 로 추적 대상"
    crashed=$((crashed + 1))
  else
    verdict="정상 종료 — 크래시 안 함(확인 필요)"
  fi
  printf "%-30s %-8s %s\n" "$name" "$code" "$verdict"
done

printf '%.0s-' {1..72}; echo
echo "크래시: $crashed / $total  (버그 코드는 전부 크래시해야 함)"

if [ "${1:-}" = "--with-solutions" ]; then
  echo
  printf "%-30s %-8s %s\n" "SOLUTION" "EXIT" "결과 (해설은 정상 종료가 정상)"
  printf '%.0s-' {1..72}; echo
  for bin in "$BUILD"/sol_*; do
    [ -f "$bin" ] && [ -x "$bin" ] || continue
    name="$(basename "$bin")"
    "$bin" >/dev/null 2>&1
    code=$?
    if [ "$code" -eq 0 ]; then verdict="정상 종료 (OK)"; else verdict="비정상 종료 ($(sig_name "$code"))"; fi
    printf "%-30s %-8s %s\n" "$name" "$code" "$verdict"
  done
fi
