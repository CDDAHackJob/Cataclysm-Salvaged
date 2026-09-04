# Contributing

**Found a bug?** Please read [ISSUES.md](../ISSUES.md) first, then open an issue.
Bug reports and suggestions are welcome.

**Want to submit code?** Cataclysm: Salvaged does not take unsolicited pull
requests for now. If you would like to work on the project, get in touch first at
<CDDA_HackJob@protonmail.com>. I will likely turn you down for the time being.
Not because I don't want the help, but because I have a vision I want to work
towards first and get rolled out to be the foundation.

Cataclysm: Salvaged is released under the Creative Commons Attribution ShareAlike
3.0 license. The code and content of the game is free to use, modify, and
redistribute for any purpose whatsoever. See
http://creativecommons.org/licenses/by-sa/3.0/ for details. This means any
contribution you make to the project will also be covered by the same license,
and this license is irrevocable.

The rest of this document describes how the code and content are structured, and
applies whether you are changing the game for yourself or for the project.

## Using a good text editor

Most of the game data is defined in JSON files. These files are intended to be
easy for you to edit, but there are some pitfalls. Using Windows Notepad can get
you into trouble, because it likes to insert a special character called a
[BOM](https://en.wikipedia.org/wiki/Byte_order_mark) at the start of the file,
which the game does not want.

If you're going to be editing JSON files consider getting a more fully-featured
editor such as [Notepad++](https://notepad-plus-plus.org/).

## Code Style

Code style is enforced across the codebase by `astyle`.
See [CODE_STYLE](CODE_STYLE.md) for details.

## Translations

See [TRANSLATING.md](TRANSLATING.md) for more information:

* [For translators](TRANSLATING.md#translators)
* [For developers](TRANSLATING.md#developers)
* [For maintainers](TRANSLATING.md#maintainers)

## Doxygen Comments

Extensive documentation of classes and class members will make the code more readable.

Use the following template for commenting classes:

```c++
/**
 * Brief description
 *
 * Lengthy description with many words. (optional)
 */
class foo {
```

Use the following template for commenting functions:

```c++
/**
 * Brief description
 *
 * Lengthy description with many words. (optional)
 * @param param1 Description of param1 (optional)
 * @return Description of return (optional)
 */
int foo(int param1);
```

Use the following template for commenting member variables:

```c++
/** Brief description **/
int foo;
```

Helpful pages:

* [Doxygen Manual - Special Commands](https://www.doxygen.nl/manual/commands.html)
* [Doxygen Manual - Standard Markdown](https://www.doxygen.nl/manual/markdown.html#markdown_std)
* [Doxygen Manual - Frequently Asked Questions](https://www.doxygen.nl/manual/faq.html)

### Guidelines for adding documentation

* Doxygen comments should describe behavior towards the outside, not implementation, but since many classes in the game are intertwined, it's often necessary to describe implementation.
* Describe things that aren't obvious just from the name.
* Don't describe redundantly: `/** Map **/; map* map;` is not a helpful comment.
* When documenting X, describe how X interacts with other components, not just what X itself does.

### Building the documentation for viewing it locally

* Install doxygen
* `doxygen doxygen_doc/doxygen_conf.txt `
* `firefox doxygen_doc/html/index.html` (replace firefox with your browser of choice)

## Tooling support

Various tools are available to help you keep your changes conforming to the appropriate style. See [DEVELOPER_TOOLING.md](DEVELOPER_TOOLING.md) for more details.

## Unit tests

There is a suite of tests built into the source tree at tests/  
You should run the test suite after ANY change to the game source.  
An ordinary invocation of `make` will build the test executable at `tests/cata_test`, and it can be invoked like any ordinary executable, or via `make check`.
Running `tests/cata_test` with no arguments will run the entire test suite; running it with `--help` will print a number of invocation options you can use to adjust its operation.

```bash
$ make
... compilation details ...
$ tests/cata_test
Starting the actual test at Fri Nov  9 04:37:03 2018
===============================================================================
All tests passed (1324684 assertions in 94 test cases)
Ended test at Fri Nov  9 04:37:45 2018
The test took 41.772 seconds
```

It is recommended to habitually invoke make like ``make YOUR BUILD OPTIONS && make check``.

If you're working with Visual Studio (and don't have `make`), see [Visual Studio-specific advice](COMPILING/COMPILING-VS-VCPKG.md#running-unit-tests).

If you want/need to add a test, see [TESTING.md](TESTING.md)

## In-game testing, test environment and the debug menu

Whether you are implementing a new feature or whether you are fixing a bug, it is always a good practice to test your changes in-game. It can be a hard task to create the exact conditions by playing a normal game to be able to test your changes, which is why there is a debug menu. There is no default key to bring up the menu so you will need to assign one first.

Bring up the keybindings menu (press `Escape` then `1`), scroll down almost to the bottom and press `+` to add a new keybinding. Press the letter that corresponds to the *Debug menu* item, then press the key you want to use to bring up the debug menu. To test your changes, create a new world with a new character. Once you are in that world, press the key you just assigned for the debug menu and you should see:

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Debug Functions - Using these will cheat not only the game, but yourself. │
│ You won't grow. You won't improve.                                        │
│ Taking this shortcut will gain you nothing. Your victory will be hollow.  │
│ Nothing will be risked and nothing will be gained.                        │
├───────────────────────────────────────────────────────────────────────────┤
│ i Info…                                                                   │
│ g Game…                                                                   │
│ s Spawning…                                                               │
│ p Player…                                                                 │
│ v Vehicle…                                                                │
│ t Teleport…                                                               │
│ m Map…                                                                    │
└───────────────────────────────────────────────────────────────────────────┘
```

With these commands, you should be able to recreate the proper conditions to test your changes.
