#include <stdio.h>
#include <string.h>

#include "fodi.h"

int resetStackAfterCallAbortRunTests(FodiVM* vm)
{
  fodiEnsureSlots(vm, 1);
  fodiGetVariable(vm, "./test/api/reset_stack_after_call_abort", "Test", 0);
  FodiHandle* testClass = fodiGetSlotHandle(vm, 0);

  FodiHandle* abortFiber = fodiMakeCallHandle(vm, "abortFiber()");
  FodiHandle* afterAbort = fodiMakeCallHandle(vm, "afterAbort(_,_)");

  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, testClass);
  fodiCall(vm, abortFiber);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, testClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiSetSlotDouble(vm, 2, 2.0);
  fodiCall(vm, afterAbort);

  fodiReleaseHandle(vm, testClass);
  fodiReleaseHandle(vm, abortFiber);
  fodiReleaseHandle(vm, afterAbort);
  return 0;
}
