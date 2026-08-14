#ifndef COOP_CTX_H
#define COOP_CTX_H

#include <stddef.h> /* offsetof, _Static_assert */
#include <stdint.h> /* uint64_t */

/* コンテキストスイッチの「正体」を最小化した形。
 *
 * System V AMD64 ABI では、関数呼び出しをまたいで「生きていなければなら
 * ない」レジスタ (callee-saved) は次の 7 本だけ:
 *
 *     rsp (スタックポインタ)
 *     rbp, rbx, r12, r13, r14, r15
 *
 * 残りのレジスタ (rax, rcx, rdx, rsi, rdi, r8-r11 等、いわゆる caller-saved)
 * は「呼出元が退避すべきもの」であり、関数呼び出し境界を超えた時点で
 * 内容が失われてもよい (= 呼出側が期待してはいけない)。
 *
 * st_yield() から呼ばれる st_ctx_swap() は普通の関数呼び出しなので、
 * この ABI 規則に乗れる。つまり「caller-saved は保存しない」でよい。
 * 保存すべきは上記 7 本だけ。これがコンテキストスイッチの最小構成。
 *
 * 現コンテキストを保存し、次のコンテキストへ切り替える処理を、libc に
 * 頼らず手書き asm で実装する。引数順序は (prev, next)。
 */

/* 保存されたコンテキスト。TCB に埋め込まれる。56 バイト。
 * 【重要】フィールドの並びとオフセットは ctx.S の即値オフセットと
 * 1 対 1 で対応する。並びを変えるときは ctx.S も同時に更新すること。 */
struct st_ctx {
    uint64_t
        rsp; /* +0  スタックポインタ (call 命令が積んだ戻りアドレスを指す) */
    uint64_t rbp; /* +8 */
    uint64_t rbx; /* +16 */
    uint64_t r12; /* +24 */
    uint64_t r13; /* +32 */
    uint64_t r14; /* +40 */
    uint64_t r15; /* +48 */
};

/* ctx.S のオフセットと構造体レイアウトの整合性をコンパイル時に検証。
 * どちらかを書き換えて不一致になればビルドエラーで即座に気付ける。 */
_Static_assert(sizeof(struct st_ctx) == 56, "st_ctx must be 56 bytes");
_Static_assert(offsetof(struct st_ctx, rsp) == 0, "rsp must be at offset 0");
_Static_assert(offsetof(struct st_ctx, r15) == 48, "r15 must be at offset 48");

/* 現コンテキストを *prev に保存し、*next のコンテキストへ切り替える。
 * 実装は ctx.S。
 *
 *   st_ctx_swap(prev, next)
 *     rdi = prev (保存先: 現スレッドの ctx)
 *     rsi = next (復元元: 次スレッドの ctx)
 *
 * 戻り時には next 側のコンテキストに完全に入れ替わっている。
 * 「戻る」とは、next->rsp が指す戻りアドレスへの jmp にすぎない。
 *
 * st_ctx_swap はインライン展開されてはならない (caller-saved が dead となる
 * 「本当の関数呼出し境界」を生成するため)。宣言が out-of-line 関数なので、
 * 最適化に関わらず呼出規約通りに振る舞う。 */
void st_ctx_swap(struct st_ctx* prev, struct st_ctx* next);

#endif /* COOP_CTX_H */
