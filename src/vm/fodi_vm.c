#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "fodi.h"
#include "fodi_common.h"
#include "fodi_compiler.h"
#include "fodi_core.h"
#include "fodi_debug.h"
#include "fodi_instructions.h"
#include "fodi_primitive.h"
#include "fodi_vm.h"

#if FODI_OPT_META
#include "fodi_opt_meta.h"
#endif
#if FODI_OPT_RANDOM
#include "fodi_opt_random.h"
#endif

#if FODI_DEBUG_TRACE_MEMORY || FODI_DEBUG_TRACE_GC || FODI_DEBUG_TRACE_INSTRUCTIONS
#include <time.h>
#include <stdio.h>
#endif

#if FODI_OPCODE_EXECUTION_COUNT
#include <stdio.h>
#endif

// The behavior of realloc() when the size is 0 is implementation defined. It
// may return a non-NULL pointer which must not be dereferenced but nevertheless
// should be freed. To prevent that, we avoid calling realloc() with a zero
// size.
static void *defaultReallocate(void *ptr, size_t newSize, void *_)
{
  if (newSize == 0)
  {
    free(ptr);
    return NULL;
  }

  return realloc(ptr, newSize);
}

int fodiGetVersionNumber()
{
  return FODI_VERSION_NUMBER;
}

void fodiInitConfiguration(FodiConfiguration *config)
{
  config->reallocateFn = defaultReallocate;
  config->resolveModuleFn = NULL;
  config->loadModuleFn = NULL;
  config->bindForeignMethodFn = NULL;
  config->bindForeignClassFn = NULL;
  config->writeFn = NULL;
  config->errorFn = NULL;
  config->initialHeapSize = 1024 * 1024 * 10;
  config->minHeapSize = 1024 * 1024;
  config->heapGrowthPercent = 50;
  config->userData = NULL;
}

FodiVM *fodiNewVM(FodiConfiguration *config)
{
  FodiReallocateFn reallocate = defaultReallocate;
  void *userData = NULL;
  if (config != NULL)
  {
    userData = config->userData;
    reallocate = config->reallocateFn ? config->reallocateFn : defaultReallocate;
  }

  FodiVM *vm = (FodiVM *)reallocate(NULL, sizeof(*vm), userData);
  memset(vm, 0, sizeof(FodiVM));

  // Copy the configuration if given one.
  if (config != NULL)
  {
    memcpy(&vm->config, config, sizeof(FodiConfiguration));

    // We choose to set this after copying,
    // rather than modifying the user config pointer
    vm->config.reallocateFn = reallocate;
  }
  else
  {
    fodiInitConfiguration(&vm->config);
  }

#if FODI_OPCODE_EXECUTION_COUNT
  memset(vm->opcodeCounts, 0, sizeof(vm->opcodeCounts));
  vm->dispatchCount = 0;
#endif

  // TODO: Should we allocate and free this during a GC?
  vm->grayCount = 0;
  // TODO: Tune this.
  vm->grayCapacity = 4;
  vm->gray = (Obj **)reallocate(NULL, vm->grayCapacity * sizeof(Obj *), userData);
  vm->nextGC = vm->config.initialHeapSize;

  fodiSymbolTableInit(&vm->methodNames);

  vm->modules = fodiNewMap(vm);
  fodiInitializeCore(vm);
  return vm;
}

void fodiFreeVM(FodiVM *vm)
{
  ASSERT(vm->methodNames.count > 0, "VM appears to have already been freed.");

  // Free all of the GC objects.
  Obj *obj = vm->first;
  while (obj != NULL)
  {
    Obj *next = obj->next;
    fodiFreeObj(vm, obj);
    obj = next;
  }

  // Free up the GC gray set.
  vm->gray = (Obj **)vm->config.reallocateFn(vm->gray, 0, vm->config.userData);

  // Tell the user if they didn't free any handles. We don't want to just free
  // them here because the host app may still have pointers to them that they
  // may try to use. Better to tell them about the bug early.
  ASSERT(vm->handles == NULL, "All handles have not been released.");

  fodiSymbolTableClear(vm, &vm->methodNames);

  DEALLOCATE(vm, vm);
}

void fodiCollectGarbage(FodiVM *vm)
{
#if FODI_DEBUG_TRACE_MEMORY || FODI_DEBUG_TRACE_GC
  printf("-- gc --\n");

  size_t before = vm->bytesAllocated;
  double startTime = (double)clock() / CLOCKS_PER_SEC;
#endif

  // Mark all reachable objects.

  // Reset this. As we mark objects, their size will be counted again so that
  // we can track how much memory is in use without needing to know the size
  // of each *freed* object.
  //
  // This is important because when freeing an unmarked object, we don't always
  // know how much memory it is using. For example, when freeing an instance,
  // we need to know its class to know how big it is, but its class may have
  // already been freed.
  vm->bytesAllocated = 0;

  fodiGrayObj(vm, (Obj *)vm->modules);

  // Temporary roots.
  for (int i = 0; i < vm->numTempRoots; i++)
  {
    fodiGrayObj(vm, vm->tempRoots[i]);
  }

  // The current fiber.
  fodiGrayObj(vm, (Obj *)vm->fiber);

  // The handles.
  for (FodiHandle *handle = vm->handles;
       handle != NULL;
       handle = handle->next)
  {
    fodiGrayValue(vm, handle->value);
  }

  // Any object the compiler is using (if there is one).
  if (vm->compiler != NULL)
    fodiMarkCompiler(vm, vm->compiler);

  // Method names.
  fodiBlackenSymbolTable(vm, &vm->methodNames);

  // Now that we have grayed the roots, do a depth-first search over all of the
  // reachable objects.
  fodiBlackenObjects(vm);

  // Collect the white objects.
  Obj **obj = &vm->first;
  while (*obj != NULL)
  {
    if (!((*obj)->isDark))
    {
      // This object wasn't reached, so remove it from the list and free it.
      Obj *unreached = *obj;
      *obj = unreached->next;
      fodiFreeObj(vm, unreached);
    }
    else
    {
      // This object was reached, so unmark it (for the next GC) and move on to
      // the next.
      (*obj)->isDark = false;
      obj = &(*obj)->next;
    }
  }

  // Calculate the next gc point, this is the current allocation plus
  // a configured percentage of the current allocation.
  vm->nextGC = vm->bytesAllocated + ((vm->bytesAllocated * vm->config.heapGrowthPercent) / 100);
  if (vm->nextGC < vm->config.minHeapSize)
    vm->nextGC = vm->config.minHeapSize;

#if FODI_DEBUG_TRACE_MEMORY || FODI_DEBUG_TRACE_GC
  double elapsed = ((double)clock() / CLOCKS_PER_SEC) - startTime;
  // Explicit cast because size_t has different sizes on 32-bit and 64-bit and
  // we need a consistent type for the format string.
  printf("GC %lu before, %lu after (%lu collected), next at %lu. Took %.3fms.\n",
         (unsigned long)before,
         (unsigned long)vm->bytesAllocated,
         (unsigned long)(before - vm->bytesAllocated),
         (unsigned long)vm->nextGC,
         elapsed * 1000.0);
#endif
}

void *fodiReallocate(FodiVM *vm, void *memory, size_t oldSize, size_t newSize)
{

#if FODI_DEBUG_TRACE_MEMORY
  // Explicit cast because size_t has different sizes on 32-bit and 64-bit and
  // we need a consistent type for the format string.
  printf("reallocate %p %lu -> %lu\n",
         memory, (unsigned long)oldSize, (unsigned long)newSize);
#endif

  // If new bytes are being allocated, add them to the total count. If objects
  // are being completely deallocated, we don't track that (since we don't
  // track the original size). Instead, that will be handled while marking
  // during the next GC.
  vm->bytesAllocated += newSize - oldSize;

#if FODI_DEBUG_GC_STRESS
  // Since collecting calls this function to free things, make sure we don't
  // recurse.
  if (newSize > 0)
    fodiCollectGarbage(vm);
#else
  if (newSize > 0 && vm->bytesAllocated > vm->nextGC)
    fodiCollectGarbage(vm);
#endif

  return vm->config.reallocateFn(memory, newSize, vm->config.userData);
}

