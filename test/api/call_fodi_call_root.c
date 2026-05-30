#include <stdio.h>
#include <string.h>

#include "fodi.h"
#include "../test.h"

int callFodiCallRootRunTests(FodiVM* vm)
{
  int exitCode = 0;
  fodiEnsureSlots(vm, 1);
  fodiGetVariable(vm, "./test/api/call_fodi_call_root", "Test", 0);
  FodiHandle* testClass = fodiGetSlotHandle(vm, 0);

  FodiHandle* run = fodiMakeCallHandle(vm, "run()");

  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, testClass);
  FodiInterpretResult result = fodiCall(vm, run);
  if (result == FODI_RESULT_RUNTIME_ERROR)
  {
    exitCode = FODI_EX_SOFTWARE;
  }
  else
  {
    printf("Missing runtime error.\n");
  }

  fodiReleaseHandle(vm, testClass);
  fodiReleaseHandle(vm, run);
  return exitCode;
}
