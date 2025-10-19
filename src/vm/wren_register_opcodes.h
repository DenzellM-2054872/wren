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

//this[B] := R[A]
REGOPCODE(SETFIELDTHIS, iABC)

//R[A] := this[B]
REGOPCODE(GETFIELDTHIS, iABC)

//R[A] := R[B]
REGOPCODE(MOVE, iABC)

//G[Bx] := R[A]
REGOPCODE(SETGLOBAL, iABx)

//R[A] := G[Bx]
REGOPCODE(GETGLOBAL, iABx)

//U[B] := R[A]
REGOPCODE(SETUPVAL, iABC)

//R[A] := U[B]
REGOPCODE(GETUPVAL, iABC)

//if R[B] != C then R[A] := R[B] else pc++
//if R[B] == C then pc++ else R[A] := R[B]
REGOPCODE(TEST, iABC)

//if R[A] pc += isJx
REGOPCODE(JUMP, isJx)

//load closure for function K[Bx] into register[A]
REGOPCODE(CLOSURE, iABx)

//add method R[B] to the class in R[A]
REGOPCODE(METHOD, iABC)

//load class for class object K[Bx] into register[A]
REGOPCODE(CLASS, iABC)

REGOPCODE(ENDCLASS, iABC)

//i dont realy know tbh
REGOPCODE(CONSTRUCT, iABx)

//call method in R[A] with B arguments and put the result in R[A]
REGOPCODE(CALL, iABC)

//call method K[C] with B arguments and put the result in R[A]
REGOPCODE(CALLK, iABC)

//ends function and puts R[A] into R[0]
REGOPCODE(RETURN, iABC)

//ends function and loads R[0] with null
REGOPCODE(RETURN0, iABC)

//does nothing, strictly debugging purposes
REGOPCODE(NOOP, iABC)