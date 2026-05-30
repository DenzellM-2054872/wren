This contains the automated validation suite for the VM and built-in libraries.

* `benchmark/` - Performance tests. These aren't strictly pass/fail, but let us
  compare performance both against other languages and against previous builds
  of Fodi itself.

* `core/` - Tests for the built in core library, mainly methods on the core
  classes. If a bug is in `fodi_core.c` or `fodi_value.c`, it will most likely
  break one of these tests.

* `language/` - Tests of the language itself, its grammar and runtime
  semantics. If a bug is in `fodi_compiler.c` or `fodi_vm.c`, it will most
  likely break one of these tests. This includes tests for the syntax for the
  literal forms of the core classes.

* `limit/` - Tests for various hardcoded limits. The language doesn't
  officially *specify* these limits, but the Fodi implementation has them.
  These tests ensure that limit behavior is well-defined and tested.