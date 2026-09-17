/*
 * Challenge 17 — 소유권 혼동 UAF (심화: 메시지 브로커)
 *
 * [시나리오]
 *   간단한 발행/구독 브로커. 발행된 메시지(Msg: 힙에 복사된 body)를 인박스 큐에 넣고,
 *   deliver() 가 하나씩 꺼내 구독자 콜백에 넘긴다. 구독자는 메시지를 처리하고 나서
 *   "소비했으니" free 한다. 브로커는 감사(audit)를 위해 발행 시점에 같은 Msg 포인터를
 *   log[] 에도 담아둔다.
 *
 * [기대 동작]
 *   모든 메시지를 배달·소비하고, 브로커를 종료하며 누수 없이 정리한 뒤 정상 종료.
 *
 * [예시 상황]
 *   채팅 서버
 *   hello를 발행하면, 수신함(inbox)에 넣어서 상대 앱에 푸시하고, 동시에 대화 이력(log)에도 같은 메시지를 남깁니다.
 * 
 * [증상]
 *   구독자가 배달받은 Msg 를 free 하는데, 브로커의 log[] 는 "같은 포인터"를 여전히
 *   들고 있다(소유권이 두 곳에 걸침). broker_shutdown() 이 log[] 를 순회하며 이미
 *   소비자가 해제한 Msg 를 다시 정리한다: msg_free() 가 해제된 구조체를 재차 읽어
 *   (m->body) 그 값을 free → use-after-free/이중 해제. 해제된 청크는 할당자가 덮어써
 *   m->body 가 엉뚱한 주소가 되므로 대개 SIGSEGV(glibc 가 감지하면 double free abort).
 *   발행/배달/감사가 서로 다른 함수에 흩어져 있어 "누가 소유자인지" 헷갈리는 것이 함정.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=17_ownership_uaf
 *   (gdb) run                        → 크래시(SIGSEGV, 또는 abort)
 *   (gdb) bt                         → broker_shutdown → msg_free(b->log[i]) 지점
 *   (gdb) print b->log[i]            → 이 주소가 앞서 구독자가 free 한 것과 같은지 확인
 *   (gdb) break on_message           → 구독자가 free 하는 주소를 기록해 두고 대조
 *
 * [printf(로그)로 잡기]
 *   "누가 어떤 주소를 free 하는지"를 추적한다:
 *     (구독자)   fprintf(stderr, "consume free msg=%p\n", (void*)m);
 *     (shutdown) fprintf(stderr, "audit   free log[%d]=%p\n", i, (void*)b->log[i]);
 *   → 같은 주소가 두 곳에서 free 되면 이중 해제.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: 소유권은 한 곳만 갖는다. log[] 는 "감사용 참조"일 뿐이므로 free 하지 않거나,
 *       배달 시 로그 슬롯을 무효화(넘긴 소유권을 추적)하세요. 소비자가 소유하면
 *       브로커는 절대 그 Msg 를 해제하지 않는다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int   id;
    char *body;      /* 힙 문자열 */
} Msg;

#define QCAP 16
typedef struct {
    Msg *inbox[QCAP];   int head, tail;      /* 원형 큐 */
    Msg *log[QCAP];     int log_n;            /* 감사용: 같은 Msg 포인터를 보관 */
} Broker;

typedef void (*Subscriber)(Msg *m);

static Msg *msg_new(int id, const char *body) {
    Msg *m = malloc(sizeof *m);
    if (!m) exit(1);
    m->id = id;
    m->body = malloc(strlen(body) + 1);
    if (!m->body) exit(1);
    strcpy(m->body, body);
    return m;
}

static void msg_free(Msg *m) {
    free(m->body);
    free(m);
}

static void publish(Broker *b, int id, const char *body) {
    Msg *m = msg_new(id, body);
    b->inbox[b->tail] = m;
    b->tail = (b->tail + 1) % QCAP;
    b->log[b->log_n++] = m;              
}

static void deliver(Broker *b, Subscriber sub) {
    while (b->head != b->tail) {
        Msg *m = b->inbox[b->head];
        b->head = (b->head + 1) % QCAP;
        sub(m);                          
    }
}

static void on_message(Msg *m) {
    printf("recv #%d: %s\n", m->id, m->body);
    msg_free(m);                         
}

static void broker_shutdown(Broker *b) {
    for (int i = 0; i < b->log_n; i++) {
        msg_free(b->log[i]);             
    }
    b->log_n = 0;
}

int main(void) {
    Broker b = { .head = 0, .tail = 0, .log_n = 0 };

    publish(&b, 1, "hello");
    publish(&b, 2, "world");
    publish(&b, 3, "broker");

    deliver(&b, on_message);             

    broker_shutdown(&b);                 
    printf("done\n");
    return 0;
}
