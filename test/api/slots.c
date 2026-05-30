#include <stdio.h>
#include <string.h>

#include "slots.h"

static void noSet(FodiVM* vm)
{
  // Do nothing.
}

static void getSlots(FodiVM* vm)
{
  bool result = true;
  if (fodiGetSlotBool(vm, 1) != true) result = false;

  int length;
  const char* bytes = fodiGetSlotBytes(vm, 2, &length);
  if (length != 5) result = false;
  if (memcmp(bytes, "by\0te", length) != 0) result = false;

  if (fodiGetSlotDouble(vm, 3) != 1.5) result = false;
  if (strcmp(fodiGetSlotString(vm, 4), "str") != 0) result = false;

  FodiHandle* handle = fodiGetSlotHandle(vm, 5);

  if (result)
  {
    // Otherwise, return the value so we can tell if we captured it correctly.
    fodiSetSlotHandle(vm, 0, handle);
  }
  else
  {
    // If anything failed, return false.
    fodiSetSlotBool(vm, 0, false);
  }

  fodiReleaseHandle(vm, handle);
}

static void setSlots(FodiVM* vm)
{
  FodiHandle* handle = fodiGetSlotHandle(vm, 1);

  fodiSetSlotBool(vm, 1, true);
  fodiSetSlotBytes(vm, 2, "by\0te", 5);
  fodiSetSlotDouble(vm, 3, 1.5);
  fodiSetSlotString(vm, 4, "str");
  fodiSetSlotNull(vm, 5);

  // Read the slots back to make sure they were set correctly.

  bool result = true;
  if (fodiGetSlotBool(vm, 1) != true) result = false;

  int length;
  const char* bytes = fodiGetSlotBytes(vm, 2, &length);
  if (length != 5) result = false;
  if (memcmp(bytes, "by\0te", length) != 0) result = false;

  if (fodiGetSlotDouble(vm, 3) != 1.5) result = false;
  if (strcmp(fodiGetSlotString(vm, 4), "str") != 0) result = false;

  if (fodiGetSlotType(vm, 5) != FODI_TYPE_NULL) result = false;

  if (result)
  {
    // Move the value into the return position.
    fodiSetSlotHandle(vm, 0, handle);
  }
  else
  {
    // If anything failed, return false.
    fodiSetSlotBool(vm, 0, false);
  }

  fodiReleaseHandle(vm, handle);
}

static void slotTypes(FodiVM* vm)
{
  bool result =
      fodiGetSlotType(vm, 1) == FODI_TYPE_BOOL &&
      fodiGetSlotType(vm, 2) == FODI_TYPE_FOREIGN &&
      fodiGetSlotType(vm, 3) == FODI_TYPE_LIST &&
      fodiGetSlotType(vm, 4) == FODI_TYPE_MAP &&
      fodiGetSlotType(vm, 5) == FODI_TYPE_NULL &&
      fodiGetSlotType(vm, 6) == FODI_TYPE_NUM &&
      fodiGetSlotType(vm, 7) == FODI_TYPE_STRING &&
      fodiGetSlotType(vm, 8) == FODI_TYPE_UNKNOWN;

  fodiSetSlotBool(vm, 0, result);
}

static void ensure(FodiVM* vm)
{
  int before = fodiGetSlotCount(vm);

  fodiEnsureSlots(vm, 20);

  int after = fodiGetSlotCount(vm);

  // Use the slots to make sure they're available.
  for (int i = 0; i < 20; i++)
  {
    fodiSetSlotDouble(vm, i, i);
  }

  int sum = 0;

  for (int i = 0; i < 20; i++)
  {
    sum += (int)fodiGetSlotDouble(vm, i);
  }

  char result[100];
  sprintf(result, "%d -> %d (%d)", before, after, sum);
  fodiSetSlotString(vm, 0, result);
}

static void ensureOutsideForeign(FodiVM* vm)
{
  // To test the behavior outside of a foreign method (which we're currently
  // in), create a new separate VM.
  FodiConfiguration config;
  fodiInitConfiguration(&config);
  FodiVM* otherVM = fodiNewVM(&config);

  int before = fodiGetSlotCount(otherVM);

  fodiEnsureSlots(otherVM, 20);

  int after = fodiGetSlotCount(otherVM);

  // Use the slots to make sure they're available.
  for (int i = 0; i < 20; i++)
  {
    fodiSetSlotDouble(otherVM, i, i);
  }

  int sum = 0;

  for (int i = 0; i < 20; i++)
  {
    sum += (int)fodiGetSlotDouble(otherVM, i);
  }

  fodiFreeVM(otherVM);

  char result[100];
  sprintf(result, "%d -> %d (%d)", before, after, sum);
  fodiSetSlotString(vm, 0, result);
}

static void foreignClassAllocate(FodiVM* vm)
{
  fodiSetSlotNewForeign(vm, 0, 0, 4);
}

static void getListCount(FodiVM* vm)
{
  fodiSetSlotDouble(vm, 0, fodiGetListCount(vm, 1));
}

static void getListElement(FodiVM* vm)
{
  int index = (int)fodiGetSlotDouble(vm, 2);
  fodiGetListElement(vm, 1, index, 0);
}

static void getMapValue(FodiVM* vm)
{
  fodiGetMapValue(vm, 1, 2, 0);
}

FodiForeignMethodFn slotsBindMethod(const char* signature)
{
  if (strcmp(signature, "static Slots.noSet") == 0) return noSet;
  if (strcmp(signature, "static Slots.getSlots(_,_,_,_,_)") == 0) return getSlots;
  if (strcmp(signature, "static Slots.setSlots(_,_,_,_,_)") == 0) return setSlots;
  if (strcmp(signature, "static Slots.slotTypes(_,_,_,_,_,_,_,_)") == 0) return slotTypes;
  if (strcmp(signature, "static Slots.ensure()") == 0) return ensure;
  if (strcmp(signature, "static Slots.ensureOutsideForeign()") == 0) return ensureOutsideForeign;
  if (strcmp(signature, "static Slots.getListCount(_)") == 0) return getListCount;
  if (strcmp(signature, "static Slots.getListElement(_,_)") == 0) return getListElement;
  if (strcmp(signature, "static Slots.getMapValue(_,_)") == 0) return getMapValue;

  return NULL;
}

void slotsBindClass(const char* className, FodiForeignClassMethods* methods)
{
  methods->allocate = foreignClassAllocate;
}
