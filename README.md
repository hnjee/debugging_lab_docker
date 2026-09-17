# Debugging Lab

C 언어 **메모리 버그**를 `gdb` 및 로그로 **크래시 지점 역추적 → 원인 분석 → 수정**하는 실습 세트입니다.
난이도별로 **20개**의 독립 실행 코드가 있으며, 각 코드에는 정확히 하나의 대표 메모리 버그가 심어져 있습니다.

> 이 세트의 모든 버그 코드는 **실행하면 반드시 크래시(SIGSEGV / SIGABRT / SIGBUS)** 하도록 만들어져 있습니다.

## 구성

```
memory-debugging-lab/
├── README.md
├── Makefile                     # 빌드/실행/디버깅 도우미 (gdb 전용)
├── Dockerfile                   # gcc/gdb 리눅스 실습 환경
├── .devcontainer/               # VS Code Dev Containers 설정
├── .vscode/                     # launch.json / tasks.json (F5 디버깅)
├── scripts/check.sh             # 전 챌린지 실행 → 크래시 여부 요약
├── challenges/                  # 버그가 있는 코드 (여기서 원인 분석)
│   ├── 01_use_after_free/bug.c
│   ├── ...
│   └── 20_vector_stale_pointer/bug.c
```

각 `bug.c` 상단 주석에 **시나리오 / 기대 동작 / 증상 / gdb 로 잡는 법 / printf 로 잡는 법**이 적혀 있습니다.

본인이 실력에 자신이 있다면, 상단 주석을 참고하지 않고 직접 문제를 수정해보는것을 추천합니다.

> **참고:** `solutions/` 는 `.gitignore` 로 제외되어 **수강생 배포본에는 포함되지 않습니다**(코치가 로컬에서만 관리). 따라서 `make solutions`, `make check-all` 등 해설 관련 명령은 코치 환경에서만 동작합니다.

## 실행 환경 안내

- **Linux, macOS** — 도커/Dev Containers 로 실습합니다.
- **Windows** — 반드시 **Docker 백엔드가 WSL2 인지 확인**하세요. (Docker Desktop → Settings → General → *"Use the WSL 2 based engine"* 체크) WSL2(리눅스 커널) 위에서 Ubuntu+glibc 컨테이너가 돌아야 크래시 재현이 리눅스와 동일합니다. Hyper-V 백엔드도 리눅스 VM이라 동작하지만, "Windows 컨테이너" 모드로 실행하면 안 됩니다.
- 크래시 유형(스택 스매싱, glibc double-free/invalid-pointer 감지 등)은 **리눅스 glibc 기준**입니다.
같은 코드라도 다른 libc/OS 에서는 크래시 신호가 달라질 수 있어, 검증은 도커(Ubuntu)에서 하세요.
- **검증 환경(컨테이너 기준):** Ubuntu 24.04 LTS · gcc 13 · glibc 2.39 · gdb 15. (호스트 OS/배포판은 무엇이든 무관 — 컨테이너 안에서 이 툴체인으로 실행됩니다.)

### 도커로 실습 (gcc + gdb)

```bash
docker build -t memdbg .
docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
    -v "$PWD":/work memdbg
# ── 컨테이너 안에서 ──
make check                                  # 20개 실행 → 크래시 여부 요약
gdb ./build/06_null_deref                   # run → bt → frame N → print 변수
```

> `--cap-add=SYS_PTRACE`, `--security-opt seccomp=unconfined` 는 컨테이너 안에서 gdb 가
> 프로세스에 붙을 수 있게 하는 옵션입니다.

## VS Code 연동 (Mac / Windows) — Dev Containers

Docker 컨테이너(리눅스 툴체인)에 VS Code 를 붙여, 맥/윈도우에서도 `gdb` 를 GUI 로 쓸 수 있습니다.
이 저장소에는 `.devcontainer/devcontainer.json`, `.vscode/launch.json`, `.vscode/tasks.json` 가 포함되어 있습니다.

### 사전 준비 (공통)

1. **Docker Desktop** 설치 후 실행
  - **Windows 는 반드시 WSL2 백엔드 사용** — Docker Desktop → Settings → General → *"Use the WSL 2 based engine"* 이 켜져 있는지 확인하세요. (이 실습은 리눅스 컨테이너 전용이며, WSL2 위에서 실행되어야 크래시 재현이 리눅스와 동일합니다.)
2. **VS Code** + 확장 **"Dev Containers"** (`ms-vscode-remote.remote-containers`)

### 방법 A — Reopen in Container (권장, 원클릭)