// Captures the local variable [local] into an [Upvalue]. If that local is
// already in an upvalue, the existing one will be used. (This is important to
// ensure that multiple closures closing over the same variable actually see
// the same variable.) Otherwise, it will create a new open upvalue and add it
// the fiber's list of upvalues.
static ObjUpvalue *captureUpvalue(FodiVM *vm, ObjFiber *fiber, Value *local)
{
  // If there are no open upvalues at all, we must need a new one.
  if (fiber->openUpvalues == NULL)
  {
    fiber->openUpvalues = fodiNewUpvalue(vm, local);
    fiber->openUpvalues->isLocal = true;
    return fiber->openUpvalues;
  }

  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = fiber->openUpvalues;

  // Walk towards the bottom of the stack until we find a previously existing
  // upvalue or pass where it should be.
  while (upvalue != NULL && upvalue->value > local)
  {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  // Found an existing upvalue for this local.
  if (upvalue != NULL && upvalue->value == local)
    return upvalue;

  // We've walked past this local on the stack, so there must not be an
  // upvalue for it already. Make a new one and link it in in the right
  // place to keep the list sorted.
  ObjUpvalue *createdUpvalue = fodiNewUpvalue(vm, local);
  createdUpvalue->isLocal = true;
  if (prevUpvalue == NULL)
  {
    // The new one is the first one in the list.
    fiber->openUpvalues = createdUpvalue;
  }
  else
  {
    prevUpvalue->next = createdUpvalue;
  }

  createdUpvalue->next = upvalue;
  return createdUpvalue;
}

// Closes any open upvalues that have been created for stack slots at [last]
// and above.
static void closeUpvalues(ObjFiber *fiber, Value *last)
{
  while (fiber->openUpvalues != NULL &&
         fiber->openUpvalues->value >= last)
  {
    ObjUpvalue *upvalue = fiber->openUpvalues;

    // Move the value into the upvalue itself and point the upvalue to it.
    upvalue->closed = *upvalue->value;
    upvalue->value = &upvalue->closed;

    // Remove it from the open upvalue list.
    fiber->openUpvalues = upvalue->next;
  }
}

// Looks up a foreign method in [moduleName] on [className] with [signature].
//
// This will try the host's foreign method binder first. If that fails, it
// falls back to handling the built-in modules.
static FodiForeignMethodFn findForeignMethod(FodiVM *vm,
                                             const char *moduleName,
                                             const char *className,
                                             bool isStatic,
                                             const char *signature)
{
  FodiForeignMethodFn method = NULL;

  if (vm->config.bindForeignMethodFn != NULL)
  {
    method = vm->config.bindForeignMethodFn(vm, moduleName, className, isStatic,
                                            signature);
  }

  // If the host didn't provide it, see if it's an optional one.
  if (method == NULL)
  {
#if FODI_OPT_META
    if (strcmp(moduleName, "meta") == 0)
    {
      method = fodiMetaBindForeignMethod(vm, className, isStatic, signature);
    }
#endif
#if FODI_OPT_RANDOM
    if (strcmp(moduleName, "random") == 0)
    {
      method = fodiRandomBindForeignMethod(vm, className, isStatic, signature);
    }
#endif
  }

  return method;
}

// Defines [methodValue] as a method on [classObj].
//
// Handles both foreign methods where [methodValue] is a string containing the
// method's signature and Fodi methods where [methodValue] is a function.
//
// Aborts the current fiber if the method is a foreign method that could not be
// found.
static void bindMethod(FodiVM *vm, bool isStatic, int symbol,
                               ObjModule *module, ObjClass *classObj, Value methodValue, Value *stackStart)
{
  const char *className = classObj->name->value;
  if (isStatic)
    classObj = classObj->obj.classObj;

  Method method;
  if (IS_STRING(methodValue))
  {
    const char *name = AS_CSTRING(methodValue);
    method.type = METHOD_FOREIGN;
    method.as.foreign = findForeignMethod(vm, module->name->value,
                                          className,
                                          isStatic,
                                          name);

    if (method.as.foreign == NULL)
    {
      vm->fiber->error = fodiStringFormat(vm,
                                          "Could not find foreign method '@' for class $ in module '$'.",
                                          methodValue, classObj->name->value, module->name->value);
      return;
    }
  }
  else
  {
    method.as.closure = AS_CLOSURE(methodValue);
    method.type = METHOD_BLOCK;

    // Patch up the bytecode now that we know the superclass.
    fodiBindMethodCode(classObj, method.as.closure, stackStart);
  }
  fodiBindMethod(vm, classObj, symbol, method);
}

static void callForeign(FodiVM *vm, ObjFiber *fiber,
                        FodiForeignMethodFn foreign, int numArgs, Value *callReg)
{
  ASSERT(vm->apiStack == NULL, "Cannot already be in foreign call.");
  vm->apiStack = callReg;
  foreign(vm);

  // Discard the stack slots for the arguments and temporaries but leave one
  // for the result.
  fiber->apiStackTop = vm->apiStack + 1;
  callReg = vm->apiStack + 1;

  vm->apiStack = NULL;
}

// Handles the current fiber having aborted because of an error.
//
// Walks the call chain of fibers, aborting each one until it hits a fiber that
// handles the error. If none do, tells the VM to stop.
static void runtimeError(FodiVM *vm)
{
  ASSERT(fodiHasError(vm->fiber), "Should only call this after an error.");

  ObjFiber *current = vm->fiber;
  Value error = current->error;

  while (current != NULL)
  {
    // Every fiber along the call chain gets aborted with the same error.
    current->error = error;

    // If the caller ran this fiber using "try", give it the error and stop.
    if (current->state == FIBER_TRY)
    {
      // Make the caller's try method return the error message.
      current->caller->stack[current->caller->lastCallReg] = vm->fiber->error;
      vm->fiber = current->caller;
      return;
    }

    // Otherwise, unhook the caller since we will never resume and return to it.
    ObjFiber *caller = current->caller;
    current->caller = NULL;
    current = caller;
  }

  // If we got here, nothing caught the error, so show the stack trace.
  fodiDebugPrintStackTrace(vm);
  vm->fiber = NULL;
  vm->apiStack = NULL;
}

// Aborts the current fiber with an appropriate method not found error for a
// method with [symbol] on [classObj].
static void methodNotFound(FodiVM *vm, ObjClass *classObj, int symbol)
{
  vm->fiber->error = fodiStringFormat(vm, "@ does not implement '$'.",
                                      OBJ_VAL(classObj->name), vm->methodNames.data[symbol]->value);
}

// Looks up the previously loaded module with [name].
//
// Returns `NULL` if no module with that name has been loaded.
static ObjModule *getModule(FodiVM *vm, Value name)
{
  Value moduleValue = fodiMapGet(vm->modules, name);
  return !IS_UNDEFINED(moduleValue) ? AS_MODULE(moduleValue) : NULL;
}

static ObjClosure *compileInModule(FodiVM *vm, Value name, const char *source,
                                   bool isExpression, bool printErrors)
{
  // See if the module has already been loaded.
  ObjModule *module = getModule(vm, name);
  if (module == NULL)
  {
    module = fodiNewModule(vm, AS_STRING(name));

    // It's possible for the fodiMapSet below to resize the modules map,
    // and trigger a GC while doing so. When this happens it will collect
    // the module we've just created. Once in the map it is safe.
    fodiPushRoot(vm, (Obj *)module);

    // Store it in the VM's module registry so we don't load the same module
    // multiple times.
    fodiMapSet(vm, vm->modules, name, OBJ_VAL(module));

    fodiPopRoot(vm);

    // Implicitly import the core module.
    ObjModule *coreModule = getModule(vm, NULL_VAL);
    for (int i = 0; i < coreModule->variables.count; i++)
    {
      fodiDefineVariable(vm, module,
                         coreModule->variableNames.data[i]->value,
                         coreModule->variableNames.data[i]->length,
                         coreModule->variables.data[i], NULL);
    }
  }

  ObjFn *fn = fodiCompile(vm, module, source, isExpression, printErrors);
  if (fn == NULL)
  {
    // TODO: Should we still store the module even if it didn't compile?
    return NULL;
  }

  // Functions are always wrapped in closures.
  fodiPushRoot(vm, (Obj *)fn);
  ObjClosure *closure = fodiNewClosure(vm, fn, false);
  fodiPopRoot(vm); // fn.

  return closure;
}

// Verifies that [superclassValue] is a valid object to inherit from. That
// means it must be a class and cannot be the class of any built-in type.
//
// Also validates that it doesn't result in a class with too many fields and
// the other limitations foreign classes have.
//
// If successful, returns `null`. Otherwise, returns a string for the runtime
// error message.
static Value validateSuperclass(FodiVM *vm, Value name, Value superclassValue,
                                int numFields)
{
  // Make sure the superclass is a class.
  if (!IS_CLASS(superclassValue))
  {
    return fodiStringFormat(vm,
                            "Class '@' cannot inherit from a non-class object.",
                            name);
  }

  // Make sure it doesn't inherit from a sealed built-in type. Primitive methods
  // on these classes assume the instance is one of the other Obj___ types and
  // will fail horribly if it's actually an ObjInstance.
  ObjClass *superclass = AS_CLASS(superclassValue);
  if (superclass == vm->classClass ||
      superclass == vm->fiberClass ||
      superclass == vm->fnClass || // Includes OBJ_CLOSURE.
      superclass == vm->listClass ||
      superclass == vm->mapClass ||
      superclass == vm->rangeClass ||
      superclass == vm->stringClass ||
      superclass == vm->boolClass ||
      superclass == vm->nullClass ||
      superclass == vm->numClass)
  {
    return fodiStringFormat(vm,
                            "Class '@' cannot inherit from built-in class '@'.",
                            name, OBJ_VAL(superclass->name));
  }

  if (superclass->numFields == -1)
  {
    return fodiStringFormat(vm,
                            "Class '@' cannot inherit from foreign class '@'.",
                            name, OBJ_VAL(superclass->name));
  }

  if (numFields == -1 && superclass->numFields > 0)
  {
    return fodiStringFormat(vm,
                            "Foreign class '@' may not inherit from a class with fields.",
                            name);
  }

  if (superclass->numFields + numFields > MAX_FIELDS)
  {
    return fodiStringFormat(vm,
                            "Class '@' may not have more than 255 fields, including inherited "
                            "ones.",
                            name);
  }

  return NULL_VAL;
}

static void bindForeignClass(FodiVM *vm, ObjClass *classObj, ObjModule *module)
{
  FodiForeignClassMethods methods;
  methods.allocate = NULL;
  methods.finalize = NULL;

  // Check the optional built-in module first so the host can override it.

  if (vm->config.bindForeignClassFn != NULL)
  {
    methods = vm->config.bindForeignClassFn(vm, module->name->value,
                                            classObj->name->value);
  }

  // If the host didn't provide it, see if it's a built in optional module.
  if (methods.allocate == NULL && methods.finalize == NULL)
  {
#if FODI_OPT_RANDOM
    if (strcmp(module->name->value, "random") == 0)
    {
      methods = fodiRandomBindForeignClass(vm, module->name->value,
                                           classObj->name->value);
    }
#endif
  }

  Method method;
  method.type = METHOD_FOREIGN;

  // Add the symbol even if there is no allocator so we can ensure that the
  // symbol itself is always in the symbol table.
  int symbol = fodiSymbolTableEnsure(vm, &vm->methodNames, "<allocate>", 10);
  if (methods.allocate != NULL)
  {
    method.as.foreign = methods.allocate;
    fodiBindMethod(vm, classObj, symbol, method);
  }

  // Add the symbol even if there is no finalizer so we can ensure that the
  // symbol itself is always in the symbol table.
  symbol = fodiSymbolTableEnsure(vm, &vm->methodNames, "<finalize>", 10);
  if (methods.finalize != NULL)
  {
    method.as.foreign = (FodiForeignMethodFn)methods.finalize;
    fodiBindMethod(vm, classObj, symbol, method);
  }
}

// Completes the process for creating a new class.
//
// The class attributes instance and the class itself should be on the
// top of the fiber's stack.
//
// This process handles moving the attribute data for a class from
// compile time to runtime, since it now has all the attributes associated
// with a class, including for methods.
static void endClass(FodiVM *vm, Value *stackStart, int classReg)
{
  // Pull the attributes and class off the stack
  Value attributes = stackStart[classReg];
  Value classValue = stackStart[classReg + 1];

  ObjClass *classObj = AS_CLASS(classValue);
  classObj->attributes = attributes;
}

// Creates a new class.
//
// If [numFields] is -1, the class is a foreign class. The name and superclass
// should be on top of the fiber's stack. After calling this, the top of the
// stack will contain the new class.
//
// Aborts the current fiber if an error occurs.
static void createClass(FodiVM *vm, int numFields, ObjModule *module, int slot)
{
  // Pull the name and superclass off the stack.
  Value name;
  Value superclass;

  name = vm->fiber->stack[slot - 1];
  superclass = vm->fiber->stack[slot];

  vm->fiber->error = validateSuperclass(vm, name, superclass, numFields);
  if (fodiHasError(vm->fiber))
    return;

  ObjClass *classObj = fodiNewClass(vm, AS_CLASS(superclass), numFields,
                                    AS_STRING(name));

  vm->fiber->stack[slot - 1] = OBJ_VAL(classObj);

  if (numFields == -1)
    bindForeignClass(vm, classObj, module);
}

static void createForeign(FodiVM *vm, ObjFiber *fiber, Value *stack)
{
  ObjClass *classObj = AS_CLASS(stack[0]);
  ASSERT(classObj->numFields == -1, "Class must be a foreign class.");

  // TODO: Don't look up every time.
  int symbol = fodiSymbolTableFind(&vm->methodNames, "<allocate>", 10);
  ASSERT(symbol != -1, "Should have defined <allocate> symbol.");

  ASSERT(classObj->methods.count > symbol, "Class should have allocator.");
  Method *method = &classObj->methods.data[symbol];
  ASSERT(method->type == METHOD_FOREIGN, "Allocator should be foreign.");

  // Pass the constructor arguments to the allocator as well.
  ASSERT(vm->apiStack == NULL, "Cannot already be in foreign call.");
  vm->apiStack = stack;

  method->as.foreign(vm);

  vm->apiStack = NULL;
}

void fodiFinalizeForeign(FodiVM *vm, ObjForeign *foreign)
{
  // TODO: Don't look up every time.
  int symbol = fodiSymbolTableFind(&vm->methodNames, "<finalize>", 10);
  ASSERT(symbol != -1, "Should have defined <finalize> symbol.");

  // If there are no finalizers, don't finalize it.
  if (symbol == -1)
    return;

  // If the class doesn't have a finalizer, bail out.
  ObjClass *classObj = foreign->obj.classObj;
  if (symbol >= classObj->methods.count)
    return;

  Method *method = &classObj->methods.data[symbol];
  if (method->type == METHOD_NONE)
    return;

  ASSERT(method->type == METHOD_FOREIGN, "Finalizer should be foreign.");

  FodiFinalizerFn finalizer = (FodiFinalizerFn)method->as.foreign;
  finalizer(foreign->data);
}

// Let the host resolve an imported module name if it wants to.
static Value resolveModule(FodiVM *vm, Value name)
{
  // If the host doesn't care to resolve, leave the name alone.
  if (vm->config.resolveModuleFn == NULL)
    return name;

  ObjFiber *fiber = vm->fiber;
  ObjFn *fn = fiber->frames[fiber->numFrames - 1].closure->fn;
  ObjString *importer = fn->module->name;

  const char *resolved = vm->config.resolveModuleFn(vm, importer->value,
                                                    AS_CSTRING(name));
  if (resolved == NULL)
  {
    vm->fiber->error = fodiStringFormat(vm,
                                        "Could not resolve module '@' imported from '@'.",
                                        name, OBJ_VAL(importer));
    return NULL_VAL;
  }

  // If they resolved to the exact same string, we don't need to copy it.
  if (resolved == AS_CSTRING(name))
    return name;

  // Copy the string into a Fodi String object.
  name = fodiNewString(vm, resolved);
  DEALLOCATE(vm, (char *)resolved);
  return name;
}

static Value importModule(FodiVM *vm, Value name)
{
  name = resolveModule(vm, name);

  // If the module is already loaded, we don't need to do anything.
  Value existing = fodiMapGet(vm->modules, name);
  if (!IS_UNDEFINED(existing))
    return existing;

  fodiPushRoot(vm, AS_OBJ(name));

  FodiLoadModuleResult result = {0};
  const char *source = NULL;

  // Let the host try to provide the module.
  if (vm->config.loadModuleFn != NULL)
  {
    result = vm->config.loadModuleFn(vm, AS_CSTRING(name));
  }

  // If the host didn't provide it, see if it's a built in optional module.
  if (result.source == NULL)
  {
    result.onComplete = NULL;
    ObjString *nameString = AS_STRING(name);
#if FODI_OPT_META
    if (strcmp(nameString->value, "meta") == 0)
      result.source = fodiMetaSource();
#endif
#if FODI_OPT_RANDOM
    if (strcmp(nameString->value, "random") == 0)
      result.source = fodiRandomSource();
#endif
  }

  if (result.source == NULL)
  {
    vm->fiber->error = fodiStringFormat(vm, "Could not load module '@'.", name);
    fodiPopRoot(vm); // name.
    return NULL_VAL;
  }

  ObjClosure *moduleClosure = compileInModule(vm, name, result.source, false, true);

  // Now that we're done, give the result back in case there's cleanup to do.
  if (result.onComplete)
    result.onComplete(vm, AS_CSTRING(name), result);

  if (moduleClosure == NULL)
  {
    vm->fiber->error = fodiStringFormat(vm,
                                        "Could not compile module '@'.", name);
    fodiPopRoot(vm); // name.
    return NULL_VAL;
  }

  fodiPopRoot(vm); // name.

  // Return the closure that executes the module.
  return OBJ_VAL(moduleClosure);
}

static Value getModuleVariable(FodiVM *vm, ObjModule *module,
                               Value variableName)
{
  ObjString *variable = AS_STRING(variableName);
  uint32_t variableEntry = fodiSymbolTableFind(&module->variableNames,
                                               variable->value,
                                               variable->length);

  // It's a runtime error if the imported variable does not exist.
  if (variableEntry != UINT32_MAX)
  {
    return module->variables.data[variableEntry];
  }

  vm->fiber->error = fodiStringFormat(vm,
                                      "Could not find a variable named '@' in module '@'.",
                                      variableName, OBJ_VAL(module->name));
  return NULL_VAL;
}

inline static bool checkArity(FodiVM *vm, Value value, int numArgs)
{
  ASSERT(IS_CLOSURE(value), "Receiver must be a closure.");
  ObjFn *fn = AS_CLOSURE(value)->fn;

  // We only care about missing arguments, not extras. The "- 1" is because
  // numArgs includes the receiver, the function itself, which we don't want to
  // count.
  if (numArgs - 1 >= fn->arity)
    return true;

  vm->fiber->error = CONST_STRING(vm, "Function expects more arguments.");
  return false;
}
// The main bytecode interpreter loop. This is where the magic happens. It is
// also, as you can imagine, highly performance critical.
static FodiInterpretResult runInterpreter(FodiVM *vm, register ObjFiber *fiber)
{
  // Remember the current fiber so we can find it if a GC happens.
  vm->fiber = fiber;
  fiber->state = FIBER_ROOT;

  // Hoist these into local variables. They are accessed frequently in the loop
  // but assigned less frequently. Keeping them in locals and updating them when
  // a call frame has been pushed or popped gives a large speed boost.
  register CallFrame *frame;
  register Value *stackStart;
  register Instruction *rip;
  register ObjFn *fn;

// These macros are designed to only be invoked within this function.
#define INSERT(value, index) *(stackStart + index) = value
#define READ(index) (*(stackStart + index))

#define RKREAD(index) index >= UINT8_MAX ? fn->constants.data[index - UINT8_MAX] : READ(index)

#define READ_INSTRUCTION() (*rip++)

// Use this before a CallFrame is pushed to store the local variables back
// into the current one.
#define STORE_FRAME() frame->rip = rip;

// Use this after a CallFrame has been pushed or popped to refresh the local
// variables.
#define LOAD_FRAME()                              \
  do                                              \
  {                                               \
    frame = &fiber->frames[fiber->numFrames - 1]; \
    stackStart = frame->stackStart;               \
    rip = frame->rip;                             \
    fn = frame->closure->fn;                      \
  } while (false)

// Terminates the current fiber with error string [error]. If another calling
// fiber is willing to catch the error, transfers control to it, otherwise
// exits the interpreter.
#define RUNTIME_ERROR()                 \
  do                                    \
  {                                     \
    STORE_FRAME();                      \
    runtimeError(vm);                   \
    if (vm->fiber == NULL)              \
      return FODI_RESULT_RUNTIME_ERROR; \
    fiber = vm->fiber;                  \
    LOAD_FRAME();                       \
    DISPATCH();                         \
  } while (false)

#if FODI_DEBUG_TRACE_INSTRUCTIONS
// Prints the stack and instruction before each instruction is executed.
#define DEBUG_TRACE_INSTRUCTIONS()                                  \
  do                                                                    \
  {                                                                      \
    int inst = rip - fn->code.data;                                   \
    fodiDumpStack(fiber, stackStart, fn->stackTop.data[inst]);     \
    fodiDumpInstruction(vm, fn, inst); \
  } while (false)
#else
#define DEBUG_TRACE_INSTRUCTIONS() \
  do                                   \
  {                                    \
  } while (false)
#endif
#if FODI_OPCODE_EXECUTION_COUNT
#define COUNT_OPCODE()                    \
  do                                      \
  {                                       \
    vm->dispatchCount++;                  \
    vm->opcodeCounts[GET_OPCODE(code)]++; \
  } while (false)

#else
#define COUNT_OPCODE() \
  do                   \
  {                    \
  } while (false)
#endif
#if FODI_COMPUTED_GOTO

  static void *dispatchTable[] = {
#define OPCODE(name, _) &&op_##name,
#include "fodi_opcodes.h"
#undef OPCODE
  };

#define DISPATCH()                                                        \
  do                                                                          \
  {                                                                           \
    DEBUG_TRACE_INSTRUCTIONS();                                           \
    COUNT_OPCODE();                                                           \
    fiber->stackTop = stackStart + fn->stackTop.data[rip - fn->code.data]; \
    goto *dispatchTable[GET_OPCODE(code = READ_INSTRUCTION())];       \
  } while (false)

#define INTERPRET_LOOP DISPATCH();
#define CASE_OP(name) op_##name

#else

#define INTERPRET_LOOP        \
  loop:                           \
  DEBUG_TRACE_INSTRUCTIONS(); \
  code = READ_INSTRUCTION();      \
  switch (GET_OPCODE(code))

#define CASE_OP(name) case OP_##name
#define DISPATCH() goto loop

#endif

  LOAD_FRAME();

  Instruction code;
  INTERPRET_LOOP
  {
    CASE_OP(LOADBOOL) : INSERT(BOOL_VAL(GET_B(code)), GET_A(code));
    if (GET_C(code) != 0)
      rip++;
    DISPATCH();

    CASE_OP(LOADNULL) : INSERT(NULL_VAL, GET_A(code));
    DISPATCH();

    CASE_OP(LOADK) : 
    {
      Value constant = fn->constants.data[GET_Bx(code)];
      if (IS_LIST(constant)){
        //copy the list primitive to avoid mutation of constant list
        ObjList *list = fodiRepeatList(vm, AS_LIST(constant), 1);
        INSERT(OBJ_VAL(list), GET_A(code));
        DISPATCH();
      }

      if (IS_MAP(constant)){
        //copy the list primitive to avoid mutation of constant list
        ObjMap *map = fodiCopyMap(vm, AS_MAP(constant));
        INSERT(OBJ_VAL(map), GET_A(code));
        DISPATCH();
      }
        
      INSERT(constant, GET_A(code));
      DISPATCH();
    }

    CASE_OP(MOVE) : INSERT(READ(GET_B(code)), GET_A(code));
    DISPATCH();

    CASE_OP(GETFIELD) :
    {
      uint8_t field = GET_C(code);
      Value receiver = READ(GET_B(code));
        ASSERT(IS_INSTANCE(receiver), "Receiver should be instance.");
        ObjInstance *instance = AS_INSTANCE(receiver);
        ASSERT(field < instance->obj.classObj->numFields, "Out of bounds field.");
        INSERT(instance->fields[field], GET_A(code));
        DISPATCH();
      }

    CASE_OP(SETFIELD) :
    {
        uint8_t field = GET_C(code);
        Value receiver = READ(GET_B(code));
        ASSERT(IS_INSTANCE(receiver), "Receiver should be instance.");
        ObjInstance *instance = AS_INSTANCE(receiver);
        ASSERT(field < instance->obj.classObj->numFields, "Out of bounds field.");
        instance->fields[field] = READ(GET_A(code));
        DISPATCH();
      }
      
    CASE_OP(SETGLOBAL) : fn->module->variables.data[GET_Bx(code)] = READ(GET_A(code));
    DISPATCH();

    CASE_OP(GETGLOBAL) : INSERT(fn->module->variables.data[GET_Bx(code)], GET_A(code));
    DISPATCH();

    CASE_OP(GETUPVAL) :
    {
      ObjUpvalue **upvalues = frame->closure->upvalues;
      INSERT(*upvalues[GET_Bx(code)]->value, GET_A(code));
      DISPATCH();
    }
    CASE_OP(SETUPVAL) :
    {
      ObjUpvalue **upvalues = frame->closure->upvalues;
      *upvalues[GET_Bx(code)]->value = READ(GET_A(code));
      DISPATCH();
    }

    CASE_OP(TEST) : if (!fodiIsFalsyValue(READ(GET_B(code))) == (bool)GET_C(code)) rip++;
    else rip += GET_sJx(*(rip)) + 1;
    DISPATCH();

    CASE_OP(JUMP) : rip += GET_sJx(code);
    DISPATCH();

    CASE_OP(CLOSURE) :
    {
      // Create the closure and push it on the stack before creating upvalues
      // so that it doesn't get collected.
      ObjClosure *KProto = AS_CLOSURE(fn->constants.data[GET_Bx(code)]);
      ObjFn *function = KProto->fn;
      ObjClosure *closure = fodiNewClosure(vm, function, false);

      INSERT(OBJ_VAL(closure), GET_A(code));

      // Capture upvalues, if any.
      for (int i = 0; i < closure->fn->numUpvalues; i++)
      {
        bool isLocal = (bool)KProto->protoUpvalues[i]->isLocal;
        uint8_t index = KProto->protoUpvalues[i]->index;
        if (isLocal)
        {
          // Make an new upvalue to close over the parent's local variable.
          closure->upvalues[i] = captureUpvalue(vm, fiber,
                                                frame->stackStart + index);
        }
        else
        {
          // Use the same upvalue as the current call frame.
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }
      DISPATCH();
    }

    // load class for class object K[Bx] into R[A]
    CASE_OP(CONSTRUCT) : if (GET_Bx(code) == 0)
    {
      ASSERT(IS_CLASS(stackStart[GET_A(code)]), "'this' should be a class.");
      stackStart[GET_A(code)] = fodiNewInstance(vm, AS_CLASS(stackStart[GET_A(code)]));
    }
    else
    {
      ASSERT(IS_CLASS(stackStart[GET_A(code)]), "'this' should be a class.");
      createForeign(vm, fiber, stackStart);
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
    }
    DISPATCH();
    
    CASE_OP(TAILCALL) :
      // reset the instruction pointer
      rip = &frame->closure->fn->code.data[0];

      // replace call operands
      for (int i = 0; i < GET_vB(code) + 1; i++)
      {
        INSERT(READ(GET_A(code) + i), i);
      }
      DISPATCH();
    {
      int numArgs;
      int symbol;

      Value *args;
      ObjClass *classObj;

      Method *method;
      // call method in R[A] with B arguments and put the result in R[A]
      //  OPCODE(CALL, iABC)
      // call method K[C] with B arguments and put the result in R[A]
      CASE_OP(CALL) :

      // Add one for the implicit receiver argument.
      numArgs = GET_vB(code) + 1;
      symbol = GET_vC(code);

      // The receiver is the first argument.
      args = stackStart + GET_A(code);
      classObj = fodiGetClassInline(vm, args[0]);

      goto completeCall;

      CASE_OP(CALLSUPER) : // Add one for the implicit receiver argument.
                            numArgs = GET_vB(code) + 1;
      symbol = GET_vC(code);

      // The receiver is the first argument.
      args = stackStart + GET_A(code);

      // The superclass is stored in a constant.
      classObj = AS_CLASS(args[numArgs]);
      goto completeCall;

    completeCall:
    int baseIndex = stackStart - fiber->stack;
    fiber->lastCallReg = baseIndex + GET_A(code);
    // If the class's method table doesn't include the symbol, bail.
    if (symbol >= classObj->methods.count ||
      (method = &classObj->methods.data[symbol])->type == METHOD_NONE)
      {
        methodNotFound(vm, classObj, symbol);
        RUNTIME_ERROR();
      }
      // printf("[%s: %d]\n", classObj->name->value, symbol);

      switch (method->type)
      {
      case METHOD_PRIMITIVE:
        if (!method->as.primitive(vm, args))
        {
          // An error, fiber switch, or call frame change occurred.
          STORE_FRAME();

          // If we don't have a fiber to switch to, stop interpreting.
          fiber = vm->fiber;
          if (fiber == NULL)
            return FODI_RESULT_SUCCESS;
          if (fodiHasError(fiber))
            RUNTIME_ERROR();
          LOAD_FRAME();
          frame->returnReg = baseIndex + GET_A(code);
        }
        break;

      case METHOD_FUNCTION_CALL:
        if (!checkArity(vm, args[0], numArgs))
        {
          RUNTIME_ERROR();
          break;
        }
        STORE_FRAME();
        method->as.primitive(vm, args);
        LOAD_FRAME();

        break;

      case METHOD_FOREIGN:
        // Set the top of the API stack in case the method is foreign
        fiber->apiStackTop = stackStart + GET_A(code) + numArgs;

        callForeign(vm, fiber, method->as.foreign, numArgs, stackStart + GET_A(code));
        stackStart = frame->stackStart; // Foreign calls can reallocate the stack.
        if (fodiHasError(fiber))
          RUNTIME_ERROR();
        break;

      case METHOD_BLOCK:
        // Set the top of the API stack in case the method is foreign
        fiber->apiStackTop = stackStart + GET_A(code) + numArgs;
        STORE_FRAME();
        fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + GET_A(code), numArgs, baseIndex + GET_A(code));
        LOAD_FRAME();
        break;

      case METHOD_NONE:
        UNREACHABLE();
        break;
      }
      DISPATCH();
    }

    {
      Value result;
      CASE_OP(RETURN) : if (GET_B(code) == 0)
                            result = NULL_VAL;
      else result = READ(GET_A(code));

      if (GET_C(code) == 1) // end module
        vm->lastModule = fn->module;

      CallFrame *oldFrame = &fiber->frames[fiber->numFrames - 1];
      fiber->numFrames--;
      // Close any upvalues still in scope.
      closeUpvalues(fiber, stackStart);

      // If the fiber is complete, end it.
      if (fiber->numFrames == 0)
      {
        // See if there's another fiber to return to. If not, we're done.
        if (fiber->caller == NULL)
        {
          // Store the final result value at the beginning of the stack so the
          // C API can get it.
          fiber->stack[0] = result;
          return FODI_RESULT_SUCCESS;
        }

        ObjFiber *resumingFiber = fiber->caller;
        fiber->caller = NULL;
        fiber = resumingFiber;
        vm->fiber = resumingFiber;
        fiber->stack[fiber->lastCallReg] = result;
      }
      if( oldFrame->returnReg != -1 )
        fiber->stack[oldFrame->returnReg] = result;
      else
        stackStart[0] = result;

      LOAD_FRAME();

      DISPATCH();
    }

    CASE_OP(ENDCLASS) : endClass(vm, stackStart, GET_A(code));
    if (fodiHasError(fiber))
      RUNTIME_ERROR();
    DISPATCH();

    CASE_OP(CLASS) :
    {
      int baseIndex = stackStart - fiber->stack;
      int fieldCount = abs(GET_sBx(code));
      if (GET_s(code) == 0)
        createClass(vm, fieldCount, NULL, baseIndex + GET_A(code));
      else
        createClass(vm, -1, fn->module, baseIndex + GET_A(code));

      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();
    }

    CASE_OP(METHOD) :
    {
      uint16_t symbol = abs(GET_sBx(code));
      ObjClass *classObj = AS_CLASS(READ(GET_A(code)));
      Value method = READ(GET_A(code) - 1);
      bindMethod(vm, GET_s(code) == 1, symbol, fn->module, classObj, method, stackStart);
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();
    }
    // does nothing, strictly debugging purposes
    CASE_OP(CLOSE) : // Close the upvalue for the local if we have one.
                     closeUpvalues(fiber, &stackStart[GET_A(code)]);
    DISPATCH();

    CASE_OP(IMPORTMODULE) :
    {
      // Make a slot on the stack for the module's fiber to place the return
      // value. It will be popped after this fiber is resumed. Store the
      // imported module's closure in the slot in case a GC happens when
      // invoking the closure.
      INSERT(importModule(vm, fn->constants.data[GET_Bx(code)]), GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      // If we get a closure, call it to execute the module body.
      if (IS_CLOSURE(READ(GET_A(code))))
      {
        STORE_FRAME();
        ObjClosure *closure = AS_CLOSURE(READ(GET_A(code)));
        fodiCallFunction(vm, fiber, closure, stackStart + GET_A(code), 1, -1);
        LOAD_FRAME();
      }
      else
      {
        // The module has already been loaded. Remember it so we can import
        // variables from it if needed.
        vm->lastModule = AS_MODULE(READ(GET_A(code)));
      }

      DISPATCH();
    }

    CASE_OP(IMPORTVAR) :
    {
      Value variable = fn->constants.data[GET_Bx(code)];
      ASSERT(vm->lastModule != NULL, "Should have already imported module.");
      Value result = getModuleVariable(vm, vm->lastModule, variable);
      if (fodiHasError(fiber))
        RUNTIME_ERROR();

      INSERT(result, GET_A(code));
      DISPATCH();
    }
    {
      int symbol;
      Value opperand;
      ObjClass *targetClass;
      Method *method;

      CASE_OP(NOT):
        opperand = READ(GET_B(code));
        if (IS_CLASS(opperand) || IS_INSTANCE(opperand))
        {
          targetClass = fodiGetClassInline(vm, opperand);
          symbol = fodiSymbolTableFind(&vm->methodNames, "!", 1);
          if (symbol < targetClass->methods.count &&
              (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
            goto unaryOverload;
        }
        INSERT(fodiNot(vm, READ(GET_B(code))), GET_A(code));
        DISPATCH();

      CASE_OP(NEG):
        opperand = READ(GET_B(code));
        if (IS_CLASS(opperand) || IS_INSTANCE(opperand))
        {
          targetClass = fodiGetClassInline(vm, opperand);
          symbol = fodiSymbolTableFind(&vm->methodNames, "-", 1);
          if (symbol < targetClass->methods.count &&
              (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
            goto unaryOverload;
        }
        INSERT(fodiNegative(vm, READ(GET_B(code))), GET_A(code));
        DISPATCH();

      unaryOverload:
        int baseIndex = stackStart - fiber->stack;
        int stackTop = fn->stackTop.data[(rip - fn->code.data)];
        int needed = stackTop + method->as.closure->fn->maxSlots;
        fodiEnsureStack(vm, fiber, baseIndex + needed);


        INSERT(opperand, stackTop);

        STORE_FRAME();
        fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 1, GET_A(code));
        LOAD_FRAME();
        DISPATCH();

    }

    {
      int symbol;
      Value left;
      Value right;
      ObjClass *targetClass;
      Method *method;

      CASE_OP(EQ) : 
        left = READ(GET_B(code));
        right = READ(GET_C(code));
        goto finishEQ;
      CASE_OP(EQK) : 
        left = GET_K(code) == 0 ? READ(GET_B(code)) : fn->constants.data[GET_C(code)];
        right = GET_K(code) == 1 ? READ(GET_B(code)) : fn->constants.data[GET_C(code)];
        goto finishEQ;

    finishEQ:
      // Check for overloaded operator
      if (IS_CLASS(left) || IS_INSTANCE(left))
      {
        targetClass = fodiGetClassInline(vm, left);
        symbol = fodiSymbolTableFind(&vm->methodNames, GET_A(code) == 0 ? "==(_)" : "!=(_)", 5);
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
          goto comparisonOverload;
      }
      if (fodiValuesEqual(left, right) != (bool)GET_A(code))
        rip++;
      DISPATCH();


      CASE_OP(LT) :
        left = RKREAD(GET_B(code));
        right = RKREAD(GET_C(code));
        goto finishLT;
      CASE_OP(LTK) :
        left = GET_K(code) == 0 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        right = GET_K(code) == 1 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        goto finishLT;
      finishLT:
        // Check for overloaded operator
        if (IS_CLASS(left) || IS_INSTANCE(left))
        {
          targetClass = fodiGetClassInline(vm, left);
          symbol = fodiSymbolTableFind(&vm->methodNames, GET_A(code) == 0 ? "<(_)" : ">=(_)", GET_A(code) == 0 ? 4 : 5);
          if (symbol < targetClass->methods.count &&
              (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
            goto comparisonOverload;
        }
        if (!IS_NUM(left))
        {
          vm->fiber->error = CONST_STRING(vm, "Left operand must be a number.");
          RUNTIME_ERROR();
        }

        if (!IS_NUM(right))
        {
          vm->fiber->error = CONST_STRING(vm, "Right operand must be a number.");
          RUNTIME_ERROR();
        }
        if ((AS_NUM(left) < AS_NUM(right)) != (bool)GET_A(code))
          rip++;
        DISPATCH();


      CASE_OP(LTE) :
        left = RKREAD(GET_B(code));
        right = RKREAD(GET_C(code));
        goto finishLTE;
      CASE_OP(LTEK) :
        left = GET_K(code) == 0 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        right = GET_K(code) == 1 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        goto finishLTE;

      finishLTE:
        if (IS_CLASS(left) || IS_INSTANCE(left))
        {
          targetClass = fodiGetClassInline(vm, left);
          symbol = fodiSymbolTableFind(&vm->methodNames, GET_A(code) == 0 ? "<=(_)" : ">(_)", GET_A(code) == 0 ? 5 : 4);
          if (symbol < targetClass->methods.count &&
              (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
            goto comparisonOverload;
        }
        if (!IS_NUM(left))
        {
          vm->fiber->error = CONST_STRING(vm, "Left operand must be a number.");
          RUNTIME_ERROR();
        }

        if (!IS_NUM(right))
        {
          vm->fiber->error = CONST_STRING(vm, "Right operand must be a number.");
          RUNTIME_ERROR();
        }

        if ((AS_NUM(left) <= AS_NUM(right)) != (bool)GET_A(code))
          rip++;
        DISPATCH();


      CASE_OP(ADD) : 
        left = READ(GET_B(code));
        right = READ(GET_C(code));
        goto finishADD;

      CASE_OP(ADDK) :
        if (!IS_LIST(READ(GET_B(code)) ))
        {
          left = GET_K(code) == 0 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
          right = GET_K(code) == 1 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        }
        else
        {
          left = READ(GET_B(code));
          right = fn->constants.data[GET_C(code)];
        }
        goto finishADD;

    finishADD:
      if (IS_CLASS(left) || IS_INSTANCE(left))
      {
        targetClass = fodiGetClassInline(vm, left);
        if(IS_LIST(left) && GET_K(code) == 1 )
          symbol = fodiSymbolTableFind(&vm->methodNames, "add(_)", 6);
        else
          symbol = fodiSymbolTableFind(&vm->methodNames, "+(_)", 4);
          
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type != METHOD_NONE)
          goto checkOverload;
      }
      INSERT(fodiAdd(vm, left, right), GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();


      CASE_OP(SUB) : 
        left = READ(GET_B(code));
        right = READ(GET_C(code));
        goto finishSUB;
      CASE_OP(SUBK) : 
        left = GET_K(code) == 0 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        right = GET_K(code) == 1 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        goto finishSUB;

    finishSUB:
      if (IS_CLASS(left) || IS_INSTANCE(left))
      {
        targetClass = fodiGetClassInline(vm, left);
        symbol = fodiSymbolTableFind(&vm->methodNames, "-(_)", 4);
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
          goto checkOverload;
      }

      INSERT(fodiSubtract(vm, left, right), GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();


      CASE_OP(MUL) : 
        left = RKREAD(GET_B(code));
        right = RKREAD(GET_C(code));
        goto finishMUL;
      CASE_OP(MULK) : 
        left = GET_K(code) == 0 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        right = GET_K(code) == 1 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        goto finishMUL;

      finishMUL:
      if (IS_CLASS(left) || IS_INSTANCE(left))
      {
        targetClass = fodiGetClassInline(vm, left);
        symbol = fodiSymbolTableFind(&vm->methodNames, "*(_)", 4);
        method = NULL;
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
          goto checkOverload;
      }

      INSERT(fodiMultiply(vm, left, right), GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();


      CASE_OP(DIV) : 
        left = RKREAD(GET_B(code));
        right = RKREAD(GET_C(code));
        goto finishDIV;
      CASE_OP(DIVK) : 
        left = GET_K(code) == 0 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        right = GET_K(code) == 1 ? RKREAD(GET_B(code)) : fn->constants.data[GET_C(code)];
        goto finishDIV;

      finishDIV:
      if (IS_CLASS(left) || IS_INSTANCE(left))
      {
        targetClass = fodiGetClassInline(vm, left);
        symbol = fodiSymbolTableFind(&vm->methodNames, "/(_)", 4);
        method = NULL;
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type == METHOD_BLOCK)
          goto checkOverload;
      }

      INSERT(fodiDivide(vm, left, right), GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();
 
    int stackTop;
    int needed;
    int baseIndex = stackStart - fiber->stack;
    checkOverload:
      stackTop = fn->stackTop.data[(rip - fn->code.data)];

      if(targetClass->methods.data[symbol].type == METHOD_BLOCK)
        needed = stackTop + method->as.closure->fn->maxSlots;
      else
        needed = stackTop + 2; // for primitive

      fodiEnsureStack(vm, fiber, baseIndex + needed);
      stackStart = frame->stackStart; // In case the stack was reallocated.
      
      INSERT(left, stackTop);
      INSERT(right, stackTop + 1);


      STORE_FRAME();

      if(targetClass->methods.data[symbol].type == METHOD_PRIMITIVE)
        method->as.primitive(vm, stackStart + stackTop);
      else
        fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 2, GET_A(code));

      LOAD_FRAME();
      DISPATCH();

    comparisonOverload:{
      stackTop = fn->stackTop.data[(rip - fn->code.data)];
      needed = stackTop + method->as.closure->fn->maxSlots;
      fodiEnsureStack(vm, fiber, baseIndex + needed);

      stackStart = frame->stackStart; // In case the stack was reallocated.
      
      int returnReg;
      if (GET_OPCODE(*rip) == OP_LOADBOOL)
      {
        setInstructionField((rip), Field_OP, OP_NOOP);
        setInstructionField((rip + 1), Field_OP, OP_NOOP);
        returnReg = GET_A(*rip);
      }
      else
      {
        returnReg = fiber->stackCapacity - 2;
      }
      
      INSERT(left, stackTop);
      INSERT(right, stackTop + 1);

      STORE_FRAME();
      fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 2, returnReg);
      LOAD_FRAME();
      DISPATCH();
    }
  }

  {
    Value left;
    Value right;
    CASE_OP(ADDELEM) :
    left = READ(GET_B(code));
    right = READ(GET_C(code));
    goto finishAddElem;

    CASE_OP(ADDELEMK) :
    left = READ(GET_B(code));
    right = fn->constants.data[GET_C(code)];
    goto finishAddElem;

    finishAddElem:
    Value list = fodiAddList(vm, AS_LIST(left), right, GET_K(code) == 0);
    if (fodiHasError(fiber))
      RUNTIME_ERROR();
    if(!IS_NULL(list))
      INSERT(list, GET_A(code));
    DISPATCH();
  }
  
  CASE_OP(ITERATE) :
    {
      Value sequence = READ(GET_B(code));
      Value iterator = GET_K(code) == 0 ? READ(GET_C(code)) : fn->constants.data[GET_C(code)];
      if (IS_CLASS(sequence) || IS_INSTANCE(sequence))
      {
        ObjClass *targetClass = fodiGetClassInline(vm, sequence);
        int symbol = fodiSymbolTableFind(&vm->methodNames, "iterate(_)", 10);
        Method *method;
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type != METHOD_NONE)
        {
          int baseIndex = stackStart - fiber->stack;
          int stackTop = fn->stackTop.data[(rip - fn->code.data)];
          int needed = stackTop + method->as.closure->fn->maxSlots;
          fodiEnsureStack(vm, fiber, baseIndex + needed);
          stackStart = frame->stackStart; // In case the stack was reallocated.

          INSERT(sequence, stackTop);
          INSERT(iterator, stackTop + 1);

          STORE_FRAME();
          fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 2, baseIndex + GET_A(code));
          LOAD_FRAME();
          DISPATCH();
        }
      }
      INSERT(fodiIterate(vm, sequence, iterator), GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();
    }

  CASE_OP(ITERATORVALUE) :{
      Value sequence = READ(GET_B(code));
      Value iterator = GET_K(code) == 0 ? READ(GET_C(code)) : fn->constants.data[GET_C(code)];
      if (IS_CLASS(sequence) || IS_INSTANCE(sequence))
      {
        ObjClass *targetClass = fodiGetClassInline(vm, sequence);
        int symbol = fodiSymbolTableFind(&vm->methodNames, "iteratorValue(_)", 16);
        Method *method;
        if (symbol < targetClass->methods.count &&
            (method = &targetClass->methods.data[symbol])->type != METHOD_NONE)
        {
          int baseIndex = stackStart - fiber->stack;
          int stackTop = fn->stackTop.data[(rip - fn->code.data)];
          int needed;
          if(method->type == METHOD_BLOCK)
            needed = stackTop + method->as.closure->fn->maxSlots;
          else
            needed = stackTop + 2; // for primitive

          fodiEnsureStack(vm, fiber, baseIndex + needed);
          stackStart = frame->stackStart; // In case the stack was reallocated.

          INSERT(sequence, stackTop);
          INSERT(iterator, stackTop + 1);
          if(method->type == METHOD_PRIMITIVE)
          {
            STORE_FRAME();
            method->as.primitive(vm, stackStart + stackTop);
            INSERT(stackStart[stackTop], GET_A(code));
            LOAD_FRAME();
            DISPATCH();
          }
          STORE_FRAME();
          fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 2, baseIndex + GET_A(code));
          LOAD_FRAME();
          DISPATCH();
        }
      }
      Value result = fodiIteratorValue(vm, sequence, iterator);
      if(IS_MAPENTRY(result) && GET_OPCODE(*rip) == OP_GETFIELD && GET_B(*rip) == GET_A(code))
      {
        if(GET_C(*rip) == 0) 
          INSERT(AS_MAPENTRY(result)->key, GET_A(*rip));
        else 
          INSERT(AS_MAPENTRY(result)->value, GET_A(*rip));

        // skip the GETFIELD instruction since we already have the value
        ++rip;
        DISPATCH();
      }

      INSERT(result, GET_A(code));
      if (fodiHasError(fiber))
        RUNTIME_ERROR();
      DISPATCH();
  }

  CASE_OP(GETSUB)  :
  {
    Value receiver = READ(GET_B(code));
    Value subscript = GET_K(code) == 0 ? READ(GET_C(code)) : fn->constants.data[GET_C(code)];
    if (IS_CLASS(receiver) || IS_INSTANCE(receiver))
    {
      ObjClass *targetClass = fodiGetClassInline(vm, receiver);
      int symbol = fodiSymbolTableFind(&vm->methodNames, "[_]", 3);
      Method *method;
      if (symbol < targetClass->methods.count &&
          (method = &targetClass->methods.data[symbol])->type != METHOD_NONE)
      {
        int baseIndex = stackStart - fiber->stack;
        int stackTop = fn->stackTop.data[(rip - fn->code.data)];
        int needed = stackTop + method->as.closure->fn->maxSlots;
        
        fodiEnsureStack(vm, fiber, baseIndex + needed);
        stackStart = frame->stackStart; // In case the stack was reallocated.

        INSERT(receiver, stackTop);
        INSERT(subscript, stackTop + 1);

        STORE_FRAME();
        fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 2, baseIndex + GET_A(code));
        LOAD_FRAME();
        DISPATCH();
      }
    }

    INSERT(fodiSubscript(vm, receiver, subscript), GET_A(code));
    if (fodiHasError(fiber))
      RUNTIME_ERROR();
    DISPATCH();
  }

  CASE_OP(SETSUB)  :
  {
    Value receiver = READ(GET_B(code));
    Value subscript = GET_K(code) == 0 ? READ(GET_C(code)) : fn->constants.data[GET_C(code)];
    Value value = READ(GET_A(code));

    if (IS_CLASS(receiver) || IS_INSTANCE(receiver))
    {
      ObjClass *targetClass = fodiGetClassInline(vm, receiver);
      int symbol = fodiSymbolTableFind(&vm->methodNames, "[_]=(_)", 7);
      Method *method;
      if (symbol < targetClass->methods.count &&
          (method = &targetClass->methods.data[symbol])->type != METHOD_NONE)
      {
        int baseIndex = stackStart - fiber->stack;
        int stackTop = fn->stackTop.data[(rip - fn->code.data)];
        int needed = stackTop + method->as.closure->fn->maxSlots;
        
        fodiEnsureStack(vm, fiber, baseIndex + needed);
        stackStart = frame->stackStart; // In case the stack was reallocated.

        INSERT(receiver, stackTop);
        INSERT(subscript, stackTop + 1);
        INSERT(value, stackTop + 2);

        STORE_FRAME();
        fodiCallFunction(vm, fiber, (ObjClosure *)method->as.closure, stackStart + stackTop, 3, baseIndex + GET_A(code));
        LOAD_FRAME();
        DISPATCH();
      }
    }

    fodiSetSubscript(vm, receiver, subscript, value);
    // INSERT(fodiSetSubscript(vm, receiver, subscript, value), GET_A(code));
    if (fodiHasError(fiber))
      RUNTIME_ERROR();
    DISPATCH();
  }

    CASE_OP(RANGE)  :
  {
    Value fromVal = READ(GET_B(code));
    Value toVal = READ(GET_C(code));
    if (!validateNum(vm, toVal, "Right hand side of range"))
      return false;

    INSERT(fodiNewRange(vm, AS_NUM(fromVal), AS_NUM(toVal), GET_K(code) == 1), GET_A(code));
    DISPATCH();
  }


    CASE_OP(NOOP) : DISPATCH();
  }
  // We should only exit this function from an explicit return from CODE_RETURN
  // or a runtime error.
  UNREACHABLE();
  return FODI_RESULT_RUNTIME_ERROR;
}

FodiHandle *fodiMakeCallHandle(FodiVM *vm, const char *signature)
{
  ASSERT(signature != NULL, "Signature cannot be NULL.");

  int signatureLength = (int)strlen(signature);
  ASSERT(signatureLength > 0, "Signature cannot be empty.");

  // Count the number parameters the method expects.
  int numParams = 0;
  if (signature[signatureLength - 1] == ')')
  {
    for (int i = signatureLength - 1; i > 0 && signature[i] != '('; i--)
    {
      if (signature[i] == '_')
        numParams++;
    }
  }

  // Count subscript arguments.
  if (signature[0] == '[')
  {
    for (int i = 0; i < signatureLength && signature[i] != ']'; i++)
    {
      if (signature[i] == '_')
        numParams++;
    }
  }

  // Add the signatue to the method table.
  int method = fodiSymbolTableEnsure(vm, &vm->methodNames,
                                     signature, signatureLength);

  // Create a little stub function that assumes the arguments are on the stack
  // and calls the method.
  ObjFn *fn = fodiNewFunction(vm, NULL, numParams + 1);

  // Wrap the function in a closure and then in a handle. Do this here so it
  // doesn't get collected as we fill it in.
  FodiHandle *value = fodiMakeHandle(vm, OBJ_VAL(fn));
  value->value = OBJ_VAL(fodiNewClosure(vm, fn, false));
  fodiInstBufferWrite(vm, &fn->code, makeInstructionvABC(OP_CALL, 0, numParams, method));
  fodiInstBufferWrite(vm, &fn->code, makeInstructionABC(OP_RETURN, 0, 1, 0, 0));
  fodiIntBufferFill(vm, &fn->debug->sourceLines, 0, 2);
  fodiFunctionBindName(vm, fn, signature, signatureLength);

  return value;
}

FodiInterpretResult fodiCall(FodiVM *vm, FodiHandle *method)
{
  ASSERT(method != NULL, "Method cannot be NULL.");
  ASSERT(IS_CLOSURE(method->value), "Method must be a method handle.");
  ASSERT(vm->fiber != NULL, "Must set up arguments for call first.");
  ASSERT(vm->apiStack != NULL, "Must set up arguments for call first.");
  ASSERT(vm->fiber->numFrames == 0, "Can not call from a foreign method.");
  ObjClosure *closure = AS_CLOSURE(method->value);
  
  //fill the stacktop buffer with the highest register used since we couldn't do it during compilation of the function
  fodiIntBufferFill(vm, &closure->fn->stackTop, closure->fn->maxSlots, closure->fn->code.count);
  
  ASSERT(vm->fiber->apiStackTop - vm->fiber->stack >= closure->fn->arity,
         "Stack must have enough arguments for method.");

  // Clear the API stack. Now that fodiCall() has control, we no longer need
  // it. We use this being non-null to tell if re-entrant calls to foreign
  // methods are happening, so it's important to clear it out now so that you
  // can call foreign methods from within calls to fodiCall().
  vm->apiStack = NULL;

  // Discard any extra temporary slots. We take for granted that the stub
  // function has exactly one slot for each argument.
  vm->fiber->apiStackTop = vm->fiber->stack + closure->fn->maxSlots;

  fodiCallFunction(vm, vm->fiber, closure, vm->fiber->stack, 0, -1);
  FodiInterpretResult result = runInterpreter(vm, vm->fiber);

  // If the call didn't abort, then set up the API stack to point to the
  // beginning of the stack so the host can access the call's return value.
  if (vm->fiber != NULL)
  {
    vm->apiStack = vm->fiber->stack;
    vm->fiber->apiStackTop = vm->fiber->stack + 1;
  }

  return result;
}

FodiHandle *fodiMakeHandle(FodiVM *vm, Value value)
{
  if (IS_OBJ(value))
    fodiPushRoot(vm, AS_OBJ(value));

  // Make a handle for it.
  FodiHandle *handle = ALLOCATE(vm, FodiHandle);
  handle->value = value;

  if (IS_OBJ(value))
    fodiPopRoot(vm);

  // Add it to the front of the linked list of handles.
  if (vm->handles != NULL)
    vm->handles->prev = handle;
  handle->prev = NULL;
  handle->next = vm->handles;
  vm->handles = handle;

  return handle;
}

void fodiReleaseHandle(FodiVM *vm, FodiHandle *handle)
{
  ASSERT(handle != NULL, "Handle cannot be NULL.");

  // Update the VM's head pointer if we're releasing the first handle.
  if (vm->handles == handle)
    vm->handles = handle->next;

  // Unlink it from the list.
  if (handle->prev != NULL)
    handle->prev->next = handle->next;
  if (handle->next != NULL)
    handle->next->prev = handle->prev;

  // Clear it out. This isn't strictly necessary since we're going to free it,
  // but it makes for easier debugging.
  handle->prev = NULL;
  handle->next = NULL;
  handle->value = NULL_VAL;
  DEALLOCATE(vm, handle);
}

FodiInterpretResult fodiInterpret(FodiVM *vm, const char *module,
                                  const char *source)
{
  ObjClosure *closure = fodiCompileSource(vm, module, source, false, true);
  if (closure == NULL)
    return FODI_RESULT_COMPILE_ERROR;

  fodiPushRoot(vm, (Obj *)closure);
  ObjFiber *fiber = fodiNewFiber(vm, closure);
  fodiPopRoot(vm); // closure.
  vm->apiStack = NULL;
  FodiInterpretResult result = runInterpreter(vm, fiber);
#if FODI_OPCODE_EXECUTION_COUNT
  printf("\n");
  printf(" ========== OPCODE COUNTS ========== \n");
  printf("Dispatches: %zu\n", vm->dispatchCount);
  vm->dispatchCount = 0;
  for (int i = 0; i < OP_COUNT; i++){
    printf("Opcode: %s (%zu)\n", getOPName(i), vm->opcodeCounts[i]);
    vm->opcodeCounts[i] = 0; // reset for next run
  }
  printf(" =================================== \n");
#endif
  return result;
}

ObjClosure *fodiCompileSource(FodiVM *vm, const char *module, const char *source,
                              bool isExpression, bool printErrors)
{
  Value nameValue = NULL_VAL;
  if (module != NULL)
  {
    nameValue = fodiNewString(vm, module);
    fodiPushRoot(vm, AS_OBJ(nameValue));
  }

  ObjClosure *closure = compileInModule(vm, nameValue, source,
                                        isExpression, printErrors);

  if (module != NULL)
    fodiPopRoot(vm); // nameValue.
  return closure;
}

Value fodiGetModuleVariable(FodiVM *vm, Value moduleName, Value variableName)
{
  ObjModule *module = getModule(vm, moduleName);
  if (module == NULL)
  {
    vm->fiber->error = fodiStringFormat(vm, "Module '@' is not loaded.",
                                        moduleName);
    return NULL_VAL;
  }

  return getModuleVariable(vm, module, variableName);
}

Value fodiFindVariable(FodiVM *vm, ObjModule *module, const char *name)
{
  int symbol = fodiSymbolTableFind(&module->variableNames, name, strlen(name));
  return module->variables.data[symbol];
}

int fodiDeclareVariable(FodiVM *vm, ObjModule *module, const char *name,
                        size_t length, int line)
{
  if (module->variables.count == MAX_MODULE_VARS)
    return -2;

  // Implicitly defined variables get a "value" that is the line where the
  // variable is first used. We'll use that later to report an error on the
  // right line.
  fodiValueBufferWrite(vm, &module->variables, NUM_VAL(line));
  return fodiSymbolTableAdd(vm, &module->variableNames, name, length);
}

int fodiDefineVariable(FodiVM *vm, ObjModule *module, const char *name,
                       size_t length, Value value, int *line)
{
  if (module->variables.count == MAX_MODULE_VARS)
    return -2;

  if (IS_OBJ(value))
    fodiPushRoot(vm, AS_OBJ(value));

  // See if the variable is already explicitly or implicitly declared.
  int symbol = fodiSymbolTableFind(&module->variableNames, name, length);

  if (symbol == -1)
  {
    // Brand new variable.
    symbol = fodiSymbolTableAdd(vm, &module->variableNames, name, length);
    fodiValueBufferWrite(vm, &module->variables, value);
  }
  else if (IS_NUM(module->variables.data[symbol]))
  {
    // An implicitly declared variable's value will always be a number.
    // Now we have a real definition.
    if (line)
      *line = (int)AS_NUM(module->variables.data[symbol]);
    module->variables.data[symbol] = value;

    // If this was a localname we want to error if it was
    // referenced before this definition.
    if (fodiIsLocalName(name))
      symbol = -3;
  }
  else
  {
    // Already explicitly declared.
    symbol = -1;
  }

  if (IS_OBJ(value))
    fodiPopRoot(vm);

  return symbol;
}

// TODO: Inline?
void fodiPushRoot(FodiVM *vm, Obj *obj)
{
  ASSERT(obj != NULL, "Can't root NULL.");
  ASSERT(vm->numTempRoots < FODI_MAX_TEMP_ROOTS, "Too many temporary roots.");

  vm->tempRoots[vm->numTempRoots++] = obj;
}

void fodiPopRoot(FodiVM *vm)
{
  ASSERT(vm->numTempRoots > 0, "No temporary roots to release.");
  vm->numTempRoots--;
}

int fodiGetSlotCount(FodiVM *vm)
{
  if (vm->apiStack == NULL)
    return 0;

  return (int)(vm->fiber->apiStackTop - vm->apiStack);
}

void fodiEnsureSlots(FodiVM *vm, int numSlots)
{
  // If we don't have a fiber accessible, create one for the API to use.
  if (vm->apiStack == NULL)
  {
    vm->fiber = fodiNewFiber(vm, NULL);
    vm->apiStack = vm->fiber->stack;
    vm->fiber->apiStackTop = vm->apiStack;
  }

  int currentSize = (int)(vm->fiber->apiStackTop - vm->apiStack);
  if (currentSize >= numSlots)
    return;

  // Grow the stack if needed.
  int needed = (int)(vm->apiStack - vm->fiber->stack) + numSlots;
  fodiEnsureStack(vm, vm->fiber, needed);

  vm->fiber->apiStackTop = vm->apiStack + numSlots;
}

// Ensures that [slot] is a valid index into the API's stack of slots.
static void validateApiSlot(FodiVM *vm, int slot)
{
  ASSERT(slot >= 0, "Slot cannot be negative.");
  ASSERT(slot < fodiGetSlotCount(vm), "Not that many slots.");
}

// Gets the type of the object in [slot].
FodiType fodiGetSlotType(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  if (IS_BOOL(vm->apiStack[slot]))
    return FODI_TYPE_BOOL;
  if (IS_NUM(vm->apiStack[slot]))
    return FODI_TYPE_NUM;
  if (IS_FOREIGN(vm->apiStack[slot]))
    return FODI_TYPE_FOREIGN;
  if (IS_LIST(vm->apiStack[slot]))
    return FODI_TYPE_LIST;
  if (IS_MAP(vm->apiStack[slot]))
    return FODI_TYPE_MAP;
  if (IS_NULL(vm->apiStack[slot]))
    return FODI_TYPE_NULL;
  if (IS_STRING(vm->apiStack[slot]))
    return FODI_TYPE_STRING;

  return FODI_TYPE_UNKNOWN;
}

bool fodiGetSlotBool(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_BOOL(vm->apiStack[slot]), "Slot must hold a bool.");

  return AS_BOOL(vm->apiStack[slot]);
}

const char *fodiGetSlotBytes(FodiVM *vm, int slot, int *length)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_STRING(vm->apiStack[slot]), "Slot must hold a string.");

  ObjString *string = AS_STRING(vm->apiStack[slot]);
  *length = string->length;
  return string->value;
}

double fodiGetSlotDouble(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_NUM(vm->apiStack[slot]), "Slot must hold a number.");

  return AS_NUM(vm->apiStack[slot]);
}

void *fodiGetSlotForeign(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_FOREIGN(vm->apiStack[slot]),
         "Slot must hold a foreign instance.");

  return AS_FOREIGN(vm->apiStack[slot])->data;
}

const char *fodiGetSlotString(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_STRING(vm->apiStack[slot]), "Slot must hold a string.");

  return AS_CSTRING(vm->apiStack[slot]);
}

