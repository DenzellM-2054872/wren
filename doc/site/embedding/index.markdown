^title Embedding

Fodi is designed to be a scripting language that lives inside a host
application, so the embedding API is as important as any of its language
features. Designing this API well requires satisfying several constraints:

1. **Fodi is dynamically typed, but C is not.** A variable can hold a value of
   any type in Fodi, but that's definitely not the case in C unless you define
   some sort of variant type, which ultimately just kicks the problem down the
   road. Eventually, we have to move data across the boundary between statically and dynamically typed code.

2. **Fodi uses garbage collection, but C manages memory manually.** GC adds a
   few constraints on the API. The VM must be able to find every Fodi object
   that is still usable, even if that object is being referenced from native C
   code. Otherwise, Fodi could free an object that's still in use.

    Also, we ideally don't want to let native C code see a bare pointer to a
    chunk of memory managed by Fodi. Many garbage collection strategies involve
    [moving objects][] in memory. If we allow C code to point directly to an
    object, that pointer will be left dangling when the object moves. Fodi's GC
    doesn't move objects today, but we would like to keep that option for the
    future.

3. **The embedding API needs to be fast.** Users may add layers of abstraction
   on top of the API to make it more pleasant to work with, but the base API
   defines the *maximum* performance you can get out of the system. It's the
   bottom of the stack, so there's no way for a user to optimize around it if
   it's too slow. There is no lower level alternative.

4. **We want the API to be pleasant to use.** This is the last constraint
   because it's the softest. Of course, we want a beautiful, usable API. But we
   really *need* to handle the above, so we're willing to make things a bit more
   of a chore to reach the first three goals.

[moving objects]: https://en.wikipedia.org/wiki/Tracing_garbage_collection#Copying_vs._mark-and-sweep_vs._mark-and-don.27t-sweep

