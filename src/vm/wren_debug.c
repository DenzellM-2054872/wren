#include <stdio.h>

#include "wren_debug.h"
#include "wren_instructions.h"

void wrenDebugRegisterPrintStackTrace(WrenVM *vm)
{
  // Bail if the host doesn't enable printing errors.
  if (vm->config.errorFn == NULL)
    return;

  ObjFiber *fiber = vm->fiber;
  if (IS_STRING(fiber->error))
  {
    vm->config.errorFn(vm, WREN_ERROR_RUNTIME,
                       NULL, -1, AS_CSTRING(fiber->error));
  }
  else
  {
    // TODO: Print something a little useful here. Maybe the name of the error's
    // class?
    vm->config.errorFn(vm, WREN_ERROR_RUNTIME,
                       NULL, -1, "[error object]");
  }

  for (int i = fiber->numFrames - 1; i >= 0; i--)
  {
    CallFrame *frame = &fiber->frames[i];
    ObjFn *fn = frame->closure->fn;

    // Skip over stub functions for calling methods from the C API.
    if (fn->module == NULL)
      continue;

    // The built-in core module has no name. We explicitly omit it from stack
    // traces since we don't want to highlight to a user the implementation
    // detail of what part of the core module is written in C and what is Wren.
    if (fn->module->name == NULL)
      continue;

    // -1 because IP has advanced past the instruction that it just executed.
    int line = fn->debug->regSourceLines.data[frame->rip - fn->regCode.data - 1];
    vm->config.errorFn(vm, WREN_ERROR_STACK_TRACE,
                       fn->module->name->value, line,
                       fn->debug->name);
  }
}

static void dumpObject(Obj *obj)
{
  switch (obj->type)
  {
  case OBJ_CLASS:
    printf("[class %s %p]", ((ObjClass *)obj)->name->value, obj);
    break;
  case OBJ_CLOSURE:
    printf("[closure %p]", obj);
    break;
  case OBJ_FIBER:
    printf("[fiber %p]", obj);
    break;
  case OBJ_FN:
    printf("[fn %p]", obj);
    break;
  case OBJ_FOREIGN:
    printf("[foreign %p]", obj);
    break;
  case OBJ_INSTANCE:
    printf("[instance %p]", obj);
    break;
  case OBJ_LIST:
    printf("[list %p]", obj);
    break;
  case OBJ_MAP:
    printf("[map %p]", obj);
    break;
  case OBJ_MODULE:
    printf("[module %p]", obj);
    break;
  case OBJ_RANGE:
    printf("[range %p]", obj);
    break;
  case OBJ_STRING:
    printf("%s", ((ObjString *)obj)->value);
    break;
  case OBJ_UPVALUE:
    printf("[upvalue %p]", obj);
    break;
  default:
    printf("[unknown object %d]", obj->type);
    break;
  }
}

void wrenDumpValue(Value value)
{
#if WREN_NAN_TAGGING
  if (IS_NUM(value))
  {
    printf("%.14g", AS_NUM(value));
  }
  else if (IS_OBJ(value))
  {
    dumpObject(AS_OBJ(value));
  }
  else
  {
    switch (GET_TAG(value))
    {
    case TAG_FALSE:
      printf("false");
      break;
    case TAG_NAN:
      printf("NaN");
      break;
    case TAG_NULL:
      printf("null");
      break;
    case TAG_TRUE:
      printf("true");
      break;
    case TAG_UNDEFINED:
      UNREACHABLE();
    }
  }
#else
  switch (value.type)
  {
  case VAL_FALSE:
    printf("false");
    break;
  case VAL_NULL:
    printf("null");
    break;
  case VAL_NUM:
    printf("%.14g", AS_NUM(value));
    break;
  case VAL_TRUE:
    printf("true");
    break;
  case VAL_OBJ:
    dumpObject(AS_OBJ(value));
    break;
  case VAL_UNDEFINED:
    UNREACHABLE();
  }
#endif
}

