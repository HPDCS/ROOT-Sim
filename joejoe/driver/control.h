#ifndef CONTROL_H
#define CONTROL_H

#define PTR_BITS		48
#define PTR_THR_BIT		(1ULL << (PTR_BITS - 1))			/* threshold bit */
#define PTR_USED_MASK 		((1ULL << PTR_BITS) - 1)			// 0x0000F...F
#define PTR_FREE_MASK		(~PTR_USED_MASK)				// 0xFFFF0...0
#define PTR_CRC_MASK		(((1ULL << (64 - PTR_BITS - 2)) - 1) << 48)	// 0x3FFF0...0
#define PTR_CCB_MASK		((1ULL << (64 - PTR_BITS - 2)) - 1)		// 0x0...03FFF
#define PROCESS_BIT		62
#define ENABLED_BIT		63
#define PROCESS_MASK		(1ULL << PROCESS_BIT)
#define ENABLED_MASK		(1ULL << ENABLED_BIT)

#define CRC_MAGIC		0xc
#define set_crc(ctl)		(ctl.crc = ctl.ccb ^ CRC_MAGIC)
#define check_crc(ctl)		(((ctl & PTR_CCB_MASK) ^ CRC_MAGIC) == ((ctl & PTR_CRC_MASK) >> 48))
#define build_ptr(ptr) 		((ptr & PTR_THR_BIT) ? PTR_FREE_MASK | ptr : PTR_USED_MASK & ptr)

#endif /* CONTROL_H */