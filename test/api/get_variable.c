#include <string.h>

#include "get_variable.h"

static void beforeDefined(FodiVM* vm)
{
  fodiGetVariable(vm, "./test/api/get_variable", "A", 0);
}

static void afterDefined(FodiVM* vm)
{
  fodiGetVariable(vm, "./test/api/get_variable", "A", 0);
}

static void afterAssigned(FodiVM* vm)
{
  fodiGetVariable(vm, "./test/api/get_variable", "A", 0);
}

static void otherSlot(FodiVM* vm)
{
  fodiEnsureSlots(vm, 3);
  fodiGetVariable(vm, "./test/api/get_variable", "B", 2);

  // Move it into return position.
  const char* string = fodiGetSlotString(vm, 2);
  fodiSetSlotString(vm, 0, string);
}

static void otherModule(FodiVM* vm)
{
  fodiGetVariable(vm, "./test/api/get_variable_module", "Variable", 0);
}

static void hasVariable(FodiVM* vm)
{
  const char* module = fodiGetSlotString(vm, 1);
  const char* variable = fodiGetSlotString(vm, 2);

  bool result = fodiHasVariable(vm, module, variable);
  fodiEnsureSlots(vm, 1);
  fodiSetSlotBool(vm, 0, result);
}

static void hasModule(FodiVM* vm)
{
  const char* module = fodiGetSlotString(vm, 1);

  bool result = fodiHasModule(vm, module);
  fodiEnsureSlots(vm, 1);
  fodiSetSlotBool(vm, 0, result);
}

FodiForeignMethodFn getVariableBindMethod(const char* signature)
{
  if (strcmp(signature, "static GetVariable.beforeDefined()") == 0) return beforeDefined;
  if (strcmp(signature, "static GetVariable.afterDefined()") == 0) return afterDefined;
  if (strcmp(signature, "static GetVariable.afterAssigned()") == 0) return afterAssigned;
  if (strcmp(signature, "static GetVariable.otherSlot()") == 0) return otherSlot;
  if (strcmp(signature, "static GetVariable.otherModule()") == 0) return otherModule;
  
  if (strcmp(signature, "static Has.variable(_,_)") == 0) return hasVariable;
  if (strcmp(signature, "static Has.module(_)") == 0) return hasModule;

  return NULL;
}
