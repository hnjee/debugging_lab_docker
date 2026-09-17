/*
 * Challenge 13 — Linked List Use-After-Free (심화: 잡 큐 필터링)
 *
 * [시나리오]
 *   우선순위가 있는 잡(Job)들을 단일 연결 리스트 큐로 관리한다. 스케줄러가
 *   "임계값 미만 우선순위"의 잡을 큐에서 제거(취소)하며, 제거된 잡의 id 를 감사
 *   로그(동적 배열)에 기록한다.
 *
 * [기대 동작]
 *   저우선순위 잡을 모두 제거하고, 취소된 개수와 남은 개수를 출력한 뒤 정상 종료.
 *
 * [증상]
 *   필터 루프가 제거 대상 노드를 job_release()로 free한 뒤, 해제된 노드의 next를
 *   읽어 다음 노드로 이동한다(UAF). free() 이후 해당 메모리의 내용은 더 이상
 *   유효하지 않으며, 메모리 할당기가 관리 정보로 덮어쓸 수도 있다. 따라서 cur->next가
 *   엉뚱한 주소가 되고, 다음 순회에서 그 주소의 필드를 역참조하다 SIGSEGV가 발생한다.
 *   크래시는 다음 순회에서 발생하지만, 근본 원인은 "free 후 next 읽기"다.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=13_linked_list_uaf
 *   (gdb) run                       → 크래시(SIGSEGV)
 *   (gdb) bt                        → filter_jobs 의 cur = cur->next / cur->priority 지점
 *   (gdb) print cur                 → 방금 free 한(또는 그로부터 파생된) 노드 주소
 *   (gdb) print *cur                → 값이 깨져 있거나 next 가 엉뚱한 주소임을 확인
 *
 * [printf(로그)로 잡기]
 *   free 전에 next 를 미리 찍고, free 후 이동한 cur 을 비교:
 *     Job *nx = cur->next;
 *     fprintf(stderr, "free id=%d cur=%p saved_next=%p\n", cur->id,(void*)cur,(void*)nx);
 *     job_release(cur);
 *     fprintf(stderr, "after free, cur->next would read freed memory\n");
 *   → free 뒤 읽은 next 가 saved_next 와 달라지거나 그 직후 크래시.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: 노드를 free 하기 "전에" next 를 지역 변수에 저장하고, 그 저장값으로 이동하세요.
 *       (해제된 메모리의 어떤 필드도 읽지 않는다)
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Job {
    int id;
    int priority;
    struct Job *next;
} Job;

typedef struct {
    int   *ids;
    size_t len, cap;
} Audit;

static void audit_add(Audit *a, int id) {
    if (a->len == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16;
        int *p = realloc(a->ids, a->cap * sizeof(int));   
        if (!p) { perror("realloc"); exit(1); }
        a->ids = p;
    }
    a->ids[a->len++] = id;
}

static Job *push_job(Job *head, int id, int priority) {
    Job *n = malloc(sizeof *n);
    if (!n) { perror("malloc"); exit(1); }
    n->id = id;
    n->priority = priority;
    n->next = head;
    return n;
}

/* 취소된 잡을 반납한다(해제 책임은 이 함수가 진다). */
static void job_release(Job *j) {
    free(j);
}

static Job *filter_jobs(Job *head, int threshold, Audit *audit) {
    Job *keep = NULL, *keep_tail = NULL;
    Job *cur = head;

    while (cur != NULL) {
        if (cur->priority < threshold) {
            audit_add(audit, cur->id);   
            job_release(cur);            
            cur = cur->next;             
        } else {
            Job *nx = cur->next;
            cur->next = NULL;
            if (keep_tail) keep_tail->next = cur; else keep = cur;
            keep_tail = cur;
            cur = nx;
        }
    }
    return keep;
}

int main(void) {
    Job *head = NULL;
    for (int i = 1; i <= 4000; i++)
        head = push_job(head, i, (i * 7) % 10);   

    Audit audit = {0};
    head = filter_jobs(head, 5, &audit);           

    int remaining = 0;
    for (Job *c = head; c; c = c->next) remaining++;
    printf("cancelled=%zu remaining=%d\n", audit.len, remaining);

    free(audit.ids);
    for (Job *c = head; c; ) { Job *nx = c->next; free(c); c = nx; }
    return 0;
}
