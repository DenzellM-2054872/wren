#include <stdio.h>
#include <string.h>

#include "error.h"

static void runtimeError(FodiVM* vm)
{
  fodiEnsureSlots(vm, 1);
  fodiSetSlotString(vm, 0, "Error!");
  fodiAbortFiber(vm, 0);
}

FodiForeignMethodFn errorBindMethod(const char* signature)
{
  if (strcmp(signature, "static Error.runtimeError") == 0) return runtimeError;

  return NULL;
}
