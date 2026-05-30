#include <string.h>

#include "maps.h"

static void newMap(FodiVM* vm)
{
  fodiSetSlotNewMap(vm, 0);
}

static void invalidInsert(FodiVM* vm)
{
  fodiSetSlotNewMap(vm, 0);
  
  fodiEnsureSlots(vm, 3);
  // Foreign Class is in slot 1
  fodiSetSlotString(vm, 2, "England");
  fodiSetMapValue(vm, 0, 1, 2); // expect this to cause errors
}

static void insert(FodiVM* vm)
{
  fodiSetSlotNewMap(vm, 0);
  
  fodiEnsureSlots(vm, 3);

  // Insert String
  fodiSetSlotString(vm, 1, "England");
  fodiSetSlotString(vm, 2, "London");
  fodiSetMapValue(vm, 0, 1, 2);

  // Insert Double
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiSetSlotDouble(vm, 2, 42.0);
  fodiSetMapValue(vm, 0, 1, 2);

  // Insert Boolean
  fodiSetSlotBool(vm, 1, false);
  fodiSetSlotBool(vm, 2, true);
  fodiSetMapValue(vm, 0, 1, 2);

  // Insert Null
  fodiSetSlotNull(vm, 1);
  fodiSetSlotNull(vm, 2);
  fodiSetMapValue(vm, 0, 1, 2);

  // Insert List
  fodiSetSlotString(vm, 1, "Empty");
  fodiSetSlotNewList(vm, 2);
  fodiSetMapValue(vm, 0, 1, 2);
}

static void removeKey(FodiVM* vm)
{
  fodiEnsureSlots(vm, 3);

  fodiSetSlotString(vm, 2, "key");
  fodiRemoveMapValue(vm, 1, 2, 0);
}

static void countFodi(FodiVM* vm)
{
  int count = fodiGetMapCount(vm, 1);
  fodiSetSlotDouble(vm, 0, count);
}

static void countAPI(FodiVM* vm)
{
  insert(vm);
  int count = fodiGetMapCount(vm, 0);
  fodiSetSlotDouble(vm, 0, count);
}

static void containsFodi(FodiVM* vm)
{
  bool result = fodiGetMapContainsKey(vm, 1, 2);
  fodiSetSlotBool(vm, 0, result);
}


static void containsAPI(FodiVM* vm)
{
  insert(vm);
  
  fodiEnsureSlots(vm, 1);
  fodiSetSlotString(vm, 1, "England");

  bool result = fodiGetMapContainsKey(vm, 0, 1);
  fodiSetSlotBool(vm, 0, result);
}

static void containsAPIFalse(FodiVM* vm)
{
  insert(vm);

  fodiEnsureSlots(vm, 1);
  fodiSetSlotString(vm, 1, "DefinitelyNotARealKey");

  bool result = fodiGetMapContainsKey(vm, 0, 1);
  fodiSetSlotBool(vm, 0, result);
}


FodiForeignMethodFn mapsBindMethod(const char* signature)
{
  if (strcmp(signature, "static Maps.newMap()") == 0) return newMap;
  if (strcmp(signature, "static Maps.insert()") == 0) return insert;
  if (strcmp(signature, "static Maps.remove(_)") == 0) return removeKey;
  if (strcmp(signature, "static Maps.count(_)") == 0) return countFodi;
  if (strcmp(signature, "static Maps.count()") == 0) return countAPI;
  if (strcmp(signature, "static Maps.contains()") == 0) return containsAPI;
  if (strcmp(signature, "static Maps.containsFalse()") == 0) return containsAPIFalse;
  if (strcmp(signature, "static Maps.contains(_,_)") == 0) return containsFodi;
  if (strcmp(signature, "static Maps.invalidInsert(_)") == 0) return invalidInsert;

  return NULL;
}

void foreignAllocate(FodiVM* vm) {
  fodiSetSlotNewForeign(vm, 0, 0, 0);
}

void mapBindClass(
    const char* className, FodiForeignClassMethods* methods)
{
  if (strcmp(className, "ForeignClass") == 0)
  {
    methods->allocate = foreignAllocate;
    return;
  }
}
