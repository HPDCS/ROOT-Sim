#include <exc/allocator/allocator.h>

#include <core/core.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>

#include <asm/prctl.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SAFE_CLZ(x)	(\
		__builtin_choose_expr( \
		__builtin_types_compatible_p (__typeof__ (x), unsigned), __builtin_clz(x),\
		__builtin_choose_expr(\
		__builtin_types_compatible_p (__typeof__ (x), unsigned long), __builtin_clzl(x),\
		__builtin_choose_expr(\
		__builtin_types_compatible_p (__typeof__ (x), unsigned long long), __builtin_clzll(x),\
		(void)0))))

#define left_child(i) (((i) << 1U) + 1U)
#define right_child(i) (((i) << 1U) + 2U)
#define parent(i) ((((i) + 1) >> 1U) - 1U)
#define is_power_of_2(i) (!((i) & ((i) - 1)))
#define next_exp_of_2(i) (sizeof(i) * CHAR_BIT - SAFE_CLZ(i))

extern void *__real_malloc(size_t mem_size);
extern void __real_free(void *ptr);

extern int arch_prctl(int code, unsigned long addr);

void allocator_init(struct lp_struct *lp)
{
	struct memory_map *self = &lp->mm;
	uint_fast8_t node_size = B_TOTAL_EXP;

	for (uint_fast32_t i = 0;
		i < sizeof(self->longest) / sizeof(*self->longest); ++i) {
		self->longest[i] = node_size;
		node_size -= is_power_of_2(i + 2);
	}

	memset(self->clean, 0, sizeof(self->clean));
	memset(self->aggr_written, 0, sizeof(self->aggr_written));

	self->current = 0;
	self->used_mem = 0;
	self->base_mem = __real_malloc((1 << B_TOTAL_EXP) * (B_LOGS_COUNT + 2));
}

void allocator_fini(struct lp_struct *lp)
{
	__real_free(lp->mm.base_mem);
}

void allocator_processing_start(struct lp_struct * lp)
{
	arch_prctl(ARCH_SET_GS, lp->mm.current * (1 << B_TOTAL_EXP));
}

uint32_t allocator_checkpoint_take(struct lp_struct *lp)
{
	struct memory_map *self = &lp->mm;
	unsigned char *old_ptr = ((unsigned char *)self->base_mem) + self->current * (1 << B_TOTAL_EXP);
	unsigned char *new_ptr = old_ptr + (1 << B_TOTAL_EXP);

#define copy_set_chunks(i) 				\
	memcpy(						\
		new_ptr + i * (1 << B_BLOCK_EXP), 	\
		old_ptr + i * (1 << B_BLOCK_EXP), 	\
		(1 << B_BLOCK_EXP)			\
	)

	bitmap_foreach_set(self->clean, sizeof(self->clean), copy_set_chunks);

#undef copy_set_chunks

	memcpy(self->clean, self->aggr_written, sizeof(self->clean));

	++self->current;

	if(self->current == B_LOGS_COUNT) {
		self->current_offset += B_LOGS_COUNT;
		memcpy(self->base_mem, (char *)self->base_mem + B_LOGS_COUNT * (1 << B_TOTAL_EXP), sizeof(self->base_mem));
		self->current = 0;
	}


	return self->current + self->current_offset;
}

void allocator_checkpoint_restore(struct lp_struct *lp, uint32_t checkpoint_t)
{
	struct memory_map *self = &lp->mm;
	checkpoint_t -= self->current_offset;
	if(checkpoint_t >= B_LOGS_COUNT){
		rootsim_error(true, "You are trying to rollback too much my friend");
		abort();
	}
	self->current = checkpoint_t;
}

void *__wrap_malloc(size_t req_size)
{
	if(!req_size)
		return NULL;

	struct memory_map *self = &current->mm;
	uint_fast8_t req_blks = max(next_exp_of_2(req_size - 1), B_BLOCK_EXP);

	if (self->longest[0] < req_blks) {
		errno = ENOMEM;
		return NULL;
	}

	/* search recursively for the child */
	uint_fast8_t node_size;
	uint_fast32_t i;
	for (
		i = 0, node_size = B_TOTAL_EXP;
		node_size > req_blks;
		--node_size
	) {
		/* choose the child with smaller longest value which
		 * is still large at least *size* */
		i = left_child(i);
		i += self->longest[i] < req_blks;
	}

	/* update the *longest* value back */
	self->longest[i] = 0;
	self->used_mem += 1 << node_size;

	uint_fast32_t offset = ((i + 1) << node_size) - (1 << B_TOTAL_EXP);

	while (i) {
		i = parent(i);
		self->longest[i] = max(
			self->longest[left_child(i)],
			self->longest[right_child(i)]
		);
	}

	return ((char *)self->base_mem) + offset;
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
	size_t tot = nmemb * size;
	void *ret = __wrap_malloc(tot);

	if(ret)
		memset(ret, 0, tot);

	return ret;
}

void __wrap_free(void *ptr)
{
	if(!ptr)
		return;

	struct memory_map *self = &current->mm;
	uint_fast8_t node_size = B_BLOCK_EXP;
	uint_fast32_t i =
		(((uintptr_t)ptr - (uintptr_t)self->base_mem) >> B_BLOCK_EXP) +
		(1 << (B_TOTAL_EXP - B_BLOCK_EXP)) - 1;

	for (; self->longest[i]; i = parent(i)) {
		++node_size;
	}

	self->longest[i] = node_size;
	self->used_mem -= 1 << node_size;

	while (i) {
		i = parent(i);

		uint_fast8_t left_longest = self->longest[left_child(i)];
		uint_fast8_t right_longest = self->longest[right_child(i)];

		if (left_longest == node_size && right_longest == node_size) {
			self->longest[i] = node_size + 1;
		} else {
			self->longest[i] = max(left_longest, right_longest);
		}
		++node_size;
	}

	// little "optimization", we reset the bitmaps so we don't keep copying stuff
	uint_fast32_t off_b = ((uintptr_t)ptr - (uintptr_t)self->base_mem) >> B_BLOCK_EXP;
	i = 1 << (node_size - B_BLOCK_EXP);
	while(i--){
		bitmap_reset(self->clean, i + off_b);
		bitmap_reset(self->aggr_written, i + off_b);
	}
}
//
//void write_mem(void *ptr)
//{
//	struct memory_map *self = &current->mm;
//	uint_fast32_t i =
//		(((uintptr_t)ptr - (uintptr_t)self->base_mem) >> B_BLOCK_EXP);
//	bitmap_reset(self->clean, i);
//	bitmap_set(self->aggr_written, i);
//}
//
///// the instrumentor calls this function to know if the address has been written
//bool is_clean(void *ptr)
//{
//	struct memory_map *self = &current->mm;
//	uint_fast32_t i =
//		(((uintptr_t)ptr - (uintptr_t)self->base_mem) >> B_BLOCK_EXP);
//	return bitmap_check(self->clean, i);
//}
