#ifndef fodi_vm_h
#define fodi_vm_h

#include "fodi_common.h"
#include "fodi_compiler.h"
#include "fodi_value.h"
#include "fodi_utils.h"

// The maximum number of temporary objects that can be made visible to the GC
// at one time.
#define FODI_MAX_TEMP_ROOTS 8

typedef enum
{
#define OPCODE(name, _) OP_##name,
#include "fodi_opcodes.h"
#undef OPCODE
  OP_COUNT
} Code;

// A handle to a value, basically just a linked list of extra GC roots.
//
// Note that even non-heap-allocated values can be stored here.
struct FodiHandle
{
  Value value;

  FodiHandle *prev;
  FodiHandle *next;
};

struct FodiVM
{
  ObjClass *boolClass;
  ObjClass *classClass;
  ObjClass *fiberClass;
  ObjClass *fnClass;
  ObjClass *listClass;
  ObjClass *mapEntryClass;
  ObjClass *mapClass;
  ObjClass *nullClass;
  ObjClass *numClass;
  ObjClass *objectClass;
  ObjClass *rangeClass;
  ObjClass *stringClass;

  // The fiber that is currently running.
  ObjFiber *fiber;

  // The loaded modules. Each key is an ObjString (except for the main module,
  // whose key is null) for the module's name and the value is the ObjModule
  // for the module.
  ObjMap *modules;

  // The most recently imported module. More specifically, the module whose
  // code has most recently finished executing.
  //
  // Not treated like a GC root since the module is already in [modules].
  ObjModule *lastModule;

  // Memory management data:

  // The number of bytes that are known to be currently allocated. Includes all
  // memory that was proven live after the last GC, as well as any new bytes
  // that were allocated since then. Does *not* include bytes for objects that
  // were freed since the last GC.
  size_t bytesAllocated;

  // The number of total allocated bytes that will trigger the next GC.
  size_t nextGC;

  // The first object in the linked list of all currently allocated objects.
  Obj *first;

  // The "gray" set for the garbage collector. This is the stack of unprocessed
  // objects while a garbage collection pass is in process.
  Obj **gray;
  int grayCount;
  int grayCapacity;

  // The list of temporary roots. This is for temporary or new objects that are
  // not otherwise reachable but should not be collected.
  //
  // They are organized as a stack of pointers stored in this array. This
  // implies that temporary roots need to have stack semantics: only the most
  // recently pushed object can be released.
  Obj *tempRoots[FODI_MAX_TEMP_ROOTS];

  int numTempRoots;

  // Pointer to the first node in the linked list of active handles or NULL if
  // there are none.
  FodiHandle *handles;

  // Pointer to the bottom of the range of stack slots available for use from
  // the C API. During a foreign method, this will be in the stack of the fiber
  // that is executing a method.
  //
  // If not in a foreign method, this is initially NULL. If the user requests
  // slots by calling fodiEnsureSlots(), a stack is created and this is
  // initialized.
  Value *apiStack;

  FodiConfiguration config;

  // Compiler and debugger data:

  // The compiler that is currently compiling code. This is used so that heap
  // allocated objects used by the compiler can be found if a GC is kicked off
  // in the middle of a compile.
  Compiler *compiler;

  // There is a single global symbol table for all method names on all classes.
  // Method calls are dispatched directly by index in this table.
  SymbolTable methodNames;

  #if FODI_OPCODE_EXECUTION_COUNT
  // Execution counts for each opcode.
  size_t opcodeCounts[OP_COUNT];
  size_t dispatchCount;
  #endif
};

// A generic allocation function that handles all explicit memory management.
// It's used like so:
//
// - To allocate new memory, [memory] is NULL and [oldSize] is zero. It should
//   return the allocated memory or NULL on failure.
//
// - To attempt to grow an existing allocation, [memory] is the memory,
//   [oldSize] is its previous size, and [newSize] is the desired size.
//   It should return [memory] if it was able to grow it in place, or a new
//   pointer if it had to move it.
//
// - To shrink memory, [memory], [oldSize], and [newSize] are the same as above
//   but it will always return [memory].
//
// - To free memory, [memory] will be the memory to free and [newSize] and
//   [oldSize] will be zero. It should return NULL.
void *fodiReallocate(FodiVM *vm, void *memory, size_t oldSize, size_t newSize);

// Invoke the finalizer for the foreign object referenced by [foreign].
void fodiFinalizeForeign(FodiVM *vm, ObjForeign *foreign);

