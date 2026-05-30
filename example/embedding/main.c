//For more details, visit https://fodi.io/embedding/

#include <stdio.h>
#include "fodi.h"

static void writeFn(FodiVM* vm, const char* text)
{
  printf("%s", text);
}

void errorFn(FodiVM* vm, FodiErrorType errorType,
             const char* module, const int line,
             const char* msg)
{
  switch (errorType)
  {
    case FODI_ERROR_COMPILE:
    {
      printf("[%s line %d] [Error] %s\n", module, line, msg);
    } break;
    case FODI_ERROR_STACK_TRACE:
    {
      printf("[%s line %d] in %s\n", module, line, msg);
    } break;
    case FODI_ERROR_RUNTIME:
    {
      printf("[Runtime Error] %s\n", msg);
    } break;
  }
}

int main()
{

  FodiConfiguration config;
  fodiInitConfiguration(&config);
    config.writeFn = &writeFn;
    config.errorFn = &errorFn;
  FodiVM* vm = fodiNewVM(&config);

  const char* module = "main";
  const char* script = "System.print(\"I am running in a VM!\")";

  FodiInterpretResult result = fodiInterpret(vm, module, script);

  switch (result)
  {
    case FODI_RESULT_COMPILE_ERROR:
      { printf("Compile Error!\n"); } break;
    case FODI_RESULT_RUNTIME_ERROR:
      { printf("Runtime Error!\n"); } break;
    case FODI_RESULT_SUCCESS:
      { printf("Success!\n"); } break;
  }

  fodiFreeVM(vm);

}