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


// OPCODE DEFINITIONS
// http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html
// http://imrannazar.com/Gameboy-Z80-Opcode-Map
// 0x00
void _ip_NOP() {
    sm_inc_clock(1);
}

// 0x01
void _ip_LD_BC_d16() {
    sm_set_reg( REG_C, sm_getmemaddr8( sm_get_reg_pc() ) );
    sm_set_reg( REG_B, sm_getmemaddr8( sm_get_reg_pc() + BYTE ) );
    sm_inc_reg_pc(2);
    sm_inc_clock(3);
}

// 0x02
void _ip_LD_dBC_A() {
    sm_setmemaddr8( sm_get_reg16(REG_B, REG_C), sm_get_reg(REG_A) );
    sm_inc_clock(2);
}

// 0x03
void _ip_INC_BC() {
    const uint16_t BC = sm_get_reg16(REG_B, REG_C) + 1;
    sm_set_reg(REG_B, BC >> 8);
    sm_set_reg(REG_C, BC & 0xFF);
    sm_inc_clock(1);
}

// 0x04
void _ip_INC_B() {
    const uint8_t B = sm_get_reg(REG_B);
    sm_set_reg(REG_B, B + 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0001, B + 1, CHECK_ZERO | CHECK_HCARRY );
    sm_inc_clock(1);
}

// 0x05
void _ip_DEC_B() {
    const uint8_t B = sm_get_reg(REG_B);
    sm_set_reg(REG_B, B - 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0101, B - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY );
    sm_inc_clock(1);
}

// 0x06
void _ip_LD_B_d8() {
    sm_set_reg(REG_B, sm_getmemaddr8(sm_get_reg_pc()));
    sm_inc_reg_pc(1);
    sm_inc_clock(2);
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
    const uint16_t HL = sm_get_reg16(REG_H, REG_L);
    const uint16_t BC = sm_get_reg16(REG_B, REG_C);
    sm_set_reg16(REG_H, REG_L, HL+BC);
    _ip_apply_flags(sm_get_reg(REG_F)&0b1000, HL+BC, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// 0x0A
void _ip_LD_A_dBC() {
    sm_set_reg(REG_A, sm_getmemaddr16(sm_get_reg16(REG_B, REG_C)));
    sm_inc_clock(2);
}

// 0x0B
void _ip_DEC_BC() {
    sm_set_reg16(REG_B, REG_C, sm_get_reg16(REG_B, REG_C) - 1);
    sm_inc_clock(2);
}

// 0x0C
void _ip_INC_C() {
    const uint8_t C = sm_get_reg(REG_C);
    sm_set_reg(REG_C, C + 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0001, C + 1, CHECK_ZERO | CHECK_HCARRY);
    sm_inc_clock(1);
}

// 0x0D
void _ip_DEC_C() {
    const uint8_t C = sm_get_reg(REG_C);
    sm_set_reg(REG_C, C - 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0001, C - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY);
    sm_inc_clock(1);
}

// 0x0E
void _ip_LD_C_d8() {
    sm_set_reg(REG_C, sm_getmemaddr8(sm_get_reg_pc()));
    sm_inc_reg_pc(1);
    sm_inc_clock(2);
}

// 0x0F
void _ip_RRC_A() {
    const uint8_t A = sm_get_reg(REG_A);
    sm_set_reg(REG_A, _ip_rotr8(A, 1));
    _ip_apply_flags(0b0000, _ip_rotr16(A, 1), CHECK_CARRY);
    sm_inc_clock(1);
}

// opcode map
static void (*_ip_opcodes[])() = {
    /* 0x */ _ip_NOP, _ip_LD_BC_d16, _ip_LD_dBC_A, _ip_INC_BC, _ip_INC_B, _ip_DEC_B, _ip_LD_B_d8, _ip_RLC_A, _ip_LD_da16_SP, _ip_ADD_HL_BC, _ip_LD_A_dBC, _ip_DEC_BC, _ip_INC_C, _ip_DEC_C, _ip_LD_C_d8, _ip_RRC_A,
    /* 1x */
};

///////**** Public ****///////

void ip_execute(uint8_t opcode) {
    (*_ip_opcodes[opcode])();
}