FodiHandle *fodiGetSlotHandle(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  return fodiMakeHandle(vm, vm->apiStack[slot]);
}

// Stores [value] in [slot] in the foreign call stack.
static void setSlot(FodiVM *vm, int slot, Value value)
{
  validateApiSlot(vm, slot);
  vm->apiStack[slot] = value;
}

void fodiSetSlotBool(FodiVM *vm, int slot, bool value)
{
  setSlot(vm, slot, BOOL_VAL(value));
}

void fodiSetSlotBytes(FodiVM *vm, int slot, const char *bytes, size_t length)
{
  ASSERT(bytes != NULL, "Byte array cannot be NULL.");
  setSlot(vm, slot, fodiNewStringLength(vm, bytes, length));
}

void fodiSetSlotDouble(FodiVM *vm, int slot, double value)
{
  setSlot(vm, slot, NUM_VAL(value));
}

void *fodiSetSlotNewForeign(FodiVM *vm, int slot, int classSlot, size_t size)
{
  validateApiSlot(vm, slot);
  validateApiSlot(vm, classSlot);
  ASSERT(IS_CLASS(vm->apiStack[classSlot]), "Slot must hold a class.");

  ObjClass *classObj = AS_CLASS(vm->apiStack[classSlot]);
  ASSERT(classObj->numFields == -1, "Class must be a foreign class.");

  ObjForeign *foreign = fodiNewForeign(vm, classObj, size);
  vm->apiStack[slot] = OBJ_VAL(foreign);
  return (void *)foreign->data;
}

