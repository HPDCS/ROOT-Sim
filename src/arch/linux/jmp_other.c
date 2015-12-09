#include "jmp.h"

void set_jmp_other(exec_context_t *env) {
	
	__asm__ __volatile__("fxsave %0\n"
			     :: "m" (&env->others));
}

void long_jmp_other(exec_context_t *env) {
	__asm__ __volatile__("fxrstor %0\n"
			     :: "m" (&env->others));
}
