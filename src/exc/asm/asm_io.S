#define STATE_SIZE 65536
.file "asm_io.S"
.text


.align 8
.globl asm_load
.type asm_load, @function

asm_load:
	test %rsi, %rsi
	jnz clean
	add $STATE_SIZE, %rdi
clean:
	mov %gs:(%rdi), %rax
	retq


.align 8
.globl asm_store
.type asm_store, @function

asm_store:
	add $STATE_SIZE, %rdi
	mov %rsi, %gs:(%rdi)
	retq