void fodiSetSlotNewList(FodiVM *vm, int slot)
{
  setSlot(vm, slot, OBJ_VAL(fodiNewList(vm, 0)));
}

void fodiSetSlotNewMap(FodiVM *vm, int slot)
{
  setSlot(vm, slot, OBJ_VAL(fodiNewMap(vm)));
}

void fodiSetSlotNull(FodiVM *vm, int slot)
{
  setSlot(vm, slot, NULL_VAL);
}

void fodiSetSlotString(FodiVM *vm, int slot, const char *text)
{
  ASSERT(text != NULL, "String cannot be NULL.");

  setSlot(vm, slot, fodiNewString(vm, text));
}

void fodiSetSlotHandle(FodiVM *vm, int slot, FodiHandle *handle)
{
  ASSERT(handle != NULL, "Handle cannot be NULL.");

  setSlot(vm, slot, handle->value);
}

int fodiGetListCount(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_LIST(vm->apiStack[slot]), "Slot must hold a list.");

  ValueBuffer elements = AS_LIST(vm->apiStack[slot])->elements;
  return elements.count;
}

void fodiGetListElement(FodiVM *vm, int listSlot, int index, int elementSlot)
{
  validateApiSlot(vm, listSlot);
  validateApiSlot(vm, elementSlot);
  ASSERT(IS_LIST(vm->apiStack[listSlot]), "Slot must hold a list.");

  ValueBuffer elements = AS_LIST(vm->apiStack[listSlot])->elements;

  uint32_t usedIndex = fodiValidateIndex(elements.count, index);
  ASSERT(usedIndex != UINT32_MAX, "Index out of bounds.");

  vm->apiStack[elementSlot] = elements.data[usedIndex];
}

