#include <stdbool.h>

#include "gcc-common.h"
#include "calls.h"

#include "print-rtl.h"

int plugin_is_GPL_compatible;

static int track_frame_size = -1;
static const char track_function1[] = "__write_mem";
static const char track_function2[] = "__read_mem";

/*
 * Mark these global variables (roots) for gcc garbage collector since
 * they point to the garbage-collected memory.
 */
static GTY(()) tree track_function_decl;

static struct plugin_info memtrace_plugin_info = {
	.version = "201707101337",
	.help = "track-min-size=nn\ttrack stack for functions with a stack frame size >= nn bytes\n"
		"disable\t\tdo not activate the plugin\n"
};

static void memtrace_add_track_stack(gimple_stmt_iterator *gsi, bool after)
{
	return;

	gimple stmt;
	gcall *dirty_mem;
	cgraph_node_ptr node;
	int frequency;
	basic_block bb;

	/* Insert call to void dirty_mem(void) */
	stmt = gimple_build_call(track_function_decl, 0);
	dirty_mem = as_a_gcall(stmt);
	if (after) {
		gsi_insert_after(gsi, dirty_mem, GSI_CONTINUE_LINKING);
	} else {
		gsi_insert_before(gsi, dirty_mem, GSI_SAME_STMT);
	}

	/* Update the cgraph */
	bb = gimple_bb(dirty_mem);
	node = cgraph_get_create_node(track_function_decl);
	gcc_assert(node);
	frequency = compute_call_stmt_bb_frequency(current_function_decl, bb);
	cgraph_create_edge(cgraph_get_node(current_function_decl), node,
			dirty_mem, bb->count, frequency);
}

static bool is_alloca(gimple stmt)
{
	if (gimple_call_builtin_p(stmt, BUILT_IN_ALLOCA))
		return true;

#if BUILDING_GCC_VERSION >= 4007
	if (gimple_call_builtin_p(stmt, BUILT_IN_ALLOCA_WITH_ALIGN))
		return true;
#endif

	return false;
}

/*
 * Work with the GIMPLE representation of the code. Insert the
 * dirty_mem() call after alloca() and into the beginning
 * of the function if it is not instrumented.
 */
static unsigned int memtrace_instrument_execute(void)
{
	basic_block bb, entry_bb;
	gimple_stmt_iterator gsi;

	return 0;

	printf("Chiamato GIMPLE\n");
	fflush(stdout);

	/*
	 * ENTRY_BLOCK_PTR is a basic block which represents possible entry
	 * point of a function. This block does not contain any code and
	 * has a CFG edge to its successor.
	 */
	gcc_assert(single_succ_p(ENTRY_BLOCK_PTR_FOR_FN(cfun)));
	entry_bb = single_succ(ENTRY_BLOCK_PTR_FOR_FN(cfun));

	/*
	 * Loop through the GIMPLE statements in each of cfun basic blocks.
	 * cfun is a global variable which represents the function that is
	 * currently processed.
	 */
	FOR_EACH_BB_FN(bb, cfun) {
		for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
			gimple stmt;

			stmt = gsi_stmt(gsi);

			printf("%d: ", stmt->code);
			print_gimple_stmt(stdout, stmt, 0, 0);

			if(stmt->code != 6 && stmt->code != 7) // TODO: find the corresponding macro
				continue;

			/* Insert dirty_mem() call after alloca() */
			//memtrace_add_track_stack(&gsi, false);
		}
	}

	return 0;
}

static bool large_stack_frame(void)
{
#if BUILDING_GCC_VERSION >= 8000
	return maybe_ge(get_frame_size(), track_frame_size);
#else
	return (get_frame_size() >= track_frame_size);
#endif
}

