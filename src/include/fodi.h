#ifndef fodi_h
#define fodi_h

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

// The Fodi semantic version number components.
#define FODI_VERSION_MAJOR 0
#define FODI_VERSION_MINOR 1
#define FODI_VERSION_PATCH 0

// A human-friendly string representation of the version.
#define FODI_VERSION_STRING "0.1.0"

// A monotonically increasing numeric representation of the version number. Use
// this if you want to do range checks over versions.
#define FODI_VERSION_NUMBER (FODI_VERSION_MAJOR * 1000000 +                    \
                             FODI_VERSION_MINOR * 1000 +                       \
                             FODI_VERSION_PATCH)

#ifndef FODI_API
  #if defined(_MSC_VER) && defined(FODI_API_DLLEXPORT)
    #define FODI_API __declspec( dllexport )
  #else
    #define FODI_API
  #endif
#endif //FODI_API

// A single virtual machine for executing Fodi code.
//
// Fodi has no global state, so all state stored by a running interpreter lives
// here.
typedef struct FodiVM FodiVM;

// A handle to a Fodi object.
//
// This lets code outside of the VM hold a persistent reference to an object.
// After a handle is acquired, and until it is released, this ensures the
// garbage collector will not reclaim the object it references.
typedef struct FodiHandle FodiHandle;

// A generic allocation function that handles all explicit memory management
// used by Fodi. It's used like so:
//
// - To allocate new memory, [memory] is NULL and [newSize] is the desired
//   size. It should return the allocated memory or NULL on failure.
//
// - To attempt to grow an existing allocation, [memory] is the memory, and
//   [newSize] is the desired size. It should return [memory] if it was able to
//   grow it in place, or a new pointer if it had to move it.
//
// - To shrink memory, [memory] and [newSize] are the same as above but it will
//   always return [memory].
//
// - To free memory, [memory] will be the memory to free and [newSize] will be
//   zero. It should return NULL.
typedef void* (*FodiReallocateFn)(void* memory, size_t newSize, void* userData);

// A function callable from Fodi code, but implemented in C.
typedef void (*FodiForeignMethodFn)(FodiVM* vm);

// A finalizer function for freeing resources owned by an instance of a foreign
// class. Unlike most foreign methods, finalizers do not have access to the VM
// and should not interact with it since it's in the middle of a garbage
// collection.
typedef void (*FodiFinalizerFn)(void* data);

// Gives the host a chance to canonicalize the imported module name,
// potentially taking into account the (previously resolved) name of the module
// that contains the import. Typically, this is used to implement relative
// imports.
typedef const char* (*FodiResolveModuleFn)(FodiVM* vm,
    const char* importer, const char* name);

// Forward declare
struct FodiLoadModuleResult;

// Called after loadModuleFn is called for module [name]. The original returned result
// is handed back to you in this callback, so that you can free memory if appropriate.
typedef void (*FodiLoadModuleCompleteFn)(FodiVM* vm, const char* name, struct FodiLoadModuleResult result);

// The result of a loadModuleFn call. 
// [source] is the source code for the module, or NULL if the module is not found.
// [onComplete] an optional callback that will be called once Fodi is done with the result.
typedef struct FodiLoadModuleResult
{
  const char* source;
  FodiLoadModuleCompleteFn onComplete;
  void* userData;
} FodiLoadModuleResult;

// Loads and returns the source code for the module [name].
typedef FodiLoadModuleResult (*FodiLoadModuleFn)(FodiVM* vm, const char* name);

// Returns a pointer to a foreign method on [className] in [module] with
// [signature].
typedef FodiForeignMethodFn (*FodiBindForeignMethodFn)(FodiVM* vm,
    const char* module, const char* className, bool isStatic,
    const char* signature);

// Displays a string of text to the user.
typedef void (*FodiWriteFn)(FodiVM* vm, const char* text);

typedef enum
{
  // A syntax or resolution error detected at compile time.
  FODI_ERROR_COMPILE,

  // The error message for a runtime error.
  FODI_ERROR_RUNTIME,

  // One entry of a runtime error's stack trace.
  FODI_ERROR_STACK_TRACE
} FodiErrorType;

