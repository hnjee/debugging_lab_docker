/*
 * Challenge 03 — Heap Buffer Overflow (심화: 동적 배열 성장 버그)
 *
 * [시나리오]
 *   자동 성장하는 정수 동적 배열 IntList (init/ensure/push/sum). 용량이 부족하면
 *   list_ensure() 가 용량을 2배로 늘리고 realloc 한다. 이 리스트로 큰 수열을
 *   만들어 합을 구한다.
 *
 * [기대 동작]
 *   0..N-1 을 100 으로 나눈 나머지를 리스트에 넣고, 길이·용량·합을 출력한 뒤 정상 종료.
 *
 * [증상]
 *   list_ensure() 가 새 용량(newcap)을 계산해 l->cap 에는 반영하지만,
 *   정작 realloc 은 "옛 용량(l->cap)" 으로 호출한다. 즉 논리 용량(cap)은 커지는데
 *   실제 버퍼는 한 세대 뒤처져, push 가 실제 버퍼 밖으로 계속 쓴다.
 *   힙 경계를 넘어 쓰면서 힙 메타데이터가 깨지거나(→ 이후 realloc/free 에서 SIGABRT)
 *   매핑되지 않은 페이지까지 밀고 나가 SIGSEGV. 크래시는 push 의 대입 지점 또는
 *   다음 realloc 에서 나지만, 원인은 ensure 의 realloc 인자다.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=03_heap_buffer_overflow
 *   (gdb) run                         → 크래시(SIGSEGV) 또는 abort
 *   (gdb) bt                          → list_push 의 l->data[l->len]=x 또는 realloc 내부
 *   (gdb) frame N ; print *l           → cap 은 큰데 실제 버퍼는 그보다 작음(불일치)
 *   (gdb) print l->len  / print l->cap → len 이 실제 확보량을 넘어섰는지 확인
 *   (gdb) break list_ensure           → newcap 과 realloc 에 넘기는 크기를 대조
 *
 * [printf(로그)로 잡기]
 *   ensure 에서 (old cap, newcap, realloc 에 넘기는 크기) 를 함께 찍어 불일치를 본다:
 *     fprintf(stderr, "ensure old=%zu new=%zu realloc_bytes=%zu\n",
 *             l->cap, newcap, l->cap * sizeof(int));
 *   → newcap 과 realloc 크기가 다르면 그게 원인.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: realloc 은 반드시 "새 용량(newcap)" 으로 호출하고, l->cap 갱신과 순서를 맞춰야 한다.
 *       (성장 로직은 '용량 필드'와 '실제 확보량'이 항상 같도록 유지해야 한다)
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int   *data;
    /* [Thinking Point]
     * 개수/크기를 담는 len, cap 을 왜 int 가 아니라 size_t 로 선언할까?
     *   tip 1. size_t 는 "이 플랫폼에서 표현 가능한 가장 큰 객체 크기"를 담도록 만든
     *          부호 없는(unsigned) 정수 타입이다. malloc/sizeof/strlen 의 타입도 size_t 다.
     *   tip 2. int 는 보통 32비트라 약 21억(2^31-1)에서 넘치고, 음수도 가능하다.
     *          원소가 그보다 많아지거나 cap*sizeof(int) 계산이 커지면 int 는 오버플로된다.
     *   생각해보기: 크기를 int 로 두면 어떤 버그가 생길 수 있을까? 
     *  -> 정수 오버플로우가 나서 음수로 뒤집히는 경우 음수 인덱스로 배열에 접근하여 메모리 오염이 될 수 있음 
     * 
     *
     * size_t는 부호가 없는 정수이므로, 
     * 개념적으로 절대 음수가 될 수 없는 값, 특히 메모리 크기나 개수를 세는 값에 관용적으로 쓰는 타입이다. 
     */
    size_t len;
    size_t cap;
} IntList;

static void list_init(IntList *l) {
    l->cap  = 8;
    l->len  = 0;
    l->data = malloc(l->cap * sizeof(int));
    if (!l->data) { perror("malloc"); exit(1); }
}

static void list_ensure(IntList *l, size_t need) {
/* list_ensure() : 적어도 need개는 들어갈 수 있는 공간이 있는지 확인하고 부족하면 그때 알아서 늘려주는 함수
need는 호출하는 쪽이 요청하는 최소한의 크기이고, 실제로 용량을 맞추는건 함수 내부의 로직에서 결정 
현재 이 함수는 현재 cap이 0이면 8으로 늘리고, 아니면 최소 2배로, 
만약 need가 현재 cap의 2배 이상으로 필요하면 need를 넘어설때까지 newcap을 2배로 곱하는 내부 전략을 가지고 있음  
*/

    if (need <= l->cap) return;
    /*
    지금 코드에서 list_ensure를 호출하는 곳은 list_push 딱 한 군데뿐이고, 거기서는 항상 l->cap + 1을 넘기니까 need가 l->cap보다 항상 큼
    하지만 list_ensure는 범용 함수로 설계되어 있다.
    즉, "지금 당장은 이렇게만 쓰이지만, 나중에 다른 곳에서 list_ensure(l, 1000)처럼 특정 개수를 미리 확보해두고 싶을 때도 쓸 수 있게" 만들어둔 것.
    */
    size_t newcap = l->cap ? l->cap * 2 : 8;
    /*
    "cap이 0인 상태에서 시작하면 2배 늘리기 전략 자체가 영원히 작동을 안 한다"는 특이 케이스를 막기 위한 방어 코드
    지금 이 프로그램에서는 list_init이 항상 cap = 8로 시작하니까 실제로 이 0 케이스가 발생하진 않지만, 
    역시 "범용 함수"로서 안전하게 만들어둔 것 
    */
    while (newcap < need) newcap *= 2;
    printf("======%d\n", need);
    int *p = realloc(l->data, l->cap * sizeof(int));

    if (!p) { perror("realloc"); free(l->data); exit(1); }

    l->data = p; //이걸 해줘야 새로 할당한 메모리 공간을 제대로 가리키게 됨 
    l->cap  = newcap;
}

static void list_push(IntList *l, int x) {
    if (l->len == l->cap) list_ensure(l, l->cap + 1);
    l->data[l->len++] = x;
}

static long long list_sum(const IntList *l) {
    long long s = 0;
    for (size_t i = 0; i < l->len; i++) s += l->data[i];
    return s;
}

static void list_free(IntList *l) {
    free(l->data);
    l->data = NULL;
    l->len = l->cap = 0;
}

int main(void) {
    IntList l;
    list_init(&l);

    const int N = 2000000;
    for (int i = 0; i < N; i++) {
        list_push(&l, i % 100);  // 0~99 
    }

    printf("len=%zu cap=%zu sum=%lld\n", l.len, l.cap, list_sum(&l));
    list_free(&l);
    return 0;
}
