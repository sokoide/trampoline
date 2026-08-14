/* 協調的ユーザスレッドランタイム (SimThread cooperative)。
 *
 * 設計の核は ctx.S の st_ctx_swap 1 個。タイマもシグナルもシグナルマスクも
 * 不要で、切替は「自発的譲渡 (st_yield / join / exit)」のときだけ起きる。
 * 切替経路にはカーネルが介在しない。
 *
 * 構成:
 *   - Thread Control Block (struct st_thread) と FIFO 実行キュー
 *   - dispatch_next: 次スレッドを選び切替
 *   - 公開 API: init / thread_start / start / yield / join / exit
 *
 * 公開 API の順序は st.h の宣言順 (init → thread_start → start → yield → join → exit →
 * tunables) に合わせてある。学習者がヘッダと実装を行き来しやすいように。
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "internal.h"
#include "rq.h"

/* ----- チューナブル ----- */

static size_t stack_bytes = ST_DEFAULT_STACK_BYTES;
static int initialized = 0;
static int started = 0;

void st_set_stack_bytes(size_t bytes) {
    if (bytes >= ST_MIN_STACK_BYTES)
        stack_bytes = bytes;
}

/* ----- グローバル状態 ----- */

/* 単一 LWP (OS スレッド1本) 上で N ユーザスレッドを時分割する M:1 モデル。
 * よって以下の globals はロック不要 (並行アクセスがない)。 */
static st_thread* current = NULL; /* RUNNING 状態のスレッド */
static st_thread main_thread; /* ブートストラップの main スレッド */
static int next_id = 0;

/* ----- 実行キュー (rq = run queue, NULL 終端単方向リスト、FIFO) ----- */

static struct st_run_queue ready_queue;
static st_thread* pending_head = NULL;
static st_thread* pending_tail = NULL;

/* ----- 内部ヘルパ: スケジューラ ----- */

/* 共通の切替ロジック。実行可能スレッドが無いときの振る舞いは prev
 * の状態で決まる: ST_DONE    : 最後のスレッドの正当な終了 → pthread
 * セマンティクスで _exit(0) ST_BLOCKED : 全スレッド待機 = デッドロック →
 * _exit(1) exit() ではなく _exit() を使う: exit() は atexit/destructor
 * を走らせるが、 それがユーザスレッドのスタック/TCB
 * に触れると未定義動作になる。 */
static void dispatch_next(void) {
    st_thread* prev = current;
    st_thread* next = rq_pop_front(&ready_queue);
    if (next == NULL) {
        if (prev->state == ST_DONE) {
            _exit(0);
        }
        static const char msg[] =
            "cooperative: no runnable thread (possible deadlock)\n";
        (void)!write(STDERR_FILENO, msg,
                     sizeof(msg) - 1); /* 戻り値は無視（診断用） */
        _exit(1);
    }

    current = next;
    next->state = ST_RUNNING;

    /* ★ コンテキストスイッチの実体 ★
     * prev のレジスタを prev->ctx へ保存し、next->ctx から復元する。
     * シグナルもカーネルも介在しない。これだけで「実行者」が切り替わる。
     * 後で prev が再スケジュールされると、この st_ctx_swap 呼び出しから戻る。
     */
    st_ctx_swap(&prev->ctx, &next->ctx);
}

/* 新スレッドの入口。fn(arg) を呼び出した後、クリーンに終了する。
 * st_ctx_swap から ret で飛び込んでくる。
 *
 * なぜ引数で self を渡さずグローバル current を読むのか:
 * st_ctx_swap の ret は「call 命令なしで」trampoline へ飛ぶ (rsp が指す戻り
 * アドレスへの jmp)。つまり trampoline が起動するとき、レジスタ経由で
 * 引数を渡す通常の呼出規約は成立していない (rdi には何も設定されていない)。
 * その代わり dispatch_next が current = next を設定済みなので、これを使う。
 *
 * preemptive/ 側の trampoline と対比: あちらは makecontext 経由で起動する
 * ため int 引数 2 つ (hi/lo に分割した TCB ポインタ) を受け取る。本
 * cooperative 版は st_ctx_swap の ret から直接入るので引数なしで済む。 */
static void trampoline(void) {
    st_thread* self = current;
    void* rv = self->fn(self->arg);
    st_thread_exit(rv);
}

