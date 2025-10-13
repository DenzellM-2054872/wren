/*
*   R[x] = the value of the register with index x
*   K[x] = the value of the constant with index x
*   G[x] = the value of the global variable with label x
*   RC[x] = the value of the register or constant with index x (R if x < #slots else C)
*
*/

//load boolean B into register[A] and pc++ if C
REGOPCODE(LOADBOOL, iABC)

//load null into register[A]
REGOPCODE(LOADNULL, iABC)

//load local[Bx] into register[A]
REGOPCODE(LOADK, iABx)

//copy the value in register[B] into register[A]
REGOPCODE(MOVE, iABC)

//set G[K[Bx]] to the value in R[A]
REGOPCODE(SETGLOBAL, iABx)

//set R[A] to the value in G[K[Bx]]
REGOPCODE(GETGLOBAL, iABx)

//load closure for function K[Bx] into register[A]
REGOPCODE(CLOSURE, iABx)

//call method in R[A] with B arguments and put the result in R[A]
REGOPCODE(CALL, iABC)

//call method K[C] with B arguments and put the result in R[A]
REGOPCODE(CALLK, iABC)

//ends function and puts R[A] into R[0] (if A is 0, returns null)
REGOPCODE(RETURN, iABC)