void wrenDumpRegStack(ObjFiber *fiber, Value *start, int stackTop)
{
  printf("(fiber %p) ", fiber);
  for (Value *slot = fiber->stack; slot <=fiber->stack + stackTop; slot++)
  {
    int offset = slot - start;
    if (offset >= 0)
      printf("%d: ", offset);
    wrenDumpValue(*slot);

    printf(" | ");
  }

  printf("\n");
}

void wrenDumpConstants(ObjFn* func)
{
  if(func->constants.count == 0){
    printf("constants : <none>\n");
    return;
  }
  
  printf("constants :");
  for (int i = 0; i < func->constants.count; i++){
    printf("[%d] ", i);
    wrenDumpValue(func->constants.data[i]);
    printf(" | ");

  }
  printf("\n");
}


static void printABC(char *name, int a, int b, int c)
{
  printf("%-16s [%5d, %5d, %5d]", name, a, b, c);
}

static void printABx(char *name, int a, int bx)
{
  printf("%-16s [%5d, %5d]", name, a, bx);
}

static void printsJx(char *name, int sjx)
{
  printf("%-16s [%5d]", name, sjx);
}

static void printABGap()
{
  printf("          ");
}

static void printABCGap()
{
  printf("   ");
}

static void printsJxGap()
{
  printf("                 ");
}