static void put_instruction_cmov(rtx insn, rtx condition, rtx then_expression, rtx else_expression, bool write_1, bool write_2)
{

	rtx parm1_then, parm2_then, parm1_else, parm2_else, call_1, call_2, push1, push2, pop1, pop2, if_then_else, label_x, label_x1, jmp_x1;
	const char *fn1 = write_1 ? "__write_mem" : "__read_mem";
	const char *fn2 = write_2 ? "__write_mem" : "__read_mem";
	if(then_expression != NULL){
		if(GET_CODE(XEXP(then_expression, 0)) == PRE_DEC || GET_CODE(XEXP(then_expression, 0)) == POST_INC)
			return;
	}
	if (else_expression != NULL){
		if(GET_CODE(XEXP(else_expression, 0)) == PRE_DEC || GET_CODE(XEXP(else_expression, 0)) == POST_INC)
			return;
	}

	printf("++++++++\n");
	if (then_expression != NULL)
		print_rtl_single(stdout, then_expression);
	if (else_expression != NULL)
		print_rtl_single(stdout, else_expression);
	printf("++++++++\n");

	label_x = gen_label_rtx();
	CODE_LABEL_NUMBER(label_x) = 200;
	label_x1 = gen_label_rtx();
	CODE_LABEL_NUMBER(label_x1) = 201;

	rtx ref_label_x =  gen_rtx_LABEL_REF(VOIDmode, label_x);
	rtx ref_label_x1 =  gen_rtx_LABEL_REF(VOIDmode, label_x1);
	rtx iff = gen_rtx_IF_THEN_ELSE(VOIDmode, condition, ref_label_x, pc_rtx);
	if_then_else = gen_rtx_SET(pc_rtx, iff);
	JUMP_LABEL(if_then_else) = label_x;
	LABEL_NUSES (label_x) += 1;

	
	jmp_x1 = gen_rtx_SET(pc_rtx, gen_rtx_LABEL_REF(VOIDmode, label_x1));
	JUMP_LABEL(jmp_x1) = label_x1;
	LABEL_NUSES (label_x1) += 1;
	


	push1 = rtx_alloc(SET);
	XEXP(push1, 0) = gen_rtx_MEM(DImode,
				gen_rtx_PRE_DEC(DImode,	gen_rtx_REG(DImode, 7))
			);
	XEXP(push1, 1) = gen_rtx_REG(DImode, 5);

	push2 = rtx_alloc(SET);
	XEXP(push2, 0) = gen_rtx_MEM(DImode,
				gen_rtx_PRE_DEC(DImode,	gen_rtx_REG(DImode, 7))
			);
	XEXP(push2, 1) = gen_rtx_REG(DImode, 4);

	pop1 = rtx_alloc(SET);
	XEXP(pop1, 0) = gen_rtx_REG(DImode, 5);
	XEXP(pop1, 1) = gen_rtx_MEM(DImode,
				gen_rtx_POST_INC(DImode, gen_rtx_REG(DImode, 7))
			);
	
	pop2 = rtx_alloc(SET);
	XEXP(pop2, 0) = gen_rtx_REG(DImode, 4);
	XEXP(pop2, 1) = gen_rtx_MEM(DImode,
				gen_rtx_POST_INC(DImode, gen_rtx_REG(DImode, 7))
			);
	if (then_expression != NULL){
		parm1_then = rtx_alloc(SET);
		XEXP(parm1_then, 0) = gen_rtx_REG(SImode, 5);
		XEXP(parm1_then, 1) = XEXP(then_expression, 0);

		parm2_then = rtx_alloc(SET);
		XEXP(parm2_then, 0) = gen_rtx_REG(DImode, 4);
		XEXP(parm2_then, 1) = gen_rtx_CONST_INT(DImode, MEM_SIZE(then_expression).to_constant()); //before it was SImode, why?

		call_1 = gen_rtx_CALL(VOIDmode,
				gen_rtx_MEM(QImode, gen_rtx_SYMBOL_REF(DImode, fn1)),
				gen_rtx_CONST_INT(VOIDmode, 0)
		);
	}
	else {
		parm1_then = NULL;
		parm2_then = NULL;
	}	
	
	if (else_expression != NULL){
		parm1_else = rtx_alloc(SET);
		XEXP(parm1_else, 0) = gen_rtx_REG(SImode, 5);
		XEXP(parm1_else, 1) = XEXP(else_expression, 0);

		parm2_else = rtx_alloc(SET);
		XEXP(parm2_else, 0) = gen_rtx_REG(DImode, 4);
		XEXP(parm2_else, 1) = gen_rtx_CONST_INT(DImode, MEM_SIZE(else_expression).to_constant()); //before it was SImode, why?

		call_2 = gen_rtx_CALL(VOIDmode,
				gen_rtx_MEM(QImode, gen_rtx_SYMBOL_REF(DImode, fn2)),
				gen_rtx_CONST_INT(VOIDmode, 0)
		);
	}
	else {
		parm1_else = NULL;
		parm2_else = NULL;
	}


	/*
	Or it is possible to write an assembly-like code like that

	push rdi
	push rsi
	(if_then_else (cond)
		jmp L_(x)
		pc))
	
	/*that code only if (else) is a MEM

	mov rdi, (else)
	mov rsi, sizeof(else)
	call __write/read_mem

	/* up to here

	jmp L_(x+1)

	/*that code only if (then) is a MEM

	code_label(L_(x))
	mov rdi, (then)
	mov rsi, sizeof(then)
	call __write/read_mem
	
	/*up to here

	code_label(L_(x+1))
	((((code_label(L_(x)))))) only in case (then) it is not a MEM
	pop rsi
	pop rdi
	*/


	emit_insn_before(push1, insn);
	emit_insn_before(push2, insn);
	emit_jump_insn_before(if_then_else, insn);
	
	if (else_expression != NULL){
		emit_insn_before(parm1_else, insn);
		emit_insn_before(parm2_else, insn);
		emit_insn_before(call_2, insn);
	}

	emit_jump_insn_before(jmp_x1, insn);

	if (then_expression != NULL){
		emit_insn_before(label_x, insn);
		emit_insn_before(parm1_then, insn);
		emit_insn_before(parm2_then, insn);
		emit_insn_before(call_1, insn);
	}
	

	emit_insn_before(label_x1, insn);

	if (then_expression == NULL){
		emit_insn_before(label_x, insn);
	}

	emit_insn_before(pop2, insn);
	emit_insn_before(pop1, insn);


	printf("********\n");
	print_rtl_single(stdout, push1);
	print_rtl_single(stdout, push2);
	print_rtl_single(stdout, if_then_else);
	if (else_expression != NULL){
		print_rtl_single(stdout, parm1_else);
		print_rtl_single(stdout, parm2_else);
		print_rtl_single(stdout, call_2);
	}
	print_rtl_single(stdout, jmp_x1);
	if (then_expression != NULL){
		print_rtl_single(stdout, label_x);
		print_rtl_single(stdout, parm1_then);
		print_rtl_single(stdout, parm2_then);
		print_rtl_single(stdout, call_1);

	}
	print_rtl_single(stdout, label_x1);
	if (then_expression == NULL)
		print_rtl_single(stdout, label_x);
		
	print_rtl_single(stdout, pop2);
	print_rtl_single(stdout, pop1);
	printf("********\n");

	return;

}

