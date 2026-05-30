#include <string.h>
#include <time.h>

#include "benchmark.h"

static void arguments(FodiVM* vm)
{
  double result = 0;

  result += fodiGetSlotDouble(vm, 1);
  result += fodiGetSlotDouble(vm, 2);
  result += fodiGetSlotDouble(vm, 3);
  result += fodiGetSlotDouble(vm, 4);

  fodiSetSlotDouble(vm, 0, result);
}

const char* testScript =
"class Test {\n"
"  static method(a, b, c, d) { a + b + c + d }\n"
"}\n";

static void call(FodiVM* vm)
{
  int iterations = (int)fodiGetSlotDouble(vm, 1);

  // Since the VM is not re-entrant, we can't call from within this foreign
  // method. Instead, make a new VM to run the call test in.
  FodiConfiguration config;
  fodiInitConfiguration(&config);
  FodiVM* otherVM = fodiNewVM(&config);

  fodiInterpret(otherVM, "main", testScript);

  FodiHandle* method = fodiMakeCallHandle(otherVM, "method(_,_,_,_)");

  fodiEnsureSlots(otherVM, 1);
  fodiGetVariable(otherVM, "main", "Test", 0);
  FodiHandle* testClass = fodiGetSlotHandle(otherVM, 0);

  double startTime = (double)clock() / CLOCKS_PER_SEC;

  double result = 0;
  for (int i = 0; i < iterations; i++)
  {
    fodiEnsureSlots(otherVM, 5);
    fodiSetSlotHandle(otherVM, 0, testClass);
    fodiSetSlotDouble(otherVM, 1, 1.0);
    fodiSetSlotDouble(otherVM, 2, 2.0);
    fodiSetSlotDouble(otherVM, 3, 3.0);
    fodiSetSlotDouble(otherVM, 4, 4.0);

    fodiCall(otherVM, method);

    result += fodiGetSlotDouble(otherVM, 0);
  }

  double elapsed = (double)clock() / CLOCKS_PER_SEC - startTime;

  fodiReleaseHandle(otherVM, testClass);
  fodiReleaseHandle(otherVM, method);
  fodiFreeVM(otherVM);

  if (result == (1.0 + 2.0 + 3.0 + 4.0) * iterations)
  {
    fodiSetSlotDouble(vm, 0, elapsed);
  }
  else
  {
    // Got the wrong result.
    fodiSetSlotBool(vm, 0, false);
  }
}

FodiForeignMethodFn benchmarkBindMethod(const char* signature)
{
  if (strcmp(signature, "static Benchmark.arguments(_,_,_,_)") == 0) return arguments;
  if (strcmp(signature, "static Benchmark.call(_)") == 0) return call;

  return NULL;
}
