
#define TYPE(x) .type x, @function
#define SIZE(x) .size x, . - x
#define HIDDEN(x) .hidden x

.text
.align

.globl libitm_setjmp
.globl libitm_longjmp

libitm_setjmp:
	.cfi_startproc
	/* rax points CFA (return address + 8bytes)   */
	leaq 8(%rsp), %rax
	/* checkpoint              registers         */
	movq %rax,   (%rdi) /*        cfa            */
	movq %rbx,  8(%rdi) /*        rbx            */
	movq %rbp, 16(%rdi) /*        rbp            */
	movq %r12, 24(%rdi) /*        r12            */   
	movq %r13, 32(%rdi) /*        r13            */
	movq %r14, 40(%rdi) /*        r14            */
	movq %r15, 48(%rdi) /*        r15            */
  movq (%rsp), %rax 
  movq %rax, 56(%rdi) /*        rip            */
  xorq %rax, %rax
	ret
	.cfi_endproc

TYPE(libitm_setjmp)
SIZE(libitm_setjmp)

libitm_longjmp:
	.cfi_startproc
	/* restore                 registers       */
	movq   (%rdi), %rcx /*        cfa          */
	movq  8(%rdi), %rbx /*        rbx          */
	movq 16(%rdi), %rbp /*        rbp          */
	movq 24(%rdi), %r12 /*        r12          */
	movq 32(%rdi), %r13 /*        r13          */
	movq 40(%rdi), %r14 /*        r14          */
	movq 48(%rdi), %r15 /*        r15          */
	.cfi_def_cfa %rdi, 0
  .cfi_register %rsp, %rcx
	movq	%rcx, %rsp /* rsp points to cfa */
	movl %esi, %eax /* 2nd argument of longjmp */
	jmpq *56(%rdi)
	.cfi_endproc

TYPE(libitm_longjmp)
SIZE(libitm_longjmp)

.section .note.GNU-stack, "", @progbits
