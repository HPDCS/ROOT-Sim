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
.globl schedule_hook_end
.type schedule_hook_end, @function
.globl schedule_hook_patch_point
.type schedule_hook_patch_point, @function
schedule_hook:
	lock incl count(%rip)
	
	# This is here just for audit and could be commented out
	incl audit_counter(%rip)

	movq the_hook(%rip), %rax
	test %rax, %rax
	jz .+4	# location counter points to jz. jz is 2 bytes, callq is 2 bytes.
	callq *%rax
schedule_hook_patch_point:
	nop	# Five nops to leave place to dynamically patch this function
	nop
	nop
	nop
	nop
	lock decl count(%rip)
	ret
schedule_hook_end:
	nop
