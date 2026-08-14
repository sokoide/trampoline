/* A tiny cooperative scheduler: FIFO queue + one assembly context switch. */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "internal.h"

static struct st_thread main_thread;
static struct st_thread* current = &main_thread;

static struct st_thread* ready_head;
static struct st_thread* ready_tail;

static void ready_push(struct st_thread* thread) {
    thread->next = NULL;
    if (ready_tail == NULL)
        ready_head = thread;
    else
        ready_tail->next = thread;
    ready_tail = thread;
}

static struct st_thread* ready_pop(void) {
    struct st_thread* thread = ready_head;
    if (thread == NULL)
        return NULL;

    ready_head = thread->next;
    if (ready_head == NULL)
        ready_tail = NULL;
    thread->next = NULL;
    return thread;
}

/* 共通の切替ロジック。実行可能スレッドが無ければ異常扱いで _exit(1)。
 * exit() は atexit handler と stdio の後始末を実行する。この最小ランタイムは
 * それらとの相互作用を定義しないため、_exit() で直ちに終了する。 */
static void switch_to_next(void) {
    struct st_thread* next = ready_pop();
    if (next == NULL)
        _exit(1);

    /* current は st_ctx_swap の【前】に更新する。新スレッドの最初の
     * 切替えでは trampoline が起動するが、ret は引数を用意しない。
     * rdi など caller-saved レジスタの値は任意であり、この最小コンテキスト
     * には fn の引数を保存する場所もない。trampoline は current から
     * 自分自身を知る。 */
    struct st_thread* previous = current;
    current = next;
    st_ctx_swap(&previous->ctx, &next->ctx);
}

/* 新スレッドの入口。st_ctx_swap の ret が最初に到達する場所。
 * ret は引数を設定しないため、グローバルの current から self を得て
 * fn(arg) を呼ぶ (switch_to_next のコメント参照)。fn が戻ったら
 * サンプルとしては役目終わりなので _exit(0) で終了する。 */
static void trampoline(void) {
    current->fn(current->arg);
    _exit(0);
}

/* 新スレッドの最初の ret 用フレームを構築する。
 *
 *   高位アドレス (stack + ST_STACK_BYTES)
 *     ... ↓ 16-byte 境界に切り下げ ...
 *     [top- 8] = 0            trampoline が誤って return した時の安全網
 *     [top-16] = &trampoline  st_ctx_swap の最初の ret 先
 *                 ^
 *                 thread->ctx.rsp
 *
 * ctx.rsp = top-16 なので 16-byte 整列。ret 後の trampoline 入口では
 * rsp = top-8、すなわち System V AMD64 の関数入口規約どおり
 * rsp % 16 == 8 になる。 */
static void setup_stack(struct st_thread* thread) {
    uint64_t* sp = (uint64_t*)(((uintptr_t)thread->stack + ST_STACK_BYTES) &
                               ~(uintptr_t)15);
    *--sp = 0;
    *--sp = (uint64_t)(uintptr_t)&trampoline;
    thread->ctx.rsp = (uint64_t)(uintptr_t)sp;
}

void st_init(void) {
    current = &main_thread;
}

/* TCB と専用スタックを作り、ready queue の末尾に置く。OS にスレッド
 * 作成を依頼しない。実行が始まるのは st_start() で最初の切替えが
 * 起きたとき。 */
void st_thread_create(st_fn fn, void* arg) {
    if (fn == NULL)
        _exit(1);

    struct st_thread* thread = calloc(1, sizeof(*thread));
    if (thread == NULL)
        _exit(1);

    thread->stack = malloc(ST_STACK_BYTES);
    if (thread->stack == NULL)
        _exit(1);

    thread->fn = fn;
    thread->arg = arg;
    setup_stack(thread);

    ready_push(thread);
}

/* main を離れ、ready queue の先頭スレッドへ制御を移す。
 * main_thread は ready queue に入らないため、main へ戻る経路はない。 */
void st_start(void) {
    switch_to_next();
    __builtin_unreachable();
}

void st_yield(void) {
    ready_push(current);
    switch_to_next();
}
