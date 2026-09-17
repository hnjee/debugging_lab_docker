/*
 * Challenge 16 — 스택 버퍼 오버플로 (심화: 용량 인자를 무시하는 append)
 *
 * [시나리오]
 *   여러 필드를 구분자로 이어 붙여 한 줄의 레코드를 스택 버퍼에 만든다.
 *   append_field() 는 대상 버퍼와 그 용량(cap)을 받아 필드를 덧붙이는 헬퍼처럼 보인다.
 *
 * [기대 동작]
 *   cap 을 지켜 필드를 이어 붙이고 정상 종료. 전체 레코드는 NUL 포함 61바이트라
 *   rec[24] 에 들어가지 않는다. 넘치면 잘라 담거나(truncate) 오류로 처리하고,
 *   전체 문자열을 출력하려면 버퍼를 키운다.
 *
 * [증상]
 *   append_field() 는 cap 을 받지만 쓰지 않는다((void)cap). 세 번째 필드
 *   ("department=Engineering") 에서 이미 rec[24] 를 크게 넘긴다.
 *   1바이트만 넘는 off-by-one 이 아니라, 경계 검사를 생략해서 나는 큰 오버플로.
 *   main 반환 시 스택 카나리 검사 실패로 "stack smashing detected" → SIGABRT
 *   (경우에 따라 SIGSEGV). "cap 을 받으니 안전하겠지"라는 착각이 함정.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=16_unused_cap_overflow
 *   (gdb) run                        → abort
 *   (gdb) bt                         → __stack_chk_fail / main 반환 근처
 *   (gdb) break append_field ; run    → *len 이 cap 을 넘어서도 계속 쓰는지 관찰
 *   (gdb) print *len ; print cap      → *len 이 cap(=24)을 초과하는 순간이 원인
 *
 * [printf(로그)로 잡기]
 *   덧붙이기 전에 현재 길이/용량/추가 길이를 출력:
 *     fprintf(stderr, "append: len=%zu cap=%zu +%zu\n", *len, cap, strlen(field));
 *   → 구분자(첫 필드가 아니면 1) + 필드 + NUL 이 cap 을 넘는데도 쓰기가 진행되면 오버플로.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: append_field 에서 cap 을 실제로 사용하세요. 첫 필드가 아니면 구분자 1바이트가
 *       더 필요합니다.
 *         extra = (*len > 0) ? 1 : 0;          // 구분자
 *         if (*len + extra + flen + 1 > cap)   // +1 은 NUL
 *       넘치면 잘라 담거나(truncate) 오류로 처리하세요.
 */
#include <stdio.h>
#include <string.h>


static void append_field(char *buf, size_t cap, size_t *len, const char *field, char sep) {
    if (*len > 0) {
        buf[(*len)++] = sep;             
    }
    size_t flen = strlen(field);
    for (size_t i = 0; i < flen; i++) {
        buf[(*len)++] = field[i];         
    }
    buf[*len] = '\0';
    (void)cap;                            
}

static void build_record(char *rec, size_t cap) {
    const char *fields[] = {
        "id=1042", "name=Jonathan", "department=Engineering", "role=maintainer",
    };
    int n = (int)(sizeof(fields) / sizeof(fields[0]));

    size_t len = 0;
    rec[0] = '\0';
    for (int i = 0; i < n; i++) {
        append_field(rec, cap, &len, fields[i], '|');   
    }
}

int main(void) {
    char rec[24];                         

    build_record(rec, sizeof rec);        

    printf("record = %s\n", rec);
    return 0;                            
}
