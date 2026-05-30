# try fodi implementation

This is the code to build the https://fodi.io/try wasm component.

### How to build

- Install emscripten sdk from https://emscripten.org/
- Make the emsdk available to your terminal/PATH 
	- for example:
    - `source ~/dev/emsdk/emsdk_env.sh`
- Run the emmake command to build
    - `emmake make`

That should be all. This builds a js + wasm file for the page.

### How does the page work?

The page is at `doc/site/try/template.html`.

It loads `fodi_try.js` which loads `fodi_try.wasm`.
The page uses emscripten API to call the `fodi_compile` C function, found in `main.try.c`.
The page hooks up `printf` logging to the console for display.

### Notes

- The binaries land in `bin/fodi_try.wasm` and `bin/fodi_try.js` when building
- The default html output from emsripten is not used, `doc/site/try/template.html` is
- The fodi_try.js and fodi_try.wasm files are copied to `doc/site/static`
- The make project is a modified version of `projects/make`
- The code relies on code in `test/`