1. VS Code 로 **이 프로젝트 폴더** 열기
2. 우하단 **"Reopen in Container"** 클릭 (없으면 `F1` → **Dev Containers: Reopen in Container**)
3. 첫 실행 시 이미지 빌드(1~3분) 후, VS Code 가 **컨테이너 안 리눅스**에 연결됨
4. 통합 터미널에서 바로 실습:

```bash
make check                                  # 20개 크래시 여부 요약
gdb ./build/06_null_deref                   # run → bt
```

1. **F5** 로 GUI 디버깅: 챌린지를 고르면 gdb 로 브레이크포인트·백트레이스·변수 확인 가능

### 방법 B — 실행 중 컨테이너에 붙기 (Attach)

```bash
cd <이 프로젝트 폴더>
docker build -t memdbg .
docker run -d --name memdbg-dev --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined -v "$PWD":/work -w /work memdbg sleep infinity
```

- VS Code: `F1` → **Dev Containers: Attach to Running Container** → `memdbg-dev` → 폴더 `/work` 열기
- Windows PowerShell 은 `"$PWD"` 를 `${PWD}` 로 바꾸세요.

### 알아둘 점

- 폴더는 컨테이너 `/work` 에 **양방향 마운트**됩니다(맥/윈도우에서 편집 → 컨테이너 즉시 반영).
- `build/` 에는 **리눅스 바이너리**가 생기므로, 호스트에서 직접 `make` 하지 말고 **컨테이너 터미널에서** 빌드/실행하세요.
- `runArgs` 의 `--cap-add=SYS_PTRACE`, `--security-opt seccomp=unconfined` 는 컨테이너 안에서 **gdb 디버깅**이 정상 동작하도록 넣은 필수 옵션입니다.
- 애플 실리콘 맥은 arm64 리눅스 컨테이너로 뜨며, 그 환경에서도 gcc/gdb 모두 정상 동작합니다.

## 빌드/실행 요약

```bash
make list                     # 챌린지 목록
make all                      # 모든 bug.c 를 위생도구 없이 빌드 → build/<이름>
make run  NAME=01_use_after_free     # 해당 챌린지 실행 (버그면 크래시)
make gdb  NAME=06_null_deref         # gdb 디버깅 (run → bt)
make check                    # 전 챌린지 실행 → 크래시 여부 요약
make clean                    # build/ 정리

# ── 아래는 코치·로컬 전용 (solutions/ 가 있을 때만 동작) ──
make solutions                # 해설 코드 빌드 → build/sol_<이름>
make check-all                # 챌린지(크래시) + 해설(정상 종료 0) 함께 검증
```

> 빌드는 `-g -O0 -fno-omit-frame-pointer` 로 하므로 gdb 백트레이스가 소스 라인/변수까지 정확히 나옵니다.

## 챌린지 목록 (20개)


| #   | 이름                      | 버그 유형                              | gdb 로 보이는 크래시            |
| --- | ----------------------- | ---------------------------------- | ------------------------ |
| 01  | use_after_free          | vtable 위젯: 해제 후 슬롯 미정리 → 함수 포인터 호출 | SIGSEGV                  |
| 02  | stack_buffer_overflow   | 삼각 인덱싱 off-by-one 으로 스택 배열 초과      | SIGABRT (stack smashing) |
| 03  | heap_buffer_overflow    | 동적 배열 성장 버그(용량·실제버퍼 불일치)           | SIGABRT (realloc 감지)     |
| 04  | double_free             | 별칭(aliasing) 인덱스 두 곳에서 같은 객체 해제    | SIGABRT (double free)    |
| 05  | null_return_deref       | 설정 템플릿 확장 중 누락 키의 NULL 반환 역참조      | SIGSEGV                  |
| 06  | null_deref              | 헤더 파서: ':' 없는 줄 → strchr NULL 에 쓰기 | SIGSEGV                  |
| 07  | stack_use_after_return  | 지역 배열 주소가 뷰로 탈출 → 프레임 재사용 후 역참조    | SIGSEGV                  |
| 08  | uninitialized_read      | 더티 힙 재사용으로 미초기화 행 포인터 역참조          | SIGSEGV                  |
| 09  | strcpy_overflow         | join 크기계산 off-by-one(마지막 조각 누락)    | SIGSEGV                  |
| 10  | realloc_dangling        | undo 스냅샷이 realloc 이동으로 댕글링 → 이중 해제 | SIGABRT (double free)    |
| 11  | global_overflow         | 전역 아레나 bump 할당기 경계 미검사             | SIGSEGV                  |
| 12  | free_non_heap           | CSV 필드(내부 포인터)를 개별 free            | SIGABRT (invalid ptr)    |
| 13  | linked_list_uaf         | 잡 큐 필터: free 후 next 읽기(UAF)        | SIGSEGV                  |
| 14  | integer_overflow_alloc  | 이미지 w*h*ch int 곱 오버플로 → 과소할당       | SIGSEGV                  |
| 15  | dangling_in_struct      | 세션이 해제된 User 의 콜백 호출               | SIGBUS/SIGSEGV           |
| 16  | unused_cap_overflow     | cap 인자를 안 쓰는 append 오버플로           | SIGABRT (stack smashing) |
| 17  | ownership_uaf           | 메시지 브로커: 소비자 해제 + 감사 로그 재해제(UAF)   | SIGSEGV                  |
| 18  | cleanup_double_free     | 다자원 goto 래더: 검증 실패 경로 tx 이중 해제     | SIGABRT (double free)    |
| 19  | realloc_shrink_overflow | 신호 버퍼 트림 후 옛 len 으로 순회             | SIGSEGV                  |
| 20  | vector_stale_pointer    | 히스토그램 hot 포인터가 성장으로 stale          | SIGSEGV                  |


