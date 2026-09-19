/*
 * Challenge 01 — Use After Free (심화: vtable 기반 위젯 시스템)
 *
 * [시나리오]
 *   아주 작은 GUI 흉내. 각 위젯(Widget)은 힙 객체이며 첫 멤버로 "vtable"
 *   (render/on_event 함수 포인터 묶음)을 가진다. Screen 은 위젯 포인터 배열을
 *   들고 있고, 이벤트를 나눠준 뒤(dispatch) 한 프레임을 그린다(render).
 *
 * [기대 동작]
 *   버튼/라벨/다이얼로그를 그리고, 닫기 이벤트 후 남은 위젯만 다시 그린 뒤
 *   정상 종료(0).
 *
 * [증상]
 *   닫기 이벤트 핸들러가 다이얼로그 위젯을 free() 하지만, Screen 의 포인터 배열에서
 *   그 슬롯을 제거(NULL 로)하지 않는다. 그 사이 앱이 상태 메시지 버퍼를 새로 할당하며
 *   방금 해제된 청크를 재사용해 vtable 포인터 자리를 덮어쓴다.
 *   다음 렌더 패스에서 해제된 위젯의 w->vtbl->render 를 호출 → 망가진 함수 포인터로
 *   점프 → SIGSEGV. 크래시는 render 루프에서 나지만, 원인은 멀리 떨어진 close 핸들러다.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=01_use_after_free
 *   (gdb) run                         → 크래시(SIGSEGV)
 *   (gdb) bt                          → screen_render() 안 w->vtbl->render(w) 지점
 *   (gdb) print w                     → 어떤 위젯인지(주소/슬롯) 확인
 *   (gdb) print w->vtbl               → 오염돼 있음
 *   (gdb) print s->items[2]           → 이미 해제된 슬롯이 그대로 남아있음
 *   (gdb) break widget_destroy        → 누가/언제 이 위젯을 free 하는지 역추적
 *
 * [printf(로그)로 잡기]
 *   위젯 해제 시점과 렌더 시점의 vtbl 값을 각각 찍어 "해제가 사용보다 먼저"인지 확인:
 *     (destroy) fprintf(stderr, "destroy id=%d w=%p vtbl=%p\n", w->id,(void*)w,(void*)w->vtbl);
 *     (render)  fprintf(stderr, "render  id=%d w=%p vtbl=%p\n", w->id,(void*)w,(void*)w->vtbl);
 *   → 같은 주소가 destroy 후 render 에서 다시 나오고, vtbl 값이 달라져 있으면 UAF.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: "해제"와 "슬롯 정리"를 한 곳에서 같이 하세요. 위젯 자신은 Screen 을 모르므로
 *       (dialog_on_event 는 self 만 안다) 이벤트 핸들러에서는 closed 표시만 남기고,
 *       Screen 쪽에서 closed 위젯을 free 한 뒤 그 슬롯을 NULL 로 만드는 편이 자연스럽습니다.
 *       이후 dispatch/render 루프가 NULL 슬롯을 건너뛰게 하세요. "해제 = 소유 포인터 무효화".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Q1. 왜 Widget 구조체를 미리 선언할까? 
// VTable과 Widget이 서로를 참조하고 있기 때문이다. 
// -> 따라서 존재를 먼저 알려주고(전방선언), 그 뒤에 내용을 채우는 2단계가 필요하다. 
typedef struct Widget Widget;

typedef struct {
/* Q2. 함수포인터는 왜 쓰는걸까? 
    함수 포인터: 함수가 있는 메모리(텍스트영역)의 주소를 담는다.
    매개변수/리턴타입(시그니처)만 같으면, 서로 다른 함수를 같은 함수포인터 자리에 바꿔 끼울 수 있다. 
    이 덕분에 "실행 시점에 어떤 함수를 부를지"를 데이터(구조체 필드)로 다룰 수 있게 되고
    이게 다형성의 C식 구현(vtable)이 된다.
*/
    void (*render)(Widget *self);
    void (*on_event)(Widget *self, int code);
} VTable;

struct Widget {
    const VTable *vtbl; 
    int id;
    int closed;
    char label[24];
    /* Q5. 왜 char *label 대신 char label[24] 배열자체를 선택했을까?
    -> 글자 수가 짧고 예측 가능하다면 → 배열로 구조체 안에 포함시켜서 malloc/free를 한 번으로 단순화.
    글자 수가 길거나 예측 불가능하다면 → 포인터 + 별도 malloc으로 유연하게, 대신 관리(free 순서 등)는 더 신경 써야 함.
    */
};