void fodiSetListElement(FodiVM *vm, int listSlot, int index, int elementSlot)
{
  validateApiSlot(vm, listSlot);
  validateApiSlot(vm, elementSlot);
  ASSERT(IS_LIST(vm->apiStack[listSlot]), "Slot must hold a list.");

  ObjList *list = AS_LIST(vm->apiStack[listSlot]);

  uint32_t usedIndex = fodiValidateIndex(list->elements.count, index);
  ASSERT(usedIndex != UINT32_MAX, "Index out of bounds.");

  list->elements.data[usedIndex] = vm->apiStack[elementSlot];
}

void fodiInsertInList(FodiVM *vm, int listSlot, int index, int elementSlot)
{
  validateApiSlot(vm, listSlot);
  validateApiSlot(vm, elementSlot);
  ASSERT(IS_LIST(vm->apiStack[listSlot]), "Must insert into a list.");

  ObjList *list = AS_LIST(vm->apiStack[listSlot]);

  // Negative indices count from the end.
  // We don't use fodiValidateIndex here because insert allows 1 past the end.
  if (index < 0)
    index = list->elements.count + 1 + index;

  ASSERT(index <= list->elements.count, "Index out of bounds.");

  fodiListInsert(vm, list, vm->apiStack[elementSlot], index);
}