> 검증: 리눅스(도커)에서 `make check` 시 20개 전부 크래시합니다.

## gdb 치트시트

```bash
gdb ./build/06_null_deref        # 디버거 시작
(gdb) run                        # 실행 → 버그 코드는 여기서 크래시
(gdb) bt                         # 백트레이스: 어느 함수/라인에서 죽었는지
(gdb) frame 1                    # 특정 스택 프레임으로 이동
(gdb) print 변수                 # 변수/포인터 값 확인 (예: print p, print i)
(gdb) info locals                # 현재 프레임의 지역 변수 전부
(gdb) list                       # 크래시 지점 주변 소스 보기
```

메모리 버그 추적에 유용한 명령:

```bash
(gdb) break 파일:라인            # 특정 라인에 브레이크포인트
(gdb) watch 변수                 # 값이 바뀌는 순간 멈춤
(gdb) x/8xg 포인터               # 포인터가 가리키는 메모리를 8워드 헥사로 덤프
(gdb) p (long)포인터 - (long)기준 # 두 포인터의 오프셋(경계 초과 판단)
```

## 두 가지 접근: gdb vs printf(로그)

각 `bug.c` 상단 주석에는 `[gdb 로 잡기]` 와 `[printf(로그)로 잡기]` 두 방법이 함께 적혀 있습니다. 둘 다 해보며 비교하는 것을 권장합니다.


|     | gdb                                         | printf(로그)                        |
| --- | ------------------------------------------- | --------------------------------- |
| 방식  | 크래시 후 `run → bt` 로 지점 역추적                   | 코드에 로그를 심어 값의 변화를 추적              |
| 장점  | 재컴파일/코드수정 없이 즉시, 변수·메모리 조회 강력               | 흐름 전체를 시간순으로 관찰, 조건부 로깅 쉬움        |
| 주의  | 크래시가 libc 안이면 `frame`/`up` 으로 내 코드까지 올라가야 함 | **stdout 은 버퍼링**되어 크래시 시 유실될 수 있음 |


> printf 로 잡을 때 핵심: 크래시 직전 로그가 사라지지 않게 `stderr` 로 찍거나(`fprintf(stderr, ...)`),
> `stdout` 을 쓸 거면 각 출력 뒤 `fflush(stdout)` 하거나 `setvbuf(stdout, NULL, _IONBF, 0)` 로 버퍼링을 끈다.

## 학습 흐름 (권장)

1. `challenges/<이름>/bug.c` 를 읽고 **증상**과 **어디서 죽을지**를 예상한다.
2. `make gdb NAME=<이름>` → `run` 으로 크래시를 내고, `bt` 로 **크래시 지점**을 찾는다.
3. `print` / `info locals` / `x` 로 **포인터·인덱스·크기**를 확인해 원인을 특정한다.
4. (또는/추가로) 주석의 `[printf(로그)로 잡기]` 대로 `fprintf(stderr, ...)` 로그를 심어, 값이 어디서 어긋나는지 시간순으로 관찰한다.
5. 원인을 제거하도록 직접 수정한 뒤, `make run NAME=<이름>` 으로 **크래시가 사라지고 정상 종료(0)** 하는지 확인한다. (표준 해설은 강사가 제공)

## 안내

> **개발 환경 관련한 질문은 받지 않습니다.** 해당 코드와 내용을 바탕으로 본인의 환경에 맞게 세팅하세요.

