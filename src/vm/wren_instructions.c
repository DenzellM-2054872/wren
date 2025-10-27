#include <assert.h>
#include <stdlib.h>
#include "wren_instructions.h"


typedef enum opMode{
    iABC,
    iABx,
    iAsBx,
    iAbCx,
    isJx
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

void setInstructionField(Instruction* instruction, Field field, int value){
    switch(field){
        case Field_A:
            *instruction = SET_A(*instruction, value);
            break;
        case Field_B:
            *instruction = SET_B(*instruction, value);
            break;
        case Field_C:
            *instruction = SET_C(*instruction, value);
            break;
        case Field_Bx:
            *instruction = SET_Bx(*instruction, value);
            break;
        case Field_sBx:
            *instruction = SET_Bx(*instruction, abs(value));
            *instruction = SET_s(*instruction, value < 0 ? 1 : 0);
            break;
        case Field_sJx:
            *instruction = SET_sJx(*instruction, value);
            break;
        default:
            //do nothing for OP
            break;
    }
}

Instruction makeInstructionABC(RegCode opcode, int a, int b, int c){
    assert(opModes[opcode] == iABC);
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)b) << POS_B) | 
            (((Instruction)c) << POS_C);
}

Instruction makeInstructionAbCx(RegCode opcode, int a, int b, int cx){
    assert(opModes[opcode] == iAbCx);
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)b) << POS_b) | 
            (((Instruction)cx) << POS_Cx);
}

Instruction makeInstructionABx(RegCode opcode, int a, int bx){
    assert(opModes[opcode] == iABx);
    return  ((Instruction)opcode) | 
            (((Instruction)a) << POS_A) | 
            (((Instruction)abs(bx)) << POS_Bx);
}

Instruction makeInstructionAsBx(RegCode opcode, int a, int bx){
    assert(opModes[opcode] == iAsBx);
    return  ((Instruction)opcode)                       | 
            (((Instruction)a) << POS_A)                 | 
            (((Instruction)abs(bx)) << POS_Bx)          |
            ((bx < 0 ? 1 : 0) << (POS_Bx + SIZE_Bx - 1));
}

Instruction makeInstructionsJx(RegCode opcode, int sJx){
    assert(opModes[opcode] == isJx);
    return  ((Instruction)opcode) | 
            (((Instruction)sJx + OFFSET_sJx) << POS_sJx);
}