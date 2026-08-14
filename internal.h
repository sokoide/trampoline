#ifndef COOP_INTERNAL_H
#define COOP_INTERNAL_H

/* cooperative/ の内部宣言。preemptive/internal.h と構造を対称にしてある
 * (教材で両ディレクトリを並べて読めるように)。 */

#include "ctx.h"
#include "st.h"

/* スレッド状態。named enum にして型名で参照できるようにする。 */
enum st_state {
    ST_READY,   /* キュー内、実行可能 */
    ST_RUNNING, /* 現在実行中 */
    ST_BLOCKED, /* 待機中（例: st_join） */
    ST_DONE     /* 終了、retval 取得可能 */
};

/* Thread Control Block。スレッドごとに1つ（ブートストラップの main も含む）。
 * preemptive/internal.h の TCB とフィールド構成を対称にしてある。
 * preemptive 側にだけ存在するフィールド (critical_depth 等) は除く。 */
struct st_thread {
    struct st_ctx ctx; /* 保存されたコンテキスト (rsp + callee-saved) */
    enum st_state state;

    void* stack; /* malloc 確保の専用スタック。main は NULL */
    size_t stack_bytes;
    int id;

    st_fn fn;
    void* arg;
    void* retval;

    struct st_thread* joiner; /* このスレッドで st_join() 待機中のスレッド */
    int join_consumed; /* st_join() 成功時に 1。二重 join を -1 で拒否 */
    struct st_thread* rq_next; /* 実行キューのリンク (NULL 終端 FIFO) */
    struct st_thread* pending_next; /* st_start() 前の生成待ちリスト */
};

#endif /* COOP_INTERNAL_H */
