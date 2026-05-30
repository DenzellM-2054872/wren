#include <stdio.h>
#include <string.h>

#include "resolution.h"

static void writeFn(FodiVM* vm, const char* text)
{
  printf("%s", text);
}

static void reportError(FodiVM* vm, FodiErrorType type,
                        const char* module, int line, const char* message)
{
  if (type == FODI_ERROR_RUNTIME) printf("%s\n", message);
}

static void loadModuleComplete(FodiVM* vm, const char* module, FodiLoadModuleResult result)
{
  free((void*)result.source);
}

static FodiLoadModuleResult loadModule(FodiVM* vm, const char* module)
{
  printf("loading %s\n", module);

  const char* source;
  if (strcmp(module, "main/baz/bang") == 0)
  {
    source = "import \"foo|bar\"";
  }
  else
  {
    source = "System.print(\"ok\")";
  }
   
  char* string = (char*)malloc(strlen(source) + 1);
  strcpy(string, source);

  FodiLoadModuleResult result = {0};
    result.onComplete = loadModuleComplete;
    result.source = string;
  return result;
}

static void runTestVM(FodiVM* vm, FodiConfiguration* configuration,
                      const char* source)
{
  configuration->writeFn = writeFn;
  configuration->errorFn = reportError;
  configuration->loadModuleFn = loadModule;

  FodiVM* otherVM = fodiNewVM(configuration);

  // We should be able to execute code.
  FodiInterpretResult result = fodiInterpret(otherVM, "main", source);
  if (result != FODI_RESULT_SUCCESS)
  {
    fodiSetSlotString(vm, 0, "error");
  }
  else
  {
    fodiSetSlotString(vm, 0, "success");
  }

  fodiFreeVM(otherVM);
}

static void noResolver(FodiVM* vm)
{
  FodiConfiguration configuration;
  fodiInitConfiguration(&configuration);

  // Should default to no resolution function.
  if (configuration.resolveModuleFn != NULL)
  {
    fodiSetSlotString(vm, 0, "Did not have null resolve function.");
    return;
  }

  runTestVM(vm, &configuration, "import \"foo/bar\"");
}

static const char* resolveToNull(FodiVM* vm, const char* importer,
                                 const char* name)
{
  return NULL;
}

static void returnsNull(FodiVM* vm)
{
  FodiConfiguration configuration;
  fodiInitConfiguration(&configuration);

  configuration.resolveModuleFn = resolveToNull;
  runTestVM(vm, &configuration, "import \"foo/bar\"");
}

static const char* resolveChange(FodiVM* vm, const char* importer,
                                 const char* name)
{
  // Concatenate importer and name.
  size_t length = strlen(importer) + 1 + strlen(name) + 1;
  char* result = (char*)malloc(length);
  strcpy(result, importer);
  strcat(result, "/");
  strcat(result, name);

  // Replace "|" with "/".
  for (size_t i = 0; i < length; i++)
  {
    if (result[i] == '|') result[i] = '/';
  }

  return result;
}

static void changesString(FodiVM* vm)
{
  FodiConfiguration configuration;
  fodiInitConfiguration(&configuration);

  configuration.resolveModuleFn = resolveChange;
  runTestVM(vm, &configuration, "import \"foo|bar\"");
}

static void shared(FodiVM* vm)
{
  FodiConfiguration configuration;
  fodiInitConfiguration(&configuration);

  configuration.resolveModuleFn = resolveChange;
  runTestVM(vm, &configuration, "import \"foo|bar\"\nimport \"foo/bar\"");
}

static void importer(FodiVM* vm)
{
  FodiConfiguration configuration;
  fodiInitConfiguration(&configuration);

  configuration.resolveModuleFn = resolveChange;
  runTestVM(vm, &configuration, "import \"baz|bang\"");
}

FodiForeignMethodFn resolutionBindMethod(const char* signature)
{
  if (strcmp(signature, "static Resolution.noResolver()") == 0) return noResolver;
  if (strcmp(signature, "static Resolution.returnsNull()") == 0) return returnsNull;
  if (strcmp(signature, "static Resolution.changesString()") == 0) return changesString;
  if (strcmp(signature, "static Resolution.shared()") == 0) return shared;
  if (strcmp(signature, "static Resolution.importer()") == 0) return importer;

  return NULL;
}

void resolutionBindClass(const char* className, FodiForeignClassMethods* methods)
{
//  methods->allocate = foreignClassAllocate;
}
