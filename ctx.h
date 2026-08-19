#ifndef TRAMPOLINE_CTX_H
#define TRAMPOLINE_CTX_H

#include <stddef.h> /* offsetof, _Static_assert */
#include <stdint.h> /* uint64_t */

/* The minimal state needed for a context switch.
 *
 * Under the System V AMD64 ABI, the callee-saved registers rbp, rbx, and
 * r12-r15 must survive a call, and rsp must be restored by the time the
 * callee returns. A context switch therefore saves and restores all of
 * these seven registers, plus the ABI-preserved floating-point control state:
 *
 *     rsp (stack pointer)
 *     rbp, rbx, r12, r13, r14, r15
 *     mxcsr (SSE: Streaming SIMD Extensions control/status register), x87 control word
 *
 * The other registers (rax, rcx, rdx, rsi, rdi, r8-r11, and so on) are
 * caller-saved. A caller must save them if it needs their values after a call.
 *
 * st_ctx_swap() is called like a normal function through the st_yield() call
 * chain (st_yield -> switch_to_next -> st_ctx_swap), so this ABI rule
 * applies. XMM data registers are caller-saved, but the x87 control word and
 * MXCSR control state must remain associated with each logical thread, so this
 * sample saves them too.
 *
 * This hand-written assembly saves the current context and switches to the
 * next one without using libc. The argument order is (prev, next).
 */

/* Saved context embedded in the TCB. It is 64 bytes.
 * Important: field order and offsets must match the constants in ctx.S.
 * Update ctx.S too if this layout changes. */
struct st_ctx {
    uint64_t
        rsp; /* +0  stack pointer, pointing to a call return address */
    uint64_t rbp; /* +8 */
    uint64_t rbx; /* +16 */
    uint64_t r12; /* +24 */
    uint64_t r13; /* +32 */
    uint64_t r14; /* +40 */
    uint64_t r15; /* +48 */
    uint32_t mxcsr; /* +56: SSE control/status register */
    uint16_t x87_cw; /* +60: x87 floating-point control word */
    uint16_t padding; /* +62: make the structure 8-byte aligned */
};

/* Check the layout against ctx.S at compile time.
 * A mismatch becomes a build error immediately. */
_Static_assert(sizeof(struct st_ctx) == 64, "st_ctx must be 64 bytes");
_Static_assert(offsetof(struct st_ctx, rsp) == 0, "rsp must be at offset 0");
_Static_assert(offsetof(struct st_ctx, r15) == 48, "r15 must be at offset 48");
_Static_assert(offsetof(struct st_ctx, mxcsr) == 56, "mxcsr must be at offset 56");
_Static_assert(offsetof(struct st_ctx, x87_cw) == 60, "x87_cw must be at offset 60");

/* Save the current context in *prev and switch to *next.
 * The implementation is in ctx.S.
 *
 *   st_ctx_swap(prev, next)
 *     rdi = prev (save destination: current thread's ctx)
 *     rsi = next (restore source: next thread's ctx)
 *
 * On return, the next context is active. Returning is just a jump to the
 * return address pointed to by next->rsp.
 *
 * st_ctx_swap must remain an out-of-line function so the compiler creates a
 * real call boundary. Its declaration and assembly implementation enforce
 * the normal calling convention even when optimization is enabled. */
void st_ctx_swap(struct st_ctx* prev, struct st_ctx* next);

#endif /* TRAMPOLINE_CTX_H */
