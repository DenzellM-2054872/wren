#include <stdio.h>
#include <string.h>

#include "foreign_class.h"

static int finalized = 0;

static void apiFinalized(FodiVM* vm)
{
  fodiSetSlotDouble(vm, 0, finalized);
}

static void counterAllocate(FodiVM* vm)
{
  // printf("counter Allocate: %d\n", fodiGetSlotCount(vm));

  double* value = (double*)fodiSetSlotNewForeign(vm, 0, 0, sizeof(double));
  *value = 0;
}

static void counterIncrement(FodiVM* vm)
{
  double* value = (double*)fodiGetSlotForeign(vm, 0);
  double increment = fodiGetSlotDouble(vm, 1);

  *value += increment;
}

static void counterValue(FodiVM* vm)
{
  double value = *(double*)fodiGetSlotForeign(vm, 0);
  fodiSetSlotDouble(vm, 0, value);
}

static void pointAllocate(FodiVM* vm)
{
  // printf("Slot before foreign: %d\n", fodiGetSlotCount(vm));

  double* coordinates = (double*)fodiSetSlotNewForeign(vm, 0, 0, sizeof(double[3]));
  // printf("Slot after foreign: %d\n", fodiGetSlotCount(vm));

  // This gets called by both constructors, so sniff the slot count to see
  // which one was invoked.
  if (fodiGetSlotCount(vm) == 1)
  {
    coordinates[0] = 0.0;
    coordinates[1] = 0.0;
    coordinates[2] = 0.0;
  }
  else
  {
    coordinates[0] = fodiGetSlotDouble(vm, 1);
    coordinates[1] = fodiGetSlotDouble(vm, 2);
    coordinates[2] = fodiGetSlotDouble(vm, 3);
  }
}

static void pointTranslate(FodiVM* vm)
{
  double* coordinates = (double*)fodiGetSlotForeign(vm, 0);
  coordinates[0] += fodiGetSlotDouble(vm, 1);
  coordinates[1] += fodiGetSlotDouble(vm, 2);
  coordinates[2] += fodiGetSlotDouble(vm, 3);
}

static void pointToString(FodiVM* vm)
{
  double* coordinates = (double*)fodiGetSlotForeign(vm, 0);
  char result[100];
  sprintf(result, "(%g, %g, %g)",
      coordinates[0], coordinates[1], coordinates[2]);
  fodiSetSlotString(vm, 0, result);
}

static void resourceAllocate(FodiVM* vm)
{
  int* value = (int*)fodiSetSlotNewForeign(vm, 0, 0, sizeof(int));
  *value = 123;
}

static void resourceFinalize(void* data)
{
  // Make sure we get the right data back.
  int* value = (int*)data;
  if (*value != 123) exit(1);

  finalized++;
}

static void badClassAllocate(FodiVM* vm)
{
  fodiEnsureSlots(vm, 1);
  fodiSetSlotString(vm, 0, "Something went wrong");
  fodiAbortFiber(vm, 0);
}

FodiForeignMethodFn foreignClassBindMethod(const char* signature)
{
  if (strcmp(signature, "static ForeignClass.finalized") == 0) return apiFinalized;
  if (strcmp(signature, "Counter.increment(_)") == 0) return counterIncrement;
  if (strcmp(signature, "Counter.value") == 0) return counterValue;
  if (strcmp(signature, "Point.translate(_,_,_)") == 0) return pointTranslate;
  if (strcmp(signature, "Point.toString") == 0) return pointToString;

  return NULL;
}

void foreignClassBindClass(
    const char* className, FodiForeignClassMethods* methods)
{
  if (strcmp(className, "Counter") == 0)
  {
    methods->allocate = counterAllocate;
    return;
  }

  if (strcmp(className, "Point") == 0)
  {
    methods->allocate = pointAllocate;
    return;
  }

  if (strcmp(className, "Resource") == 0)
  {
    methods->allocate = resourceAllocate;
    methods->finalize = resourceFinalize;
    return;
  }

  if (strcmp(className, "BadClass") == 0)
  {
    methods->allocate = badClassAllocate;
    return;
  }
}
