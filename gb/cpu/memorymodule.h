//
//  statemachine.h
//  CGBA
//

#ifndef __CGBA__statemachine__
#define __CGBA__statemachine__

//General Internal Memory
//00000000-00003FFF   BIOS - System ROM         (16 KBytes)
//00004000-01FFFFFF   Not used
//02000000-0203FFFF   WRAM - On-board Work RAM  (256 KBytes) 2 Wait
//02040000-02FFFFFF   Not used
//03000000-03007FFF   WRAM - On-chip Work RAM   (32 KBytes)
//03008000-03FFFFFF   Not used
//04000000-040003FE   I/O Registers
//04000400-04FFFFFF   Not used
//Internal Display Memory
//05000000-050003FF   BG/OBJ Palette RAM        (1 Kbyte)
//05000400-05FFFFFF   Not used
//06000000-06017FFF   VRAM - Video RAM          (96 KBytes)
//06018000-06FFFFFF   Not used
//07000000-070003FF   OAM - OBJ Attributes      (1 Kbyte)
//07000400-07FFFFFF   Not used
//External Memory (Game Pak)
//08000000-09FFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 0
//0A000000-0BFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 1
//0C000000-0DFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 2
//0E000000-0E00FFFF   Game Pak SRAM    (max 64 KBytes) - 8bit Bus width
//0E010000-0FFFFFFF   Not used
//Unused Memory Area
//10000000-FFFFFFFF   Not used (upper 4bits of address bus unused)

uint8_t  sm_getmemaddr8 (uint16_t addr);
uint16_t sm_getmemaddr16(uint16_t addr);
void sm_setmemaddr8 (uint16_t addr, uint8_t  data);
void sm_setmemaddr16(uint16_t addr, uint16_t data);

void sm_inc_clock(uint16_t);
void sm_inc_mclock(uint16_t val);
uint16_t sm_get_mclock();
void sm_inc_tclock(uint16_t val);
uint16_t sm_get_tclock();

enum sm_regs {
    REG_A,
    REG_B,
    REG_C,
    REG_D,
    REG_E,
    REG_H,
    REG_L,
    REG_F
};
typedef enum sm_regs sm_regs;

void sm_set_reg(sm_regs e, uint8_t data);
uint8_t sm_get_reg(sm_regs e);
uint16_t sm_get_reg16(sm_regs e1, sm_regs e2);
void sm_set_reg16(sm_regs e1, sm_regs e2, uint16_t val);
void sm_set_reg_pc(uint16_t data);
void sm_inc_reg_pc(uint16_t inc );
void sm_set_reg_sp(uint16_t data);
void sm_inc_reg_sp(uint16_t inc );
uint16_t sm_get_reg_pc();
uint16_t sm_get_reg_sp();
void sm_set_reg_halt(uint8_t b);
void sm_set_reg_stop(uint8_t b);
void sm_set_reg_intr(uint8_t b);

#endif /* defined(__CGBA__statemachine__) */
