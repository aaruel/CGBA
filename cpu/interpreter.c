//
//  interpreter.c
//  CGBA
//

#include "interpreter.h"
#include "memorymodule.h"
#include <execinfo.h>

///////**** Private ****///////

enum flag_check {
    CHECK_CARRY  = 0b0001,
    CHECK_HCARRY = 0b0010,
    CHECK_OP     = 0b0100,
    CHECK_ZERO   = 0b1000
};
typedef enum flag_check flag_check;

// https://stackoverflow.com/questions/776508/best-practices-for-circular-shift-rotate-operations-in-c
static inline uint8_t _ip_rotl8 (uint8_t n, uint8_t c) {
    const unsigned int mask = (8*sizeof(n) - 1);
    c &= mask;
    return (n<<c) | (n>>( (-c)&mask ));
}

static inline uint16_t _ip_rotl16 (uint16_t n, uint8_t c) {
    const unsigned int mask = (8*sizeof(n) - 1);
    c &= mask;
    return (n<<c) | (n>>( (-c)&mask ));
}

static inline uint8_t _ip_rotr8 (uint8_t n, uint8_t c) {
    const unsigned int mask = (8*sizeof(n) - 1);
    c &= mask;
    return (n>>c) | (n<<( (-c)&mask ));
}

static inline uint16_t _ip_rotr16 (uint16_t n, uint8_t c) {
    const unsigned int mask = (8*sizeof(n) - 1);
    c &= mask;
    return (n>>c) | (n<<( (-c)&mask ));
}

static inline uint16_t _ip_rotlc (uint8_t n) {
    return ((uint16_t) n) << 1;
}

static inline uint16_t _ip_rotrc (uint8_t n) {
    uint16_t nn = (uint16_t)n;
    if (nn & 1) {
        return (nn >> 1) | 0x100;
    }
    else {
        return (nn >> 1);
    }
}

// note: 32bit arg for carry checking
static inline void _ip_apply_flags(uint8_t init, uint32_t condition, flag_check flags) {
    uint8_t f = init;
    if (flags & CHECK_CARRY && condition > 0xFFFF) f |= F_CARRY;
    if (flags & CHECK_HCARRY && condition > 0x00FF) f |= F_HALFCARRY;
    if (flags & CHECK_OP) f |= F_OPERATION;
    if (flags & CHECK_ZERO && !condition) f |= F_ZERO;
    sm_set_reg(REG_F, f);
}


// OPCODE Types

/*
 * SPECIAL
 */

void _ip_flag_checker(void (*func)(), flag_check flags, uint8_t flag_invert, uint8_t clock) {
    const uint8_t f = (sm_get_reg(REG_F)) & flags;
    if (flag_invert && !f) {
        func();
    }
    else if (f) {
        func();
    }
    else {
        sm_inc_clock(clock);
    }
}

/*
 * LOADS
 */

// LD 8bit reg, 8bit reg
void _ip_LD_r_r(sm_regs e1, sm_regs e2) {
    sm_set_reg(e1, e2);
    sm_inc_clock(1);
}

// LD 8bit reg, (8bit num)
void _ip_LD_r_n(sm_regs e1) {
    sm_set_reg(e1, sm_getmemaddr8(sm_get_reg_pc()));
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// LD 8bit reg, (16bit num)
void _ip_LD_r_dnn(sm_regs e1) {
    sm_set_reg(e1, sm_getmemaddr8(sm_get_reg_pc()));
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// LD (8bit number), 8bit reg
void _ip_LD_dr_n(sm_regs e1) {
    const uint8_t data = sm_getmemaddr8(sm_get_reg_pc());
    sm_setmemaddr8( 0xFF00 + sm_getmemaddr8(sm_get_reg_pc()), data);
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// LD 8bit reg, (16bit regs)
void _ip_LD_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    sm_set_reg(e1, sm_getmemaddr16(sm_get_reg16(e2, e3)));
    sm_inc_clock(2);
}

// LD 8bit reg, (8bit reg)
void _ip_LD_r_dr(sm_regs e1, sm_regs e2) {
    sm_set_reg(e1, sm_getmemaddr8(sm_get_reg(e2)));
    sm_inc_clock(2);
}

// LD (8bit reg), 8bit reg
void _ip_LD_dr_r(sm_regs e1, sm_regs e2) {
    sm_setmemaddr16(0xFF00 + sm_get_reg(e1), e2);
    sm_inc_clock(2);
}

// LD 16bit regs, 16bit num
void _ip_LD_rr_nn(sm_regs e1, sm_regs e2) {
    sm_set_reg( e2, sm_getmemaddr8( sm_get_reg_pc() ) );
    sm_set_reg( e1, sm_getmemaddr8( sm_get_reg_pc() + BYTE ) );
    sm_inc_reg_pc(HALFWORD);
    sm_inc_clock(3);
}

// LD (16bit regs) , 8bit num
void _ip_LD_drr_n(sm_regs e1, sm_regs e2) {
    sm_setmemaddr8( sm_get_reg16(e1, e2), sm_getmemaddr8( sm_get_reg_pc() ) );
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(3);
}

// LD (16bit regs) , 8bit reg
void _ip_LD_drr_r(sm_regs e1, sm_regs e2, sm_regs e3) {
    sm_setmemaddr8( sm_get_reg16(e1, e2), sm_get_reg(e3) );
    sm_inc_clock(2);
}

// LD (8bit number), 8bit reg
void _ip_LD_dn_r(sm_regs e1) {
    sm_setmemaddr8( 0xFF00 + sm_getmemaddr8(sm_get_reg_pc()), sm_get_reg(e1));
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// LD (16bit number), 8bit reg
void _ip_LD_dnn_r(sm_regs e1) {
    sm_setmemaddr8(sm_getmemaddr16(sm_get_reg_pc()), sm_get_reg(e1));
    sm_inc_reg_pc(HALFWORD);
    sm_inc_clock(3);
}

// LD special reg, 16bit num
void _ip_LD_special_nn(void(*setter)(uint16_t)) {
    const uint16_t nn = sm_getmemaddr16(sm_get_reg_pc());
    (*setter)(nn);
    sm_inc_reg_pc(2);
    sm_inc_clock(3);
}

// LD special reg, 16bit num
void _ip_LD_special_rr(void(*setter)(uint16_t), sm_regs e1, sm_regs e2) {
    const uint16_t nn = sm_get_reg16(e1, e2);
    (*setter)(nn);
    sm_inc_clock(3);
}

// LD special reg, 16bit num
void _ip_LD_rr_special(sm_regs e1, sm_regs e2, uint16_t(*getter)(), uint8_t offset) {
    const uint16_t nn = (*getter)() + offset;
    sm_set_reg16(e1, e2, nn);
    sm_inc_clock(3);
}

/*
 * INC
 */

// INC 8bit reg
void _ip_INC_r(sm_regs e1) {
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r + 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00010000, r + 1, CHECK_ZERO | CHECK_HCARRY );
    sm_inc_clock(1);
}

// INC 16bit regs
void _ip_INC_rr(sm_regs e1, sm_regs e2) {
    const uint16_t rr = sm_get_reg16(e1, e2) + 1;
    sm_set_reg(e1, rr >> 8);
    sm_set_reg(e2, rr & 0xFF);
    sm_inc_clock(1);
}

// INC (16bit regs)
void _ip_INC_drr(sm_regs e1, sm_regs e2) {
    const uint16_t rr = sm_get_reg16(e1, e2);
    const uint16_t nn = sm_getmemaddr16(rr);
    sm_setmemaddr16(rr, nn + 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00010000, nn + 1, CHECK_ZERO | CHECK_HCARRY );
}

// INC special
void _ip_INC_special(uint16_t(*getter)(), void(*setter)(uint16_t)) {
    const uint16_t rr = (*getter)() + 1;
    (*setter)(rr);
    sm_inc_clock(1);
}

/*
 * DEC
 */

// DEC 8bit reg
void _ip_DEC_r(sm_regs e1) {
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r - 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b01010000, r - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY );
    sm_inc_clock(1);
}

// DEC 16bit regs
void _ip_DEC_rr(sm_regs e1, sm_regs e2) {
    sm_set_reg16(e1, e2, sm_get_reg16(e1, e2) - 1);
    sm_inc_clock(2);
}

// DEC (16bit regs)
void _ip_DEC_drr(sm_regs e1, sm_regs e2) {
    const uint16_t rr = sm_get_reg16(e1, e2);
    const uint16_t nn = sm_getmemaddr16(rr);
    sm_setmemaddr16(rr, nn - 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b01010000, nn - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY );
}

// DEC special
void _ip_DEC_special(uint16_t(*getter)(), void(*setter)(uint16_t)) {
    const uint16_t rr = (*getter)() - 1;
    (*setter)(rr);
    sm_inc_clock(1);
}

/*
 * ADD
 */

// ADD 8bit reg, 8bit reg
void _ip_ADD_r_r(sm_regs e1, sm_regs e2) {
    const uint16_t r1 = sm_get_reg(e1);
    const uint16_t r2 = sm_get_reg(e2);
    sm_set_reg(e1, r1+r2);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00000000, r1+r2, CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(1);
}

// ADD 8bit reg, (16bit reg)
void _ip_ADD_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    sm_set_reg(e1, n+r);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00000000, n+r, CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY);
    sm_inc_clock(2);
}

