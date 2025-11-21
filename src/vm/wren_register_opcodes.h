/*
 *   R[x] = the value of the register with index x
 *   K[x] = the value of the constant with index x
 *   U[x] = the value of the upvalue with index x
 *   G[x] = the value of the global variable with label x
 *   RC[x] = the value of the register or constant with index x (R if x < #slots else C)
 *
 */

// R[A] := K[Bx]
REGOPCODE(LOADK, iABx)
// R[A] := null
REGOPCODE(LOADNULL, iABC)
// R[A] := B, if C pc++
REGOPCODE(LOADBOOL, iABC)

// R[A] := R[B]
REGOPCODE(MOVE, iABC)

// R[A] := U[Bx]
REGOPCODE(GETUPVAL, iABx)
// U[Bx] := R[A]
REGOPCODE(SETUPVAL, iABx)

// R[A] := G[Bx]
REGOPCODE(GETGLOBAL, iABx)
// G[Bx] := R[A]
REGOPCODE(SETGLOBAL, iABx)

// R[A] := R[B][R[C]]
REGOPCODE(GETFIELD, iABC)
// R[B][R[C]] := R[A]
REGOPCODE(SETFIELD, iABC)

// R[A] := R[A].Cx(R[A + 1], ... R[A + B])
REGOPCODE(CALLK, ivABC)
// R[A] := R[A + b + 1].Cx(R[A + 1], ... R[A + B])
REGOPCODE(CALLSUPERK, ivABC)

// if R[B] == C then pc++
// we assume the next instruction is a jump
REGOPCODE(TEST, iABC)
// if R[A] pc += isJx
REGOPCODE(JUMP, isJx)

// ends function and puts R[A] into R[0] if B == 1 else puts null into R[0]
REGOPCODE(RETURN, iABC)

// close upvalue in R[A]
REGOPCODE(CLOSE, iABC)

// load closure for function K[Bx] into register[A]
REGOPCODE(CLOSURE, iABx)

// create new instance of class in R[A] if (bool)Bx the class is foreign
REGOPCODE(CONSTRUCT, iABx)

// load class for object in R[A] with |B| fields, is foreign if B < 0
REGOPCODE(CLASS, iAsBx)
// ends class definition for class in R[A]
REGOPCODE(ENDCLASS, iABC)

// add method R[A - 1] to the class in R[A] with symbol |B|, if B < 0 the method is static
REGOPCODE(METHOD, iAsBx)

// import module with name K[Bx] into R[A]
REGOPCODE(IMPORTMODULE, iABx)
// import variable K[Bx] into R[A]
REGOPCODE(IMPORTVAR, iABx)

// does nothing, strictly debugging purposes
REGOPCODE(NOOP, iABC)

//=== New opcodes ==//
// if (R[B] == R[C]) ~= A then pc++
REGOPCODE(EQ, iABC)
// if (R[B] < R[C]) ~= A then pc++
REGOPCODE(LT, iABC)
// if (R[B] <= R[C]) ~= A then pc++
REGOPCODE(LTE, iABC)

// R[A] = R[B] + R[C]
REGOPCODE(ADD, iABC)
// R[A] = R[B] - R[C]
REGOPCODE(SUB, iABC)
// R[A] = R[B] * R[C]
REGOPCODE(MUL, iABC)
// R[A] = R[B] / R[C]
REGOPCODE(DIV, iABC)
// R[A] = -R[B]
REGOPCODE(NEG, iABC)
// R[A] = !R[B]
REGOPCODE(NOT, iABC)

// if (R[B] == K[C]) ~= A then pc++
// if k the order of operands is swapped
REGOPCODE(EQK, iABC)
// if (R[B] < K[C]) ~= A then pc++
// if k the order of operands is swapped
REGOPCODE(LTK, iABC)
// if (R[B] <= K[C]) ~= A then pc++
// if k the order of operands is swapped
REGOPCODE(LTEK, iABC)

// R[A] = R[B] + K[C]
// if k the order of operands is swapped
REGOPCODE(ADDK, iABC)
// R[A] = R[B] - K[C]
// if k the order of operands is swapped
REGOPCODE(SUBK, iABC)
// R[A] = R[B] * K[C]
// if k the order of operands is swapped
REGOPCODE(MULK, iABC)
// R[A] = R[B] / K[C]
// if k the order of operands is swapped
REGOPCODE(DIVK, iABC)

// R[A] = R[B] iter R[C]
REGOPCODE(ITERATE, iABC)
