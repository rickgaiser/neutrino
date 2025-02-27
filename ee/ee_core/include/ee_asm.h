#ifndef EE_ASM_H
#define EE_ASM_H


#include <tamtypes.h>
#include <ee_cop0_defs.h>


// COP0 register getters and setters
#define __ee_mfc0(reg) ({ u32 val; asm volatile("mfc0 %0, " #reg : "=r"(val)::"memory"); val; })
#define _ee_mfc0(reg) __ee_mfc0(reg)
#define __ee_mtc0(reg, val) ({ asm volatile("mtc0 %0, " #reg :: "r"(val):"memory"); })
#define _ee_mtc0(reg, val) __ee_mtc0(reg, val)

// DEBUG register getters and setters
static inline u32  _ee_mfbpc() { u32 val; asm volatile("mfbpc  %0":"=r"(val)::"memory"); return val; }
static inline u32  _ee_mfiab() { u32 val; asm volatile("mfiab  %0":"=r"(val)::"memory"); return val; }
static inline u32  _ee_mfiabm(){ u32 val; asm volatile("mfiabm %0":"=r"(val)::"memory"); return val; }
static inline u32  _ee_mfdab() { u32 val; asm volatile("mfdab  %0":"=r"(val)::"memory"); return val; }
static inline u32  _ee_mfdabm(){ u32 val; asm volatile("mfdabm %0":"=r"(val)::"memory"); return val; }
static inline u32  _ee_mfdvb() { u32 val; asm volatile("mfdvb  %0":"=r"(val)::"memory"); return val; }
static inline u32  _ee_mfdvbm(){ u32 val; asm volatile("mfdvbm %0":"=r"(val)::"memory"); return val; }
static inline void _ee_mtbpc (u32 val) {  asm volatile("mtbpc  %0"::"r"(val) :"memory"); }
static inline void _ee_mtiab (u32 val) {  asm volatile("mtiab  %0"::"r"(val) :"memory"); }
static inline void _ee_mtiabm(u32 val) {  asm volatile("mtiabm %0"::"r"(val) :"memory"); }
static inline void _ee_mtdab (u32 val) {  asm volatile("mtdab  %0"::"r"(val) :"memory"); }
static inline void _ee_mtdabm(u32 val) {  asm volatile("mtdabm %0"::"r"(val) :"memory"); }
static inline void _ee_mtdvb (u32 val) {  asm volatile("mtdvb  %0"::"r"(val) :"memory"); }
static inline void _ee_mtdvbm(u32 val) {  asm volatile("mtdvbm %0"::"r"(val) :"memory"); }

// SYNC instructions
static inline void _mem_barrier() { asm volatile(""      :::"memory"); }
static inline void _ee_sync  ()   { asm volatile("sync"  :::"memory"); } // same as sync.l
static inline void _ee_sync_l()   { asm volatile("sync.l":::"memory"); }
static inline void _ee_sync_p()   { asm volatile("sync.p":::"memory"); }

// Registers
static inline u32  _ee_get_reg_sp() { u32 val; asm volatile("move %0, $sp":"=r"(val)::"memory"); return val; }
static inline u32  _ee_get_reg_ra() { u32 val; asm volatile("move %0, $ra":"=r"(val)::"memory"); return val; }

// COP1 / FPU
static inline u32  _ee_cfc1_r31() { u32 val; asm volatile("cfc1 %0, $31":"=r"(val)::"memory"); return val; }

// DEBUG enable/disable
static inline u32 _ee_disable_bpc() {
    u32 val = _ee_mfbpc();
    _ee_sync_l();
    _ee_sync_p();
    _ee_mtbpc(EE_BPC_BED);
    _ee_sync_p();
    return val;
}
static inline void _ee_enable_bpc(u32 val) {
    _ee_mtbpc(val);
    _ee_sync_p();
}


#endif