static void put_instruction(rtx insn, rtx operand, bool write)
{

	rtx parm1, parm2, call, push1, push2, pop1, pop2;
	const char *fn = write ? "__write_mem" : "__read_mem";

	if(GET_CODE(XEXP(operand, 0)) == PRE_DEC || GET_CODE(XEXP(operand, 0)) == POST_INC)
		return;

	printf("++++++++\n");
	print_rtl_single(stdout, operand);
	printf("++++++++\n");

	push1 = rtx_alloc(SET);
	XEXP(push1, 0) = gen_rtx_MEM(DImode,
				gen_rtx_PRE_DEC(DImode,	gen_rtx_REG(DImode, 7))
			);
	XEXP(push1, 1) = gen_rtx_REG(DImode, 5);

	push2 = rtx_alloc(SET);
	XEXP(push2, 0) = gen_rtx_MEM(DImode,
				gen_rtx_PRE_DEC(DImode,	gen_rtx_REG(DImode, 7))
			);
	XEXP(push2, 1) = gen_rtx_REG(DImode, 4);

	pop1 = rtx_alloc(SET);
	XEXP(pop1, 0) = gen_rtx_REG(DImode, 5);
	XEXP(pop1, 1) = gen_rtx_MEM(DImode,
				gen_rtx_POST_INC(DImode, gen_rtx_REG(DImode, 7))
			);
	
	pop2 = rtx_alloc(SET);
	XEXP(pop2, 0) = gen_rtx_REG(DImode, 4);
	XEXP(pop2, 1) = gen_rtx_MEM(DImode,
				gen_rtx_POST_INC(DImode, gen_rtx_REG(DImode, 7))
			);
	
	parm1 = rtx_alloc(SET);
	XEXP(parm1, 0) = gen_rtx_REG(DImode, 5);
	XEXP(parm1, 1) = XEXP(operand, 0);

	parm2 = rtx_alloc(SET);
	XEXP(parm2, 0) = gen_rtx_REG(DImode, 4);
	XEXP(parm2, 1) = gen_rtx_CONST_INT(DImode, MEM_SIZE(operand).to_constant()); //before it was SImode, why?


	call = gen_rtx_CALL(VOIDmode,
				gen_rtx_MEM(QImode, gen_rtx_SYMBOL_REF(DImode, fn)),
				gen_rtx_CONST_INT(VOIDmode, 0)
		);

	emit_insn_before(push1, insn);
	emit_insn_before(push2, insn);
	emit_insn_before(parm1, insn);
	emit_insn_before(parm2, insn);
	emit_insn_before(call, insn);
	emit_insn_before(pop2, insn);
	emit_insn_before(pop1, insn);

	printf("********\n");
	print_rtl_single(stdout, push1);
	print_rtl_single(stdout, push2);
	print_rtl_single(stdout, parm1);
	print_rtl_single(stdout, parm2);
	print_rtl_single(stdout, call);
	print_rtl_single(stdout, pop2);
	print_rtl_single(stdout, pop1);
	printf("********\n");

	return;
}