// ADD 16bit regs, 16bit regs
void _ip_ADD_rr_rr(sm_regs e1, sm_regs e2, sm_regs e3, sm_regs e4) {
    const uint16_t rr1 = sm_get_reg16(e1, e2);
    const uint16_t rr2 = sm_get_reg16(e3, e4);
    sm_set_reg16(e1, e2, rr1+rr2);
    _ip_apply_flags(sm_get_reg(REG_F)&0b10000000, rr1+rr2, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// ADD 8bit reg, 8bit num
void _ip_ADD_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r+n);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00000000, r+n, CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// ADD 16bit regs, special
void _ip_ADD_rr_special(sm_regs e1, sm_regs e2, uint16_t(*getter)()) {
    const uint16_t rr1 = sm_get_reg16(e1, e2);
    const uint16_t rr2 = (*getter)();
    sm_set_reg16(e1, e2, rr1+rr2);
    _ip_apply_flags(sm_get_reg(REG_F)&0b10000000, rr1+rr2, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// ADD special, relative 8bit number
void _ip_ADD_special_r8(uint16_t(*getter)()) {
    const uint16_t rr = (*getter)();
    const uint16_t r8 = sm_getmemaddr8(sm_get_reg_pc());
    sm_set_reg_sp(rr+r8);
    _ip_apply_flags(0b00000000,rr+r8, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

/*
 * ADC
 */

// ADC 8bit reg, 8bit reg
void _ip_ADC_r_r(sm_regs e1, sm_regs e2) {
    const uint8_t r1 = sm_get_reg(e1);
    const uint8_t r2 = sm_get_reg(e2);
    const uint8_t c = sm_get_reg(REG_F) & F_CARRY ? 1 : 0;
    sm_set_reg(e1, r1+r2+c);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00000000, r1+r2+c, CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(1);
}

// ADC 8bit reg, 8bit number
void _ip_ADC_r_n(sm_regs e1) {
    const uint8_t r1 = sm_get_reg(e1);
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t c = sm_get_reg(REG_F) & F_CARRY ? 1 : 0;
    sm_set_reg(e1, r1+n+c);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00000000, r1+n+c, CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(2);
}

// ADC 8bit reg, (16bit reg)
void _ip_ADC_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    const uint8_t c = sm_get_reg(REG_F) & F_CARRY ? 1 : 0;
    sm_set_reg(e1, n+r+c);
    _ip_apply_flags(sm_get_reg(REG_F)&0b00000000, n+r+c, CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY);
    sm_inc_clock(2);
}

/*
 * SUB
 */

// SUB 8bit reg, 8bit reg
void _ip_SUB_r_r(sm_regs e1, sm_regs e2) {
    const uint16_t r1 = sm_get_reg(e1);
    const uint16_t r2 = sm_get_reg(e2);
    sm_set_reg(e1, r1-r2);
    _ip_apply_flags(0, r1-r2, CHECK_OP | CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(1);
}

// SUB 8bit reg, 8bit num
void _ip_SUB_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r-n);
    _ip_apply_flags(0, r-n, CHECK_OP | CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// SUB 8bit reg, (16bit reg)
void _ip_SUB_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    sm_set_reg(e1, r-n);
    _ip_apply_flags(0, r-n, CHECK_OP | CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY);
    sm_inc_clock(2);
}

/*
 * SBC
 */

// SBC 8bit reg, 8bit reg
void _ip_SBC_r_r(sm_regs e1, sm_regs e2) {
    const uint16_t r1 = sm_get_reg(e1);
    const uint16_t r2 = sm_get_reg(e2);
    const uint8_t c = sm_get_reg(REG_F) & F_CARRY ? 1 : 0;
    sm_set_reg(e1, r1-r2-c);
    _ip_apply_flags(0, r1-r2-c, CHECK_OP | CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(1);
}

// SBC 8bit reg, 8bit num
void _ip_SBC_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    const uint8_t c = sm_get_reg(REG_F) & F_CARRY ? 1 : 0;
    sm_set_reg(e1, r-n-c);
    _ip_apply_flags(0, r-n-c, CHECK_OP | CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// SBC 8bit reg, (16bit reg)
void _ip_SBC_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    const uint8_t c = sm_get_reg(REG_F) & F_CARRY ? 1 : 0;
    sm_set_reg(e1, r-n-c);
    _ip_apply_flags(0, r-n-c, CHECK_OP | CHECK_ZERO | CHECK_HCARRY | CHECK_CARRY);
    sm_inc_clock(2);
}

/*
 * AND
 */

// AND 8bit reg, 8bit reg
void _ip_AND_r_r(sm_regs e1, sm_regs e2) {
    const uint8_t r1 = sm_get_reg(e1);
    const uint8_t r2 = sm_get_reg(e2);
    sm_set_reg(e1, r1&r2);
    _ip_apply_flags(0b00100000, r1&r2, CHECK_ZERO);
    sm_inc_clock(1);
}

// AND 8bit reg, 8bit num
void _ip_AND_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r&n);
    _ip_apply_flags(0b00100000, r&n, CHECK_ZERO);
    sm_inc_clock(2);
}

// AND 8bit reg, (16bit reg)
void _ip_AND_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    sm_set_reg(e1, n&r);
    _ip_apply_flags(0b00100000, n&r, CHECK_ZERO);
    sm_inc_clock(2);
}

/*
 * XOR
 */

// XOR 8bit reg, 8bit reg
void _ip_XOR_r_r(sm_regs e1, sm_regs e2) {
    const uint8_t r1 = sm_get_reg(e1);
    const uint8_t r2 = sm_get_reg(e2);
    sm_set_reg(e1, r1^r2);
    _ip_apply_flags(0b00000000, r1^r2, CHECK_ZERO);
    sm_inc_clock(1);
}

// XOR 8bit reg, 8bit num
void _ip_XOR_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r^n);
    _ip_apply_flags(0, r^n, CHECK_ZERO);
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// XOR 8bit reg, (16bit reg)
void _ip_XOR_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    sm_set_reg(e1, n^r);
    _ip_apply_flags(0b00000000, n^r, CHECK_ZERO);
    sm_inc_clock(2);
}

/*
 * OR
 */

// OR 8bit reg, 8bit reg
void _ip_OR_r_r(sm_regs e1, sm_regs e2) {
    const uint8_t r1 = sm_get_reg(e1);
    const uint8_t r2 = sm_get_reg(e2);
    sm_set_reg(e1, r1^r2);
    _ip_apply_flags(0b00000000, r1^r2, CHECK_ZERO);
    sm_inc_clock(1);
}

// OR 8bit reg, 8bit num
void _ip_OR_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r|n);
    _ip_apply_flags(0, r|n, CHECK_ZERO);
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// OR 8bit reg, (16bit reg)
void _ip_OR_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    sm_set_reg(e1, n|r);
    _ip_apply_flags(0b00000000, n|r, CHECK_ZERO);
    sm_inc_clock(2);
}

/*
 * CP
 */