static int dumpRegisterInstruction(WrenVM *vm, ObjFn *fn, int i, int *lastLine)
{
  int start = i;

  if (start >= fn->regCode.count)
  {
    return -1;
  }

  Instruction *bytecode = fn->regCode.data;
  RegCode code = (RegCode)bytecode[i];

  int line = fn->debug->regSourceLines.data[i];
  if (lastLine == NULL || *lastLine != line)
  {
    printf("%4d:", line);
    if (lastLine != NULL)
      *lastLine = line;
  }
  else
  {
    printf("     ");
  }

  printf(" %04d  ", i++);

  switch (GET_OPCODE(code))
  {
  case OP_LOADBOOL:
    printABC("LOADBOOL", GET_A(code), GET_B(code), GET_C(code));
    printABCGap();
    printf("[ ");
    printf(GET_B(code) ? "TRUE" : "FALSE");
    printf(" ]");
    break;

  case OP_LOADNULL:
    printABC("LOADNULL", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_NOOP:
    printABC("NOOP", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_LOADK:
    printABx("LOADK", GET_A(code), GET_Bx(code));
    printABGap();
    printf("[ ");
    if (fn->constants.count <= GET_Bx(code))
      printf("INDEX OUT OF BOUNDS");
    else
      wrenDumpValue(fn->constants.data[GET_Bx(code)]);
    printf(" ]");
    break;

  case OP_MOVE:
    printABC("MOVE", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_SETFIELD:
    printABC("SETFIELD", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_GETFIELD:
    printABC("GETFIELD", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_SETUPVAL:
    printABx("SETUPVAL", GET_A(code), GET_Bx(code));
    break;

  case OP_GETUPVAL:
    printABx("GETUPVAL", GET_A(code), GET_Bx(code));
    break;

  case OP_SETGLOBAL:
    printABx("SETGLOBAL", GET_A(code), GET_Bx(code));
    printABGap();
    printf("'%s'", fn->module->variableNames.data[GET_Bx(code)]->value);
    break;

  case OP_GETGLOBAL:
    printABx("GETGLOBAL", GET_A(code), GET_Bx(code));
    printABGap();
    printf("'%s'", fn->module->variableNames.data[GET_Bx(code)]->value);
    break;

  case OP_TEST:
    printABC("TEST", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_JUMP:
    printsJx("JUMP", GET_sJx(code));
    printsJxGap();
    printf("to %d", i + GET_sJx(code));
    break;

  case OP_CLOSE:
    printABC("CLOSE", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_CALLK:
    printABC("CALLK", GET_A(code), GET_vB(code), GET_vC(code));
    printABCGap();
    printf("'%s'", vm->methodNames.data[GET_vC(code)]->value);
    break;

  case OP_CALLSUPERK:
    printABC("CALLSUPERK", GET_A(code), GET_vB(code), GET_vC(code));
    printABCGap();
    printf("'%s'", vm->methodNames.data[GET_vC(code)]->value);
    break;

  case OP_CLOSURE:
    printABx("CLOSURE", GET_A(code), GET_Bx(code));
    printABGap();
    wrenDumpValue(fn->constants.data[GET_Bx(code)]);
    ObjClosure *loadedClosure = AS_CLOSURE(fn->constants.data[GET_Bx(code)]);
    printf(" '%s'", loadedClosure->fn->debug->name);
    for (int j = 0; j < loadedClosure->fn->numUpvalues; j++)
    {
      if (j > 0)
        printf(",");
      printf(" %d: %s", loadedClosure->protoUpvalues[j]->index, loadedClosure->protoUpvalues[j]->isLocal ? "local" : "upvalue");
    }
    break;

  case OP_CONSTRUCT:
    printABx("CONSTRUCT", GET_A(code), GET_Bx(code));
    break;

  case OP_METHOD:
    printABC("METHOD", GET_A(code), GET_s(code), abs(GET_sBx(code)));
    printABCGap();
    printf("%s: '%s'", GET_s(code) == 0 ? "i" : "s", vm->methodNames.data[abs(GET_sBx(code))]->value);
    break;

  case OP_CLASS:
    printABC("CLASS", GET_A(code), GET_s(code), abs(GET_sBx(code)));
    break;

  case OP_ENDCLASS:
    printABC("ENDCLASS", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_RETURN:
    printABC("RETURN", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_IMPORTMODULE:
    printABx("IMPORTMODULE", GET_A(code), GET_Bx(code));
    printABGap();
    printf("'");
    wrenDumpValue(fn->constants.data[GET_Bx(code)]);
    printf("'");
    break;

  case OP_IMPORTVAR:
    printABx("IMPORTVAR", GET_A(code), GET_Bx(code));
    printABGap();
    printf("'");
    wrenDumpValue(fn->constants.data[GET_Bx(code)]);
    printf("'");
    break;

  case OP_EQ:
    printABC("EQ", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_LT:
    printABC("LT", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_LTE:
    printABC("LTE", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_EQK:
    printABC("EQK", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_LTK:
    printABC("LTK", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_LTEK:
    printABC("LTEK", GET_A(code), GET_B(code), GET_C(code));
    break;
    
  case OP_NEG:
    printABC("NEG", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_NOT:
    printABC("NOT", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_ADD:
    printABC("ADD", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_SUB:
    printABC("SUB", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_MUL:
    printABC("MUL", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_DIV:
    printABC("DIV", GET_A(code), GET_B(code), GET_C(code));
    break;

  case OP_ADDK:
    printABC("ADDK", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_SUBK:
    printABC("SUBK", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_MULK:
    printABC("MULK", GET_A(code), GET_B(code), GET_C(code));
    break;
  case OP_DIVK:
    printABC("DIVK", GET_A(code), GET_B(code), GET_C(code));
    break;

  default:
    printf("UNKNOWN! [%d]", bytecode[i - 1]);
    break;
  }

  printf("\n");

  // Return how many bytes this instruction takes, or -1 if it's an END.
  // if (code == CODE_END) return -1;
  return i - start;
}

int wrenDumpRegisterInstruction(WrenVM *vm, ObjFn *fn, int i)
{
  return dumpRegisterInstruction(vm, fn, i, NULL);
}

void wrenDumpRegisterCode(WrenVM *vm, ObjFn *fn, int constantNr)
{
  printf("%s: %s[%d]\n",
         fn->module->name == NULL ? "<core>" : fn->module->name->value,
         fn->debug->name,
         constantNr);

  int i = 0;
  int lastLine = -1;
  for (;;)
  {
    int offset = dumpRegisterInstruction(vm, fn, i, &lastLine);
    if (offset == -1)
      break;
    i += offset;
  }

  printf("\n");
}