int fodiGetMapCount(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  ASSERT(IS_MAP(vm->apiStack[slot]), "Slot must hold a map.");

  ObjMap *map = AS_MAP(vm->apiStack[slot]);
  return map->count;
}

bool fodiGetMapContainsKey(FodiVM *vm, int mapSlot, int keySlot)
{
  validateApiSlot(vm, mapSlot);
  validateApiSlot(vm, keySlot);
  ASSERT(IS_MAP(vm->apiStack[mapSlot]), "Slot must hold a map.");

  Value key = vm->apiStack[keySlot];
  ASSERT(fodiMapIsValidKey(key), "Key must be a value type");
  if (!validateKey(vm, key))
    return false;

  ObjMap *map = AS_MAP(vm->apiStack[mapSlot]);
  Value value = fodiMapGet(map, key);

  return !IS_UNDEFINED(value);
}

void fodiGetMapValue(FodiVM *vm, int mapSlot, int keySlot, int valueSlot)
{
  validateApiSlot(vm, mapSlot);
  validateApiSlot(vm, keySlot);
  validateApiSlot(vm, valueSlot);
  ASSERT(IS_MAP(vm->apiStack[mapSlot]), "Slot must hold a map.");

  ObjMap *map = AS_MAP(vm->apiStack[mapSlot]);
  Value value = fodiMapGet(map, vm->apiStack[keySlot]);
  if (IS_UNDEFINED(value))
  {
    value = NULL_VAL;
  }

  vm->apiStack[valueSlot] = value;
}

