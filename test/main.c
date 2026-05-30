#include "./test.h"
#include "./api/api_tests.h"

#include <stdio.h>
#include <string.h>

static FodiVM* vm = NULL;

//This is a simple test runner that serves one purpose:
//To run the language level tests and benchmarks for Fodi.
//It is not a general purpose vm or REPL.
//See fodi-cli if you're looking for that.

static FodiVM* initVM(bool isAPITest)
{
  FodiConfiguration config;
  fodiInitConfiguration(&config);

  config.resolveModuleFn = resolveModule;
  config.loadModuleFn = readModule;
  config.writeFn = vm_write;
  config.errorFn = reportError;

  if(isAPITest) {
    config.bindForeignClassFn = APITest_bindForeignClass;
    config.bindForeignMethodFn = APITest_bindForeignMethod;
  }

  // Since we're running in a standalone process, be generous with memory.
  config.initialHeapSize = 1024 * 1024 * 100;
  return fodiNewVM(&config);
}

int main(int argc, const char* argv[]) {

  int handled = handle_args(argc, argv);
  if(handled != 0) return handled;

  int exitCode = 0;
  const char* testName = argv[1];
  bool isAPITest = isModuleAnAPITest(testName);

  vm = initVM(isAPITest);
  FodiInterpretResult result = runFile(vm, testName);

  if(isAPITest) {
    exitCode = APITest_Run(vm, testName);
  }

  if (result == FODI_RESULT_COMPILE_ERROR) return FODI_EX_DATAERR;
  if (result == FODI_RESULT_RUNTIME_ERROR) return FODI_EX_SOFTWARE;

  fodiFreeVM(vm);

  return exitCode;

}

