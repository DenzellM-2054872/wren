/*
*   R[x] = the value of the register with index x
*   C[x] = the value of the constant with index x
*   G[x] = the value of the global variable with label x
*   RC[x] = the value of the register or constant with index x (R if x < #slots else C)
*
*/


//load local[Bx] into register[A]
REGOPCODE(LOADK, iABx)

//set G[C[Bx]] to the value in R[A]
REGOPCODE(SETGLOBAL, iABx)

//set R[A] to the value in G[C[Bx]]
REGOPCODE(GETGLOBAL, iABx)