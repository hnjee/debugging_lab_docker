/*
 * Challenge 19 — realloc 로 줄인 뒤 옛 길이로 접근 (심화: 신호 버퍼 트림)
 *
 * [시나리오]
 *   센서 신호를 담는 Signal 버퍼. 앞부분의 유효 구간만 남기고 나머지를 잘라내
 *   메모리를 절약하는 signal_trim() 을 호출한 뒤, 에너지(제곱합)를 계산한다.
 *
 * [예시 상황]
 *   마이크 / 음성
 *   음성 구간 검출(VAD)이 이 패턴을 사용한다.
 *   44,100Hz로 녹음하면 1초에 samples가 44,100개다.
 *   무음인 앞부분만 남기고 뒤를 자르는 게 signal_trim이고, 소리가 얼마나 큰지(에너지)를 보려면 제곱합을 쓴다.
 *
 * [기대 동작]
 *   트림 후에는 남은 표본 개수(len)만큼만 접근/계산.
 *
 * [증상]
 *   signal_trim() 이 realloc 으로 버퍼를 "축소"하고 용량(cap)은 갱신하지만, 길이
 *   필드(len)를 갱신하지 않는다. 이후 signal_energy() 는 여전히 옛 len(수백만)으로
 *   순회하므로, 축소된(해제되어 unmap 된) 영역까지 읽어 SIGSEGV.
 *   크래시는 energy 루프의 samples[i] 에서 나지만, 원인은 "트림 시 len 미갱신".
 *
 * [gdb 로 잡기]
 *   make gdb NAME=19_realloc_shrink_overflow
 *   (gdb) run                        → 크래시(SIGSEGV)
 *   (gdb) bt                         → signal_energy 의 s->samples[i] 지점
 *   (gdb) print i ; print s->len ; print s->cap
 *        → len 이 cap 보다 훨씬 큼(트림으로 cap 만 줄었고 len 은 옛값)
 *   (gdb) break signal_trim          → 트림 후 len/cap 이 어긋나는지 확인
 *
 * [printf(로그)로 잡기]
 *   순회 인덱스와 len/cap 을 비교 출력:
 *     fprintf(stderr, "i=%zu len=%zu cap=%zu\n", i, s->len, s->cap);
 *   → i 가 cap 을 넘어서는(=축소된 버퍼 밖) 순간이 위험 지점.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: 버퍼를 축소하면 길이(len)도 함께 새 크기로 갱신하고, 이후 접근은 갱신된
 *       len 으로만 하세요. (cap 과 len 을 항상 정합적으로 유지)
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    double *samples;
    size_t  len;      
    size_t  cap;      
} Signal;

static void signal_init(Signal *s, size_t n) {
    s->samples = malloc(n * sizeof(double));
    if (!s->samples) { perror("malloc"); exit(1); }
    s->len = s->cap = n;
    for (size_t i = 0; i < n; i++) s->samples[i] = (double)(i % 7) - 3.0;
}

static void signal_trim(Signal *s, size_t keep) {
    if (keep > s->cap) return;
    double *p = realloc(s->samples, keep * sizeof(double));
    if (p) s->samples = p;
    s->cap = keep;                 
}

static double signal_energy(const Signal *s) {
    double e = 0.0;
    for (size_t i = 0; i < s->len; i++) {   
        e += s->samples[i] * s->samples[i];
    }
    return e;
}


int main(void) {
    Signal s;
    signal_init(&s, 2000000);       

    signal_trim(&s, 8);             

    double e = signal_energy(&s);   
    
    printf("energy = %.1f (len=%zu cap=%zu)\n", e, s.len, s.cap);
    free(s.samples);
    return 0;
}
