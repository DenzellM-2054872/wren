^title Contributing

Like the bird, Fodi's ecosystem is small but full of life. Almost everything is
under active development and there's lots to do. We'd be delighted to have you
help.

The first thing to do is to join [the discord community][discord] (or [the mailing list][list]) and say,
"Hi". There are no strangers to Fodi, just friends we haven't met yet.

## Growing the ecosystem

The simplest and often most helpful way to join the Fodi party is to be a Fodi
*user*. Create an application that embeds Fodi. Write a library or a handy
utility in Fodi. Add syntax highlighting support for Fodi to your favorite text
editor. Share that stuff and it will help the next Fodi user to come along.

If you do any of the above, let us know by adding it to [the wiki][wiki].   
We like to keep track of:

[wiki]: https://github.com/fodi-lang/fodi/wiki

* [Applications][] that host Fodi as a scripting language.
* [Modules][] written in Fodi that others can use.
* [Language bindings][] that let you interact with Fodi from other
  languages.
* [Tools and utilities][] that make it easier to be a Fodi programmer.

[applications]: https://github.com/fodi-lang/fodi/wiki/Applications
[modules]: https://github.com/fodi-lang/fodi/wiki/Modules
[language bindings]: https://github.com/fodi-lang/fodi/wiki/Language-Bindings
[tools and utilities]: https://github.com/fodi-lang/fodi/wiki/Tools

## Contributing to Fodi

You're also more than welcome to contribute to Fodi itself, both the core VM and
the command-line interpreter. The source is developed [on GitHub][github]. Our
hope is that the codebase, tests, and [documentation][docs] are easy to
understand and contribute to. If they aren't, that's a bug.

You can learn how to build fodi on the [getting started page](getting-started.html#building-fodi).

### Finding something to hack on

Between the [issue tracker][issue] and searching for `TODO` comments in the
code, it's pretty easy to find something that needs doing, though we don't
always do a good job of writing everything down.

If nothing there suits your fancy, new ideas are welcome as well! If you have an
idea for a significant change or addition, please file a [proposal][] to discuss
it before writing lots of code. Fodi tries very *very* hard to be minimal which
means often having to say "no" to language additions, even really cool ones.

### Hacking on docs

The [documentation][] is one of the easiest&mdash;and most
important!&mdash;parts of Fodi to contribute to. The source for the site is
written in [Markdown][] and lives under `doc/site`. A
simple Python 3 script, `util/generate_docs.py`, converts that to HTML and CSS.

[documentation]: /
[markdown]: http://daringfireball.net/projects/markdown/

    $ python util/generate_docs.py

This generates the site in `build/docs/`. You can run any simple static web
server from there. Python includes one:

    $ cd build/docs
    $ python -m http.server

Running that script every time you change a line of Markdown can be slow,
so there is also a file watching version that will automatically regenerate the
docs when you edit a file:

    $ python util/generate_docs.py --watch

### Hacking on the VM

The basic process is simple:

1. **Make sure you can build and run the tests locally.** It's good to ensure
   you're starting from a happy place before you poke at the code. Running the
   tests is as simple as [building the vm project](getting-started.html#building-fodi),
   which generates `bin/fodi_test` and then running the following python 3 script:

        $ python util/test.py

    If there are no failures, you're good to go.

2. **[Fork the repo][fork] so you can change it locally.** Please make your
   changes in separate [feature branches][] to make things a little easier.

3. **Change the code.** Please follow the style of the surrounding code. That
   basically means `camelCase` names, `{` on the next line, keep within 80
   columns, and two spaces of indentation. If you see places where the existing
   code is inconsistent, let us know.

4. **Write some tests for your new functionality.** They live under `test/`.
   Take a look at some existing tests to get an idea of how to define
   expectations.

5. **Make sure the tests all pass, both the old ones and your new ones.**

6. **Add your name and email to the [AUTHORS][] file if you haven't already.**

7. **Send a [pull request][].** Pat yourself on the back for contributing to a
   fun open source project! 

## Getting help

If at any point you have questions, feel free to [file an issue][issue] or ask
on the [discord community][discord] (or the [mailing list][list]). If you're a Redditor, try the
[/r/fodi_lang][subreddit] subreddit. You can also email me directly (`robert` at
`stuffwithstuff.com`) if you want something less public.

[mit]: http://opensource.org/licenses/MIT
[github]: https://github.com/fodi-lang/
[fork]: https://help.github.com/articles/fork-a-repo/
[docs]: https://github.com/fodi-lang/fodi/tree/main/doc/site
[issue]: https://github.com/fodi-lang/fodi/issues
[proposal]: https://github.com/fodi-lang/fodi/labels/proposal
[feature branches]: https://www.atlassian.com/git/tutorials/comparing-workflows/centralized-workflow
[authors]: https://github.com/fodi-lang/fodi/tree/main/AUTHORS
[pull request]: https://github.com/fodi-lang/fodi/pulls
[list]: https://groups.google.com/forum/#!forum/fodi-lang
[subreddit]: https://www.reddit.com/r/fodi_lang/
[discord]: https://discord.gg/Kx6PxSX
