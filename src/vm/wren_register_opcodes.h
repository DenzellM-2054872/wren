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

//R[B][R[C]] := R[A]
REGOPCODE(SETFIELD, iABC)

//R[A] := R[B][R[C]]
REGOPCODE(GETFIELD, iABC)

//R[A] := R[B]
REGOPCODE(MOVE, iABC)

//G[Bx] := R[A]
REGOPCODE(SETGLOBAL, iABx)

//R[A] := G[Bx]
REGOPCODE(GETGLOBAL, iABx)

//U[Bx] := R[A]
REGOPCODE(SETUPVAL, iABx)

//R[A] := U[Bx]
REGOPCODE(GETUPVAL, iABx)

//if R[B] == C then pc++ else R[A] := R[B]
REGOPCODE(TEST, iABC)

//if R[A] pc += isJx
REGOPCODE(JUMP, isJx)

//close upvalue in R[A]
REGOPCODE(CLOSE, iABC)

//load closure for function K[Bx] into register[A]
REGOPCODE(CLOSURE, iABx)

//add method R[A - 1] to the class in R[A] with symbol |B|, if B < 0 the method is static
REGOPCODE(METHOD, iAsBx)

//load class for object in R[A] with |B| fields, is foreign if B < 0
REGOPCODE(CLASS, iAsBx)

//ends class definition for class in R[A]
REGOPCODE(ENDCLASS, iABC)

//i dont realy know tbh
REGOPCODE(CONSTRUCT, iABx)

//call method K[C] with arguments R[A]...R[A + B] and put the result in R[A]
REGOPCODE(CALLK, iAbCx)

//call super method K[C] with arguments R[A]...R[A + B] and put the result in R[A]
REGOPCODE(CALLSUPERK, iAbCx)

//ends function and puts R[A] into R[0]
REGOPCODE(RETURN, iABC)

//ends function and loads R[0] with null
REGOPCODE(RETURN0, iABC)

//does nothing, strictly debugging purposes
REGOPCODE(NOOP, iABC)

//does nothing, strictly debugging purposes
REGOPCODE(DATA, iABx)