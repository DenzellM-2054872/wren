#include <string.h>

#include "handle.h"

static FodiHandle* handle;

static void setValue(FodiVM* vm)
{
  handle = fodiGetSlotHandle(vm, 1);
}

static void getValue(FodiVM* vm)
{
  fodiSetSlotHandle(vm, 0, handle);
  fodiReleaseHandle(vm, handle);
}

FodiForeignMethodFn handleBindMethod(const char* signature)
{
  if (strcmp(signature, "static Handle.value=(_)") == 0) return setValue;
  if (strcmp(signature, "static Handle.value") == 0) return getValue;

  return NULL;
}
