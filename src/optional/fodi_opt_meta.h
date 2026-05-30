#ifndef fodi_opt_meta_h
#define fodi_opt_meta_h

#include "fodi_common.h"
#include "fodi.h"

// This module defines the Meta class and its associated methods.
#if FODI_OPT_META

const char* fodiMetaSource();
FodiForeignMethodFn fodiMetaBindForeignMethod(FodiVM* vm,
                                              const char* className,
                                              bool isStatic,
                                              const char* signature);

#endif

#endif
