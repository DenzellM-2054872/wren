/*
 *   R[x] = the value of the register with index x
 *   K[x] = the value of the constant with index x
 *   U[x] = the value of the upvalue with index x
 *   G[x] = the value of the global variable with label x
 *   RC[x] = the value of the register or constant with index x (R if x < #slots else C)
 *
 */

// R[A] := K[Bx]
OPCODE(LOADK, iABx)
// R[A] := null
// Only gets called by compiler generated code. user NULLs go through constant table
OPCODE(LOADNULL, iABC)
// R[A] := B, if C pc++
// Only gets called by compiler generated code. user boolean go through constant table
OPCODE(LOADBOOL, iABC)

// R[A] := R[B]
OPCODE(MOVE, iABC)

// R[A] := U[Bx]
OPCODE(GETUPVAL, iABx)
// U[Bx] := R[A]
OPCODE(SETUPVAL, iABx)

// R[A] := G[Bx]
OPCODE(GETGLOBAL, iABx)
// G[Bx] := R[A]
OPCODE(SETGLOBAL, iABx)

// R[A] := R[B][R[C]]
OPCODE(GETFIELD, iABC)
// R[B][R[C]] := R[A]
OPCODE(SETFIELD, iABC)

// R[A] := R[A].Cx(R[A + 1], ... R[A + B])
OPCODE(TAILCALL, ivABC)

// R[A] := R[A].Cx(R[A + 1], ... R[A + B])
OPCODE(CALL, ivABC)

// R[A] := R[A + b + 1].Cx(R[A + 1], ... R[A + B])
OPCODE(CALLSUPER, ivABC)

// if R[B] == C then pc++
// we assume the next instruction is a jump
OPCODE(TEST, iABC)
// if R[A] pc += isJx
OPCODE(JUMP, isJx)

// ends function and puts R[A] into R[0] if B == 1 else puts null into R[0]
OPCODE(RETURN, iABC)

// close upvalue in R[A]
OPCODE(CLOSE, iABC)

// load closure for function K[Bx] into register[A]
OPCODE(CLOSURE, iABx)

// create new instance of class in R[A] if (bool)Bx the class is foreign
OPCODE(CONSTRUCT, iABx)

// load class for object in R[A] with |B| fields, is foreign if B < 0
OPCODE(CLASS, iAsBx)
// ends class definition for class in R[A]
OPCODE(ENDCLASS, iABC)

// add method R[A - 1] to the class in R[A] with symbol |B|, if B < 0 the method is static
OPCODE(METHOD, iAsBx)

// import module with name K[Bx] into R[A]
OPCODE(IMPORTMODULE, iABx)
// import variable K[Bx] into R[A]
OPCODE(IMPORTVAR, iABx)

// does nothing, strictly debugging purposes
OPCODE(NOOP, iABC)

//=== New opcodes ==//
// if (R[B] == R[C]) ~= A then pc++
OPCODE(EQ, iABC)
// if (R[B] < R[C]) ~= A then pc++
OPCODE(LT, iABC)
// if (R[B] <= R[C]) ~= A then pc++
OPCODE(LTE, iABC)

// R[A] = R[B] + R[C]
// if R[B] is a list and k is set, concatenate R[C] and R[B]
// else append R[C] to R[B]
OPCODE(ADD, iABC)
// R[A] = R[B] - R[C]
OPCODE(SUB, iABC)
// R[A] = R[B] * R[C]
OPCODE(MUL, iABC)
// R[A] = R[B] / R[C]
OPCODE(DIV, iABC)
// R[A] = -R[B]
OPCODE(NEG, iABC)
// R[A] = !R[B]
OPCODE(NOT, iABC)

// if (R[B] == K[C]) ~= A then pc++
// if k the order of operands is swapped
OPCODE(EQK, iABC)
// if (R[B] < K[C]) ~= A then pc++
// if k the order of operands is swapped
OPCODE(LTK, iABC)
// if (R[B] <= K[C]) ~= A then pc++
// if k the order of operands is swapped
OPCODE(LTEK, iABC)

// R[A] = R[B] + K[C]
// if k the order of operands is swapped
OPCODE(ADDK, iABC)
// R[A] = R[B] - K[C]
// if k the order of operands is swapped
OPCODE(SUBK, iABC)
// R[A] = R[B] * K[C]
// if k the order of operands is swapped
OPCODE(MULK, iABC)
// R[A] = R[B] / K[C]
// if k the order of operands is swapped
OPCODE(DIVK, iABC)

// R[A] = R[B] iterate R[C]
// if k == 1 use K[C] instead of R[C]
OPCODE(ITERATE, iABC)

// R[A] = R[B] iteratorValue R[C]
// if k == 1 use K[C] instead of R[C]
OPCODE(ITERATORVALUE, iABC)

OPCODE(GETSUB, iABC)
OPCODE(SETSUB, iABC)

// add R[C] to list R[B] and store the result in R[A]
// if k == 1 concatenate R[C] instead of adding
OPCODE(ADDELEM, iABC)
OPCODE(ADDELEMK, iABC)

//set R[A] to a range from R[B] to R[C] or K[C] if k == 1
OPCODE(RANGE, iABC)