// Reports an error to the user.
//
// An error detected during compile time is reported by calling this once with
// [type] `FODI_ERROR_COMPILE`, the resolved name of the [module] and [line]
// where the error occurs, and the compiler's error [message].
//
// A runtime error is reported by calling this once with [type]
// `FODI_ERROR_RUNTIME`, no [module] or [line], and the runtime error's
// [message]. After that, a series of [type] `FODI_ERROR_STACK_TRACE` calls are
// made for each line in the stack trace. Each of those has the resolved
// [module] and [line] where the method or function is defined and [message] is
// the name of the method or function.
typedef void (*FodiErrorFn)(
    FodiVM* vm, FodiErrorType type, const char* module, int line,
    const char* message);

typedef struct
{
  // The callback invoked when the foreign object is created.
  //
  // This must be provided. Inside the body of this, it must call
  // [fodiSetSlotNewForeign()] exactly once.
  FodiForeignMethodFn allocate;

  // The callback invoked when the garbage collector is about to collect a
  // foreign object's memory.
  //
  // This may be `NULL` if the foreign class does not need to finalize.
  FodiFinalizerFn finalize;
} FodiForeignClassMethods;

// Returns a pair of pointers to the foreign methods used to allocate and
// finalize the data for instances of [className] in resolved [module].
typedef FodiForeignClassMethods (*FodiBindForeignClassFn)(
    FodiVM* vm, const char* module, const char* className);

typedef struct
{
  // The callback Fodi will use to allocate, reallocate, and deallocate memory.
  //
  // If `NULL`, defaults to a built-in function that uses `realloc` and `free`.
  FodiReallocateFn reallocateFn;

  // The callback Fodi uses to resolve a module name.
  //
  // Some host applications may wish to support "relative" imports, where the
  // meaning of an import string depends on the module that contains it. To
  // support that without baking any policy into Fodi itself, the VM gives the
  // host a chance to resolve an import string.
  //
  // Before an import is loaded, it calls this, passing in the name of the
  // module that contains the import and the import string. The host app can
  // look at both of those and produce a new "canonical" string that uniquely
  // identifies the module. This string is then used as the name of the module
  // going forward. It is what is passed to [loadModuleFn], how duplicate
  // imports of the same module are detected, and how the module is reported in
  // stack traces.
  //
  // If you leave this function NULL, then the original import string is
  // treated as the resolved string.
  //
  // If an import cannot be resolved by the embedder, it should return NULL and
  // Fodi will report that as a runtime error.
  //
  // Fodi will take ownership of the string you return and free it for you, so
  // it should be allocated using the same allocation function you provide
  // above.
  FodiResolveModuleFn resolveModuleFn;

  // The callback Fodi uses to load a module.
  //
  // Since Fodi does not talk directly to the file system, it relies on the
  // embedder to physically locate and read the source code for a module. The
  // first time an import appears, Fodi will call this and pass in the name of
  // the module being imported. The method will return a result, which contains
  // the source code for that module. Memory for the source is owned by the 
  // host application, and can be freed using the onComplete callback.
  //
  // This will only be called once for any given module name. Fodi caches the
  // result internally so subsequent imports of the same module will use the
  // previous source and not call this.
  //
  // If a module with the given name could not be found by the embedder, it
  // should return NULL and Fodi will report that as a runtime error.
  FodiLoadModuleFn loadModuleFn;

  // The callback Fodi uses to find a foreign method and bind it to a class.
  //
  // When a foreign method is declared in a class, this will be called with the
  // foreign method's module, class, and signature when the class body is
  // executed. It should return a pointer to the foreign function that will be
  // bound to that method.
  //
  // If the foreign function could not be found, this should return NULL and
  // Fodi will report it as runtime error.
  FodiBindForeignMethodFn bindForeignMethodFn;

  // The callback Fodi uses to find a foreign class and get its foreign methods.
  //
  // When a foreign class is declared, this will be called with the class's
  // module and name when the class body is executed. It should return the
  // foreign functions uses to allocate and (optionally) finalize the bytes
  // stored in the foreign object when an instance is created.
  FodiBindForeignClassFn bindForeignClassFn;

  // The callback Fodi uses to display text when `System.print()` or the other
  // related functions are called.
  //
  // If this is `NULL`, Fodi discards any printed text.
  FodiWriteFn writeFn;

  // The callback Fodi uses to report errors.
  //
  // When an error occurs, this will be called with the module name, line
  // number, and an error message. If this is `NULL`, Fodi doesn't report any
  // errors.
  FodiErrorFn errorFn;

  // The number of bytes Fodi will allocate before triggering the first garbage
  // collection.
  //
  // If zero, defaults to 10MB.
  size_t initialHeapSize;

  // After a collection occurs, the threshold for the next collection is
  // determined based on the number of bytes remaining in use. This allows Fodi
  // to shrink its memory usage automatically after reclaiming a large amount
  // of memory.
  //
  // This can be used to ensure that the heap does not get too small, which can
  // in turn lead to a large number of collections afterwards as the heap grows
  // back to a usable size.
  //
  // If zero, defaults to 1MB.
  size_t minHeapSize;

  // Fodi will resize the heap automatically as the number of bytes
  // remaining in use after a collection changes. This number determines the
  // amount of additional memory Fodi will use after a collection, as a
  // percentage of the current heap size.
  //
  // For example, say that this is 50. After a garbage collection, when there
  // are 400 bytes of memory still in use, the next collection will be triggered
  // after a total of 600 bytes are allocated (including the 400 already in
  // use.)
  //
  // Setting this to a smaller number wastes less memory, but triggers more
  // frequent garbage collections.
  //
  // If zero, defaults to 50.
  int heapGrowthPercent;

  // User-defined data associated with the VM.
  void* userData;

} FodiConfiguration;

