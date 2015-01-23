/**
 * Author: Alessandro Pellegrini <pellegrini@dis.uniroma1.it>
 *
 * This header is an attempt to create a debugging support to trace bugs which
 * are not handled by gdb (due to stack corruption, or similar issues).
 *
 * It tries to be as portable as possibile, but nevertheless relies on GCC intrinsic
 * definitions, machine-dependent structures and POSIX signal/context headers.
 *
 * USAGE: Just include this header where main() is defined, and place INIT_BACKTRACE()
 * right at the beginning of it.
 *
 * This is not compatible with other signal hanlders, so please disable them when
 * using this.
 *
 * To allow printing of function names, your code must be compiled using the -rdynamic
 * option. If this option is not specified, only hex addresses will be printed.
 * 
 * This header needs addr2line (binutils) to retrieve the file:line string given an address.
 */

#pragma once
#ifndef BACKTRACE_H
#define BACKTRACE_H


#if defined(OS_LINUX)


#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>

extern char *program_name;


/* get REG_EIP / REG_RIP from ucontext.h */
#include <ucontext.h>

	#ifndef EIP
//	#define EIP     REG_EIP
	#define EIP     14
	#endif

	#if (defined (__x86_64__))
		#ifndef REG_RIP
//		#define REG_RIP REG_INDEX(rip) /* seems to be 16 */
		#define REG_RIP 16
		#endif
	#endif


/* This is the initialization procedure */
#define INIT_BACKTRACE() do {\
				struct sigaction sa;\
				sa.sa_sigaction = (void *)bt_sighandler;\
				sigemptyset (&sa.sa_mask);\
				sa.sa_flags = SA_RESTART | SA_SIGINFO;\
				sigaction(SIGSEGV, &sa, NULL);\
				sigaction(SIGABRT, &sa, NULL);\
				sigaction(SIGBUS, &sa, NULL);\
				sigaction(SIGILL, &sa, NULL);\
				sigaction(SIGFPE, &sa, NULL);\
				sigaction(SIGUSR1, &sa, NULL);\
				sigaction(SIGUSR2, &sa, NULL);\
			} while(0)



/* Known signals */
typedef struct { char name[10]; int id; char description[40]; } signal_def;
signal_def signal_data[] =
{
	{ "SIGHUP", SIGHUP, "Hangup (POSIX)" },
	{ "SIGINT", SIGINT, "Interrupt (ANSI)" },
	{ "SIGQUIT", SIGQUIT, "Quit (POSIX)" },
	{ "SIGILL", SIGILL, "Illegal instruction (ANSI)" },
	{ "SIGTRAP", SIGTRAP, "Trace trap (POSIX)" },
	{ "SIGABRT", SIGABRT, "Abort (ANSI)" },
	{ "SIGIOT", SIGIOT, "IOT trap (4.2 BSD)" },
	{ "SIGBUS", SIGBUS, "BUS error (4.2 BSD)" },
	{ "SIGFPE", SIGFPE, "Floating-point exception (ANSI)" },
	{ "SIGKILL", SIGKILL, "Kill, unblockable (POSIX)" },
	{ "SIGUSR1", SIGUSR1, "User-defined signal 1 (POSIX)" },
	{ "SIGSEGV", SIGSEGV, "Segmentation violation (ANSI)" },
	{ "SIGUSR2", SIGUSR2, "User-defined signal 2 (POSIX)" },
	{ "SIGPIPE", SIGPIPE, "Broken pipe (POSIX)" },
	{ "SIGALRM", SIGALRM, "Alarm clock (POSIX)" },
	{ "SIGTERM", SIGTERM, "Termination (ANSI)" },
	{ "SIGSTKFLT", SIGSTKFLT, "Stack fault" },
	{ "SIGCHLD", SIGCHLD, "Child status has changed (POSIX)" },
	{ "SIGCLD", SIGCLD, "Same as SIGCHLD (System V)" },
	{ "SIGCONT", SIGCONT, "Continue (POSIX)" },
	{ "SIGSTOP", SIGSTOP, "Stop, unblockable (POSIX)" },
	{ "SIGTSTP", SIGTSTP, "Keyboard stop (POSIX)" },
	{ "SIGTTIN", SIGTTIN, "Background read from tty (POSIX)" },
	{ "SIGTTOU", SIGTTOU, "Background write to tty (POSIX)" },
	{ "SIGURG", SIGURG, "Urgent condition on socket (4.2 BSD)" },
	{ "SIGXCPU", SIGXCPU, "CPU limit exceeded (4.2 BSD)" },
	{ "SIGXFSZ", SIGXFSZ, "File size limit exceeded (4.2 BSD)" },
	{ "SIGVTALRM", SIGVTALRM, "Virtual alarm clock (4.2 BSD)" },
	{ "SIGPROF", SIGPROF, "Profiling alarm clock (4.2 BSD)" },
	{ "SIGWINCH", SIGWINCH, "Window size change (4.3 BSD, Sun)" },
	{ "SIGIO", SIGIO, "I/O now possible (4.2 BSD)" },
	{ "SIGPOLL", SIGPOLL, "Pollable event occurred (System V)" },
	{ "SIGPWR", SIGPWR, "Power failure restart (System V)" },
	{ "SIGSYS", SIGSYS, "Bad system ca ll" },
};




