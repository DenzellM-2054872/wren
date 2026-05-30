^title

## Fodi is a small, fast, class-based concurrent scripting language

---

Think Smalltalk in a Lua-sized package with a dash of Erlang and wrapped up in
a familiar, modern [syntax][].

<pre class="snippet">
System.print("Hello, world!")

class Fodi {
  flyTo(city) {
    System.print("Flying to %(city)")
  }
}

var adjectives = Fiber.new {
  ["small", "clean", "fast"].each {|word| Fiber.yield(word) }
}

while (!adjectives.isDone) System.print(adjectives.call())
</pre>

 *  **Fodi is small.** The VM implementation is under [4,000 semicolons][src].
    You can skim the whole thing in an afternoon. It's *small*, but not
    *dense*. It is readable and [lovingly-commented][nan].

 *  **Fodi is fast.** A fast single-pass compiler to tight bytecode, and a
    compact object representation help Fodi [compete with other dynamic
    languages][perf].

 *  **Fodi is class-based.** There are lots of scripting languages out there,
    but many have unusual or non-existent object models. Fodi places
    [classes][] front and center.

 *  **Fodi is concurrent.** Lightweight [fibers][] are core to the execution
    model and let you organize your program into a flock of communicating
    coroutines.

 *  **Fodi is a scripting language.** Fodi is intended for embedding in
    applications. It has no dependencies, a small standard library,
    and [an easy-to-use C API][embedding]. It compiles cleanly as C99, C++98
    or anything later.

---

You can try it [in your browser][browser]!   
If you like the sound of this, [let's get started][started].    
Excited? You're also welcome to [get involved][contribute]!

[syntax]: syntax.html
[src]: https://github.com/fodi-lang/fodi/tree/main/src
[nan]: https://github.com/fodi-lang/fodi/blob/46c1ba92492e9257aba6418403161072d640cb29/src/fodi_value.h#L378-L433
[perf]: performance.html
[classes]: classes.html
[fibers]: concurrency.html
[embedding]: embedding
[started]: getting-started.html
[browser]: try
[contribute]: contributing.html
