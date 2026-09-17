# gdb 로 메모리 버그를 디버깅하는 Linux 실습 환경.
# (macOS 는 gdb 지원이 불안정하므로, 이 컨테이너 안에서 gdb 실습을 권장)
#
# 사용:
#   docker build -t memdbg .
#   docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
#       -v "$PWD":/work memdbg                 # 셸 진입
#   # 컨테이너 안에서:
#   make check                    # 전 챌린지 실행 요약 (크래시 신호)
#   gdb ./build/06_null_deref     # run → bt → frame N → print 변수
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential gcc gdb make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# gdb 시작 시 debuginfod(디버그 심볼 인터넷 자동 다운로드) 질문/지연 끄기.
# (버그는 -g 로 빌드한 우리 bug.c 안에 있어 시스템 라이브러리 심볼이 필요 없음)
RUN echo 'set debuginfod enabled off' >> /root/.gdbinit

WORKDIR /work
CMD ["/bin/bash"]
