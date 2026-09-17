/*
 * Challenge 12 — free() 대상이 힙이 아님 (심화: CSV 필드 내부 포인터)
 *
 * [시나리오]
 *   한 줄짜리 CSV 를 파싱한다. 원본 줄을 힙에 복사(strdup)한 뒤 strtok 으로 쉼표를
 *   '\0' 로 바꿔가며 각 필드의 시작 주소를 Row.fields[] 에 담는다.
 *
 * [기대 동작]
 *   필드들을 출력하고, 할당한 버퍼를 누수 없이 해제.
 *
 * [증상]
 *   각 필드 포인터는 "하나의 원본 버퍼 안"을 가리키는 내부(interior) 포인터다.
 *   (fields[0] 만 버퍼의 시작이고, 나머지는 중간을 가리킨다)
 *   정리 함수가 필드마다 free() 를 호출하면, 힙 청크의 "시작"이 아닌 내부 포인터를
 *   해제하려다 glibc "free(): invalid pointer" 로 abort. (또는 시작 포인터를 먼저
 *   해제한 뒤 그 버퍼의 내부를 또 해제 → 손상)
 *
 * [gdb 로 잡기]
 *   make gdb NAME=12_free_non_heap
 *   (gdb) run                        → abort
 *   (gdb) bt                         → row_free 의 free(r->fields[i]) 지점
 *   (gdb) print r->fields[0]         → 버퍼 시작(원본 malloc 포인터)
 *   (gdb) print r->fields[i]         → fields[0] 보다 뒤(중간)를 가리키는 내부 포인터
 *   (gdb) print r->fields[i] - r->fields[0]   → 시작에서 얼마나 전진했는지(>0)
 *
 * [printf(로그)로 잡기]
 *   해제 직전, 각 필드가 버퍼 시작에서 얼마나 떨어졌는지 출력:
 *     fprintf(stderr, "free fields[%d]=%p (base=%p off=%ld)\n",
 *             i, (void*)r->fields[i], (void*)r->base,
 *             (long)(r->fields[i] - r->base));
 *   → off 가 0 이 아닌 포인터를 free 하면 그게 원인.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: strtok 이 준 필드 포인터들은 "원본 버퍼 안의 내부 포인터"일 뿐 개별 할당이
 *       아니다. malloc 이 돌려준 "원본 버퍼 하나"만 free 하세요.
 */
/* [Thinking Point]
 * _POSIX_C_SOURCE 는 "이 소스가 어느 POSIX 표준 버전까지의 API 를 쓰겠다"고 미리
 * 선언하는 '기능 시험 매크로(feature test macro)'다. 값 200809L 은 POSIX.1-2008 을 뜻한다.
 *   tip 1. 반드시 <string.h> 등 헤더를 include 하기 '전에' 정의해야 효력이 있다.
 *          (헤더가 이 값을 보고 어떤 함수 선언을 노출할지 결정하기 때문)
 *   tip 2. strdup 은 C 표준(C11)에는 없고 POSIX 에 있는 함수다. -std=c11 로 엄격히
 *          컴파일하면 이 매크로가 없을 경우 strdup 선언이 감춰져 '암시적 선언' 경고가 나고,
 *          반환값이 int 로 잘못 취급돼 포인터가 깨지는 별도 버그로 이어질 수 있다.
 *   생각해보기 1 (POSIX 란?): POSIX 는 유닉스 계열 OS 가 공통으로 제공하기로 약속한
 *               '운영체제 인터페이스 표준'이다(파일·프로세스·스레드·문자열 등의 API 규격).
 *               리눅스·macOS 등이 이를 따르므로, POSIX 함수를 쓰면 여러 OS 에서 같은
 *               코드가 동작한다. 그런데 왜 C 표준(C11)과 POSIX 를 굳이 구분할까?
 *   생각해보기 2 (버전 관리 관점): 왜 "쓸 수 있는 표준 버전"을 코드가 스스로 선언하게 할까?
 *               (숫자 200809L = 표준의 '연-월' 버전. 값이 클수록 더 최신 표준) */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 8
typedef struct {
    char *base;                  /* 원본(=malloc 이 돌려준) 버퍼 */
    char *fields[MAX_FIELDS];    /* 각 필드 시작(대개 base 내부를 가리킴) */
    int   n;
} Row;

static void parse_row(Row *r, const char *csv) {
    r->base = strdup(csv);       
    if (!r->base) { perror("strdup"); exit(1); }
    r->n = 0;

    for (char *tok = strtok(r->base, ","); tok && r->n < MAX_FIELDS;
         tok = strtok(NULL, ",")) {
        r->fields[r->n++] = tok;  /* fields[0]=base, 나머지는 내부 포인터 */
    }
}

static void row_print(const Row *r) {
    printf("%d fields:", r->n);
    for (int i = 0; i < r->n; i++) printf(" [%s]", r->fields[i]);
    printf("\n");
}

static void row_free(Row *r) {
    for (int i = 0; i < r->n; i++) {
        free(r->fields[i]);       
    }
    r->n = 0;
}

int main(void) {
    Row r;
    parse_row(&r, "id,name,dept,salary");
    row_print(&r);

    row_free(&r);                 
    printf("done\n");
    return 0;
}