// CP 8bit reg, 8bit reg
void _ip_CP_r_r(sm_regs e1, sm_regs e2) {
    const uint8_t r1 = sm_get_reg(e1);
    const uint8_t r2 = sm_get_reg(e2);
    _ip_apply_flags(0, r1-r2, CHECK_OP | CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(1);
}

// OR 8bit reg, 8bit num
void _ip_CP_r_n(sm_regs e1) {
    const uint8_t n = sm_getmemaddr8(sm_get_reg_pc());
    const uint8_t r = sm_get_reg(e1);
    _ip_apply_flags(0, r-n, CHECK_OP | CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO);
    sm_inc_reg_pc(BYTE);
    sm_inc_clock(2);
}

// CP 8bit reg, (16bit reg)
void _ip_CP_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    const uint16_t rr = sm_get_reg16(e2, e3);
    const uint16_t r = sm_get_reg(e1);
    const uint16_t n = sm_getmemaddr16(rr);
    _ip_apply_flags(0, r-n, CHECK_OP | CHECK_HCARRY | CHECK_CARRY | CHECK_ZERO );
    sm_inc_clock(2);
}

/*
 * JUMP
 */

// JP a16
void _ip_JP_a16() {
    const int16_t rr = sm_get_reg_pc();
    sm_set_reg_pc(sm_getmemaddr16(rr));
    sm_inc_clock(4);
}

// JP chk flag, a16
void _ip_JP_f_a16(flag_check flags, uint8_t flag_invert /* For the NZ case */) {
    _ip_flag_checker(_ip_JP_a16, flags, flag_invert, 3);
}

// JP (16bit regs)
void _ip_JP_drr(sm_regs e1, sm_regs e2) {
    sm_set_reg_pc(sm_getmemaddr16(sm_get_reg16(e1, e2)));
    sm_inc_clock(1);
}

// JR r8
void _ip_JR_r8() {
    const uint8_t pc = sm_get_reg_pc();
    const int8_t r8 = sm_getmemaddr8(pc);
    sm_set_reg_pc(pc + r8);
    sm_inc_clock(3);
}

// JR chk flag, r8
void _ip_JR_f_r8(flag_check flags, uint8_t flag_invert /* For the NZ case */) {
    _ip_flag_checker(_ip_JR_r8, flags, flag_invert, 2);
}

/*
 * Stack Operations
 */

// RET
void _ip_RET() {
    const uint16_t a = sm_getmemaddr16(sm_get_reg_sp());
    sm_set_reg_pc(a);
    sm_inc_reg_sp(HALFWORD);
    sm_inc_clock(3);
}

// RET chk flag
void _ip_RET_f(flag_check flags, uint8_t flag_invert) {
    _ip_flag_checker(_ip_RET, flags, flag_invert, 2);
}

// POP 16 bit reg
void _ip_POP_rr(sm_regs e1, sm_regs e2) {
    const uint16_t nn = sm_getmemaddr16(sm_get_reg_sp());
    sm_set_reg16(e1, e2, nn);
    sm_inc_reg_sp(HALFWORD);
    sm_inc_clock(3);
}

// PUSH 16 bit reg
void _ip_PUSH_rr(sm_regs e1, sm_regs e2) {
    sm_inc_reg_sp(-HALFWORD);
    sm_setmemaddr16(sm_get_reg_sp(), sm_get_reg16(e1, e2));
    sm_inc_clock(3);
}

// PUSH special
void _ip_PUSH_special(uint16_t (*getter)(), uint8_t offset) {
    sm_inc_reg_sp(-HALFWORD);
    sm_setmemaddr16(sm_get_reg_sp(), getter()+offset);
    sm_inc_clock(3);
}

// CALL a16
void _ip_CALL_a16() {
    sm_inc_reg_sp(-2);
    sm_setmemaddr16(sm_get_reg_sp(), sm_get_reg_pc()+HALFWORD);
    sm_set_reg_pc(sm_getmemaddr16(sm_get_reg_pc()));
    sm_inc_clock(5);
}

// CALL chk flag a16
void _ip_CALL_f_a16(flag_check flags, uint8_t flag_invert) {
    _ip_flag_checker(_ip_CALL_a16, flags, flag_invert, 5);
}

// RST addr
void _ip_RST_addr(uint16_t addr) {
    _ip_PUSH_special(sm_get_reg_sp, HALFWORD);
    sm_set_reg_pc(addr);
    sm_inc_clock(4);
}


// OPCODE DEFINITIONS
// http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html
// http://imrannazar.com/Gameboy-Z80-Opcode-Map
// http://www.devrs.com/gb/files/opcodes.html
// 0x00
void _ip_NOP() {
    sm_inc_clock(1);
}

// 0x01
void _ip_LD_BC_d16() {
    _ip_LD_rr_nn(REG_B, REG_C);
}

// 0x02
void _ip_LD_dBC_A() {
    _ip_LD_drr_r(REG_B, REG_C, REG_A);
}

// 0x03
void _ip_INC_BC() {
    _ip_INC_rr(REG_B, REG_C);
}

// 0x04
void _ip_INC_B() {
    _ip_INC_r(REG_B);
}

// 0x05
void _ip_DEC_B() {
    _ip_DEC_r(REG_B);
}

// 0x06
void _ip_LD_B_d8() {
    _ip_LD_r_n(REG_B);
}

// 0x07
void _ip_RLC_A() {
    const uint8_t A = sm_get_reg(REG_A);
    sm_set_reg(REG_A, _ip_rotl8(A, 1));
    _ip_apply_flags(0b00000000, _ip_rotl16(A, 1), CHECK_CARRY);
    sm_inc_clock(1);
}

// 0x08
void _ip_LD_da16_SP() {
    sm_setmemaddr16(sm_getmemaddr16(sm_getmemaddr16(sm_get_reg_pc())), sm_get_reg_sp());
    sm_inc_reg_pc(2);
    sm_inc_clock(5);
}

// 0x09
void _ip_ADD_HL_BC() {
    _ip_ADD_rr_rr(REG_H, REG_L, REG_B, REG_C);
}

// 0x0A
void _ip_LD_A_dBC() {
    _ip_LD_r_drr(REG_A, REG_B, REG_C);
}

// 0x0B
void _ip_DEC_BC() {
    _ip_DEC_rr(REG_B, REG_C);
}

// 0x0C
void _ip_INC_C() {
    _ip_INC_r(REG_C);
}

// 0x0D
void _ip_DEC_C() {
    _ip_DEC_r(REG_C);
}

// 0x0E
void _ip_LD_C_d8() {
    _ip_LD_r_n(REG_C);
}

// 0x0F
void _ip_RRC_A() {
    const uint8_t A = sm_get_reg(REG_A);
    sm_set_reg(REG_A, _ip_rotr8(A, 1));
    _ip_apply_flags(0b00000000, _ip_rotr16(A, 1), CHECK_CARRY);
    sm_inc_clock(1);
}

// 0x10 // undefined
void _ip_STOP() {
    sm_set_reg_stop(TRUE);
    // pause program
    printf("STOP or UNDEFINED command called");
    
    #ifdef __GNUC__
    {
        const uint8_t btsize = 10;
        void * backtrace_stack[btsize] = {NULL};
        backtrace(&backtrace_stack[0], btsize);
    }
    #endif
    
    getchar();
}

// 0x11
void _ip_LD_DE_d16() {
    _ip_LD_rr_nn(REG_D, REG_E);
}

// 0x12
void _ip_LD_dDE_A() {
    _ip_LD_drr_r(REG_D, REG_E, REG_A);
}

// 0x13
void _ip_INC_DE() {
    _ip_INC_rr(REG_D, REG_E);
}

// 0x14
void _ip_INC_D() {
    _ip_INC_r(REG_D);
}

// 0x15
void _ip_DEC_D() {
    _ip_DEC_r(REG_D);
}

// 0x16
void _ip_LD_D_d8() {
    _ip_LD_r_n(REG_D);
}

// 0x17
void _ip_RLA() {
    const uint8_t A = sm_get_reg(REG_A);
    sm_set_reg(REG_A, _ip_rotlc(A));
    _ip_apply_flags(0b00000000, _ip_rotlc(A), CHECK_CARRY);
    sm_inc_clock(1);
}

// 0x18
// TYPE CALL ABOVE