static unsigned int memtrace_cleanup_execute(void)
{
	rtx_insn *insn, *next;
	int code;

	for (insn = get_insns(); insn; insn = next) {
		printf("----------------------------\n");
		rtx body;

		body = PATTERN(insn);
		if(!(NOTE_P(insn) || BARRIER_P(insn)))
			print_rtl_single(stdout, body);

 		next = NEXT_INSN(insn);

 		/* Check the expression code of the insn */
		if (!INSN_P(insn) || BARRIER_P(insn) || NOTE_P(insn) || CALL_P(insn))
			continue;
		
		/* CMOVE %eax %ebx
		(set (reg:SI 3 bx [orig:90 _4 ] [90])
    		(if_then_else:SI (ne (reg:CCZ 17 flags)
            		(const_int 0 [0]))
        		(reg:SI 3 bx [orig:90 _4 ] [90])
        		(reg:SI 0 ax [118])))
		*/
		if(GET_CODE(body) == SET){
			rtx first = XEXP(body, 0);
			//print_rtl_single(stdout, first);
			if (GET_CODE(first) == MEM){
				// dest operand
				printf("dst: MEMORY ACCESS FOUND!\n");

				put_instruction(insn, first, true);

			}
			else if (GET_CODE(first) == IF_THEN_ELSE){
				rtx then_expression = XEXP(first, 1);
				rtx else_expression = XEXP(first, 2);
				if (GET_CODE(then_expression) == MEM && GET_CODE(else_expression) == MEM){
					printf("dst: MEMORY ACCESS FOUND!\n");
					// insert instructions
					rtx condition = XEXP(first, 0);
					put_instruction_cmov(insn, condition, then_expression, else_expression, true, true);
				}
				else if (GET_CODE(then_expression) == MEM){
					printf("dst: MEMORY ACCESS FOUND!\n");
					// insert instructions
					rtx condition = XEXP(first, 0);
					put_instruction_cmov(insn, condition, then_expression, NULL, true, false);
				}
				else if (GET_CODE(else_expression) == MEM){
					printf("dst: MEMORY ACCESS FOUND!\n");
					// insert instructions
					rtx condition = XEXP(first, 0);
					put_instruction_cmov(insn, condition, NULL, else_expression, false, true);
				}
			}
			rtx second = XEXP(body, 1);
			if (GET_CODE(second) == IF_THEN_ELSE){
				rtx try_then_expression = XEXP(second, 1);
				rtx try_else_expression = XEXP(second, 2);
				//rtx then_expression = (GET_CODE(try_then_expression) != REG)? try_then_expression : gen_rtx_MEM(DImode, try_then_expression);
				rtx then_expression = try_then_expression;
				//rtx else_expression = (GET_CODE(try_else_expression) != REG)? try_else_expression : gen_rtx_MEM(DImode, try_else_expression);
				rtx else_expression = try_else_expression;
				//print_rtl_single(stdout, else_expression);
				if (GET_CODE(then_expression) == MEM && GET_CODE(else_expression) == MEM){
					printf("src: MEMORY ACCESS FOUND!\n");
					// insert instructions
					rtx condition = XEXP(second, 0);
					print_rtl_single(stdout, condition);
					put_instruction_cmov(insn, condition, then_expression, else_expression, false, false);
				}
				else if (GET_CODE(then_expression) == MEM){
					printf("src: MEMORY ACCESS FOUND!\n");
					// insert instructions
					put_instruction_cmov(insn, XEXP(second, 0), then_expression, NULL, false, false);
				}
				else if (GET_CODE(else_expression) == MEM){
					printf("src: MEMORY ACCESS FOUND!\n");
					put_instruction_cmov(insn, XEXP(second, 0), NULL, else_expression, false, false);
				}
			}
			else if (GET_CODE(second) == MEM){
				// src operand
				printf("src: MEMORY ACCESS FOUND!\n");

				put_instruction(insn, second, false);

			}
		}
		else if (GET_CODE(body) == PARALLEL){
			int i;
			for (i = 0; i < XVECLEN(body, 0); i++){
				rtx expression = XVECEXP(body, 0, i);
				if (GET_CODE(expression) == SET){
					rtx first, second;
					first = XEXP(expression, 0);
					second = XEXP(expression, 1);
					if (GET_CODE(first) == MEM){
						printf("dst: MEMORY ACCESS FOUND:\n");
						print_rtl_single(stdout, XEXP(first, 0));
						put_instruction(insn, first, true);
					} 
					if (GET_CODE(second) == MEM){
						printf("src: MEMORY ACCESS FOUND:\n");
						print_rtl_single(stdout, XEXP(second, 0));
						put_instruction(insn, second, false);
					} 
				}
			}
		}
	}

	printf("END OF PASS!\n");
 	return 0;
}

