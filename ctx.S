/* コンテキストスイッチの実体 (x86_64 System V ABI, Intel 構文)。
 *
 * これが「スレッド切替」の正体そのもの。特別なカーネル機能は一切使わない。
 * やっていることは:
 *   1. 現在の callee-saved レジスタを prev 構造体へ退避
 *   2. rsp を next->rsp に差し替える   ← ★ ここで「実行者」が切り替わる
 *   3. next 構造体から callee-saved を復元
 *   4. ret で next->rsp が指す戻りアドレスへジャンプ
 *
 * なぜこれだけで済むのか:
 *   - st_ctx_swap は普通の `call` 命令で呼ばれる。call は戻りアドレスを
 *     スタックに積むので、現 rsp が指す先 = 「後で戻るべきアドレス」。
 *     これを prev->rsp に保存すれば、後で prev に戻すとき rsp を復元して
 *     ret するだけで元の場所に帰れる。
 *   - rbp/rbx/r12-r15 は ABI 上「呼出先が勝手に壊してはいけない」レジスタ
 *     (callee-saved)。だから保存対象。それ以外 (rax/rcx/rdx/rsi/rdi/r8-r11 等) は
 *     「呼出元が自分で保存すべき」レジスタ (caller-saved) で、st_ctx_swap 呼び出しの
 *     時点で既に dead であってよい。よって保存しない。
 *   - 【注意】SSE の xmm レジスタは保存しないが、協調モードではこれは正しい。
 *     System V AMD64 ABI では xmm0-15 は全て caller-saved であり、協調 yield は
 *     本関数への通常の call 境界で起きるため、生きている xmm は呼出元
 *     (= yield するスレッド) のコンパイラが退避する。だからここで保存しなくても
 *     FP 状態は壊れない。FP 保存問題は「実行の途中を非同期に割って切替える」
 *     プリエンプティブ版に固有である (兄弟教材 ../threading/completed/preemptive
 *     の on_sigvtalrm は fpregs を切替えない)。
 *     (厳密な ABI 準拠では MXCSR 制御ビットと x87 FCW も callee-saved だが、
 *     通常の整数・倍精度演算では影響しない。)
 *
 * 引数順序は swapcontext(oucp, ucp) と同じ (prev, next) に揃えてある。
 * プリエンプティブ版 (../threading/completed/preemptive) が libc の
 * swapcontext(&prev->uc, &next->uc) で同じ役割を果たすのと並行して
 * 読めるようにするため (教材の核)。
 *
 * Intel 構文 (mov dest, src) を使う。GNU as は .intel_syntax noprefix で
 * 切り替える。コメント中のレジスタ名も Intel 表記 (rax/rsp 等) で統一。
 */
    .intel_syntax noprefix
    .text
    .globl st_ctx_swap
    .type  st_ctx_swap, @function
st_ctx_swap:
    /* rdi = prev (保存先), rsi = next (復元元)
     * call 命令が戻りアドレスを push 済み。現 rsp はそれを指す。 */

    /* --- prev (rdi) へ現コンテキストを保存 --- */
    mov     [rdi +  0], rsp       /* prev->rsp = rsp */
    mov     [rdi +  8], rbp
    mov     [rdi + 16], rbx
    mov     [rdi + 24], r12
    mov     [rdi + 32], r13
    mov     [rdi + 40], r14
    mov     [rdi + 48], r15

    /* --- next (rsi) からコンテキストを復元 --- */
    mov     rsp, [rsi +  0]       /* rsp = next->rsp */
    mov     rbp, [rsi +  8]
    mov     rbx, [rsi + 16]
    mov     r12, [rsi + 24]
    mov     r13, [rsi + 32]
    mov     r14, [rsi + 40]
    mov     r15, [rsi + 48]

    ret                            /* next->rsp が指す戻りアドレスへジャンプ */
    .size  st_ctx_swap, .-st_ctx_swap

/* 明示的に non-executable stack を宣言。これが無いとリンカが
 *「アセンブリソース由来で .note.GNU-stack が無い = 実行可能スタック」
 * と解釈して警告を出す（現代のセキュリティ要件では NG）。 */
    .section .note.GNU-stack,"",@progbits
