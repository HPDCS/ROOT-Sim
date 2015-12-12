#include <stdio.h>
#include "jmp.h"

exec_context_t env;


void print_registers(void) {
	register long long rax asm ("rax");
	register long long rdx asm ("rdx");
	register long long rcx asm ("rcx");
	register long long rbx asm ("rbx");
	register long long rsi asm ("rsi");
	register long long rdi asm ("rdi");
	register long long r8 asm ("r8");
	register long long r9 asm ("r9");
	register long long r10 asm ("r10");
	register long long r11 asm ("r11");
	register long long r12 asm ("r12");
	register long long r13 asm ("r13");
	register long long r14 asm ("r14");
	register long long r15 asm ("r15");

	printf("%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu \n",
		rax, rdx, rcx, rbx, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15);
}

void set_registers_1(void) {
	register long long rax asm ("rax") = 1L;
	register long long rdx asm ("rdx") = 1L;
	register long long rcx asm ("rcx") = 1L;
	register long long rbx asm ("rbx") = 1L;
	register long long rsi asm ("rsi") = 1L;
	register long long rdi asm ("rdi") = 1L;
	register long long r8 asm ("r8") = 1L;
	register long long r9 asm ("r9") = 1L;
	register long long r10 asm ("r10") = 1L;
	register long long r11 asm ("r11") = 1L;
	register long long r12 asm ("r12") = 1L;
	register long long r13 asm ("r13") = 1L;
	register long long r14 asm ("r14") = 1L;
	register long long r15 asm ("r15") = 1L;
}


void set_registers_2(void) {
	register long long rax asm ("rax") = 2L;
	register long long rdx asm ("rdx") = 2L;
	register long long rcx asm ("rcx") = 2L;
	register long long rbx asm ("rbx") = 2L;
	register long long rsi asm ("rsi") = 2L;
	register long long rdi asm ("rdi") = 2L;
	register long long r8 asm ("r8") = 2L;
	register long long r9 asm ("r9") = 2L;
	register long long r10 asm ("r10") = 2L;
	register long long r11 asm ("r11") = 2L;
	register long long r12 asm ("r12") = 2L;
	register long long r13 asm ("r13") = 2L;
	register long long r14 asm ("r14") = 2L;
	register long long r15 asm ("r15") = 2L;
}

int main(void) {
	int flag = 0;

	set_registers_1();
	print_registers();

	set_jmp(&env);

	print_registers();

	if(!flag) {
		set_registers_2();
		print_registers();
		flag = 1;
		long_jmp(&env, 1);
	}

	return 0;
}
