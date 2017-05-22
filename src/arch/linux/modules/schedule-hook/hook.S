/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file hook.S
* @brief This hook is dynamically attached to Linux Kernel's finish_task_switch
*        to call a module-provided function at the end of the reschedule operation.
* @author Alessandro Pellegrini
* @date February, 2016
*/

.file "preempt_callback.S"
.extern the_hook
.extern count
.extern audit_counter


.text
.globl schedule_hook
.type schedule_hook, @function
.globl schedule_hook_2
.type schedule_hook_2, @function
.globl schedule_hook_end
.type schedule_hook_end, @function
.globl schedule_hook_patch_point
.type schedule_hook_patch_point, @function
.globl schedule_hook_patch_point_2
.type schedule_hook_patch_point_2, @function


schedule_hook:
	pushq %rax
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	# jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	jz restore
	callq *%rax
restore:
	popq %rax	
schedule_hook_patch_point:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_2:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_2:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_3:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_3:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_4:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_4:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_5:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_5:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_6:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_6:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_7:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_7:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_8:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_8:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_9:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_9:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_10:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_10:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_11:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_11:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_12:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_12:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_13:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_13:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_14:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_14:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_15:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_15:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_16:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_16:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_17:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_17:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_18:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_18:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_19:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_19:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_20:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point_20:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_end:
	nop