// 0x19
void _ip_ADD_HL_DE() {
    _ip_ADD_rr_rr(REG_H, REG_L, REG_D, REG_E);
}

// 0x1A
void _ip_LD_A_dDE() {
    _ip_LD_r_drr(REG_A, REG_D, REG_E);
}

// 0x1B
void _ip_DEC_DE() {
    _ip_DEC_rr(REG_D, REG_E);
}

// 0x1C
void _ip_INC_E() {
    _ip_INC_r(REG_E);
}

// 0x1D
void _ip_DEC_E() {
    _ip_DEC_r(REG_E);
}

// 0x1E
void _ip_LD_E_d8() {
    _ip_LD_r_n(REG_E);
}

// 0x1F
void _ip_RRA() {
    const uint8_t A = sm_get_reg(REG_A);
    sm_set_reg(REG_A, _ip_rotrc(A));
    _ip_apply_flags(0b00000000, _ip_rotrc(A), CHECK_CARRY);
    sm_inc_clock(1);
}

// 0x20
void _ip_JR_NZ_r8() {
    _ip_JR_f_r8(CHECK_ZERO, TRUE);
}

// 0x21
void _ip_LD_HL_d16() {
    _ip_LD_rr_nn(REG_H, REG_L);
}

// 0x22
void _ip_LD_dHLp_A() {
    _ip_LD_drr_r(REG_H, REG_L, REG_A);
    _ip_INC_rr(REG_H, REG_L);
}

// 0x23
void _ip_INC_HL() {
    _ip_INC_rr(REG_H, REG_L);
}

// 0x24
void _ip_INC_H() {
    _ip_INC_r(REG_H);
}

// 0x25
void _ip_DEC_H() {
    _ip_DEC_r(REG_H);
}

// 0x26
void _ip_LD_H_d8() {
    _ip_LD_r_n(REG_H);
}

// 0x27
void _ip_DAA() {
    const uint8_t A = sm_get_reg(REG_A);
    const uint8_t F = sm_get_reg(REG_F);
    uint16_t r = 0;
    if ((A & 0x0F) > 9 || F & CHECK_HCARRY) {
        r = A + 0x06;
        sm_set_reg(REG_A, r);
    }
    else if ((A & 0xF0) > 9 || F & CHECK_CARRY) {
        r = A + 0x60;
        sm_set_reg(REG_A, r);
    }
    _ip_apply_flags(sm_get_reg(REG_F) & 0b01000000, r, CHECK_ZERO | CHECK_CARRY);
    sm_inc_clock(1);
}

// 0x28
void _ip_JR_Z_r8() {
    _ip_JR_f_r8(CHECK_ZERO, FALSE);
}

// 0x29
void _ip_ADD_HL_HL() {
    _ip_ADD_rr_rr(REG_H, REG_L, REG_H, REG_L);
}

// 0x2A
void _ip_LD_A_dHLp() {
    _ip_LD_r_drr(REG_A, REG_H, REG_L);
    _ip_INC_rr(REG_H, REG_L);
}

// 0x2B
void _ip_DEC_HL() {
    _ip_DEC_rr(REG_H, REG_L);
}

// 0x2C
void _ip_INC_L() {
    _ip_INC_r(REG_L);
}

// 0x2D
void _ip_DEC_L() {
    _ip_DEC_r(REG_L);
}

// 0x2E
void _ip_LD_L_d8() {
    _ip_LD_r_n(REG_L);
}

// 0x2F
void _ip_CPL() {
    const uint8_t A = sm_get_reg(REG_A);
    const uint8_t F = sm_get_reg(REG_F);
    sm_set_reg(REG_A, ~A);
    _ip_apply_flags(F | 0b01100000, 0, 0);
    sm_inc_clock(1);
}

// 0x30
void _ip_JR_NC_r8() {
    _ip_JR_f_r8(CHECK_CARRY, TRUE);
}

// 0x31
void _ip_LD_SP_d16() {
    _ip_LD_special_nn(sm_set_reg_sp);
}

// 0x32
void _ip_LD_dHLd_A() {
    _ip_LD_drr_r(REG_H, REG_L, REG_A);
    _ip_DEC_rr(REG_H, REG_L);
}

// 0x33
void _ip_INC_SP() {
    _ip_INC_special(sm_get_reg_sp, sm_set_reg_sp);
}

// 0x34
void _ip_INC_dHL() {
    _ip_INC_drr(REG_H, REG_L);
}

// 0x35
void _ip_DEC_dHL() {
    _ip_DEC_drr(REG_H, REG_L);
}

// 0x36
void _ip_LD_dHL_d8() {
    _ip_LD_drr_n(REG_H, REG_L);
}

// 0x37
void _ip_SCF() {
    _ip_apply_flags(sm_get_reg(REG_F) | F_CARRY, 0, 0);
}

// 0x38
void _ip_JR_C_r8() {
    _ip_JR_f_r8(CHECK_CARRY, FALSE);
}

// 0x39
void _ip_ADD_HL_SP() {
    _ip_ADD_rr_special(REG_H, REG_L, sm_get_reg_sp);
}

// 0x3A
void _ip_LD_A_dHLd() {
    _ip_LD_r_drr(REG_A, REG_H, REG_L);
    _ip_DEC_rr(REG_H, REG_L);
}

// 0x3B
void _ip_DEC_SP() {
    _ip_DEC_special(sm_get_reg_sp, sm_set_reg_sp);
}

// 0x3C
void _ip_INC_A() {
    _ip_INC_r(REG_A);
}

// 0x3D
void _ip_DEC_A() {
    _ip_DEC_r(REG_A);
}

// 0x3E
void _ip_LD_A_d8() {
    _ip_LD_r_n(REG_A);
}

// 0x3F
void _ip_CCF() {
    _ip_apply_flags(sm_get_reg(REG_F) & (~F_CARRY), 0, 0);
}

// 0x40
void _ip_LD_B_B() {
    _ip_LD_r_r(REG_B, REG_B);
}

// 0x41
void _ip_LD_B_C() {
    _ip_LD_r_r(REG_B, REG_C);
}

// 0x42
void _ip_LD_B_D() {
    _ip_LD_r_r(REG_B, REG_D);
}

// 0x43
void _ip_LD_B_E() {
    _ip_LD_r_r(REG_B, REG_E);
}

// 0x44
void _ip_LD_B_H() {
    _ip_LD_r_r(REG_B, REG_H);
}

// 0x45
void _ip_LD_B_L() {
    _ip_LD_r_r(REG_B, REG_L);
}

// 0x46
void _ip_LD_B_dHL() {
    _ip_LD_r_drr(REG_B, REG_H, REG_L);
}

// 0x47
void _ip_LD_B_A() {
    _ip_LD_r_r(REG_B, REG_A);
}

// 0x48
void _ip_LD_C_B() {
    _ip_LD_r_r(REG_C, REG_B);
}

// 0x49
void _ip_LD_C_C() {
    _ip_LD_r_r(REG_C, REG_C);
}

// 0x4A
void _ip_LD_C_D() {
    _ip_LD_r_r(REG_C, REG_D);
}

// 0x4B
void _ip_LD_C_E() {
    _ip_LD_r_r(REG_C, REG_E);
}

// 0x4C
void _ip_LD_C_H() {
    _ip_LD_r_r(REG_C, REG_H);
}

// 0x4D
void _ip_LD_C_L() {
    _ip_LD_r_r(REG_C, REG_L);
}

// 0x4E
void _ip_LD_C_dHL() {
    _ip_LD_r_drr(REG_C, REG_H, REG_L);
}

// 0x4F
void _ip_LD_C_A() {
    _ip_LD_r_r(REG_C, REG_A);
}

// 0x50
void _ip_LD_D_B() {
    _ip_LD_r_r(REG_D, REG_B);
}

// 0x51
void _ip_LD_D_C() {
    _ip_LD_r_r(REG_D, REG_C);
}

// 0x52
void _ip_LD_D_D() {
    _ip_LD_r_r(REG_D, REG_D);
}

// 0x53
void _ip_LD_D_E() {
    _ip_LD_r_r(REG_D, REG_E);
}

// 0x54
void _ip_LD_D_H() {
    _ip_LD_r_r(REG_D, REG_H);
}