#define MAX_WIDGETS 8 //메크로: 전처리 단계에서 텍스트로 치환해 버림 
typedef struct {
    Widget *items[MAX_WIDGETS]; //Widget을 가리키는 포인터 배열 
    int count;
} Screen;

/* ── 위젯 종류별 동작 ─────────────────────────────────────────── */
/* ── 1. render 함수 ──────────── */
static void button_render(Widget *self) {
    printf("  [Button #%d] \"%s\"\n", self->id, self->label);
}
static void label_render(Widget *self) {
    printf("  Label #%d: %s\n", self->id, self->label);
}
static void dialog_render(Widget *self) {
    printf("  <<Dialog #%d>> %s\n", self->id, self->label);
}

/* ── 2. on_event 함수 ──────────── */
static void widget_noop_event(Widget *self, int code) { (void)self; (void)code; }
//함수 포인터 맞추려면 매개변수 받긴해야하지만, 막상 그 안에서는 아무일도 안해야하는 함수 
//(void)self; -> warning: unused parameter 'self' 매개변수 받았는데 왜 안쓰니 라는 파라미터 경고 피하기 위함 

/* 다이얼로그는 이벤트 코드 1(닫기)을 받으면 스스로 정리(파괴)된다 */
static void dialog_on_event(Widget *self, int code);

/* Q7. static, const를 왜 붙일까?
    static: 저장 기간을 static으로, 연결성을 internal로 만든다. 
    (단, 해당 변수가 이미 그 상태였으면 바뀌지 않는 것처럼 보임.)
    저장 기간(storage duration): 얼마나 오래 살아있는가 (함수 끝나면 죽는가, 프로그램 끝까지 사는가) -> 프로그램 끝까지 살도록 함 
    연결성(linkage): 다른 파일에서도 접근 가능한가 -> 다른 파일에서 접근 못하도록 함
    static이 붙은건 파일 내부 구현이라 외부(다른 .c 파일)에 노출할 필요 없음

    const: 초기화 후 값 변경 금지
*/
/* ── 3. VTable 구조체 정의하기 ──────────── */
static const VTable BUTTON_VT = { button_render, widget_noop_event };
static const VTable LABEL_VT  = { label_render,  widget_noop_event };
static const VTable DIALOG_VT = { dialog_render, dialog_on_event  };

/* ── 4. Widget 구조체 생성/삭제 함수 ──────────── */
static Widget *widget_new(const VTable *vt, int id, const char *label) {
    Widget *w = malloc(sizeof *w);
    if (!w) { perror("malloc"); exit(1); }
    /*  Q3. w 에 아직 아무 값도 넣지 않았는데, sizeof *w 로 *w 를 써도 괜찮은 이유는 뭘까?
        -> sizeof는 피연산자의 값을 역참조하지 않고 타입만을 본다. 
        그래서 sizeof(*w)는 w의 타입이 Widget만 확인하여 컴파일 타임에 상수로 치환됨    
    */
    /*  Q4. sizeof(Widget) 대신 sizeof *w 로 쓰면 어떤 장점이 있을까?
        -> w의 타입을 Widget에서 다른걸로 바꿔도 sizeof 코드를 고칠 필요 없다. (유지보수성 Don't Repeat Yourself)
        sizeof는 타입이 안 맞아도 컴파일 에러가 안난다. 
        하지만 포인터가 할당받은 공간을 벗어난 곳을 사용하는 순간 힙 오버플로우가 발생함
        이 시점에는 크래시가 나지 않지만, 원래 거기 있어야할 다른 데이터를 오염시키고, -> 원인 
        이후 오염된 데이터를 누군가 사용하려고 할 때 크래시가 터짐 -> 결과 
        문제는 이런 버그는 디버깅하기가 정말 어려워진다. 원인과 결과가 시간적, 코드 위치적으로도 멀리 떨어져있기 때문이다. 
    */  

    w->vtbl = vt;
    w->id = id; 
    w->closed = 0; 
    //label이 가리키는 주소에서부터 실제 저장된 문자들을 읽어서, w->label이 가리키는 위치에 하나하나 복사해 넣어라는 뜻.
    strncpy(w->label, label, sizeof(w->label) - 1); 
    w->label[sizeof(w->label) - 1] = '\0';
    /* Q6. strncpy를 쓴 이유 
        strcpy: label의 길이를 초과하는 긴 문자열이 들어와도 24바이트 공간에 그냥 다 밀어넣으려고 한다. 
        24바이트를 넘는 부분은 label 필드 뒤에 있는 다른 메모리 영역(구조체의 다음 필드거나, 힙의 다른 데이터)을 덮어써버리는데
        이게 바로 버퍼 오버플로우(buffer overflow) (힙에서 생기면 힙버퍼오버플로우, 스택에서 생기면 스택버퍼 오버플로우)
        -> strncpy는 "최대 몇 바이트까지만 복사해라"라는 제한을 걸 수 있는 버전
        첫 줄: "너무 긴 문자열이 들어와도 2ㄹ호옹4바이트를 넘어서 다른 메모리를 침범하지 않게 막는다"
        -> strncpy는 좀 특이하게 동작하는데, 복사할 원본 문자열이 제한 길이보다 길면, NUL 문자를 아예 안 붙여줌 
        둘째 줄: "그렇게 자르고 나서도, 결과물이 항상 올바르게 끝나는(NUL로 종료된) 문자열이 되도록 보장한다"
    */
    return w;
}

