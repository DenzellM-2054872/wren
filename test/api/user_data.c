#include <string.h>

#include "user_data.h"

static const char* data = "my user data";
static const char* otherData = "other user data";

void* testReallocateFn(void* ptr, size_t newSize, void* userData) {
  if (strcmp(userData, data) != 0) return NULL;

  if (newSize == 0)
  {
    free(ptr);
    return NULL;
  }

  return realloc(ptr, newSize);
}

static void test(FodiVM* vm)
{
  FodiConfiguration configuration;
  fodiInitConfiguration(&configuration);

  // Should default to NULL.
  if (configuration.userData != NULL)
  {
    fodiSetSlotBool(vm, 0, false);
    return;
  }

  configuration.reallocateFn = testReallocateFn;
  configuration.userData = (void*)data;

  FodiVM* otherVM = fodiNewVM(&configuration);

  // Should be able to get it.
  if (fodiGetUserData(otherVM) != data)
  {
    fodiSetSlotBool(vm, 0, false);
    fodiFreeVM(otherVM);
    return;
  }

  // Should be able to set it.
  fodiSetUserData(otherVM, (void*)otherData);

  if (fodiGetUserData(otherVM) != otherData)
  {
    fodiSetSlotBool(vm, 0, false);
    fodiFreeVM(otherVM);
    return;
  }

  fodiSetSlotBool(vm, 0, true);
  fodiFreeVM(otherVM);
}

FodiForeignMethodFn userDataBindMethod(const char* signature)
{
  if (strcmp(signature, "static UserData.test") == 0) return test;

  return NULL;
}