// 0x55
void _ip_LD_D_L() {
    _ip_LD_r_r(REG_D, REG_L);
}

// 0x56
void _ip_LD_D_dHL() {
    _ip_LD_r_drr(REG_D, REG_H, REG_L);
}

// 0x57
void _ip_LD_D_A() {
    _ip_LD_r_r(REG_D, REG_A);
}

// 0x58
void _ip_LD_E_B() {
    _ip_LD_r_r(REG_E, REG_B);
}

// 0x59
void _ip_LD_E_C() {
    _ip_LD_r_r(REG_E, REG_C);
}

// 0x5A
void _ip_LD_E_D() {
    _ip_LD_r_r(REG_E, REG_D);
}

// 0x5B
void _ip_LD_E_E() {
    _ip_LD_r_r(REG_E, REG_E);
}

// 0x5C
void _ip_LD_E_H() {
    _ip_LD_r_r(REG_E, REG_H);
}

// 0x5D
void _ip_LD_E_L() {
    _ip_LD_r_r(REG_E, REG_L);
}

// 0x5E
void _ip_LD_E_dHL() {
    _ip_LD_r_drr(REG_E, REG_H, REG_L);
}

// 0x5F
void _ip_LD_E_A() {
    _ip_LD_r_r(REG_E, REG_A);
}

// 0x60
void _ip_LD_H_B() {
    _ip_LD_r_r(REG_H, REG_B);
}

// 0x61
void _ip_LD_H_C() {
    _ip_LD_r_r(REG_H, REG_C);
}

// 0x62
void _ip_LD_H_D() {
    _ip_LD_r_r(REG_H, REG_D);
}

// 0x63
void _ip_LD_H_E() {
    _ip_LD_r_r(REG_H, REG_E);
}

// 0x64
void _ip_LD_H_H() {
    _ip_LD_r_r(REG_H, REG_H);
}

// 0x65
void _ip_LD_H_L() {
    _ip_LD_r_r(REG_H, REG_L);
}

// 0x66
void _ip_LD_H_dHL() {
    _ip_LD_r_drr(REG_H, REG_H, REG_L);
}

// 0x67
void _ip_LD_H_A() {
    _ip_LD_r_r(REG_H, REG_A);
}

// 0x68
void _ip_LD_L_B() {
    _ip_LD_r_r(REG_L, REG_B);
}

// 0x69
void _ip_LD_L_C() {
    _ip_LD_r_r(REG_L, REG_C);
}

// 0x6A
void _ip_LD_L_D() {
    _ip_LD_r_r(REG_L, REG_D);
}

// 0x6B
void _ip_LD_L_E() {
    _ip_LD_r_r(REG_L, REG_E);
}

// 0x6C
void _ip_LD_L_H() {
    _ip_LD_r_r(REG_L, REG_H);
}

// 0x6D
void _ip_LD_L_L() {
    _ip_LD_r_r(REG_L, REG_L);
}

// 0x6E
void _ip_LD_L_dHL() {
    _ip_LD_r_drr(REG_L, REG_H, REG_L);
}

// 0x6F
void _ip_LD_L_A() {
    _ip_LD_r_r(REG_L, REG_A);
}

// 0x70
void _ip_LD_dHL_B() {
    _ip_LD_drr_r(REG_H, REG_L, REG_B);
}

// 0x71
void _ip_LD_dHL_C() {
    _ip_LD_drr_r(REG_H, REG_L, REG_C);
}

// 0x72
void _ip_LD_dHL_D() {
    _ip_LD_drr_r(REG_H, REG_L, REG_D);
}

// 0x73
void _ip_LD_dHL_E() {
    _ip_LD_drr_r(REG_H, REG_L, REG_E);
}

// 0x74
void _ip_LD_dHL_H() {
    _ip_LD_drr_r(REG_H, REG_L, REG_H);
}

// 0x75
void _ip_LD_dHL_L() {
    _ip_LD_drr_r(REG_H, REG_L, REG_L);
}

// 0x76
void _ip_HALT() {
    // HALT!
    sm_set_reg_stop(1);
    sm_inc_clock(1);
    getchar();
}

// 0x77
void _ip_LD_dHL_A() {
    _ip_LD_drr_r(REG_H, REG_L, REG_A);
}

// 0x78
void _ip_LD_A_B() {
    _ip_LD_r_r(REG_A, REG_B);
}

// 0x79
void _ip_LD_A_C() {
    _ip_LD_r_r(REG_A, REG_C);
}

// 0x7A
void _ip_LD_A_D() {
    _ip_LD_r_r(REG_A, REG_D);
}

// 0x7B
void _ip_LD_A_E() {
    _ip_LD_r_r(REG_A, REG_E);
}

// 0x7C
void _ip_LD_A_H() {
    _ip_LD_r_r(REG_A, REG_H);
}

// 0x7D
void _ip_LD_A_L() {
    _ip_LD_r_r(REG_A, REG_L);
}

// 0x7E
void _ip_LD_A_dHL() {
    _ip_LD_r_drr(REG_A, REG_H, REG_L);
}

// 0x7F
void _ip_LD_A_A() {
    _ip_LD_r_r(REG_A, REG_A);
}

// 0x80
void _ip_ADD_A_B() {
    _ip_ADD_r_r(REG_A, REG_B);
}

// 0x81
void _ip_ADD_A_C() {
    _ip_ADD_r_r(REG_A, REG_C);
}

// 0x82
void _ip_ADD_A_D() {
    _ip_ADD_r_r(REG_A, REG_D);
}

// 0x83
void _ip_ADD_A_E() {
    _ip_ADD_r_r(REG_A, REG_E);
}

// 0x84
void _ip_ADD_A_H() {
    _ip_ADD_r_r(REG_A, REG_H);
}

// 0x85
void _ip_ADD_A_L() {
    _ip_ADD_r_r(REG_A, REG_L);
}

// 0x86
void _ip_ADD_A_dHL() {
    _ip_ADD_r_drr(REG_A, REG_H, REG_L);
}

// 0x87
void _ip_ADD_A_A() {
    _ip_ADD_r_r(REG_A, REG_A);
}

// 0x88
void _ip_ADC_A_B() {
    _ip_ADC_r_r(REG_A, REG_B);
}

// 0x89
void _ip_ADC_A_C() {
    _ip_ADC_r_r(REG_A, REG_C);
}

// 0x8A
void _ip_ADC_A_D() {
    _ip_ADC_r_r(REG_A, REG_D);
}

// 0x8B
void _ip_ADC_A_E() {
    _ip_ADC_r_r(REG_A, REG_E);
}

// 0x8C
void _ip_ADC_A_H() {
    _ip_ADC_r_r(REG_A, REG_H);
}

// 0x8D
void _ip_ADC_A_L() {
    _ip_ADC_r_r(REG_A, REG_L);
}

// 0x8E
void _ip_ADC_A_dHL() {
    _ip_ADC_r_drr(REG_A, REG_H, REG_L);
}

// 0x8F
void _ip_ADC_A_A() {
    _ip_ADC_r_r(REG_A, REG_A);
}

// 0x90
void _ip_SUB_A_B() {
    _ip_SUB_r_r(REG_A, REG_B);
}

// 0x91
void _ip_SUB_A_C() {
    _ip_SUB_r_r(REG_A, REG_C);
}

// 0x92
void _ip_SUB_A_D() {
    _ip_SUB_r_r(REG_A, REG_D);
}

// 0x93
void _ip_SUB_A_E() {
    _ip_SUB_r_r(REG_A, REG_E);
}

// 0x94
void _ip_SUB_A_H() {
    _ip_SUB_r_r(REG_A, REG_H);
}

// 0x95
void _ip_SUB_A_L() {
    _ip_SUB_r_r(REG_A, REG_L);
}

// 0x96
void _ip_SUB_A_dHL() {
    _ip_SUB_r_drr(REG_A, REG_H, REG_L);
}

// 0x97
void _ip_SUB_A_A() {
    _ip_SUB_r_r(REG_A, REG_A);
}

// 0x98
void _ip_SBC_A_B() {
    _ip_SBC_r_r(REG_A, REG_B);
}

// 0x99
void _ip_SBC_A_C() {
    _ip_SBC_r_r(REG_A, REG_C);
}