void fodiSetMapValue(FodiVM *vm, int mapSlot, int keySlot, int valueSlot)
{
  validateApiSlot(vm, mapSlot);
  validateApiSlot(vm, keySlot);
  validateApiSlot(vm, valueSlot);
  ASSERT(IS_MAP(vm->apiStack[mapSlot]), "Must insert into a map.");

  Value key = vm->apiStack[keySlot];
  ASSERT(fodiMapIsValidKey(key), "Key must be a value type");

  if (!validateKey(vm, key))
  {
    return;
  }

  Value value = vm->apiStack[valueSlot];
  ObjMap *map = AS_MAP(vm->apiStack[mapSlot]);

  fodiMapSet(vm, map, key, value);
}

void fodiRemoveMapValue(FodiVM *vm, int mapSlot, int keySlot,
                        int removedValueSlot)
{
  validateApiSlot(vm, mapSlot);
  validateApiSlot(vm, keySlot);
  ASSERT(IS_MAP(vm->apiStack[mapSlot]), "Slot must hold a map.");

  Value key = vm->apiStack[keySlot];
  if (!validateKey(vm, key))
  {
    return;
  }

  ObjMap *map = AS_MAP(vm->apiStack[mapSlot]);
  Value removed = fodiMapRemoveKey(vm, map, key);
  setSlot(vm, removedValueSlot, removed);
}

