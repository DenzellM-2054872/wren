^title Calling C from Fodi

When we are ensconced within the world of Fodi, the external C world is
"foreign" to us. There are two reasons we might want to bring some foreign
flavor into our VM:

* We want to execute code written in C.
* We want to store raw C data.

Since Fodi is object-oriented, behavior lives in methods, so for the former we
have **foreign methods**. Likewise, data lives in objects, so for the latter, we
define **foreign classes**. This page is about the first, foreign methods. The
[next page][] covers foreign classes.

[next page]: /embedding/storing-c-data.html

A foreign method looks to Fodi like a regular method. It is defined on a Fodi
class, it has a name and signature, and calls to it are dynamically dispatched.
The only difference is that the *body* of the method is written in C.

A foreign method is declared in Fodi like so:

<pre class="snippet">
class Math {
  foreign static add(a, b)
}
</pre>

The `foreign` keyword tells Fodi that the method `add()` is declared on `Math`,
but implemented in C. Both static and instance methods can be foreign.

## Binding Foreign Methods

When you call a foreign method, Fodi needs to figure out which C function to
execute. This process is called *binding*. Binding is performed on-demand by the
VM. When a class that declares a foreign method is executed -- when the `class`
statement itself is evaluated -- the VM asks the host application for the C
function that should be used for the foreign method.

It does this through the `bindForeignMethodFn` callback you give it when you
first [configure the VM][config]. This callback isn't the foreign method itself.
It's the binding function your app uses to *look up* foreign methods.

[config]: configuring-the-vm.html

Its signature is:

<pre class="snippet" data-lang="c">
FodiForeignMethodFn bindForeignMethodFn(
    FodiVM* vm,
    const char* module,
    const char* className,
    bool isStatic,
    const char* signature);
</pre>

Every time a foreign method is first declared, the VM invokes this callback. It
passes in the module containing the class declaration, the name of the class
containing the method, the method's signature, and whether or not it's a static
method. In the above example, it would pass something like:

<pre class="snippet" data-lang="c">
bindForeignMethodFn(vm, "main", "Math", true, "add(_,_)");
</pre>

When you configure the VM, you give it a C callback that looks up the
appropriate function for the given foreign method and returns a pointer to it.
Something like:

<pre class="snippet" data-lang="c">
FodiForeignMethodFn bindForeignMethod(
    FodiVM* vm,
    const char* module,
    const char* className,
    bool isStatic,
    const char* signature)
{
  if (strcmp(module, "main") == 0)
  {
    if (strcmp(className, "Math") == 0)
    {
      if (isStatic && strcmp(signature, "add(_,_)") == 0)
      {
        return mathAdd; // C function for Math.add(_,_).
      }
      // Other foreign methods on Math...
    }
    // Other classes in main...
  }
  // Other modules...
}
</pre>

This implementation is pretty tedious, but you get the idea. Feel free to do
something more clever here in your host application.

The important part is that it returns a pointer to a C function to use for that
foreign method. Fodi does this binding step *once* when the class definition is
first executed. It then keeps the function pointer you return and associates it
with that method. This way, *calls* to the foreign method are fast.

## Implementing a Foreign Method

All C functions for foreign methods have the same signature:

<pre class="snippet" data-lang="c">
void foreignMethod(FodiVM* vm);
</pre>

Arguments passed from Fodi are not passed as C arguments, and the method's
return value is not a C return value. Instead -- you guessed it -- we go through
the [slot array][].

[slot array]: /embedding/slots-and-handles.html

When a foreign method is called from Fodi, the VM sets up the slot array with
the receiver and arguments to the call. As in calling Fodi from C, the receiver
object is in slot zero, and arguments are in consecutive slots after that.

You use the slot API to read those arguments, and then perform whatever work you
want to in C. If you want the foreign method to return a value, place it in slot
zero. Like so:

<pre class="snippet" data-lang="c">
void mathAdd(FodiVM* vm)
{
  double a = fodiGetSlotDouble(vm, 1);
  double b = fodiGetSlotDouble(vm, 2);
  fodiSetSlotDouble(vm, 0, a + b);
}
</pre>

While your foreign method is executing, the VM is completely suspended. No other
fibers run until your foreign method returns. You should *not* try to resume the
VM from within a foreign method by calling `fodiCall()` or `fodiInterpret()`.
The VM is not re-entrant.

This covers foreign behavior, but what about foreign *state*? For that, we need
a foreign *class*...

<a class="right" href="storing-c-data.html">Storing C Data &rarr;</a>
<a href="calling-fodi-from-c.html">&larr; Calling Fodi from C</a>
