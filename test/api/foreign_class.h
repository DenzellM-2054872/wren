#include "fodi.h"

FodiForeignMethodFn foreignClassBindMethod(const char* signature);
void foreignClassBindClass(
    const char* className, FodiForeignClassMethods* methods);
