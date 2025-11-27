#ifndef wren_debug_h
#define wren_debug_h

#include "wren_value.h"
#include "wren_vm.h"

// Prints the stack trace for the current fiber.
//
// Used when a fiber throws a runtime error which is not caught.
void wrenDebugRegisterPrintStackTrace(WrenVM *vm);

// The "dump" functions are used for debugging Wren itself. Normal code paths
// will not call them unless one of the various DEBUG_ flags is enabled.

// Prints a representation of [value] to stdout.
void wrenDumpValue(Value value);

void wrenDumpRegStack(ObjFiber *fiber, Value *start, int stackTop);

// Prints a representation of the bytecode for [fn] at instruction [i].
int wrenDumpRegisterInstruction(WrenVM *vm, ObjFn *fn, int i);

// Prints the disassembled code for [fn] to stdout.
void wrenDumpRegisterCode(WrenVM *vm, ObjFn *fn, int protoSlot);

void wrenDumpConstants(ObjFn* func);

#endif
