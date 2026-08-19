/* The context switch implementation (x86_64 System V ABI, Intel syntax).
 *
 * This is the actual thread switch. It uses no special kernel feature.
 * It does four things:
 *   1. Save rsp, the current callee-saved registers, and FP control state into prev.
 *   2. Replace rsp with next->rsp. This changes the active stack.
 *   3. Restore the callee-saved registers and FP control state from next.
 *   4. Use ret to jump to the address at next->rsp.
 *
 * Why is this enough?
 *   - st_ctx_swap is called with an ordinary call instruction. The call pushes
 *     a return address, so the current rsp points to the place to resume.
 *     Saving it in prev->rsp lets a later ret resume the old thread.
 *   - rbp/rbx/r12-r15 are callee-saved by the ABI, so they must be saved.
 *     Registers such as rax, rcx, rdx, rsi, rdi, and r8-r11 are caller-saved;
 *     the caller cannot expect their values to survive this call.
 *   - SSE (Streaming SIMD Extensions) xmm0-xmm15 registers are also caller-saved
 *     under the System V AMD64 ABI.
 *     Since yield happens at a normal call boundary, the compiler handles any
 *     live values. The MXCSR and x87 control word are preserved explicitly:
 *     for example, one thread's rounding-mode change must not affect another.
 *
 * The argument order is (prev, next): save into prev and restore from next.
 *
 * This file uses Intel syntax (mov dest, src). GNU as enables it with
 * .intel_syntax noprefix. Register names in comments also use Intel syntax.
 */
    .intel_syntax noprefix
    .text
    .globl st_ctx_swap
    .type  st_ctx_swap, @function
st_ctx_swap:
    /* rdi = prev (save destination), rsi = next (restore source).
     * The call has pushed a return address, so rsp points to it. */

    /* --- Save the current context into prev (rdi). --- */
    mov     [rdi +  0], rsp       /* prev->rsp = rsp */
    mov     [rdi +  8], rbp
    mov     [rdi + 16], rbx
    mov     [rdi + 24], r12
    mov     [rdi + 32], r13
    mov     [rdi + 40], r14
    mov     [rdi + 48], r15
    stmxcsr [rdi + 56]
    fnstcw  WORD PTR [rdi + 60]

    /* --- Restore the next context from next (rsi). --- */
    mov     rsp, [rsi +  0]       /* rsp = next->rsp */
    mov     rbp, [rsi +  8]
    mov     rbx, [rsi + 16]
    mov     r12, [rsi + 24]
    mov     r13, [rsi + 32]
    mov     r14, [rsi + 40]
    mov     r15, [rsi + 48]
    ldmxcsr [rsi + 56]
    fldcw   WORD PTR [rsi + 60]

    ret                            /* Jump to the return address at next->rsp. */
    /* Record the byte length of st_ctx_swap for the object-file symbol table.
     * `.` is the current address after `ret`, so `.-st_ctx_swap` is the
     * distance from the function's first instruction to this point. It does
     * not emit a machine instruction and has no effect on the context switch. */
    .size  st_ctx_swap, .-st_ctx_swap

/* Mark the stack as non-executable. Without this note, the linker may warn
 * that assembly code has an executable stack. */
    .section .note.GNU-stack,"",@progbits
