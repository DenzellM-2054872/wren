#include <stdio.h>
#include <string.h>

#include "call.h"

int callRunTests(FodiVM* vm)
{
  fodiEnsureSlots(vm, 1);
  fodiGetVariable(vm, "./test/api/call", "Call", 0);
  FodiHandle* callClass = fodiGetSlotHandle(vm, 0);

  FodiHandle* noParams = fodiMakeCallHandle(vm, "noParams");
  FodiHandle* zero = fodiMakeCallHandle(vm, "zero()");
  FodiHandle* one = fodiMakeCallHandle(vm, "one(_)");
  FodiHandle* two = fodiMakeCallHandle(vm, "two(_,_)");
  FodiHandle* unary = fodiMakeCallHandle(vm, "-");
  FodiHandle* binary = fodiMakeCallHandle(vm, "-(_)");
  FodiHandle* subscript = fodiMakeCallHandle(vm, "[_,_]");
  FodiHandle* subscriptSet = fodiMakeCallHandle(vm, "[_,_]=(_)");

  // Different arity.
  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiCall(vm, noParams);

  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiCall(vm, zero);

  fodiEnsureSlots(vm, 2);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiCall(vm, one);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiSetSlotDouble(vm, 2, 2.0);
  fodiCall(vm, two);

  // Operators.
  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiCall(vm, unary);

  fodiEnsureSlots(vm, 2);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiCall(vm, binary);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiSetSlotDouble(vm, 2, 2.0);
  fodiCall(vm, subscript);

  fodiEnsureSlots(vm, 4);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotDouble(vm, 1, 1.0);
  fodiSetSlotDouble(vm, 2, 2.0);
  fodiSetSlotDouble(vm, 3, 3.0);
  fodiCall(vm, subscriptSet);

  // Returning a value.
  FodiHandle* getValue = fodiMakeCallHandle(vm, "getValue()");
  fodiEnsureSlots(vm, 1);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiCall(vm, getValue);
  printf("slots after call: %d\n", fodiGetSlotCount(vm));
  FodiHandle* value = fodiGetSlotHandle(vm, 0);

  // Different argument types.
  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotBool(vm, 1, true);
  fodiSetSlotBool(vm, 2, false);
  fodiCall(vm, two);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotDouble(vm, 1, 1.2);
  fodiSetSlotDouble(vm, 2, 3.4);
  fodiCall(vm, two);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotString(vm, 1, "string");
  fodiSetSlotString(vm, 2, "another");
  fodiCall(vm, two);

  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotNull(vm, 1);
  fodiSetSlotHandle(vm, 2, value);
  fodiCall(vm, two);

  // Truncate a string, or allow null bytes.
  fodiEnsureSlots(vm, 3);
  fodiSetSlotHandle(vm, 0, callClass);
  fodiSetSlotBytes(vm, 1, "string", 3);
  fodiSetSlotBytes(vm, 2, "b\0y\0t\0e", 7);
  fodiCall(vm, two);

  // Call ignores with extra temporary slots on stack.
  fodiEnsureSlots(vm, 10);
  fodiSetSlotHandle(vm, 0, callClass);
  for (int i = 1; i < 10; i++)
  {
    fodiSetSlotDouble(vm, i, i * 0.1);
  }
  fodiCall(vm, one);

  fodiReleaseHandle(vm, callClass);
  fodiReleaseHandle(vm, noParams);
  fodiReleaseHandle(vm, zero);
  fodiReleaseHandle(vm, one);
  fodiReleaseHandle(vm, two);
  fodiReleaseHandle(vm, getValue);
  fodiReleaseHandle(vm, value);
  fodiReleaseHandle(vm, unary);
  fodiReleaseHandle(vm, binary);
  fodiReleaseHandle(vm, subscript);
  fodiReleaseHandle(vm, subscriptSet);

  return 0;
}