// 0x9A
void _ip_SBC_A_D() {
    _ip_SBC_r_r(REG_A, REG_D);
}

// 0x9B
void _ip_SBC_A_E() {
    _ip_SBC_r_r(REG_A, REG_E);
}

// 0x9C
void _ip_SBC_A_H() {
    _ip_SBC_r_r(REG_A, REG_H);
}

// 0x9D
void _ip_SBC_A_L() {
    _ip_SBC_r_r(REG_A, REG_L);
}

// 0x9E
void _ip_SBC_A_dHL() {
    _ip_SBC_r_drr(REG_A, REG_H, REG_L);
}

// 0x9F
void _ip_SBC_A_A() {
    _ip_SBC_r_r(REG_A, REG_A);
}

// 0xA0
void _ip_AND_A_B() {
    _ip_AND_r_r(REG_A, REG_B);
}

// 0xA1
void _ip_AND_A_C() {
    _ip_AND_r_r(REG_A, REG_C);
}

// 0xA2
void _ip_AND_A_D() {
    _ip_AND_r_r(REG_A, REG_D);
}

// 0xA3
void _ip_AND_A_E() {
    _ip_AND_r_r(REG_A, REG_E);
}

// 0xA4
void _ip_AND_A_H() {
    _ip_AND_r_r(REG_A, REG_H);
}

// 0xA5
void _ip_AND_A_L() {
    _ip_AND_r_r(REG_A, REG_L);
}

// 0xA6
void _ip_AND_A_dHL() {
    _ip_AND_r_drr(REG_A, REG_H, REG_L);
}

// 0xA7
void _ip_AND_A_A() {
    _ip_AND_r_r(REG_A, REG_A);
}

// 0xA8
void _ip_XOR_A_B() {
    _ip_XOR_r_r(REG_A, REG_B);
}

// 0xA9
void _ip_XOR_A_C() {
    _ip_XOR_r_r(REG_A, REG_C);
}

// 0xAA
void _ip_XOR_A_D() {
    _ip_XOR_r_r(REG_A, REG_D);
}

// 0xAB
void _ip_XOR_A_E() {
    _ip_XOR_r_r(REG_A, REG_E);
}

// 0xAC
void _ip_XOR_A_H() {
    _ip_XOR_r_r(REG_A, REG_H);
}

// 0xAD
void _ip_XOR_A_L() {
    _ip_XOR_r_r(REG_A, REG_L);
}

// 0xAE
void _ip_XOR_A_dHL() {
    _ip_XOR_r_drr(REG_A, REG_H, REG_L);
}

// 0xAF
void _ip_XOR_A_A() {
    _ip_XOR_r_r(REG_A, REG_A);
}

// 0xB0
void _ip_OR_A_B() {
    _ip_OR_r_r(REG_A, REG_B);
}

// 0xB1
void _ip_OR_A_C() {
    _ip_OR_r_r(REG_A, REG_C);
}

// 0xB2
void _ip_OR_A_D() {
    _ip_OR_r_r(REG_A, REG_D);
}

// 0xB3
void _ip_OR_A_E() {
    _ip_OR_r_r(REG_A, REG_E);
}

// 0xB4
void _ip_OR_A_H() {
    _ip_OR_r_r(REG_A, REG_H);
}

// 0xB5
void _ip_OR_A_L() {
    _ip_OR_r_r(REG_A, REG_L);
}

// 0xB6
void _ip_OR_A_dHL() {
    _ip_OR_r_drr(REG_A, REG_H, REG_L);
}

// 0xB7
void _ip_OR_A_A() {
    _ip_OR_r_r(REG_A, REG_A);
}

// 0xB8
void _ip_CP_A_B() {
    _ip_CP_r_r(REG_A, REG_B);
}

// 0xB9
void _ip_CP_A_C() {
    _ip_CP_r_r(REG_A, REG_C);
}

// 0xBA
void _ip_CP_A_D() {
    _ip_CP_r_r(REG_A, REG_D);
}

// 0xBB
void _ip_CP_A_E() {
    _ip_CP_r_r(REG_A, REG_E);
}

// 0xBC
void _ip_CP_A_H() {
    _ip_CP_r_r(REG_A, REG_H);
}

// 0xBD
void _ip_CP_A_L() {
    _ip_CP_r_r(REG_A, REG_L);
}

// 0xBE
void _ip_CP_A_dHL() {
    _ip_CP_r_drr(REG_A, REG_H, REG_L);
}

// 0xBF
void _ip_CP_A_A() {
    _ip_CP_r_r(REG_A, REG_A);
}

// 0xC0
void _ip_RET_NZ() {
    _ip_RET_f(CHECK_ZERO, TRUE);
}

// 0xC1
void _ip_POP_BC() {
    _ip_POP_rr(REG_B, REG_C);
}

// 0xC2
void _ip_JP_NZ_a16() {
    _ip_JP_f_a16(CHECK_ZERO, TRUE);
}

// 0xC3
//void _ip_JP_a16() {
//    
//}

// 0xC4
void _ip_CALL_NZ_a16() {
    _ip_CALL_f_a16(CHECK_ZERO, TRUE);
}

// 0xC5
void _ip_PUSH_BC() {
    _ip_PUSH_rr(REG_B, REG_C);
}

// 0xC6
void _ip_ADD_A_d8() {
    _ip_ADD_r_n(REG_A);
}

// 0xC7
void _ip_RST_00H() {
    _ip_RST_addr(0x00);
}

// 0xC8
void _ip_RET_Z() {
    _ip_RET_f(CHECK_ZERO, FALSE);
}

// 0xC9
//void _ip_RET() {
//    
//}

// 0xCA
void _ip_JP_Z_a16() {
    _ip_JP_f_a16(CHECK_ZERO, FALSE);
}

// 0xCB
void _ip_PREFIX_CB() {
    // call extension
}

// 0xCC
void _ip_CALL_Z_a16() {
    _ip_CALL_f_a16(CHECK_ZERO, FALSE);
}

// 0xCD
//void _ip_CALL_a16() {
//    
//}

// 0xCE
void _ip_ADC_A_d8() {
    _ip_ADC_r_n(REG_A);
}

// 0xCF
void _ip_RST_08H() {
    _ip_RST_addr(0x08);
}

// 0xD0
void _ip_RET_NC() {
    _ip_RET_f(CHECK_CARRY, TRUE);
}

// 0xD1
void _ip_POP_DE() {
    _ip_POP_rr(REG_D, REG_E);
}

// 0xD2
void _ip_JP_NC_a16() {
    _ip_JP_f_a16(CHECK_CARRY, TRUE);
}

// 0xD3
void _ip_0xD3() {
    _ip_STOP();
}

// 0xD4
void _ip_CALL_NC_a16() {
    _ip_CALL_f_a16(CHECK_CARRY, FALSE);
}

// 0xD5
void _ip_PUSH_DE() {
    _ip_PUSH_rr(REG_D, REG_E);
}

// 0xD6
void _ip_SUB_d8() {
    _ip_SUB_r_n(REG_A);
}

// 0xD7
void _ip_RST_10H() {
    _ip_RST_addr(0x10);
}

// 0xD8
void _ip_RET_C() {
    _ip_RET_f(CHECK_CARRY, FALSE);
}

// 0xD9
void _ip_RETI() {
    sm_set_reg_intr(TRUE);
    _ip_RET();
}

// 0xDA
void _ip_JP_C_a16() {
    _ip_JR_f_r8(CHECK_CARRY, FALSE);
}

// 0xDB
void _ip_0xDB() {
    _ip_STOP();
}

// 0xDC
void _ip_CALL_C_a16() {
    _ip_CALL_f_a16(CHECK_CARRY, FALSE);
}

// 0xDD
void _ip_0xDD() {
    _ip_STOP();
}

// 0xDE
void _ip_SBC_A_d8() {
    _ip_SBC_r_n(REG_A);
}

// 0xDF
void _ip_RST_18H() {
    _ip_RST_addr(0x18);
}

// 0xE0
void _ip_LDH_dn_A() {
    _ip_LD_dn_r(REG_A);
}

// 0xE1
void _ip_POP_HL() {
    _ip_POP_rr(REG_H, REG_L);
}