Fortunately, we aren't the first people to tackle this. If you're familiar with
[Lua's C API][lua], you'll find Fodi's similar.

[lua]: https://www.lua.org/pil/24.html

### Performance and safety

When code is safely snuggled within the confines of the VM, it's pretty safe.
Method calls are dynamically checked and generate runtime errors which can be
caught and handled. The stack grows if it gets close to overflowing. In general,
when you're within Fodi code, it tries very hard to avoid crashing and burning.

This is why you use a high level language after all&mdash;it's safer and more
productive than C. C, meanwhile, really assumes you know what you're doing. You
can cast pointers in invalid ways, misinterpret bits, use memory after freeing
it, etc. What you get in return is blazing performance. Many of the reasons C is
fast are because it takes all the governors and guardrails off.

Fodi's embedding API defines the border between those worlds, and takes on some
of the characteristics of C. When you call any of the embedding API functions,
it assumes you are calling them correctly. If you invoke a Fodi method from C
that expects three arguments, it trusts that you gave it three arguments.

In debug builds, Fodi has assertions to check as many things as it can, but in
release builds, Fodi expects you to do the right thing. This means you need to
take care when using the embedding API, just like you do in all C code you
write. In return, you get an API that is quite fast.

## Including Fodi

There are two (well, three) ways to get the Fodi VM into your program:

1.  **Link to the static or dynamic library.** When you [build Fodi][build], it
    generates both shared and static libraries in `lib` that you can link to.

2.  **Include the source directly in your application.** If you want to include
    the source directly in your program, you don't need to run any build steps.
    Just add the source files in `src/vm` to your project. They should compile
    cleanly as C99 or C++98 or anything later.

[build]: ../getting-started.html

In either case, you also want to add `src/include` to your include path so you
can find the [public header for Fodi][fodi.h]:

[fodi.h]: https://github.com/fodi-lang/fodi/blob/main/src/include/fodi.h

<pre class="snippet" data-lang="c">
#include "fodi.h"
</pre>

Fodi depends only on the C standard library, so you don't usually need to link
to anything else. On some platforms (at least BSD and Linux) some of the math
functions in `math.h` are implemented in a separate library, [libm][], that you
have to explicitly link to.

[libm]: https://en.wikipedia.org/wiki/C_mathematical_functions#libm

If your program is in C++ but you are linking to the Fodi library compiled as C,
this header handles the differences in calling conventions between C and C++:

<pre class="snippet" data-lang="c">
#include "fodi.hpp"
</pre>

## Creating a Fodi VM

Once you've integrated the code into your executable, you need to create a
virtual machine. To do that, you create a `FodiConfiguration` object and
initialize it.

<pre class="snippet" data-lang="c">
    FodiConfiguration config;
    fodiInitConfiguration(&config);
</pre>

This gives you a basic configuration that has reasonable defaults for
everything. We'll [learn more][configuration] about what you can configure later,
but for now we'll just add the `writeFn`, so that we can print text.

First we need a function that will do something with the output
that Fodi sends us from `System.print` (or `System.write`). *Note that it doesn't
include a newline in the output.*

<pre class="snippet" data-lang="c">
void writeFn(FodiVM* vm, const char* text) {
  printf("%s", text);
}
</pre>

And then, we update the configuration to point to it.

<pre class="snippet" data-lang="c">
  FodiConfiguration config;
  fodiInitConfiguration(&config);
    config.writeFn = &writeFn;
</pre>

[configuration]: configuring-the-vm.html

With this ready, you can create the VM:

<pre class="snippet" data-lang="c">
FodiVM* vm = fodiNewVM(&config);
</pre>

This allocates memory for a new VM and initializes it. The Fodi C implementation
has no global state, so every single bit of data Fodi uses is bundled up inside
a FodiVM. You can have multiple Fodi VMs running independently of each other
without any problems, even concurrently on different threads.

`fodiNewVM()` stores its own copy of the configuration, so after calling it, you
can discard the FodiConfiguration struct you filled in. Now you have a live
VM, waiting to run some code!

## Executing Fodi code

You execute a string of Fodi source code like so:

<pre class="snippet" data-lang="c">
FodiInterpretResult result = fodiInterpret(
    vm,
    "my_module",
    "System.print(\"I am running in a VM!\")");
</pre>

The string is a series of one or more statements separated by newlines. Fodi
copies the string, so you can free it after calling this. When you call
`fodiInterpret()`, Fodi first compiles your source to bytecode. If an error
occurs, it returns immediately with `FODI_RESULT_COMPILE_ERROR`.

Otherwise, Fodi spins up a new [fiber][] and executes the code in that. Your
code can in turn spawn whatever other fibers it wants. It keeps running fibers
until they all complete or one [suspends].

[fiber]: ../concurrency.html
[suspends]: ../modules/core/fiber.html#fiber.suspend()

If a [runtime error][] occurs (and another fiber doesn't handle it), Fodi aborts
fibers all the way back to the main one and returns `FODI_RESULT_RUNTIME_ERROR`.
Otherwise, when the last fiber successfully returns, it returns
`FODI_RESULT_SUCCESS`.

[runtime error]: ../error-handling.html

All code passed to `fodiInterpret()` runs in a special "main" module. That way,
top-level names defined in one call can be accessed in later ones. It's similar
to a REPL session.

## Shutting down a VM

Once the party is over and you're ready to end your relationship with a VM, you
need to free any memory it allocated. You do that like so:

<pre class="snippet" data-lang="c">
fodiFreeVM(vm);
</pre>

After calling that, you obviously cannot use the `FodiVM*` you passed to it
again. It's dead.

Note that Fodi will yell at you if you still have any live [FodiHandle][handle]
objects when you call this. This makes sure you haven't lost track of any of
them (which leaks memory) and you don't try to use any of them after the VM has
been freed.

## A complete example

Below is a complete example of the above.
You can find this file in the [example](https://github.com/fodi-lang/fodi/blob/main/example/embedding/main.c) folder.

<pre class="snippet" data-lang="c">
//For more details, visit https://fodi.io/embedding/

#include &lt;stdio.h>
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

  switch (result) {
    case FODI_RESULT_COMPILE_ERROR:
      { printf("Compile Error!\n"); } break;
    case FODI_RESULT_RUNTIME_ERROR:
      { printf("Runtime Error!\n"); } break;
    case FODI_RESULT_SUCCESS:
      { printf("Success!\n"); } break;
  }

  fodiFreeVM(vm);

}
</pre>

[handle]: slots-and-handles.html#handles

Next, we'll learn to make that VM do useful stuff...

<a class="right" href="slots-and-handles.html">Slots and Handles &rarr;</a>
