#include <stdio.h>
#include <string.h>

#include "fodi.h"

static void counterAllocate(FodiVM* vm)
{
  double* counter = (double*)fodiSetSlotNewForeign(vm, 0, 0, sizeof(double));
  *counter = fodiGetSlotDouble(vm, 1);
}

void resetStackAfterForeignConstructBindClass(
    const char* className, FodiForeignClassMethods* methods)
{
  if (strcmp(className, "ResetStackForeign") == 0)
  {
    methods->allocate = counterAllocate;
    return;
  }
}

int resetStackAfterForeignConstructRunTests(FodiVM* vm)
{
  fodiEnsureSlots(vm, 1);
  fodiGetVariable(vm,
      "./test/api/reset_stack_after_foreign_construct", "Test", 0);
  FodiHandle* testClass = fodiGetSlotHandle(vm, 0);

  FodiHandle* callConstruct = fodiMakeCallHandle(vm, "callConstruct()");
  FodiHandle* afterConstruct = fodiMakeCallHandle(vm, "afterConstruct(_,_)");

  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, testClass);
  fodiCall(vm, callConstruct);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, testClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiSetSlotDouble(vm, 2, 2.0);
  fodiCall(vm, afterConstruct);

  fodiReleaseHandle(vm, testClass);
  fodiReleaseHandle(vm, callConstruct);
  fodiReleaseHandle(vm, afterConstruct);

  return 0;
}
