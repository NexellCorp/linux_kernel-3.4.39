#ifndef __TW9900_PRESET_H__
#define __TW9900_PRESET_H__

#include "tw9900.h"

struct reg_val tw9900_init_reg[] =
{
    {0x02, 0x44}, //Mux1 selected                                               
    {0x03, 0xa2},                                                               
    {0x07, 0x02},                                                               
    {0x08, 0x12},                                                               
    {0x09, 0xf0},                                                               
    {0x0a, 0x1c},                                                               
    {0x0b, 0xc0}, // 704                                                        
    {0x1b, 0x00},                                                               
    {0x10, 0x1e},                                                               
    {0x11, 0x64},                                                               
    {0x2f, 0xe6},                                                               
    {0x55, 0x00},                                                               
    {0xaf, 0x00},                                                               
    {0xb1, 0x20},                                                               
    {0xb4, 0x20}, 
};

#define TW9900_REGS	\
	(sizeof(tw9900_init_reg) / sizeof(tw9900_init_reg[0]))

#endif /*__TW9900_PRESET_H__*/