typedef enum
{
  FODI_RESULT_SUCCESS,
  FODI_RESULT_COMPILE_ERROR,
  FODI_RESULT_RUNTIME_ERROR
} FodiInterpretResult;

// The type of an object stored in a slot.
//
// This is not necessarily the object's *class*, but instead its low level
// representation type.
typedef enum
{
  FODI_TYPE_BOOL,
  FODI_TYPE_NUM,
  FODI_TYPE_FOREIGN,
  FODI_TYPE_LIST,
  FODI_TYPE_MAP,
  FODI_TYPE_NULL,
  FODI_TYPE_STRING,

  // The object is of a type that isn't accessible by the C API.
  FODI_TYPE_UNKNOWN
} FodiType;

// Get the current fodi version number.
//
// Can be used to range checks over versions.
FODI_API int fodiGetVersionNumber();

// Initializes [configuration] with all of its default values.
//
// Call this before setting the particular fields you care about.
FODI_API void fodiInitConfiguration(FodiConfiguration* configuration);

// Creates a new Fodi virtual machine using the given [configuration]. Fodi
// will copy the configuration data, so the argument passed to this can be
// freed after calling this. If [configuration] is `NULL`, uses a default
// configuration.
FODI_API FodiVM* fodiNewVM(FodiConfiguration* configuration);

// Disposes of all resources is use by [vm], which was previously created by a
// call to [fodiNewVM].
FODI_API void fodiFreeVM(FodiVM* vm);

// Immediately run the garbage collector to free unused memory.
FODI_API void fodiCollectGarbage(FodiVM* vm);

// Runs [source], a string of Fodi source code in a new fiber in [vm] in the
// context of resolved [module].
FODI_API FodiInterpretResult fodiInterpret(FodiVM* vm, const char* module,
                                  const char* source);

// Creates a handle that can be used to invoke a method with [signature] on
// using a receiver and arguments that are set up on the stack.
//
// This handle can be used repeatedly to directly invoke that method from C
// code using [fodiCall].
//
// When you are done with this handle, it must be released using
// [fodiReleaseHandle].
FODI_API FodiHandle* fodiMakeCallHandle(FodiVM* vm, const char* signature);

// Calls [method], using the receiver and arguments previously set up on the
// stack.
//
// [method] must have been created by a call to [fodiMakeCallHandle]. The
// arguments to the method must be already on the stack. The receiver should be
// in slot 0 with the remaining arguments following it, in order. It is an
// error if the number of arguments provided does not match the method's
// signature.
//
// After this returns, you can access the return value from slot 0 on the stack.
FODI_API FodiInterpretResult fodiCall(FodiVM* vm, FodiHandle* method);

// Releases the reference stored in [handle]. After calling this, [handle] can
// no longer be used.
FODI_API void fodiReleaseHandle(FodiVM* vm, FodiHandle* handle);

