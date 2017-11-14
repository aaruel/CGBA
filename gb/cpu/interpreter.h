//
//  interpreter.h
//  CGBA
//

/*
 * There are a few main methods in emulation for code execution:
 * - Interpretation
 *    - easiest to implement
 *    - easy to control for timing regulations
 * - Dynamic Compilation
 *    - analogous to JIT
 *    - generate code based on execution
 * - Static Compilation
 *    - brute force a compilation before running
 *    - very hard to do right
 */

#ifndef __CGBA__interpreter__
#define __CGBA__interpreter__

#define F_CARRY     0x10
#define F_HALFCARRY 0x20
#define F_OPERATION 0x40
#define F_ZERO      0x80

#define BYTE     0x1
#define HALFWORD 0x2
#define WORD     0x4

#define TRUE  1
#define FALSE 0

#include <stdio.h>

void ip_execute(uint8_t opcode);

#endif /* defined(__CGBA__interpreter__) */
