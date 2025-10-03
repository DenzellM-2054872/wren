#include "wren_instructions.h"


Instruction makeInstructionABC(Code opcode, int a, int b, int c){
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)b) << POS_B) | 
            (((Instruction)c) << POS_C);
}

Instruction makeInstructionABx(Code opcode, int a, int bx){
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)bx) << POS_Bx);
}

Instruction makeInstructionAsBx(Code opcode, int a, int bx){
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)bx + OFFSET_sBx) << POS_Bx);
}

Instruction makeInstructionsJx(Code opcode, int sJx){
    return  ((Instruction)opcode) | 
            (((Instruction)sJx + OFFSET_sJx) << POS_sJx);
}