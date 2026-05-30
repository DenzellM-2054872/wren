#include <string.h>

#include "lists.h"

static void newList(FodiVM* vm)
{
  fodiSetSlotNewList(vm, 0);
}

// Helper function to store a double in a slot then insert it into the list at
// slot zero.
static void insertNumber(FodiVM* vm, int index, double value)
{
  fodiSetSlotDouble(vm, 1, value);
  fodiInsertInList(vm, 0, index, 1);
}

// Helper function to append a double in a slot then insert it into the list at
// slot zero.
static void appendNumber(FodiVM* vm, double value)
{
  fodiSetSlotDouble(vm, 1, value);
  fodiInsertInList(vm, 0, -1, 1);
}

static void insert(FodiVM* vm)
{
  fodiSetSlotNewList(vm, 0);

  fodiEnsureSlots(vm, 2);

  // Appending.
  insertNumber(vm, 0, 1.0);
  insertNumber(vm, 1, 2.0);
  insertNumber(vm, 2, 3.0);

  // Inserting.
  insertNumber(vm, 0, 4.0);
  insertNumber(vm, 1, 5.0);
  insertNumber(vm, 2, 6.0);

  // Negative indexes.
  insertNumber(vm, -1, 7.0);
  insertNumber(vm, -2, 8.0);
  insertNumber(vm, -3, 9.0);
}

static void get(FodiVM* vm)
{
  int listSlot = 1;
  int index = (int)fodiGetSlotDouble(vm, 2);

  fodiGetListElement(vm, listSlot, index, 0);
}

static void set(FodiVM* vm)
{
  fodiSetSlotNewList(vm, 0);

  fodiEnsureSlots(vm, 2);

  appendNumber(vm, 1.0);
  appendNumber(vm, 2.0);
  appendNumber(vm, 3.0);
  appendNumber(vm, 4.0);
  
  //list[2] = 33
  fodiSetSlotDouble(vm, 1, 33);
  fodiSetListElement(vm, 0, 2, 1);

  //list[-1] = 44
  fodiSetSlotDouble(vm, 1, 44);
  fodiSetListElement(vm, 0, -1, 1);
}

FodiForeignMethodFn listsBindMethod(const char* signature)
{
  if (strcmp(signature, "static Lists.newList()") == 0) return newList;
  if (strcmp(signature, "static Lists.insert()") == 0) return insert;
  if (strcmp(signature, "static Lists.set()") == 0) return set;
  if (strcmp(signature, "static Lists.get(_,_)") == 0) return get;

  return NULL;
}