static bool memtrace_gate(void)
{
	tree section;

	printf("Chiamato GATE\n");
	fflush(stdout);

	return true;

	section = lookup_attribute("section", DECL_ATTRIBUTES(current_function_decl));
	if (section && TREE_VALUE(section)) {
		section = TREE_VALUE(TREE_VALUE(section));

		if (!strncmp(TREE_STRING_POINTER(section), ".init.text", 10))
			return false;
		if (!strncmp(TREE_STRING_POINTER(section), ".devinit.text", 13))
			return false;
		if (!strncmp(TREE_STRING_POINTER(section), ".cpuinit.text", 13))
			return false;
		if (!strncmp(TREE_STRING_POINTER(section), ".meminit.text", 13))
			return false;
	}

	return track_frame_size >= 0;
}

/* Build the function declaration for dirty_mem() */
static void memtrace_start_unit(void *gcc_data __unused,
				 void *user_data __unused)
{
	tree fntype;

	/* void dirty_mem(void) */
	fntype = build_function_type_list(void_type_node, NULL_TREE);
	track_function_decl = build_fn_decl(track_function1, fntype);
	DECL_ASSEMBLER_NAME(track_function_decl); /* for LTO */
	TREE_PUBLIC(track_function_decl) = 1;
	TREE_USED(track_function_decl) = 1;
	DECL_EXTERNAL(track_function_decl) = 1;
	DECL_ARTIFICIAL(track_function_decl) = 1;
	DECL_PRESERVE_P(track_function_decl) = 1;
}

/*
 * Pass gate function is a predicate function that gets executed before the
 * corresponding pass. If the return value is 'true' the pass gets executed,
 * otherwise, it is skipped.
 */
static bool memtrace_instrument_gate(void)
{
	return memtrace_gate();
}

