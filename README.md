## This entire repo is full of cancer and fail, and you shouldn't use it under any circumstances.

Most of it was ripped out of ManuvrOS.
Some of ManuvrOS was GoodCode(tm). But it was quickly overtaken by its own virtues and became unmaintainable. The meaning of this doesn't matter. Just know that it ended up sucking, despite being composed of often well-executed modules.

Most of the best of ManuvrOS ended up in [CppPotpourri](https://github.com/jspark311/CppPotpourri). Most of the data structures, templates, and support material were well-tested under many circumstances, even if not GoodCode(tm).

Drivers for specific hardware were migrated to [ManuvrDrivers](https://github.com/jspark311/ManuvrDrivers), and re-written to take account of all we had learned to that point (more on that later).

The notion of "Platform" was cut back down to basics and moved to CppPotpourri as an abstract notion, and fulfilled by shims like those in this repo.


# Regarding platform support and abstraction

### On Arduino

I admire Arduino for having made their API as general and portable as they have managed to do, despite hating the choices sometimes. Many of the abstractions and function names were aped from Arduino or one of its offshoots where there was an uncommon proportion of GoodCode(tm).


### Things I learned the hard way

  * Platform support is unavoidably connected to build system. And nothing is meant to work with anything else.

  * Every good idea is quickly buried under five bad ideas that are trying to cargo cult its success (worst case), or trying to make square pegs fit into round holes (best case).

  * No one is smart enough to write software. Asking that any given piece of code not only WORK, but work EVERYWHERE should make me LMAOBBQ. But since one day I'm going to die, and I can't afford to rewrite everything for every purpose everytime I need to solve anything, I'm obliged to try anyhow and hope I don't fux0r it up too badly.


### Build system

For now, all build systems and strategies are treated isomorphically. That is: They are all completely disregarded. If you are misguided enough to use this code, you can figure out how to make it work in your particular schizo codebase.
Suffer.


### Adding to this pile of suck

    bool x: You've written a set of shims for a platform/ENV that wasn't previously here.
    bool y: You've made a change to a shim that lowers the proportion of suck.
    bool z: You're able to laugh at how terrible a job of it you almost certainly did.
    if ((x | y) & z) {
      // I will probably take your PR or link to your repo.
    }
