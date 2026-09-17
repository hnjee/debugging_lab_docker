/*
 * Challenge 14 — Integer Overflow → 과소할당 → 오버플로 (심화: 이미지 버퍼)
 *
 * [시나리오]
 *   RGBA 이미지 버퍼를 만든다. 픽셀 바이트 수 = width * height * channels 로 계산해
 *   할당하고, 전 픽셀을 초기값으로 채운다.
 *
 * [기대 동작]
 *   이미지 버퍼를 할당·초기화하고 몇몇 픽셀을 읽어 확인한 뒤 정상 종료.
 *
 * [사용 예제]
 *   RGB만 있으면 픽셀은 “무슨 색이냐”만 표현한다.
 *   A(Alpha) 가 붙으면 “얼마나 보이는가”까지 표현한다.
 *     A = 255 (또는 1.0): 완전 불투명. 아래 사진이 안 보임
 *     A = 0: 완전 투명. 로고 자리는 비어 있고 사진만 보임
 *     A = 128: 반투명. 로고 색과 사진 색이 섞임
 *     웹·앱에서 PNG 로고, 게임 캐릭터, UI 버튼 그림자가 다 이 방식
 *    
 * [증상]
 *   크기 계산 `width * height * channels` 가 'int' 산술로 먼저 이뤄진 뒤에야 size_t 로
 *   넓혀진다. 큰 해상도에서는 이 int 곱이 32비트 범위를 넘어 래핑되어, malloc 은
 *   실제 필요량보다 훨씬 작은(심하면 0에 가까운) 크기로 할당된다.
 *   반면 초기화 루프는 "진짜 픽셀 수"(size_t 로 정확히 계산)만큼 돌기 때문에, 작은
 *   버퍼 밖으로 대량으로 써서 힙을 넘어선다 → SIGSEGV.
 *   크래시는 채우기 루프의 대입에서 나지만, 원인은 "int 곱셈 오버플로".
 *
 * [gdb 로 잡기]
 *   make gdb NAME=14_integer_overflow_alloc
 *   (gdb) run                        → 크래시(SIGSEGV)
 *   (gdb) bt                         → image_fill 의 px[i] = ... 지점
 *   (gdb) frame N ; print img->nbytes  → int 곱이 래핑되어 실제보다 작음
 *   (gdb) print (long)img->width * img->height * img->channels  → 올바른(큰) 값과 대조
 *
 * [printf(로그)로 잡기]
 *   할당 크기(래핑된 int)와 올바른 크기(size_t)를 나란히 출력:
 *     fprintf(stderr, "alloc=%d correct=%zu\n",
 *             img->nbytes, (size_t)img->width * img->height * img->channels);
 *   → 두 값이 크게 다르면 곱셈 오버플로로 과소할당된 것.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: 크기 계산을 size_t 로 (각 인자를 (size_t)로 캐스팅) 하고, 곱셈
 *       오버플로를 검사하세요(SIZE_MAX를 이용해서 검사)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // 수정할 때 SIZE_MAX 로 곱셈 오버플로를 검사하라고 미리 넣어 둔 헤더

typedef struct {
    int width;
    int height;
    int channels;
    int nbytes;              
    unsigned char *px;
} Image;

static Image *image_new(int width, int height, int channels) {
    Image *img = malloc(sizeof *img);
    if (!img) { perror("malloc"); exit(1); }
    img->width = width;
    img->height = height;
    img->channels = channels;

    img->nbytes = width * height * channels;
    img->px = malloc((size_t)img->nbytes);     
    if (!img->px) { perror("malloc px"); exit(1); }
    return img;
}

static void image_fill(Image *img, unsigned char value) {

    size_t total = (size_t)img->width * (size_t)img->height * (size_t)img->channels;
    for (size_t i = 0; i < total; i++) {
        img->px[i] = value;                     
    }
}

int main(void) {

    /* [Thinking Point]
     * 65536 x 65536 픽셀에 채널 4개(RGBA: Red/Green/Blue/Alpha=불투명도)를 요청한다.
     * 실제 필요한 바이트 수를 손으로 계산해보자: 65536 * 65536 * 4 = 17,179,869,184 (약 16GB).
     *   tip 1. 이 곱셈을 int(보통 32비트, 최대 약 21억)로 하면 결과가 '한 바퀴 돌아(wrap)'
     *          엉뚱하게 작은 값(심지어 0)이 된다. 65536*65536 = 2^32 → int 로는 0 이다.
     *   tip 2. image_new 안에서 크기를 int 로 계산해 malloc 하면, 실제보다 훨씬 작은(또는 0)
     *          버퍼가 잡힌다. 그런데 image_fill 은 size_t 로 '진짜 16GB' 만큼 순회한다.
     *   생각해보기: 할당은 작게, 쓰기는 크게 → 무슨 일이 벌어질까? 그리고 왜 채널이 4(RGBA)
     *               일 때가 3(RGB)일 때보다 오버플로가 더 쉽게 터질까?
     *               (해결 힌트: 크기 계산을 size_t 로 승격하고, 곱셈 오버플로를 검사한다) */
    Image *img = image_new(65536, 65536, 4);
    printf("allocated nbytes(int)=%d for %dx%d x%d\n",
           img->nbytes, img->width, img->height, img->channels);

    image_fill(img, 0xFF);                       

    printf("px[0]=%u\n", img->px[0]);
    free(img->px);
    free(img);
    return 0;
}
