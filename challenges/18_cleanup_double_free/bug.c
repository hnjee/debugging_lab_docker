/*
 * Challenge 18 — 에러 처리(goto cleanup) 경로의 Double Free (심화: 다자원 래더)
 *
 * [시나리오]
 *   연결(Conn)을 열며 여러 자원을 순서대로 확보한다: 수신 버퍼(rx) → 송신 버퍼(tx)
 *   → 세션 상태(state). 확보 도중 실패하면 goto 라벨 사다리로 "역순 정리"한다.
 *   마지막에 핸드셰이크 검증을 수행하고, 실패하면 역시 정리 경로로 빠진다.
 *
 * [예시 상황]
 *  TCP 소켓 + TLS (HTTPS)
 *  소켓을 열고, 읽기/쓰기 버퍼를 잡고, SSL 세션을 만든 뒤 SSL_do_handshake()로 인증서를 확인. 
 *  핸드셰이크가 실패하면 소켓·SSL 객체·버퍼를 역순으로 닫음. handshake_ok가 바로 이 단계.
 *
 * [기대 동작]
 *   각 자원을 확보한 만큼만, 정확히 한 번씩 해제하고 실패 코드를 반환.
 *
 * [증상]
 *   핸드셰이크 검증 실패 분기에서 tx 버퍼를 "특별 처리"한다며 먼저 free 한 뒤
 *   `goto fail_tx` 로 점프한다. 그런데 fail_tx 라벨도 tx 를 free 한다 → 같은 포인터
 *   이중 해제 → glibc "double free detected" abort. 자원이 많고 라벨 사다리가 길어
 *   "어느 경로가 무엇을 이미 해제했는지" 추적하기 어렵다.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=18_cleanup_double_free
 *   (gdb) run                        → abort
 *   (gdb) bt                         → conn_open 의 fail_tx: free(c->tx) 지점
 *   (gdb) break conn_open            → 각 free 호출 순서를 따라가며 tx 가 두 번 해제되는지 확인
 *   (gdb) print c->tx                → 검증 실패 분기의 free 후에도 같은 주소가 fail_tx 에서 또 해제됨
 *
 * [printf(로그)로 잡기]
 *   각 free 에 라벨을 붙여 경로별 해제를 추적:
 *     fprintf(stderr, "free tx @validate tx=%p\n", (void*)c->tx); free(c->tx);
 *     fail_tx: fprintf(stderr, "free tx @fail_tx tx=%p\n", (void*)c->tx); free(c->tx);
 *   → 같은 tx 주소가 두 번 출력되면 이중 해제.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: "해제한 자원은 즉시 포인터를 NULL 로" 만들어 라벨에서 다시 해제해도 무해하게
 *       하거나(free(NULL) 안전), 특정 자원을 조기 해제하지 말고 정리 경로 한 곳에만
 *       해제 책임을 두세요.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *rx;
    char *tx;
    int  *state;
} Conn;

static int handshake_ok(const Conn *c) {
    (void)c;
    return 0;                     /* 실패 */
}

static int conn_open(Conn *c, size_t bufsz) {
    c->rx = c->tx = NULL;
    c->state = NULL;

    c->rx = malloc(bufsz);
    if (!c->rx) goto fail_rx;

    c->tx = malloc(bufsz);
    if (!c->tx) goto fail_tx;

    c->state = malloc(sizeof(int) * 4);
    if (!c->state) goto fail_state;

    strcpy(c->rx, "rx-ready");
    strcpy(c->tx, "tx-ready");
    for (int i = 0; i < 4; i++) c->state[i] = i;

    if (!handshake_ok(c)) {

        free(c->tx);              
        goto fail_tx;             
    }

    return 0;                     

fail_state:
    free(c->state);
fail_tx:
    free(c->tx);                 
fail_rx:
    free(c->rx);
    return -1;
}

int main(void) {
    Conn c;
    int rc = conn_open(&c, 32);   
    printf("conn_open rc=%d\n", rc);
    return 0;
}
