#include "fodi.h"

FodiForeignMethodFn mapsBindMethod(const char* signature);
void mapBindClass(
    const char* className, FodiForeignClassMethods* methods);
