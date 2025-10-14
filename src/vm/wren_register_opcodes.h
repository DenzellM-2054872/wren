/*
*   R[x] = the value of the register with index x
*   K[x] = the value of the constant with index x
*   U[x] = the value of the upvalue with index x
*   G[x] = the value of the global variable with label x
*   RC[x] = the value of the register or constant with index x (R if x < #slots else C)
*
*/

// R[A] := B, if C pc++ 
REGOPCODE(LOADBOOL, iABC)

// R[A] := null
REGOPCODE(LOADNULL, iABC)

//R[A] := K[Bx]
REGOPCODE(LOADK, iABx)

//R[A] := R[B]
REGOPCODE(MOVE, iABC)

//G[K[Bx]] := R[A]
REGOPCODE(SETGLOBAL, iABx)

//R[A] := G[K[Bx]]
REGOPCODE(GETGLOBAL, iABx)

//U[B] := R[A]
REGOPCODE(SETUPVAL, iABC)

//R[A] := U[B]
REGOPCODE(GETUPVAL, iABC)

//if R[B] != C then R[A] := R[B] else pc++
REGOPCODE(TEST, iABC)

//if R[A] pc += isJx
REGOPCODE(JUMP, isJx)

//load closure for function K[Bx] into register[A]
REGOPCODE(CLOSURE, iABx)

//call method in R[A] with B arguments and put the result in R[A]
REGOPCODE(CALL, iABC)

//call method K[C] with B arguments and put the result in R[A]
REGOPCODE(CALLK, iABC)

//ends function and puts R[A] into R[0] (if A is 0, returns null)
REGOPCODE(RETURN, iABC)

//does nothing, strictly debugging purposes
REGOPCODE(NOOP, iABC)