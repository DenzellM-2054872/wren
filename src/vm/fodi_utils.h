#ifndef fodi_utils_h
#define fodi_utils_h

#include "fodi.h"
#include "fodi_common.h"

// Reusable data structures and other utility functions.

// Forward declare this here to break a cycle between fodi_utils.h and
// fodi_value.h.
typedef struct sObjString ObjString;

typedef uint32_t Instruction;

// We need buffers of a few different types. To avoid lots of casting between
// void* and back, we'll use the preprocessor as a poor man's generics and let
// it generate a few type-specific ones.
#define DECLARE_BUFFER(name, type)                                                      \
  typedef struct                                                                        \
  {                                                                                     \
    type *data;                                                                         \
    int count;                                                                          \
    int capacity;                                                                       \
  } name##Buffer;                                                                       \
  void fodi##name##BufferInit(name##Buffer *buffer);                                    \
  void fodi##name##BufferClear(FodiVM *vm, name##Buffer *buffer);                       \
  void fodi##name##BufferFill(FodiVM *vm, name##Buffer *buffer, type data,              \
                              int count);                                               \
  void fodi##name##BufferWrite(FodiVM *vm, name##Buffer *buffer, type data);            \
  void fodi##name##BufferInsert(FodiVM *vm, name##Buffer *buffer, type data, int index);\
  void fodi##name##BufferRemove(FodiVM *vm, name##Buffer *buffer, int index);

// This should be used once for each type instantiation, somewhere in a .c file.
#define DEFINE_BUFFER(name, type)                                                                      \
  void fodi##name##BufferInit(name##Buffer *buffer)                                                    \
  {                                                                                                    \
    buffer->data = NULL;                                                                               \
    buffer->capacity = 0;                                                                              \
    buffer->count = 0;                                                                                 \
  }                                                                                                    \
                                                                                                       \
  void fodi##name##BufferClear(FodiVM *vm, name##Buffer *buffer)                                       \
  {                                                                                                    \
    fodiReallocate(vm, buffer->data, 0, 0);                                                            \
    fodi##name##BufferInit(buffer);                                                                    \
  }                                                                                                    \
                                                                                                       \
  void fodi##name##BufferFill(FodiVM *vm, name##Buffer *buffer, type data,                             \
                              int count)                                                               \
  {                                                                                                    \
    if (buffer->capacity < buffer->count + count)                                                      \
    {                                                                                                  \
      int capacity = fodiPowerOf2Ceil(buffer->count + count);                                          \
      buffer->data = (type *)fodiReallocate(vm, buffer->data,                                          \
                                            buffer->capacity * sizeof(type), capacity * sizeof(type)); \
      buffer->capacity = capacity;                                                                     \
    }                                                                                                  \
                                                                                                       \
    for (int i = 0; i < count; i++)                                                                    \
    {                                                                                                  \
      buffer->data[buffer->count++] = data;                                                            \
    }                                                                                                  \
  }                                                                                                    \
                                                                                                       \
  void fodi##name##BufferWrite(FodiVM *vm, name##Buffer *buffer, type data)                            \
  {                                                                                                    \
    fodi##name##BufferFill(vm, buffer, data, 1);                                                       \
  }                                                                                                    \
                                                                                                       \
  void fodi##name##BufferInsert(FodiVM *vm, name##Buffer *buffer, type data, int index)                \
  {                                                                                                    \
    if (buffer->capacity < buffer->count + 1)                                                          \
    {                                                                                                  \
      int capacity = fodiPowerOf2Ceil(buffer->count + 1);                                              \
      buffer->data = (type *)fodiReallocate(vm, buffer->data,                                          \
                                            buffer->capacity * sizeof(type), capacity * sizeof(type)); \
      buffer->capacity = capacity;                                                                     \
    }                                                                                                  \
                                                                                                       \
    memmove(buffer->data + index + 1, buffer->data + index, (buffer->count - index) * sizeof(type));   \
    buffer->data[index] = data;                                                                        \
    buffer->count++;                                                                                   \
  }                                                                                                    \
                                                                                                       \
  void fodi##name##BufferRemove(FodiVM *vm, name##Buffer *buffer, int index)                \
  {                                                                                                    \
    memmove(buffer->data + index, buffer->data + index + 1, (buffer->count - index - 1) * sizeof(type));   \
    buffer->count--;                                                                                   \
  }  

DECLARE_BUFFER(Byte, uint8_t);
DECLARE_BUFFER(Inst, Instruction);
DECLARE_BUFFER(Int, int);
DECLARE_BUFFER(String, ObjString *);

// TODO: Change this to use a map.
typedef StringBuffer SymbolTable;

// Initializes the symbol table.
void fodiSymbolTableInit(SymbolTable *symbols);

// Frees all dynamically allocated memory used by the symbol table, but not the
// SymbolTable itself.
void fodiSymbolTableClear(FodiVM *vm, SymbolTable *symbols);

// Adds name to the symbol table. Returns the index of it in the table.
int fodiSymbolTableAdd(FodiVM *vm, SymbolTable *symbols,
                       const char *name, size_t length);

// Adds name to the symbol table. Returns the index of it in the table. Will
// use an existing symbol if already present.
int fodiSymbolTableEnsure(FodiVM *vm, SymbolTable *symbols,
                          const char *name, size_t length);

// Looks up name in the symbol table. Returns its index if found or -1 if not.
int fodiSymbolTableFind(const SymbolTable *symbols,
                        const char *name, size_t length);

void fodiBlackenSymbolTable(FodiVM *vm, SymbolTable *symbolTable);

// Returns the number of bytes needed to encode [value] in UTF-8.
//
// Returns 0 if [value] is too large to encode.
int fodiUtf8EncodeNumBytes(int value);

// Encodes value as a series of bytes in [bytes], which is assumed to be large
// enough to hold the encoded result.
//
// Returns the number of written bytes.
int fodiUtf8Encode(int value, uint8_t *bytes);

// Decodes the UTF-8 sequence starting at [bytes] (which has max [length]),
// returning the code point.
//
// Returns -1 if the bytes are not a valid UTF-8 sequence.
int fodiUtf8Decode(const uint8_t *bytes, uint32_t length);

// Returns the number of bytes in the UTF-8 sequence starting with [byte].
//
// If the character at that index is not the beginning of a UTF-8 sequence,
// returns 0.
int fodiUtf8DecodeNumBytes(uint8_t byte);

// Returns the smallest power of two that is equal to or greater than [n].
int fodiPowerOf2Ceil(int n);

// Validates that [value] is within `[0, count)`. Also allows
// negative indices which map backwards from the end. Returns the valid positive
// index value. If invalid, returns `UINT32_MAX`.
uint32_t fodiValidateIndex(uint32_t count, int64_t value);

#endif