void fodiGetVariable(FodiVM *vm, const char *module, const char *name,
                     int slot)
{
  ASSERT(module != NULL, "Module cannot be NULL.");
  ASSERT(name != NULL, "Variable name cannot be NULL.");

  Value moduleName = fodiStringFormat(vm, "$", module);
  fodiPushRoot(vm, AS_OBJ(moduleName));

  ObjModule *moduleObj = getModule(vm, moduleName);
  ASSERT(moduleObj != NULL, "Could not find module.");

  fodiPopRoot(vm); // moduleName.

  int variableSlot = fodiSymbolTableFind(&moduleObj->variableNames,
                                         name, strlen(name));
  ASSERT(variableSlot != -1, "Could not find variable.");

  setSlot(vm, slot, moduleObj->variables.data[variableSlot]);
}

bool fodiHasVariable(FodiVM *vm, const char *module, const char *name)
{
  ASSERT(module != NULL, "Module cannot be NULL.");
  ASSERT(name != NULL, "Variable name cannot be NULL.");

  Value moduleName = fodiStringFormat(vm, "$", module);
  fodiPushRoot(vm, AS_OBJ(moduleName));

  // We don't use fodiHasModule since we want to use the module object.
  ObjModule *moduleObj = getModule(vm, moduleName);
  ASSERT(moduleObj != NULL, "Could not find module.");

  fodiPopRoot(vm); // moduleName.

  int variableSlot = fodiSymbolTableFind(&moduleObj->variableNames,
                                         name, strlen(name));

  return variableSlot != -1;
}

bool fodiHasModule(FodiVM *vm, const char *module)
{
  ASSERT(module != NULL, "Module cannot be NULL.");

  Value moduleName = fodiStringFormat(vm, "$", module);
  fodiPushRoot(vm, AS_OBJ(moduleName));

  ObjModule *moduleObj = getModule(vm, moduleName);

  fodiPopRoot(vm); // moduleName.

  return moduleObj != NULL;
}

void fodiAbortFiber(FodiVM *vm, int slot)
{
  validateApiSlot(vm, slot);
  vm->fiber->error = vm->apiStack[slot];
}

void *fodiGetUserData(FodiVM *vm)
{
  return vm->config.userData;
}

void fodiSetUserData(FodiVM *vm, void *userData)
{
  vm->config.userData = userData;
}