static inline char *get_file_line(char *addr) {
	#define MAX_CHARS 255
	FILE *fp;
	char *in_buffer = rsalloc(MAX_CHARS);
	char cmd[MAX_CHARS];
	
	sprintf(cmd, "addr2line --exe %s %s", program_name, addr);
	
	fp = popen(cmd, "r");
	fgets(in_buffer, MAX_CHARS, fp);

	pclose(fp);
	
	return in_buffer;
	#undef MAX_CHARS
}


static inline char *extract_word_from_string(char *input_str, char start_word_delim, char end_word_delim) {
	
	char *c;
	char *start;
	
	c = input_str;
	while(*c != start_word_delim) {
		c++;
	}
	start = c + 1;
	
	while(*c != end_word_delim) {
		c++;
	}
	*c = '\0';
	
	return start;
}

/* Actual signal handler */

/*
From man getcontext:

The mcontext_t type is machine-dependent and opaque. 
The ucontext_t type is a structure that has at least the following fields:

typedef struct ucontext {
	struct ucontext *uc_link;
	sigset_t         uc_sigmask;
	stack_t          uc_stack;
	mcontext_t       uc_mcontext;
	...
} ucontext_t;
*/
void bt_sighandler(int sig, siginfo_t *info, void *opaque) {

	void *trace[16];
	char **messages = (char **)NULL;
	unsigned int i;
	unsigned int trace_size = 0;

	/* Print some information on the received signal, if available */
	signal_def *d = NULL;
	for (i = 0; i < sizeof(signal_data) / sizeof(signal_def); i++)
		if (sig == signal_data[i].id)
			{ d = &signal_data[i]; break; }
	if (d) {
		printf("Got signal 0x%02X (%s): %s\n", sig, signal_data[i].name, signal_data[i].description);
	} else {
		printf("Got signal 0x%02X\n", sig);
	}


	/* Determine which instruction caused the signal to be raised */
	void *pnt = NULL;
	#if defined(__x86_64__)
		ucontext_t* uc = (ucontext_t*)opaque;
		pnt = (void*) uc->uc_mcontext.gregs[REG_RIP] ;
	#elif defined(__hppa__)
		ucontext_t* uc = (ucontext_t*)opaque;
		pnt = (void*) uc->uc_mcontext.sc_iaoq[0] & ~0Ã—3UL ;
	#elif (defined (__ppc__)) || (defined (__powerpc__))
		ucontext_t* uc = (ucontext_t*)opaque;
		pnt = (void*) uc->uc_mcontext.regs->nip ;
	#elif defined(__sparc__)
	struct sigcontext* sc = (struct sigcontext*) opaque;
		#if __WORDSIZE == 64
			pnt = (void*) scp->sigc_regs.tpc ;
		#else
			pnt = (void*) scp->si_regs.pc ;
		#endif
	#elif defined(__i386__)
		ucontext_t* uc = (ucontext_t*) opaque;
		pnt = (void*) uc->uc_mcontext.gregs[REG_EIP] ;
	#endif
	/* potentially correct for other archs:
	 * alpha: ucp->m_context.sc_pc
	 * arm: ucp->m_context.ctx.arm_pc
	 * ia64: ucp->m_context.sc_ip & ~0Ã—3UL
	 * mips: ucp->m_context.sc_pc
	 * s390: ucp->m_context.sregs->regs.psw.addr
	 */

	if (sig == SIGSEGV)
		printf("Faulty address is %p, called from %p\n", info->si_addr, pnt);

	/* The first two entries in the stack frame chain when you
	 * get into the signal handler contain, respectively, a
	 * return address inside your signal handler and one inside
	 * sigaction() in libc. The stack frame of the last function
	 * called before the signal (which, in case of fault signals,
	 * also is the one that supposedly caused the problem) is lost.
	 */

	/* the third parameter to the signal handler points to an
	 * ucontext_t structure that contains the values of the CPU
	 * registers when the signal was raised.
	 */

	trace_size = backtrace(trace, 16);
	/* overwrite sigaction with caller's address */
	trace[1] = pnt;

	messages = backtrace_symbols(trace, trace_size);
	/* skip first stack frame (points here) and second (was overwritten)*/
	printf("[BACKTRACE] Execution path:\n");
	for (i = 2; i < trace_size; ++i) {
		printf("#%d %s] at %s", i-2, messages[i], get_file_line(extract_word_from_string(messages[i], '[', ']')));
	}

	exit(0);
}


#else /* OS_LINUX */

#define INIT_BACKTRACE()	{}
#warning Backtrace not available on this platform, option is disabled

#endif /* OS_LINUX */


#endif /* BACKTRACE_H */
