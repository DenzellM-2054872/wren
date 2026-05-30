#include "../test/test.h"

#include <stdio.h>
#include <string.h>

static FodiVM* vm = NULL;

//This is a simple program that exposes fodi to the browser
//for https://fodi.io/try and runs scripts.

static FodiVM* initVM()
{
  FodiConfiguration config;
  fodiInitConfiguration(&config);

  config.resolveModuleFn = resolveModule;
  config.loadModuleFn = readModule;
  config.writeFn = vm_write;
  config.errorFn = reportError;

  // Might be a more reasonable value, 
  // but since this is simple, keep it simple.
  config.initialHeapSize = 1024 * 1024 * 100;
  return fodiNewVM(&config);
}

//The endpoint we call from the browser
int fodi_compile(const char* input) {
  FodiVM* vm = initVM();
  FodiInterpretResult result = fodiInterpret(vm, "compile", input);
  fodiFreeVM(vm);
  return (int)result;
}

//Main not used, but required. We call fodi_compile directly.
int main(int argc, const char* argv[]) {
  return 0;
}