// The following functions are intended to be called from foreign methods or
// finalizers. The interface Fodi provides to a foreign method is like a
// register machine: you are given a numbered array of slots that values can be
// read from and written to. Values always live in a slot (unless explicitly
// captured using fodiGetSlotHandle(), which ensures the garbage collector can
// find them.
//
// When your foreign function is called, you are given one slot for the receiver
// and each argument to the method. The receiver is in slot 0 and the arguments
// are in increasingly numbered slots after that. You are free to read and
// write to those slots as you want. If you want more slots to use as scratch
// space, you can call fodiEnsureSlots() to add more.
//
// When your function returns, every slot except slot zero is discarded and the
// value in slot zero is used as the return value of the method. If you don't
// store a return value in that slot yourself, it will retain its previous
// value, the receiver.
//
// While Fodi is dynamically typed, C is not. This means the C interface has to
// support the various types of primitive values a Fodi variable can hold: bool,
// double, string, etc. If we supported this for every operation in the C API,
// there would be a combinatorial explosion of functions, like "get a
// double-valued element from a list", "insert a string key and double value
// into a map", etc.
//
// To avoid that, the only way to convert to and from a raw C value is by going
// into and out of a slot. All other functions work with values already in a
// slot. So, to add an element to a list, you put the list in one slot, and the
// element in another. Then there is a single API function fodiInsertInList()
// that takes the element out of that slot and puts it into the list.
//
// The goal of this API is to be easy to use while not compromising performance.
// The latter means it does not do type or bounds checking at runtime except
// using assertions which are generally removed from release builds. C is an
// unsafe language, so it's up to you to be careful to use it correctly. In
// return, you get a very fast FFI.

// Returns the number of slots available to the current foreign method.
FODI_API int fodiGetSlotCount(FodiVM* vm);

// Ensures that the foreign method stack has at least [numSlots] available for
// use, growing the stack if needed.
//
// Does not shrink the stack if it has more than enough slots.
//
// It is an error to call this from a finalizer.
FODI_API void fodiEnsureSlots(FodiVM* vm, int numSlots);

// Gets the type of the object in [slot].
FODI_API FodiType fodiGetSlotType(FodiVM* vm, int slot);

// Reads a boolean value from [slot].
//
// It is an error to call this if the slot does not contain a boolean value.
FODI_API bool fodiGetSlotBool(FodiVM* vm, int slot);

// Reads a byte array from [slot].
//
// The memory for the returned string is owned by Fodi. You can inspect it
// while in your foreign method, but cannot keep a pointer to it after the
// function returns, since the garbage collector may reclaim it.
//
// Returns a pointer to the first byte of the array and fill [length] with the
// number of bytes in the array.
//
// It is an error to call this if the slot does not contain a string.
FODI_API const char* fodiGetSlotBytes(FodiVM* vm, int slot, int* length);

// Reads a number from [slot].
//
// It is an error to call this if the slot does not contain a number.
FODI_API double fodiGetSlotDouble(FodiVM* vm, int slot);

// Reads a foreign object from [slot] and returns a pointer to the foreign data
// stored with it.
//
// It is an error to call this if the slot does not contain an instance of a
// foreign class.
FODI_API void* fodiGetSlotForeign(FodiVM* vm, int slot);

// Reads a string from [slot].
//
// The memory for the returned string is owned by Fodi. You can inspect it
// while in your foreign method, but cannot keep a pointer to it after the
// function returns, since the garbage collector may reclaim it.
//
// It is an error to call this if the slot does not contain a string.
FODI_API const char* fodiGetSlotString(FodiVM* vm, int slot);

// Creates a handle for the value stored in [slot].
//
// This will prevent the object that is referred to from being garbage collected
// until the handle is released by calling [fodiReleaseHandle()].
FODI_API FodiHandle* fodiGetSlotHandle(FodiVM* vm, int slot);

// Stores the boolean [value] in [slot].
FODI_API void fodiSetSlotBool(FodiVM* vm, int slot, bool value);

// Stores the array [length] of [bytes] in [slot].
//
// The bytes are copied to a new string within Fodi's heap, so you can free
// memory used by them after this is called.
FODI_API void fodiSetSlotBytes(FodiVM* vm, int slot, const char* bytes, size_t length);

// Stores the numeric [value] in [slot].
FODI_API void fodiSetSlotDouble(FodiVM* vm, int slot, double value);

