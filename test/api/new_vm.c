#include <string.h>

#include "new_vm.h"

static void nullConfig(FodiVM* vm)
{
  FodiVM* otherVM = fodiNewVM(NULL);

  // We should be able to execute code.
  FodiInterpretResult result = fodiInterpret(otherVM, "main", "1 + 2");
  fodiSetSlotBool(vm, 0, result == FODI_RESULT_SUCCESS);

  fodiFreeVM(otherVM);
}

static void multipleInterpretCalls(FodiVM* vm)
{
  FodiVM* otherVM = fodiNewVM(NULL);
  FodiInterpretResult result;

  bool correct = true;

  // Handles should be valid across calls into Fodi code.
  FodiHandle* absMethod = fodiMakeCallHandle(otherVM, "abs");

  result = fodiInterpret(otherVM, "main", "import \"random\" for Random");
  correct = correct && (result == FODI_RESULT_SUCCESS);

  for (int i = 0; i < 5; i++) {
    // Calling `fodiEnsureSlots()` before `fodiInterpret()` should not introduce
    // problems later.
    fodiEnsureSlots(otherVM, 2);

    // Calling a foreign function should succeed.
    result = fodiInterpret(otherVM, "main", "Random.new(12345)");
    correct = correct && (result == FODI_RESULT_SUCCESS);

    fodiEnsureSlots(otherVM, 2);
    fodiSetSlotDouble(otherVM, 0, -i);
    result = fodiCall(otherVM, absMethod);
    correct = correct && (result == FODI_RESULT_SUCCESS);

    double absValue = fodiGetSlotDouble(otherVM, 0);
    correct = correct && (absValue == (double)i);
  }

  fodiSetSlotBool(vm, 0, correct);

  fodiReleaseHandle(otherVM, absMethod);
  fodiFreeVM(otherVM);
}

FodiForeignMethodFn newVMBindMethod(const char* signature)
{
  if (strcmp(signature, "static VM.nullConfig()") == 0) return nullConfig;
  if (strcmp(signature, "static VM.multipleInterpretCalls()") == 0) return multipleInterpretCalls;

  return NULL;
}
