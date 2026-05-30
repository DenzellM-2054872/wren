#ifndef fodi_opt_random_h
#define fodi_opt_random_h

#include "fodi_common.h"
#include "fodi.h"

#if FODI_OPT_RANDOM

const char* fodiRandomSource();
FodiForeignClassMethods fodiRandomBindForeignClass(FodiVM* vm,
                                                   const char* module,
                                                   const char* className);
FodiForeignMethodFn fodiRandomBindForeignMethod(FodiVM* vm,
                                                const char* className,
                                                bool isStatic,
                                                const char* signature);

#endif

#endif