// Creates a new instance of the foreign class stored in [classSlot] with [size]
// bytes of raw storage and places the resulting object in [slot].
//
// This does not invoke the foreign class's constructor on the new instance. If
// you need that to happen, call the constructor from Fodi, which will then
// call the allocator foreign method. In there, call this to create the object
// and then the constructor will be invoked when the allocator returns.
//
// Returns a pointer to the foreign object's data.
FODI_API void* fodiSetSlotNewForeign(FodiVM* vm, int slot, int classSlot, size_t size);

// Stores a new empty list in [slot].
FODI_API void fodiSetSlotNewList(FodiVM* vm, int slot);

// Stores a new empty map in [slot].
FODI_API void fodiSetSlotNewMap(FodiVM* vm, int slot);

// Stores null in [slot].
FODI_API void fodiSetSlotNull(FodiVM* vm, int slot);

// Stores the string [text] in [slot].
//
// The [text] is copied to a new string within Fodi's heap, so you can free
// memory used by it after this is called. The length is calculated using
// [strlen()]. If the string may contain any null bytes in the middle, then you
// should use [fodiSetSlotBytes()] instead.
FODI_API void fodiSetSlotString(FodiVM* vm, int slot, const char* text);

// Stores the value captured in [handle] in [slot].
//
// This does not release the handle for the value.
FODI_API void fodiSetSlotHandle(FodiVM* vm, int slot, FodiHandle* handle);

// Returns the number of elements in the list stored in [slot].
FODI_API int fodiGetListCount(FodiVM* vm, int slot);

// Reads element [index] from the list in [listSlot] and stores it in
// [elementSlot].
FODI_API void fodiGetListElement(FodiVM* vm, int listSlot, int index, int elementSlot);

// Sets the value stored at [index] in the list at [listSlot], 
// to the value from [elementSlot]. 
FODI_API void fodiSetListElement(FodiVM* vm, int listSlot, int index, int elementSlot);

// Takes the value stored at [elementSlot] and inserts it into the list stored
// at [listSlot] at [index].
//
// As in Fodi, negative indexes can be used to insert from the end. To append
// an element, use `-1` for the index.
FODI_API void fodiInsertInList(FodiVM* vm, int listSlot, int index, int elementSlot);

// Returns the number of entries in the map stored in [slot].
FODI_API int fodiGetMapCount(FodiVM* vm, int slot);

// Returns true if the key in [keySlot] is found in the map placed in [mapSlot].
FODI_API bool fodiGetMapContainsKey(FodiVM* vm, int mapSlot, int keySlot);

// Retrieves a value with the key in [keySlot] from the map in [mapSlot] and
// stores it in [valueSlot].
FODI_API void fodiGetMapValue(FodiVM* vm, int mapSlot, int keySlot, int valueSlot);

// Takes the value stored at [valueSlot] and inserts it into the map stored
// at [mapSlot] with key [keySlot].
FODI_API void fodiSetMapValue(FodiVM* vm, int mapSlot, int keySlot, int valueSlot);

// Removes a value from the map in [mapSlot], with the key from [keySlot],
// and place it in [removedValueSlot]. If not found, [removedValueSlot] is
// set to null, the same behaviour as the Fodi Map API.
FODI_API void fodiRemoveMapValue(FodiVM* vm, int mapSlot, int keySlot,
                        int removedValueSlot);

// Looks up the top level variable with [name] in resolved [module] and stores
// it in [slot].
FODI_API void fodiGetVariable(FodiVM* vm, const char* module, const char* name,
                     int slot);

// Looks up the top level variable with [name] in resolved [module], 
// returns false if not found. The module must be imported at the time, 
// use fodiHasModule to ensure that before calling.
FODI_API bool fodiHasVariable(FodiVM* vm, const char* module, const char* name);

// Returns true if [module] has been imported/resolved before, false if not.
FODI_API bool fodiHasModule(FodiVM* vm, const char* module);

// Sets the current fiber to be aborted, and uses the value in [slot] as the
// runtime error object.
FODI_API void fodiAbortFiber(FodiVM* vm, int slot);

// Returns the user data associated with the FodiVM.
FODI_API void* fodiGetUserData(FodiVM* vm);

// Sets user data associated with the FodiVM.
FODI_API void fodiSetUserData(FodiVM* vm, void* userData);

#endif