// Creates a new [FodiHandle] for [value].
FodiHandle *fodiMakeHandle(FodiVM *vm, Value value);

// Compile [source] in the context of [module] and wrap in a fiber that can
// execute it.
//
// Returns NULL if a compile error occurred.
ObjClosure *fodiCompileSource(FodiVM *vm, const char *module,
                              const char *source, bool isExpression,
                              bool printErrors);

// Looks up a variable from a previously-loaded module.
//
// Aborts the current fiber if the module or variable could not be found.
Value fodiGetModuleVariable(FodiVM *vm, Value moduleName, Value variableName);

// Returns the value of the module-level variable named [name] in the main
// module.
Value fodiFindVariable(FodiVM *vm, ObjModule *module, const char *name);

// Adds a new implicitly declared top-level variable named [name] to [module]
// based on a use site occurring on [line].
//
// Does not check to see if a variable with that name is already declared or
// defined. Returns the symbol for the new variable or -2 if there are too many
// variables defined.
int fodiDeclareVariable(FodiVM *vm, ObjModule *module, const char *name,
                        size_t length, int line);

// Adds a new top-level variable named [name] to [module], and optionally
// populates line with the line of the implicit first use (line can be NULL).
//
// Returns the symbol for the new variable, -1 if a variable with the given name
// is already defined, or -2 if there are too many variables defined.
// Returns -3 if this is a top-level lowercase variable (localname) that was
// used before being defined.
int fodiDefineVariable(FodiVM *vm, ObjModule *module, const char *name,
                       size_t length, Value value, int *line);

// Pushes [closure] onto [fiber]'s callstack to invoke it. Expects [numArgs]
// arguments (including the receiver) to be on the top of the stack already.
static inline void fodiCallFunction(FodiVM *vm, ObjFiber *fiber,
                                    ObjClosure *closure, Value* callReg, int numArgs, int returnReg)
{
  // Grow the call frame array if needed.
  if (fiber->numFrames + 1 > fiber->frameCapacity)
  {
    int max = fiber->frameCapacity * 2;
    fiber->frames = (CallFrame *)fodiReallocate(vm, fiber->frames,
                                                sizeof(CallFrame) * fiber->frameCapacity, sizeof(CallFrame) * max);
    fiber->frameCapacity = max;
  }

  // Grow the stack if needed.
  // int stacksize2 = fiber->stack
  int callIndex = (int)(callReg - fiber->stack);
  int needed = callIndex + closure->fn->maxSlots + 1;
  fodiEnsureStack(vm, fiber, needed);
  fodiAppendCallFrame(vm, fiber, closure, fiber->stack + callIndex, returnReg);
}

// Marks [obj] as a GC root so that it doesn't get collected.
void fodiPushRoot(FodiVM *vm, Obj *obj);

// Removes the most recently pushed temporary root.
void fodiPopRoot(FodiVM *vm);

// Returns the class of [value].
//
// Defined here instead of in fodi_value.h because it's critical that this be
// inlined. That means it must be defined in the header, but the fodi_value.h
// header doesn't have a full definitely of FodiVM yet.
static inline ObjClass *fodiGetClassInline(FodiVM *vm, Value value)
{
  if (IS_NUM(value))
    return vm->numClass;
  if (IS_OBJ(value))
    return AS_OBJ(value)->classObj;

#if FODI_NAN_TAGGING
  switch (GET_TAG(value))
  {
  case TAG_FALSE:
    return vm->boolClass;
    break;
  case TAG_NAN:
    return vm->numClass;
    break;
  case TAG_NULL:
    return vm->nullClass;
    break;
  case TAG_TRUE:
    return vm->boolClass;
    break;
  case TAG_UNDEFINED:
    UNREACHABLE();
  }
#else
  switch (value.type)
  {
  case VAL_FALSE:
    return vm->boolClass;
  case VAL_NULL:
    return vm->nullClass;
  case VAL_NUM:
    return vm->numClass;
  case VAL_TRUE:
    return vm->boolClass;
  case VAL_OBJ:
    return AS_OBJ(value)->classObj;
  case VAL_UNDEFINED:
    UNREACHABLE();
  }
#endif

  UNREACHABLE();
  return NULL;
}

// Returns `true` if [name] is a local variable name (starts with a lowercase
// letter).
static inline bool fodiIsLocalName(const char *name)
{
  return name[0] >= 'a' && name[0] <= 'z';
}

static inline bool fodiIsFalsyValue(Value value)
{
  return IS_FALSE(value) || IS_NULL(value);
}

#endif