#define PASS_NAME memtrace_instrument
#define PROPERTIES_REQUIRED PROP_gimple_leh | PROP_cfg
#define TODO_FLAGS_START TODO_verify_ssa | TODO_verify_flow | TODO_verify_stmts
#define TODO_FLAGS_FINISH TODO_verify_ssa | TODO_verify_stmts | TODO_dump_func \
			| TODO_update_ssa | TODO_rebuild_cgraph_edges
#include "gcc-generate-gimple-pass.h"


static bool memtrace_cleanup_gate(void)
{
	return memtrace_gate();
}

#define PASS_NAME memtrace_cleanup
#define TODO_FLAGS_FINISH TODO_dump_func
#include "gcc-generate-rtl-pass.h"


/*
 * Every gcc plugin exports a plugin_init() function that is called right
 * after the plugin is loaded. This function is responsible for registering
 * the plugin callbacks and doing other required initialization.
 */
__visible int plugin_init(struct plugin_name_args *plugin_info,
			  struct plugin_gcc_version *version)
{
	const char * const plugin_name = plugin_info->base_name;
	const int argc = plugin_info->argc;
	const struct plugin_argument * const argv = plugin_info->argv;
	int i = 0;

	/* Extra GGC root tables describing our GTY-ed data */
	static const struct ggc_root_tab gt_ggc_r_gt_memtrace[] = {
		{
			.base = &track_function_decl,
			.nelt = 1,
			.stride = sizeof(track_function_decl),
			.cb = &gt_ggc_mx_tree_node,
			.pchw = &gt_pch_nx_tree_node
		},
		LAST_GGC_ROOT_TAB
	};

	/*
	 * The memtrace_instrument pass should be executed before the
	 * "optimized" pass, which is the control flow graph cleanup that is
	 * performed just before expanding gcc trees to the RTL. In former
	 * versions of the plugin this new pass was inserted before the
	 * "tree_profile" pass, which is currently called "profile".
	 */
	PASS_INFO(memtrace_instrument, "optimized", 1, PASS_POS_INSERT_BEFORE);

	/*
	 * The stackleak_cleanup pass should be executed before the "*free_cfg"
	 * pass. It's the moment when the stack frame size is already final,
	 * function prologues and epilogues are generated, and the
	 * machine-dependent code transformations are not done.
	 */
	PASS_INFO(memtrace_cleanup, "*free_cfg", 1, PASS_POS_INSERT_AFTER);
	//PASS_INFO(memtrace_cleanup, "ira", 1, PASS_POS_INSERT_AFTER);

	if (!plugin_default_version_check(version, &gcc_version)) {
		error(G_("incompatible gcc/plugin versions"));
		return 1;
	}

	/* Parse the plugin arguments */
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i].key, "disable"))
			return 0;

		if (!strcmp(argv[i].key, "track-min-size")) {
			if (!argv[i].value) {
				error(G_("no value supplied for option '-fplugin-arg-%s-%s'"),
					plugin_name, argv[i].key);
				return 1;
			}

			track_frame_size = atoi(argv[i].value);
			if (track_frame_size < 0) {
				error(G_("invalid option argument '-fplugin-arg-%s-%s=%s'"),
					plugin_name, argv[i].key, argv[i].value);
				return 1;
			}
		} else {
			error(G_("unknown option '-fplugin-arg-%s-%s'"),
					plugin_name, argv[i].key);
			return 1;
		}
	}

	/* Give the information about the plugin */
	register_callback(plugin_name, PLUGIN_INFO, NULL,
						&memtrace_plugin_info);

	/* Register to be called before processing a translation unit */
	register_callback(plugin_name, PLUGIN_START_UNIT,
					&memtrace_start_unit, NULL);

	/* Register an extra GCC garbage collector (GGC) root table */
	register_callback(plugin_name, PLUGIN_REGISTER_GGC_ROOTS, NULL,
					(void *)&gt_ggc_r_gt_memtrace);

	/*
	 * Hook into the Pass Manager to register new gcc passes.
	 *
	 * The stack frame size info is available only at the last RTL pass,
	 * when it's too late to insert complex code like a function call.
	 * So we register two gcc passes to instrument every function at first
	 * and remove the unneeded instrumentation later.
	 */
	register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
					&memtrace_instrument_pass_info);

	register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
					&memtrace_cleanup_pass_info);

	return 0;
}
