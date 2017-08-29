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

// LD 16bit regs, 16bit num
void _ip_LD_rr_nn(sm_regs e1, sm_regs e2) {
    sm_set_reg( e2, sm_getmemaddr8( sm_get_reg_pc() ) );
    sm_set_reg( e1, sm_getmemaddr8( sm_get_reg_pc() + BYTE ) );
    sm_inc_reg_pc(2);
    sm_inc_clock(3);
}

// LD special reg, 16bit num
void _ip_LD_special_nn(void(*setter)(uint16_t)) {
    const uint16_t nn = sm_getmemaddr16(sm_get_reg_pc());
    (*setter)(nn);
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

// INC (16bit regs)
void _ip_INC_drr(sm_regs e1, sm_regs e2) {
    const uint16_t rr = sm_get_reg16(e1, e2);
    const uint16_t nn = sm_getmemaddr16(rr);
    sm_setmemaddr16(rr, nn + 1);
    _ip_apply_flags(sm_get_reg(REG_F)&0b0001, nn + 1, CHECK_ZERO | CHECK_HCARRY );
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
    _ip_apply_flags(sm_get_reg(REG_F)&0b0101, r - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY );
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
    _ip_apply_flags(sm_get_reg(REG_F)&0b0101, nn - 1, CHECK_ZERO | CHECK_OP | CHECK_HCARRY );
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

// ADD 16bit regs, 16bit regs
void _ip_ADD_rr_rr(sm_regs e1, sm_regs e2, sm_regs e3, sm_regs e4) {
    const uint16_t rr1 = sm_get_reg16(e1, e2);
    const uint16_t rr2 = sm_get_reg16(e3, e4);
    sm_set_reg16(REG_H, REG_L, rr1+rr2);
    _ip_apply_flags(sm_get_reg(REG_F)&0b1000, rr1+rr2, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

// ADD 16bit regs, special
void _ip_ADD_rr_special(sm_regs e1, sm_regs e2, uint16_t(*getter)()) {
    const uint16_t rr1 = sm_get_reg16(e1, e2);
    const uint16_t rr2 = (*getter)();
    sm_set_reg16(REG_H, REG_L, rr1+rr2);
    _ip_apply_flags(sm_get_reg(REG_F)&0b1000, rr1+rr2, CHECK_HCARRY | CHECK_CARRY );
    sm_inc_clock(2);
}

/*
 * JUMP
 */

// JR r8
void _ip_JR_r8() {
    const int8_t r8 = (int8_t)sm_get_reg_pc();
    const uint8_t pc = sm_get_reg_pc() + BYTE;
    sm_set_reg_pc(pc + r8);
    sm_inc_clock(3);
}

// JR chk flag, r8
void _ip_JR_f_r8(flag_check flags, uint8_t flag_invert /* For the NZ case */) {
    const uint8_t f = (sm_get_reg(REG_F)) & flags;
    if (flag_invert && !f) {
        // calls across instructions
        _ip_JR_r8();
    }
    else if (f) {
        _ip_JR_r8();
    }
    else {
        sm_inc_clock(2);
    }
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

// 0x17
void _ip_RLA() {
    const uint8_t A = sm_get_reg(REG_A);
    sm_set_reg(REG_A, _ip_rotlc(A));
    _ip_apply_flags(0b0000, _ip_rotlc(A), CHECK_CARRY);
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
    _ip_apply_flags(0b0000, _ip_rotrc(A), CHECK_CARRY);
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
    _ip_apply_flags(sm_get_reg(REG_F) & 0b0100, r, CHECK_ZERO | CHECK_CARRY);
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
void _ip_LD_A_dHL() {
    _ip_LD_r_drr(REG_A, REG_H, REG_L);
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
    _ip_apply_flags(F | 0b0110, 0, 0);
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


// opcode map
static void (*_ip_opcodes[])() = {
    /* 0x */ _ip_NOP, _ip_LD_BC_d16, _ip_LD_dBC_A, _ip_INC_BC, _ip_INC_B, _ip_DEC_B, _ip_LD_B_d8, _ip_RLC_A, _ip_LD_da16_SP, _ip_ADD_HL_BC, _ip_LD_A_dBC, _ip_DEC_BC, _ip_INC_C, _ip_DEC_C, _ip_LD_C_d8, _ip_RRC_A,
    /* 1x */ _ip_STOP, _ip_LD_DE_d16, _ip_LD_dDE_A, _ip_INC_DE, _ip_INC_D, _ip_DEC_D, _ip_LD_D_d8, _ip_RLA, _ip_JR_r8, _ip_ADD_HL_DE, _ip_LD_A_dDE, _ip_DEC_DE, _ip_INC_E, _ip_DEC_E, _ip_LD_E_d8, _ip_RRA,
    /* 2x */ _ip_JR_NZ_r8, _ip_LD_HL_d16, _ip_LD_dHLp_A, _ip_INC_HL, _ip_INC_H, _ip_DEC_H, _ip_LD_H_d8, _ip_DAA, _ip_JR_Z_r8, _ip_ADD_HL_HL, _ip_LD_A_dHL, _ip_DEC_HL, _ip_INC_L, _ip_DEC_L, _ip_LD_L_d8, _ip_CPL,
    /* 3x */ _ip_JR_NC_r8, _ip_LD_SP_d16, _ip_LD_dHLd_A, _ip_INC_SP, _ip_INC_dHL, _ip_DEC_dHL, _ip_LD_dHL_d8, _ip_SCF, _ip_JR_C_r8, _ip_ADD_HL_SP, _ip_LD_A_dHLd, _ip_DEC_SP, _ip_INC_A, _ip_DEC_A, _ip_LD_A_d8, _ip_CCF,
};

///////**** Public ****///////

void ip_execute(uint8_t opcode) {
    (*_ip_opcodes[opcode])();
}