// 0xE2
void _ip_LDH_dC_A() {
    _ip_LD_dr_r(REG_C, REG_A);
}

// 0xE3
void _ip_0xE3() {
    _ip_STOP();
}

// 0xE4
void _ip_0xE4() {
    _ip_STOP();
}

// 0xE5
void _ip_PUSH_HL() {
    _ip_PUSH_rr(REG_H, REG_L);
}

// 0xE6
void _ip_AND_n() {
    _ip_AND_r_n(REG_A);
}

// 0xE7
void _ip_RST_20H() {
    _ip_RST_addr(0x20);
}

// 0xE8
void _ip_ADD_SP_r8() {
    _ip_ADD_special_r8(sm_get_reg_sp);
}

// 0xE9
void _ip_JP_dHL() {
    _ip_JP_drr(REG_H, REG_L);
}

// 0xEA
void _ip_LD_dnn_A() {
    _ip_LD_dnn_r(REG_A);
}

// 0xEB
void _ip_0xEB() {
    _ip_STOP();
}

// 0xEC
void _ip_0xEC() {
    _ip_STOP();
}

// 0xED
void _ip_0xED() {
    _ip_STOP();
}

// 0xEE
void _ip_XOR_d8() {
    _ip_XOR_r_n(REG_A);
}

// 0xEF
void _ip_RST_28H() {
    _ip_RST_addr(0x28);
}

// 0xF0
void _ip_LDH_A_dn() {
    _ip_LD_r_n(REG_A);
}

// 0xF1
void _ip_POP_AF() {
    _ip_POP_rr(REG_A, REG_F);
}

// 0xF2
void _ip_LD_A_dC() {
    _ip_LD_r_dr(REG_A, REG_C);
}

// 0xF3
void _ip_DI() {
    sm_set_reg_intr(FALSE);
}

// 0xF4
void _ip_0xF4() {
    _ip_STOP();
}

// 0xF5
void _ip_PUSH_AF() {
    _ip_PUSH_rr(REG_A, REG_F);
}

// 0xF6
void _ip_OR_n() {
    _ip_OR_r_n(REG_A);
}

// 0xF7
void _ip_RST_30H() {
    _ip_RST_addr(0x30);
}

// 0xF8
void _ip_LD_HL_SPn() {
    _ip_LD_rr_special(REG_H, REG_L, sm_get_reg_sp, sm_getmemaddr8(sm_get_reg_pc()));
    sm_inc_reg_pc(BYTE);
}

// 0xF9
void _ip_LD_SP_HL() {
    _ip_LD_special_rr(sm_set_reg_sp, REG_H, REG_L);
}

// 0xFA
void _ip_LD_A_dnn() {
    _ip_LD_r_dnn(REG_A);
}

// 0xFB
void _ip_EI() {
    sm_set_reg_intr(TRUE);
}

// 0xFC
void _ip_0xFC() {
    _ip_STOP();
}

// 0xFD
void _ip_0xFD() {
    _ip_STOP();
}

// 0xFE
void _ip_CP_d8() {
    _ip_CP_r_n(REG_A);
}

// 0xFF
void _ip_RST_38H() {
    _ip_RST_addr(0x38);
}


