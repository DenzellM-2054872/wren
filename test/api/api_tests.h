#pragma once
#ifndef FODI_API_TESTS_H
#define FODI_API_TESTS_H

#include <stdio.h>
#include <string.h>

#include "fodi.h"

#include "benchmark.h"
#include "call.h"
#include "call_calls_foreign.h"
#include "call_fodi_call_root.h"
#include "error.h"
#include "get_variable.h"
#include "foreign_class.h"
#include "handle.h"
#include "lists.h"
#include "maps.h"
#include "new_vm.h"
#include "reset_stack_after_call_abort.h"
#include "reset_stack_after_foreign_construct.h"
#include "resolution.h"
#include "slots.h"
#include "user_data.h"

int APITest_Run(FodiVM* vm, const char* inTestName);

FodiForeignMethodFn APITest_bindForeignMethod(
    FodiVM* vm, const char* module, const char* className,
    bool isStatic, const char* signature);

FodiForeignClassMethods APITest_bindForeignClass(
    FodiVM* vm, const char* module, const char* className);


#endif //FODI_API_TESTS_H