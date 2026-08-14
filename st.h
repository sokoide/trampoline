#ifndef COOP_ST_H
#define COOP_ST_H

/* 協調的ユーザスレッドライブラリ（cooperative threads）。
 *
 * 【命名】公開 API の st_ 接頭辞は SimThread (ユーザ空間で OS のスレッド
 * 機構をシミュレートする教材、という由来) に因む。内部実装も st_ で統一
 * (例: st_ctx_swap, struct st_ctx)。file-static な補助関数のみ prefix なし。
 *
 * タイマもシグナルも使わない。コンテキストスイッチは st_yield() の
 * 「自発的譲渡」でのみ発生する。切替経路にはカーネルが介在せず、切替の正体は
 *   「callee-saved レジスタの退避・復元 + スタックポインタの差し替え」
 * だけ (ctx.S 参照)。
 *
 * 公開 API は start / yield / join / exit の最小セット。
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明なスレッドハンドル。内部は Thread Control Block を指す。 */
typedef struct st_thread st_thread;

/* スレッドエントリ関数の型（pthread と同じシグネチャ）。 */
typedef void* (*st_fn)(void* arg);

/* スレッドあたりのスタックサイズ（バイト）。 */
#define ST_DEFAULT_STACK_BYTES (64 * 1024)
/* 初期フレームと通常の関数呼出しに必要な最小スタックサイズ。 */
#define ST_MIN_STACK_BYTES (16 * 1024)

/* ランタイムを初期化する。main スレッドを実行中スレッドとして設定する。
 * 他の st_* 関数より前に1回だけ呼ぶ。成功時 0、2回目以降は -1 を返す。 */
int st_init(void);

/* 新しいスレッドを生成する。ここでは fn(arg) を実行しない。
 * st_start() が呼ばれたとき、生成順の FIFO ready queue から実行を開始する。 */
st_thread* st_thread_start(st_fn fn, void* arg);

/* 互換用の別名。これもスレッドを作るだけで、自動実行はしない。 */
st_thread* st_thread_create(st_fn fn, void* arg);

/* ready queue の先頭スレッドへ切り替えて実行を開始する。戻らない。 */
void st_start(void) __attribute__((noreturn));

/* 現在実行中のスレッドのハンドルを返す（main スレッドを含む）。
 * 自分自身を他スレッドに join させるような連鎖構造で有用。 */
st_thread* st_self(void);

/* 明示的に CPU を手放す。現在のスレッドはキュー末尾に回り、
 * 次の READY スレッドが実行される。 */
void st_yield(void);

/* t が終了するまで呼び出し元をブロックする。t の戻り値を *retval に格納する
 * （retval != NULL の場合）。成功時 0、t が NULL、t が自分自身 (st_self() の
 * 戻り値と等価)、または既に join 済みの場合 -1。 */
int st_join(st_thread* t, void** retval);

/* 呼び出し元スレッドを終了し、戻り値を設定する。戻らない。
 * 呼び出し元が最後の実行可能スレッドの場合はプロセスを status 0 で終了する
 * （最後のスレッド終了時の pthread セマンティクスに準拠）。全スレッドが
 * ブロックされている場合（デッドロック）は status 1 で終了する。 */
void st_thread_exit(void* retval) __attribute__((noreturn));

/* スレッドあたりのスタックサイズをバイト単位で設定。
 * st_thread_start() より前に呼ぶこと。ST_MIN_STACK_BYTES 未満は無視する。 */
void st_set_stack_bytes(size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* COOP_ST_H */
