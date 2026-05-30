#ifndef fodi_debug_h
#define fodi_debug_h

#include "fodi_value.h"
#include "fodi_vm.h"

// Prints the stack trace for the current fiber.
//
// Used when a fiber throws a runtime error which is not caught.
void fodiDebugPrintStackTrace(FodiVM *vm);

// The "dump" functions are used for debugging Fodi itself. Normal code paths
// will not call them unless one of the various DEBUG_ flags is enabled.

// Prints a representation of [value] to stdout.
void fodiDumpValue(Value value);

void fodiDumpStack(ObjFiber *fiber, Value *start, int stackTop);

// Prints a representation of the bytecode for [fn] at instruction [i].
int fodiDumpInstruction(FodiVM *vm, ObjFn *fn, int i);

// Prints the disassembled code for [fn] to stdout.
void fodiDumpCode(FodiVM *vm, ObjFn *fn, int constantNr);

void fodiDumpConstants(ObjFn* func);

#endif