static void widget_destroy(Widget *w) {
    free(w);
}

/* ── Screen ──────────────────────────────────────────────────── */
static void screen_add(Screen *s, Widget *w) {
    if (s->count < MAX_WIDGETS) s->items[s->count++] = w;
}

static void screen_dispatch(Screen *s, int code) {
    for (int i = 0; i < s->count; i++) {
        Widget *w = s->items[i];
        w->vtbl->on_event(w, code);
    }
}

static void screen_render(Screen *s) {
    for (int i = 0; i < s->count; i++) {
        Widget *w = s->items[i];
        if(w != NULL){
            w->vtbl->render(w);  
        }
    }
}

static void dialog_on_event(Widget *self, int code) {
    if (code == 1) {
        self->closed = 1;   
    }
}

static char *app_build_status(const char *text) {
    char *msg = malloc(sizeof(Widget));   
    if (!msg) exit(1);

    /* [테스트용 연출] 재사용한 메모리를 0xAB 로 '일부러' 덮어써서 오염시킨다.
     * 실무라면 다른 기능이 우연히 이 자리를 덮어쓰겠지만, 여기서는 UAF 크래시를
     * 매번 똑같이(결정적으로) 재현하기 위해 인위적으로 채운다. 
     * glibc(리눅스) 환경 (tcache)에서만 유효하다. 환경&상황에 따라 msg는 새로운 주소로 할당될 수 있다.
     */
    memset(msg, 0xAB, sizeof(Widget));
    //memset: "지정한 메모리 범위를 특정 값으로 싹 채워라"는 함수
    //여기선 msg가 가리키는 Widget 크기만큼을 전부 0xAB라는 값으로 채움 

    snprintf(msg, sizeof(Widget), "STATUS: %s", text);
    /*Q8. snprintf란?
        - printf(): 표준출력 (버퍼 사용안함)
        - sprintf(): 문자열 버퍼 사용, 크기 제한 없어서 버퍼오버플로우 위험 
        - snprintf(): 문자열 버퍼 사용, 저장할 최대 크기를 지정하여 메모리 안전하게 보호 

        "STATUS: "라는 접두어 뒤에 text(예: "dialog closed")를 이어붙여서, 
        sizeof(Widget) - 1개의 "실제 문자"를 쓰고, 마지막 1자리에 \0 붙여줌
        그리고 그 문자열을 msg가 가리키는 자리에 써넣는다.  

    */

    //snprintf
    return msg;
}

int main(void) {
    Screen s = { .count = 0 };

    screen_add(&s, widget_new(&LABEL_VT,  10, "Welcome"));
    screen_add(&s, widget_new(&BUTTON_VT, 11, "OK"));
    screen_add(&s, widget_new(&DIALOG_VT, 12, "Are you sure?"));  /* items[2] */
    screen_add(&s, widget_new(&BUTTON_VT, 13, "Cancel"));

    printf("frame 1:\n"); 
    screen_render(&s);
    screen_dispatch(&s, 1);

    /* TODO 닫힌(closed) 위젯을 여기서 정리(free + 해당 슬롯 NULL)할 필요가 있음 */
    for(int i=0; i<s.count; i++){
        if(s.items[i] != NULL && s.items[i]->closed){
            free(s.items[i]);
            s.items[i] = NULL;
        }
    }

    char *status = app_build_status("dialog closed");
    printf("%s\n", status);

    printf("frame 2:\n");
    screen_render(&s);           

    free(status);
    for (int i = 0; i < s.count; i++) free(s.items[i]);
    return 0;
}