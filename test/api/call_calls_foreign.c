#include <stdio.h>
#include <string.h>

#include "fodi.h"

static void api(FodiVM *vm) {
  // Grow the slot array. This should trigger the stack to be moved.
  fodiEnsureSlots(vm, 10);
  fodiSetSlotNewList(vm, 0);

  for (int i = 1; i < 10; i++)
  {
    fodiSetSlotDouble(vm, i, i);
    fodiInsertInList(vm, 0, -1, i);
  }
}

FodiForeignMethodFn callCallsForeignBindMethod(const char* signature)
{
  if (strcmp(signature, "static CallCallsForeign.api()") == 0) return api;

  return NULL;
}

int callCallsForeignRunTests(FodiVM* vm)
{
  fodiEnsureSlots(vm, 1);
  fodiGetVariable(vm, "./test/api/call_calls_foreign", "CallCallsForeign", 0);
  FodiHandle* apiClass = fodiGetSlotHandle(vm, 0);
  FodiHandle *call = fodiMakeCallHandle(vm, "call(_)");

  fodiEnsureSlots(vm, 2);
  fodiSetSlotHandle(vm, 0, apiClass);
  fodiSetSlotString(vm, 1, "parameter");

  printf("slots before %d\n", fodiGetSlotCount(vm));
  fodiCall(vm, call);

  // We should have a single slot count for the return.
  printf("slots after %d\n", fodiGetSlotCount(vm));

  fodiReleaseHandle(vm, call);
  fodiReleaseHandle(vm, apiClass);
  return 0;
}