/* 新スレッドの最初の ret 用 frame を構築する。tutorial/cooperative の Step 4
 * と同じ境界にしてあり、受講者はこの関数を参照解答として照合できる。
 *
 * makecontext は使わない。st_ctx_swap が next の rsp を復元して ret すると、
 * frame 先頭の trampoline を pop して開始する。
 *
 *   高位アドレス (stack + stack_bytes)
 *     ... ↓ 16-byte 境界に切り下げ ...
 *     [top- 8] = 0            trampoline が誤って return した時の安全網
 *     [top-16] = &trampoline  st_ctx_swap の最初の ret 先
 *                 ^
 *                 t->ctx.rsp
 *
 * ctx.rsp は top-16 なので 16-byte 整列。ret 後の trampoline 入口では
 * rsp = top-8、すなわち System V AMD64 の関数入口規約どおり 8 mod 16 になる。
 */
static void setup_initial_stack(st_thread* t) {
    uint64_t* sp =
        (uint64_t*)(((uintptr_t)t->stack + t->stack_bytes) & ~(uintptr_t)15);
    *--sp = 0;
    *--sp = (uint64_t)(uintptr_t)&trampoline;
    t->ctx.rsp = (uint64_t)(uintptr_t)sp;
}

/* ----- 公開 API (st.h 宣言順) ----- */

int st_init(void) {
    if (initialized)
        return -1;
    rq_init(&ready_queue);
    main_thread.id = next_id++;
    main_thread.state = ST_RUNNING;
    main_thread.stack = NULL; /* main はシステム提供のスタックで動作 */
    main_thread.fn = NULL;
    current = &main_thread;
    initialized = 1;
    return 0;
}

st_thread* st_thread_start(st_fn fn, void* arg) {
    if (fn == NULL)
        return NULL;
    st_thread* t = (st_thread*)calloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;

    t->stack = malloc(stack_bytes);
    if (t->stack == NULL) {
        free(t);
        return NULL;
    }
    t->stack_bytes = stack_bytes;
    t->id = next_id++;
    t->fn = fn;
    t->arg = arg;
    t->state = ST_READY;

    setup_initial_stack(t);

    if (pending_tail == NULL)
        pending_head = t;
    else
        pending_tail->pending_next = t;
    pending_tail = t;
    return t;
}

st_thread* st_thread_create(st_fn fn, void* arg) {
    return st_thread_start(fn, arg);
}

void st_start(void) {
    if (!initialized || started)
        _exit(1);
    started = 1;
    while (pending_head != NULL) {
        st_thread* t = pending_head;
        pending_head = t->pending_next;
        t->pending_next = NULL;
        rq_push_back(&ready_queue, t);
    }
    pending_tail = NULL;
    dispatch_next();
    for (;;) {
    }
}

st_thread* st_self(void) {
    return current;
}

void st_yield(void) {
    current->state = ST_READY;
    rq_push_back(&ready_queue, current);
    dispatch_next();
}

int st_join(st_thread* t, void** retval) {
    if (t == NULL)
        return -1;

    /* 自分自身を join しようとしたら拒否。
     * 自分が終了するまで自分をブロックすることは不可能 (自分が走り続けないと
     * 終了判定できないため)。pthread_join(pthread_self()) が EDEADLK になるのと
     * 同じ rationale。 */
    if (t == current)
        return -1;

    if (t->join_consumed) {
        /* 既に join 済み: 拒否 */
        return -1;
    }
    /* join の権利をここで先に確定する。
     * これにより、待機中に別スレッドが同じ t を join して
     * joiner を上書きする競合を防ぐ。 */
    t->join_consumed = 1;
    if (t->state != ST_DONE) {
        /* t が終了するまでブロック。st_thread_exit が起床する */
        current->state = ST_BLOCKED;
        t->joiner = current;
        dispatch_next(); /* current は BLOCKED、キューに戻さない */
        /* 再開: t は終了しているはず */
    }

    if (retval != NULL)
        *retval = t->retval;
    return 0;
}

void st_thread_exit(void* retval) {
    current->state = ST_DONE;
    current->retval = retval;

    /* このスレッドで st_join() 待機中のスレッドを起床 */
    if (current->joiner != NULL) {
        st_thread* j = current->joiner;
        current->joiner = NULL;
        j->state = ST_READY;
        rq_push_back(&ready_queue, j);
    }

    /* current は DONE。キューに戻さない。 */
    /* この教材では終了済み TCB / stack の回収は意図的に行わない。
     * 終了順や join 順に依存する解放ルールまで入れると、学習対象の
     * 「切替と待機」の本質が見えにくくなるため。 */
    dispatch_next();
    /* 到達不能: dispatch_next は他スレッドへ切替えるか _exit する。
     * st_thread_exit は noreturn 宣言なので、コンパイラに「戻らない」ことを
     * 保証するための安全ネット。 */
    for (;;) {
    }
}
