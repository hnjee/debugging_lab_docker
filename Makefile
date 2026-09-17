# Memory Debug Challenge — 빌드/실행 도우미 (gdb 전용)
#
# 사용법:
#   make list            # 챌린지 목록
#   make all             # 모든 버그 코드를 위생도구 없이 빌드 (build/<name>)
#   make solutions       # 해설(수정) 코드 빌드 (build/sol_<name>)
#   make run NAME=01_use_after_free      # 해당 챌린지 실행 (버그면 크래시함)
#   make gdb NAME=01_use_after_free      # gdb 로 실행 (run → bt 로 크래시 지점 확인)
#   make check           # 전 챌린지 실행 결과 요약 (scripts/check.sh)
#   make check-all       # 챌린지 + 해설까지 함께 검증 (해설은 정상 종료 확인)
#   make clean

CC      ?= gcc
CSTD    ?= -std=c11
WARN    := -Wall -Wextra -Wno-unused-parameter
# -g: 디버그 심볼, -O0: 최적화 끔, 프레임포인터 유지 → gdb 백트레이스가 정확해짐
DBG     := -g -O0 -fno-omit-frame-pointer

BUILD   := build
BUG_SRCS := $(sort $(wildcard challenges/*/bug.c))
NAMES    := $(patsubst challenges/%/bug.c,%,$(BUG_SRCS))
PLAIN_BINS := $(addprefix $(BUILD)/,$(NAMES))

SOL_SRCS := $(sort $(wildcard solutions/*/fix.c))
SOL_NAMES := $(patsubst solutions/%/fix.c,%,$(SOL_SRCS))
SOL_BINS := $(addprefix $(BUILD)/sol_,$(SOL_NAMES))

.PHONY: all solutions list run gdb check check-all clean help
.DEFAULT_GOAL := help

all: $(PLAIN_BINS)         ## 모든 버그 코드 빌드
solutions: $(SOL_BINS)     ## 해설 코드 빌드

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%: challenges/%/bug.c | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(DBG) $< -o $@

$(BUILD)/sol_%: solutions/%/fix.c | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(DBG) $< -o $@

list:
	@echo "사용 가능한 챌린지:"; \
	for n in $(NAMES); do echo "  - $$n"; done

# make run NAME=01_use_after_free
run: $(BUILD)/$(NAME)
	@echo "==> ./$(BUILD)/$(NAME) 실행 (버그 코드는 크래시하는 것이 정상)"
	@./$(BUILD)/$(NAME) || echo "   (종료 코드 $$? — 크래시 발생. gdb 로 원인 추적하세요)"

# make gdb NAME=01_use_after_free
gdb: $(BUILD)/$(NAME)
	gdb ./$(BUILD)/$(NAME)

check: all
	@bash scripts/check.sh

# 챌린지(크래시) + 해설(정상 종료)까지 한 번에 검증.
# 참고: `make check --with-solutions` 는 make 가 --with-solutions 를 make 옵션으로
# 해석해 실패한다. 대신 이 타깃을 사용하세요.
check-all: all solutions
	@bash scripts/check.sh --with-solutions

clean:
	rm -rf $(BUILD)

help:
	@echo "Memory Debug Challenge (gdb 전용)"; \
	echo "  make list                 챌린지 목록"; \
	echo "  make all | solutions      빌드"; \
	echo "  make run NAME=<이름>       실행 (버그면 크래시)"; \
	echo "  make gdb NAME=<이름>       gdb 디버깅 (run → bt)"; \
	echo "  make check                전체 실행 요약 (크래시 신호)"; \
	echo "  make check-all            챌린지 + 해설(정상 종료)까지 검증"; \
	echo "  make clean"
