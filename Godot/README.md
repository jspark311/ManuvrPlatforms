# C3P Under Godot

A collection of shims for running ManuvrDrivers and CppPotpourri on top of Godot.
This shim has only been tested against `godot-cpp` v4.2.
https://github.com/godotengine/godot-cpp/tree/4.2


## Usage

Trying to include this isolated dirctory into Godot project is probably more
trouble than it is worth. Honestly, you would be better off hard-forking the two
relevant source files into your own codebase, directly.

------------------------

### Dependencies

This shim relies on [CppPotpourri](https://github.com/jspark311/CppPotpourri).
But you probably already have that elsewhere in your project's build system.

------------------------

### Building the examples

No examples here yet.



# Aside: Regarding the appropriateness of this code in "Platform"

C3P treats Godot as if it were a platform, and this is therefore the
"functionally pure" repo for it to reside in. But it would probably be far less
practical trouble for eveyone if it simply lived in C3P as if it were any other
library that was supported.

  * Option P: Keep it as a platform, and tolerate the hard-forking and build system complexity.
  * Option C: Move it to C3P and rephrase it as library support, forgoing a platform layer completely.

Option C has the notable advantage that support for specific Godot-target platforms
can be thoughtlessly supported for any target also supported by godot-cpp, *AND*
additional platform support outside of Godot could still be achieved by using the
(otherwise vacant) platform layer. In which case, all of these functions should be
made weak-references to allow more specific platform support to take ultimate
prioritry.
