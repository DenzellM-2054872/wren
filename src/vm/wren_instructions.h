#ifndef WREN_INSTRUCTIONS_H
#define WREN_INSTRUCTIONS_H

#include "wren_vm.h"

typedef enum Field
{
    Field_OP,
    Field_A,
    Field_B,
    Field_C,
    Field_s,
    Field_Bx,
    Field_sBx,
    Field_sJx
} Field;

/*
 *   |0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|
 *   |   OP(6)   |        A(8)       |           B(9)           |           C(9)           |
 *   |   OP(6)   |        A(8)       |                      (s)Bx(18)                      |
 *   |   OP(6)   |                                 sJx(26)                                 |
 *   |   OP(6)   |        A(8)       |       vB(6)     |               vC(12)              |
 */
#define POS_OP 0
#define SIZE_OP 6

#define POS_A POS_OP + SIZE_OP
#define SIZE_A 8

#define POS_B POS_A + SIZE_A
#define SIZE_B 9

#define POS_C POS_B + SIZE_B
#define SIZE_C 9

#define POS_Bx POS_A + SIZE_A
#define SIZE_Bx 18

#define POS_sJx POS_OP + SIZE_OP
#define SIZE_sJx 26

#define POS_vB POS_A + SIZE_A
#define SIZE_vB 6

#define POS_vC POS_vB + SIZE_vB
#define SIZE_vC 12

#define MAXARG_Bx ((1 << SIZE_Bx) - 1)
#define OFFSET_sBx (MAXARG_Bx >> 1)

#define MAXARG_sJx ((1 << SIZE_sJx) - 1)
#define OFFSET_sJx (MAXARG_sJx >> 1)

#define MASK1(n, p) ((~((~(Instruction)0) << (n))) << (p))
#define MASK0(n, p) (~MASK1(n, p))

#define setarg(i, v, pos, size) ((i) = (((i) & MASK0(size, pos)) | \
                                        ((((Instruction)v) << pos) & MASK1(size, pos))))

#define getarg(i, pos, size) (int)(((i) >> (pos)) & MASK1(size, 0))

#define GET_OPCODE(i) (RegCode)(((i) >> POS_OP) & MASK1(SIZE_OP, 0))

#define GET_A(i) getarg(i, POS_A, SIZE_A)
#define SET_A(i, v) setarg(i, v, POS_A, SIZE_A)

#define GET_B(i) getarg(i, POS_B, SIZE_B)
#define SET_B(i, v) setarg(i, v, POS_B, SIZE_B)

#define GET_C(i) getarg(i, POS_C, SIZE_C)
#define SET_C(i, v) setarg(i, v, POS_C, SIZE_C)

#define GET_Bx(i) getarg(i, POS_Bx, SIZE_Bx)
#define SET_Bx(i, v) setarg(i, v, POS_Bx, SIZE_Bx)

#define GET_sBx(i) getarg(i, POS_Bx, SIZE_Bx - 1) * (getarg(i, POS_Bx + SIZE_Bx - 1, 1) == 0 ? 1 : -1)

#define GET_vB(i) getarg(i, POS_vB, SIZE_vB)
#define SET_vB(i, v) setarg(i, v, POS_vB, SIZE_vB)

#define GET_vC(i) getarg(i, POS_vC, SIZE_vC)
#define SET_vC(i, v) setarg(i, v, POS_vC, SIZE_vC)

#define setJx(i, v) setarg(i, v, POS_sJx, SIZE_sJx)

#define GET_s(i) getarg(i, POS_Bx + SIZE_Bx - 1, 1)
#define SET_s(i, v) setarg(i, v, POS_Bx + SIZE_Bx - 1, 1)

#define GET_sJx(i) \
    getarg(i, POS_sJx, SIZE_sJx) - OFFSET_sJx
#define SET_sJx(i, b) setJx((i), ((unsigned int)(b) + OFFSET_sJx))

// sets the A field of the last instruction in the buffer to target
void insertTarget(InstBuffer *instructions, int target);
void setInstructionField(Instruction *instruction, Field field, int value);

char* getOPName(int opcode);

Instruction makeInstructionABC(RegCode opcode, int a, int b, int c);
Instruction makeInstructionABx(RegCode opcode, int a, int bx);
Instruction makeInstructionAsBx(RegCode opcode, int a, int bx, bool s);
Instruction makeInstructionvABC(RegCode opcode, int a, int b, int cx);
Instruction makeInstructionsJx(RegCode opcode, int sJx);

#endif // WREN_INSTRUCTIONS_H