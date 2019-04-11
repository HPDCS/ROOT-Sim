#include <stdio.h>
#include <stddef.h>

int variable;

//-mgeneral-regs-only
//__attribute__((no_caller_saved_registers))
__attribute__((used))
void __write_mem(unsigned char *addr, size_t size) {
	variable = 1;
	//printf("Scrittura di %d byte su %p\n", size, addr);
	//puts("Scrittura\n");
	//fflush(stdout);
}

//__attribute__((no_caller_saved_registers))
__attribute__((used))
void __read_mem(unsigned char *addr, size_t size) {
	variable = 2;
//	printf("Lettura di %d byte da %p\n", size, addr);
	//~ puts("Lettura\n");
	//~ fflush(stdout);
}