// opcode map
static void (*_ip_opcodes[])() = {
    
    /* 0x */
    /* x0 */ _ip_NOP,
    /* x1 */ _ip_LD_BC_d16,
    /* x2 */ _ip_LD_dBC_A,
    /* x3 */ _ip_INC_BC,
    /* x4 */ _ip_INC_B,
    /* x5 */ _ip_DEC_B,
    /* x6 */ _ip_LD_B_d8,
    /* x7 */ _ip_RLC_A,
    /* x8 */ _ip_LD_da16_SP,
    /* x9 */ _ip_ADD_HL_BC,
    /* xA */ _ip_LD_A_dBC,
    /* xB */ _ip_DEC_BC,
    /* xC */ _ip_INC_C,
    /* xD */ _ip_DEC_C,
    /* xE */ _ip_LD_C_d8,
    /* xF */ _ip_RRC_A,
    
    /* 1x */
    /* x0 */ _ip_STOP,
    /* x1 */ _ip_LD_DE_d16,
    /* x2 */ _ip_LD_dDE_A,
    /* x3 */ _ip_INC_DE,
    /* x4 */ _ip_INC_D,
    /* x5 */ _ip_DEC_D,
    /* x6 */ _ip_LD_D_d8,
    /* x7 */ _ip_RLA,
    /* x8 */ _ip_JR_r8,
    /* x9 */ _ip_ADD_HL_DE,
    /* xA */ _ip_LD_A_dDE,
    /* xB */ _ip_DEC_DE,
    /* xC */ _ip_INC_E,
    /* xD */ _ip_DEC_E,
    /* xE */ _ip_LD_E_d8,
    /* xF */ _ip_RRA,
    
    /* 2x */
    /* x0 */ _ip_JR_NZ_r8,
    /* x1 */ _ip_LD_HL_d16,
    /* x2 */ _ip_LD_dHLp_A,
    /* x3 */ _ip_INC_HL,
    /* x4 */ _ip_INC_H,
    /* x5 */ _ip_DEC_H,
    /* x6 */ _ip_LD_H_d8,
    /* x7 */ _ip_DAA,
    /* x8 */ _ip_JR_Z_r8,
    /* x9 */ _ip_ADD_HL_HL,
    /* xA */ _ip_LD_A_dHLp,
    /* xB */ _ip_DEC_HL,
    /* xC */ _ip_INC_L,
    /* xD */ _ip_DEC_L,
    /* xE */ _ip_LD_L_d8,
    /* xF */ _ip_CPL,
    
    /* 3x */
    /* x0 */ _ip_JR_NC_r8,
    /* x1 */ _ip_LD_SP_d16,
    /* x2 */ _ip_LD_dHLd_A,
    /* x3 */ _ip_INC_SP,
    /* x4 */ _ip_INC_dHL,
    /* x5 */ _ip_DEC_dHL,
    /* x6 */ _ip_LD_dHL_d8,
    /* x7 */ _ip_SCF,
    /* x8 */ _ip_JR_C_r8,
    /* x9 */ _ip_ADD_HL_SP,
    /* xA */ _ip_LD_A_dHLd,
    /* xB */ _ip_DEC_SP,
    /* xC */ _ip_INC_A,
    /* xD */ _ip_DEC_A,
    /* xE */ _ip_LD_A_d8,
    /* xF */ _ip_CCF,
    
    /* 4x */
    /* x0 */ _ip_LD_B_B,
    /* x1 */ _ip_LD_B_C,
    /* x2 */ _ip_LD_B_D,
    /* x3 */ _ip_LD_B_E,
    /* x4 */ _ip_LD_B_H,
    /* x5 */ _ip_LD_B_L,
    /* x6 */ _ip_LD_B_dHL,
    /* x7 */ _ip_LD_B_A,
    /* x8 */ _ip_LD_C_B,
    /* x9 */ _ip_LD_C_C,
    /* xA */ _ip_LD_C_D,
    /* xB */ _ip_LD_C_E,
    /* xC */ _ip_LD_C_H,
    /* xD */ _ip_LD_C_L,
    /* xE */ _ip_LD_C_dHL,
    /* xF */ _ip_LD_C_A,
    
    /* 5x */
    /* x0 */ _ip_LD_D_B,
    /* x1 */ _ip_LD_D_C,
    /* x2 */ _ip_LD_D_D,
    /* x3 */ _ip_LD_D_E,
    /* x4 */ _ip_LD_D_H,
    /* x5 */ _ip_LD_D_L,
    /* x6 */ _ip_LD_D_dHL,
    /* x7 */ _ip_LD_D_A,
    /* x8 */ _ip_LD_E_B,
    /* x9 */ _ip_LD_E_C,
    /* xA */ _ip_LD_E_D,
    /* xB */ _ip_LD_E_E,
    /* xC */ _ip_LD_E_H,
    /* xD */ _ip_LD_E_L,
    /* xE */ _ip_LD_E_dHL,
    /* xF */ _ip_LD_E_A,
    
    /* 6x */
    /* x0 */ _ip_LD_H_B,
    /* x1 */ _ip_LD_H_C,
    /* x2 */ _ip_LD_H_D,
    /* x3 */ _ip_LD_H_E,
    /* x4 */ _ip_LD_H_H,
    /* x5 */ _ip_LD_H_L,
    /* x6 */ _ip_LD_H_dHL,
    /* x7 */ _ip_LD_H_A,
    /* x8 */ _ip_LD_L_B,
    /* x9 */ _ip_LD_L_C,
    /* xA */ _ip_LD_L_D,
    /* xB */ _ip_LD_L_E,
    /* xC */ _ip_LD_L_H,
    /* xD */ _ip_LD_L_L,
    /* xE */ _ip_LD_L_dHL,
    /* xF */ _ip_LD_L_A,
    
    /* 7x */
    /* x0 */ _ip_LD_dHL_B,
    /* x1 */ _ip_LD_dHL_C,
    /* x2 */ _ip_LD_dHL_D,
    /* x3 */ _ip_LD_dHL_E,
    /* x4 */ _ip_LD_dHL_H,
    /* x5 */ _ip_LD_dHL_L,
    /* x6 */ _ip_HALT,
    /* x7 */ _ip_LD_dHL_A,
    /* x8 */ _ip_LD_A_B,
    /* x9 */ _ip_LD_A_C,
    /* xA */ _ip_LD_A_D,
    /* xB */ _ip_LD_A_E,
    /* xC */ _ip_LD_A_H,
    /* xD */ _ip_LD_A_L,
    /* xE */ _ip_LD_A_dHL,
    /* xF */ _ip_LD_A_A,
    
    /* 8x */
    /* x0 */ _ip_ADD_A_B,
    /* x1 */ _ip_ADD_A_C,
    /* x2 */ _ip_ADD_A_D,
    /* x3 */ _ip_ADD_A_E,
    /* x4 */ _ip_ADD_A_H,
    /* x5 */ _ip_ADD_A_L,
    /* x6 */ _ip_ADD_A_dHL,
    /* x7 */ _ip_ADD_A_A,
    /* x8 */ _ip_ADC_A_B,
    /* x9 */ _ip_ADC_A_C,
    /* xA */ _ip_ADC_A_D,
    /* xB */ _ip_ADC_A_E,
    /* xC */ _ip_ADC_A_H,
    /* xD */ _ip_ADC_A_L,
    /* xE */ _ip_ADC_A_dHL,
    /* xF */ _ip_ADC_A_A,
    
    /* 9x */
    /* x0 */ _ip_SUB_A_B,
    /* x1 */ _ip_SUB_A_C,
    /* x2 */ _ip_SUB_A_D,
    /* x3 */ _ip_SUB_A_E,
    /* x4 */ _ip_SUB_A_H,
    /* x5 */ _ip_SUB_A_L,
    /* x6 */ _ip_SUB_A_dHL,
    /* x7 */ _ip_SUB_A_A,
    /* x8 */ _ip_SBC_A_B,
    /* x9 */ _ip_SBC_A_C,
    /* xA */ _ip_SBC_A_D,
    /* xB */ _ip_SBC_A_E,
    /* xC */ _ip_SBC_A_H,
    /* xD */ _ip_SBC_A_L,
    /* xE */ _ip_SBC_A_dHL,
    /* xF */ _ip_SBC_A_A,
    
    /* Ax */
    /* x0 */ _ip_AND_A_B,
    /* x1 */ _ip_AND_A_C,
    /* x2 */ _ip_AND_A_D,
    /* x3 */ _ip_AND_A_E,
    /* x4 */ _ip_AND_A_H,
    /* x5 */ _ip_AND_A_L,
    /* x6 */ _ip_AND_A_dHL,
    /* x7 */ _ip_AND_A_A,
    /* x8 */ _ip_XOR_A_B,
    /* x9 */ _ip_XOR_A_C,
    /* xA */ _ip_XOR_A_D,
    /* xB */ _ip_XOR_A_E,
    /* xC */ _ip_XOR_A_H,
    /* xD */ _ip_XOR_A_L,
    /* xE */ _ip_XOR_A_dHL,
    /* xF */ _ip_XOR_A_A,
    
    /* Bx */
    /* x0 */ _ip_OR_A_B,
    /* x1 */ _ip_OR_A_C,
    /* x2 */ _ip_OR_A_D,
    /* x3 */ _ip_OR_A_E,
    /* x4 */ _ip_OR_A_H,
    /* x5 */ _ip_OR_A_L,
    /* x6 */ _ip_OR_A_dHL,
    /* x7 */ _ip_OR_A_A,
    /* x8 */ _ip_CP_A_B,
    /* x9 */ _ip_CP_A_C,
    /* xA */ _ip_CP_A_D,
    /* xB */ _ip_CP_A_E,
    /* xC */ _ip_CP_A_H,
    /* xD */ _ip_CP_A_L,
    /* xE */ _ip_CP_A_dHL,
    /* xF */ _ip_CP_A_A,
    
    /* Cx */
    /* x0 */ _ip_RET_NZ,
    /* x1 */ _ip_POP_BC,
    /* x2 */ _ip_JP_NZ_a16,
    /* x3 */ _ip_JP_a16,
    /* x4 */ _ip_CALL_NZ_a16,
    /* x5 */ _ip_PUSH_BC,
    /* x6 */ _ip_ADD_A_d8,
    /* x7 */ _ip_RST_00H,
    /* x8 */ _ip_RET_Z,
    /* x9 */ _ip_RET,
    /* xA */ _ip_JP_Z_a16,
    /* xB */ _ip_PREFIX_CB,
    /* xC */ _ip_CALL_Z_a16,
    /* xD */ _ip_CALL_a16,
    /* xE */ _ip_ADC_A_d8,
    /* xF */ _ip_RST_08H,
    
    /* Dx */
    /* x0 */ _ip_RET_NC,
    /* x1 */ _ip_POP_DE,
    /* x2 */ _ip_JP_NC_a16,
    /* x3 */ _ip_0xD3,
    /* x4 */ _ip_CALL_NC_a16,
    /* x5 */ _ip_PUSH_DE,
    /* x6 */ _ip_SUB_d8,
    /* x7 */ _ip_RST_10H,
    /* x8 */ _ip_RET_C,
    /* x9 */ _ip_RETI,
    /* xA */ _ip_JP_C_a16,
    /* xB */ _ip_0xDB,
    /* xC */ _ip_CALL_C_a16,
    /* xD */ _ip_0xDD,
    /* xE */ _ip_SBC_A_d8,
    /* xF */ _ip_RST_18H,
    
    /* Ex */
    /* x0 */ _ip_LDH_dn_A,
    /* x1 */ _ip_POP_HL,
    /* x2 */ _ip_LDH_dC_A,
    /* x3 */ _ip_0xE3,
    /* x4 */ _ip_0xE4,
    /* x5 */ _ip_PUSH_HL,
    /* x6 */ _ip_AND_n,
    /* x7 */ _ip_RST_20H,
    /* x8 */ _ip_ADD_SP_r8,
    /* x9 */ _ip_JP_dHL,
    /* xA */ _ip_LD_dnn_A,
    /* xB */ _ip_0xEB,
    /* xC */ _ip_0xEC,
    /* xD */ _ip_0xED,
    /* xE */ _ip_XOR_d8,
    /* xF */ _ip_RST_28H,
    
    /* Fx */
    /* x0 */ _ip_LDH_A_dn,
    /* x1 */ _ip_POP_AF,
    /* x2 */ _ip_LD_A_dC,
    /* x3 */ _ip_DI,
    /* x4 */ _ip_0xF4,
    /* x5 */ _ip_PUSH_AF,
    /* x6 */ _ip_OR_n,
    /* x7 */ _ip_RST_30H,
    /* x8 */ _ip_LD_HL_SPn,
    /* x9 */ _ip_LD_SP_HL,
    /* xA */ _ip_LD_A_dnn,
    /* xB */ _ip_EI,
    /* xC */ _ip_0xFC,
    /* xD */ _ip_0xFD,
    /* xE */ _ip_CP_d8,
    /* xF */ _ip_RST_38H
};

///////**** Public ****///////

void ip_execute(uint8_t opcode) {
    (*_ip_opcodes[opcode])();
}




