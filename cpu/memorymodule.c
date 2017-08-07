//
//  statemachine.c
//  CGBA
//

#include <inttypes.h>
#include "memorymodule.h"

///////**** Private ****///////

/* 
 *  Main memory pool
 */
static uint8_t _sm_mem[0x0000FFFF] = {0};

/*
 *  Z80 clocks
 */
static uint16_t _sm_clock_m = 0;
static uint16_t _sm_clock_t = 0;

/*
 *  Z80 8-bit Registers
 */
static uint8_t  _sm_reg_a = 0, \
                _sm_reg_b = 0, \
                _sm_reg_c = 0, \
                _sm_reg_d = 0, \
                _sm_reg_e = 0, \
                _sm_reg_h = 0, \
                _sm_reg_l = 0, \
                _sm_reg_f = 0;

/*
 *  Z80 16-bit Registers
 */

static uint16_t _sm_reg_pc = 0, \
                _sm_reg_sp = 0;

/*
 *  Z80 Special Registers
 */

static uint8_t  _sm_reg_halt = 0, \
                _sm_reg_stop = 0;

///////**** Public ****///////

/*
 *  Memory interfaces
 */
uint8_t sm_getmemaddr8(uint16_t addr) {
    return (uint8_t)_sm_mem[(uint16_t)addr];
}

uint16_t sm_getmemaddr16(uint16_t addr) {
    return (uint16_t)_sm_mem[(uint16_t)addr];
}

void sm_setmemaddr8 (uint16_t addr, uint8_t  data) {
    *(uint8_t*)(&_sm_mem[addr]) = data;
}

void sm_setmemaddr16(uint16_t addr, uint16_t data) {
    *(uint16_t*)(&_sm_mem[addr]) = data;
}

/*
 *  clock interfaces
 */
// calc clock t automatically (in most cases)
void sm_inc_clock(uint16_t val) {
    _sm_clock_m += val;
    _sm_clock_t += val * 4;
}

void sm_inc_mclock(uint16_t val) {
    _sm_clock_m += val;
}

uint16_t sm_get_mclock() {
    return _sm_clock_m;
}

void sm_inc_tclock(uint16_t val) {
    _sm_clock_t += val;
}

uint16_t sm_get_tclock() {
    return _sm_clock_t;
}

/*
 *  register interfaces
 */

void sm_set_reg(sm_regs e, uint8_t data) {
    switch (e) {
        case REG_A: _sm_reg_a = data; break;
        case REG_B: _sm_reg_b = data; break;
        case REG_C: _sm_reg_c = data; break;
        case REG_D: _sm_reg_d = data; break;
        case REG_E: _sm_reg_e = data; break;
        case REG_H: _sm_reg_h = data; break;
        case REG_L: _sm_reg_l = data; break;
        case REG_F: _sm_reg_f = data; break;
    }
}

uint8_t sm_get_reg(sm_regs e) {
    switch (e) {
        case REG_A: return _sm_reg_a;
        case REG_B: return _sm_reg_b;
        case REG_C: return _sm_reg_c;
        case REG_D: return _sm_reg_d;
        case REG_E: return _sm_reg_e;
        case REG_H: return _sm_reg_h;
        case REG_L: return _sm_reg_l;
        case REG_F: return _sm_reg_f;
    }
}

uint16_t sm_get_reg16(sm_regs e1, sm_regs e2) {
    return (sm_get_reg(e1) << 8) | sm_get_reg(e2);
}

void sm_set_reg16(sm_regs e1, sm_regs e2, uint16_t val) {
    sm_set_reg(e1, val >> 8  );
    sm_set_reg(e2, val & 0xFF);
}

void sm_set_reg_pc(uint16_t data) { _sm_reg_pc = data; }
void sm_inc_reg_pc(uint16_t inc ) { _sm_reg_pc += inc; }
void sm_set_reg_sp(uint16_t data) { _sm_reg_sp = data; }
void sm_inc_reg_sp(uint16_t inc ) { _sm_reg_sp += inc; }

uint16_t sm_get_reg_pc() { return _sm_reg_pc; }
uint16_t sm_get_reg_sp() { return _sm_reg_sp; }

void sm_set_reg_halt(uint8_t b) { _sm_reg_halt = b; }
void sm_set_reg_stop(uint8_t b) { _sm_reg_stop = b; }
