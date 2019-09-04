
#define TYPE(x) .type x, @function
#define SIZE(x) .size x, . - x
#define HIDDEN(x) .hidden x

.text
.align

.globl _ITM_beginTransaction
.globl GTM_longjmp

_ITM_beginTransaction:
	.cfi_startproc
	/* rax points CFA (return address + 8bytes)   */
	leaq 8(%rsp), %rax
	/* make room for 9 8-byte registers           */
	subq $72, %rsp
	/* save codeProperties into the stack         */
	movl %edi, 8(%rsp)
	/* checkpoint registers       STACK           */
	/*                       return address       */
	movq %rax, -64(%rax) /*        r15            */
	movq %rbx, -56(%rax) /*        r14            */
	movq %rbp, -48(%rax) /*        r13            */
	movq %r12, -40(%rax) /*        r12            */   
	movq %r13, -32(%rax) /*        rbp            */
	movq %r14, -24(%rax) /*        rbx            */
	movq %r15, -16(%rax) /*        rax            */
	/*                             edi            */
	/*  2nd argument of ITM_beginTransaction      */
	/*  points to jumpbuf                         */
	leaq -64(%rax), %rsi
	call GTM_beginTransaction@plt
	movl 8(%rsp), %edi /* restore codeProperties  */
	addq $72, %rsp /* free stack                  */
	ret /* return to application code             */
	.cfi_endproc

TYPE(_ITM_beginTransaction)
SIZE(_ITM_beginTransaction)

GTM_longjmp:
	.cfi_startproc
	pushq %rsi
	pushq %rdi
	call GTM_rollback@plt
	popq %rdi
	popq %rsi
	/* restore checkpointed registers  */
	movq   (%rsi), %rcx
	movq  8(%rsi), %rbx
	movq 16(%rsi), %rbp
	movq 24(%rsi), %r12
	movq 32(%rsi), %r13
	movq 40(%rsi), %r14
	movq 48(%rsi), %r15
	movl %edi, %eax /* 1st longjmp argument */
	/* ajusts stack frame as if _ITM_beginTransaction was not called */
	.cfi_def_cfa %rsi, 0
	.cfi_offset %rip, 56
  .cfi_register %rsp, %rcx
	movq	%rcx, %rsp /* rsp points to rax on the stack */
	jmpq *56(%rsi) /* jump to the call _ITM_beginTransaction */ 
	.cfi_endproc

TYPE(GTM_longjmp)
HIDDEN(GTM_longjmp)
SIZE(GTM_longjmp)

.section .note.GNU-stack, "", @progbits
