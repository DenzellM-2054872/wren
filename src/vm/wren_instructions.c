#include <assert.h>

#include "wren_instructions.h"


typedef enum opMode{
    iABC,
    iABx,
    iAsBx,
    iJx
} opMode;

static const opMode opModes[] = {
  #define REGOPCODE(_, mode) mode,
  #include "wren_register_opcodes.h"
  #undef REGOPCODE
};

void insertTarget(InstBuffer* instructions, int target){
    Instruction inst = instructions->data[instructions->count - 1];
    instructions->data[instructions->count - 1] = SET_A(inst, target);
}

Instruction makeInstructionABC(RegCode opcode, int a, int b, int c){
    assert(opModes[opcode] == iABC);
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)b) << POS_B) | 
            (((Instruction)c) << POS_C);
}

Instruction makeInstructionABx(RegCode opcode, int a, int bx){
    assert(opModes[opcode] == iABx);
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)bx) << POS_Bx);
}

Instruction makeInstructionAsBx(RegCode opcode, int a, int bx){
    assert(opModes[opcode] == iAsBx);
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)bx + OFFSET_sBx) << POS_Bx);
}

Instruction makeInstructionsJx(RegCode opcode, int sJx){
    assert(opModes[opcode] == iJx);
    return  ((Instruction)opcode) | 
            (((Instruction)sJx + OFFSET_sJx) << POS_sJx);
}