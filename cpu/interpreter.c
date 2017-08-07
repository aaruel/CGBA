//
//  interpreter.c
//  CGBA
//

#include "interpreter.h"
#include "memorymodule.h"

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
 * LOADS
 */

// LD 8bit reg, 8bit reg
void _ip_LD_r_r(sm_regs e1, sm_regs e2) {
    sm_set_reg(e1, e2);
    sm_inc_clock(1);
}

// LD 8bit reg, 8bit num
void _ip_LD_r_n(sm_regs e1) {
    sm_set_reg(e1, sm_getmemaddr8(sm_get_reg_pc()));
    sm_inc_reg_pc(1);
    sm_inc_clock(2);
}

// LD (16bit regs) , 8bit num
void _ip_LD_drr_n(sm_regs e1, sm_regs e2) {
    sm_setmemaddr8( sm_get_reg16(e1, e2), sm_getmemaddr8( sm_get_reg_pc() ) );
    sm_inc_clock(3);
}

// LD (16bit regs) , 8bit reg
void _ip_LD_drr_r(sm_regs e1, sm_regs e2, sm_regs e3) {
    sm_setmemaddr8( sm_get_reg16(e1, e2), sm_get_reg(e3) );
    sm_inc_clock(2);
}

// LD 8bit reg, (16bit regs)
void _ip_LD_r_drr(sm_regs e1, sm_regs e2, sm_regs e3) {
    sm_set_reg(e1, sm_getmemaddr16(sm_get_reg16(e2, e3)));
    sm_inc_clock(2);
}

// LD 16bit regs , 16bit num
void _ip_LD_rr_nn(sm_regs e1, sm_regs e2) {
    sm_set_reg( e2, sm_getmemaddr8( sm_get_reg_pc() ) );
    sm_set_reg( e1, sm_getmemaddr8( sm_get_reg_pc() + BYTE ) );
    sm_inc_reg_pc(2);
    sm_inc_clock(3);
}

/*
 * INC
 */

// INC 8bit reg
void _ip_INC_r(sm_regs e1) {
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r + 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0001, r + 1, CHECK_ZERO | CHECK_HCARRY );
    sm_inc_clock(1);
}

// INC 16bit regs
void _ip_INC_rr(sm_regs e1, sm_regs e2) {
    const uint16_t rr = sm_get_reg16(e1, e2) + 1;
    sm_set_reg(e1, rr >> 8);
    sm_set_reg(e2, rr & 0xFF);
    sm_inc_clock(1);
}

/*
 * DEC
 */

// DEC 8bit reg
void _ip_DEC_r(sm_regs e1) {
    const uint8_t r = sm_get_reg(e1);
    sm_set_reg(e1, r - 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0101, r - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY );
    sm_inc_clock(1);
}

// DEC 16bit regs
void _ip_DEC_rr(sm_regs e1, sm_regs e2) {
    sm_set_reg16(e1, e2, sm_get_reg16(e1, e2) - 1);
    sm_inc_clock(2);
}

/*
 * ADD
 */

// ADD 16bit regs, 16bit regs
void _ip_ADD_rr_rr(sm_regs e1, sm_regs e2, sm_regs e3, sm_regs e4) {
    const uint16_t rr1 = sm_get_reg16(e1, e2);
    const uint16_t rr2 = sm_get_reg16(e3, e4);
    sm_set_reg16(REG_H, REG_L, rr1+rr2);
    _ip_apply_flags(sm_get_reg(REG_F)&0b1000, rr1+rr2, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
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
    _ip_apply_flags(0b0000, _ip_rotl16(A, 1), CHECK_CARRY);
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
    _ip_apply_flags(0b0000, _ip_rotr16(A, 1), CHECK_CARRY);
    sm_inc_clock(1);
}

// 0x10 // undefined
void _ip_STOP() {
    sm_set_reg_stop(TRUE);
    // pause program
    printf("STOP or UNDEFINED command called");
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

// opcode map
static void (*_ip_opcodes[])() = {
    /* 0x */ _ip_NOP,  _ip_LD_BC_d16, _ip_LD_dBC_A, _ip_INC_BC, _ip_INC_B, _ip_DEC_B, _ip_LD_B_d8, _ip_RLC_A, _ip_LD_da16_SP, _ip_ADD_HL_BC, _ip_LD_A_dBC, _ip_DEC_BC, _ip_INC_C, _ip_DEC_C, _ip_LD_C_d8, _ip_RRC_A,
    /* 1x */ _ip_STOP, _ip_LD_DE_d16, _ip_LD_dDE_A, _ip_INC_DE, _ip_INC_D, _ip_DEC_D, _ip_LD_D_d8
};

///////**** Public ****///////

void ip_execute(uint8_t opcode) {
    (*_ip_opcodes[opcode])();
}




