## This entire repo is full of cancer and fail, and you shouldn't use it under any circumstances.

Most of it was ripped out of ManuvrOS.
Some of ManuvrOS was GoodCode(tm). But it was quickly overtaken by its own virtues and became unmaintainable. The meaning of this doesn't matter. Just know that it ended up sucking, despite being composed of often well-executed modules.

Most of the best of ManuvrOS ended up in CppPotpourri. Most of the data structures, templates, and support material were well-tested under many circumstances, even if not GoodCode(tm).

The drivers for specific hardware was migrated to ManuvrDrivers, and re-written to take account of all we had learned to that point (more on that later).

The notion of "Platform" was cut back down to basics and moved to CppPotpourri, as well.


# Regarding platform support and abstraction

### On Arduino

I admire Arduino for having made their API as general and portable as they have managed to do, despite hating the choices sometimes. Some of the things I think they nailed, and which I want to replicate....




### Things I learned the hard way

Platform support is unavoidably connected to build system. And nothing is meant to work with anything else.

Every good idea is quickly buried under five bad ideas that are trying to cargo cult its success (worst case), or trying to make square pegs fit into round holes (best case).

No one is smart enough to write software. Asking that any given piece of code not only WORK, but work EVERYWHERE should make me LMAOBBQ. But since one day I'm going to die, and I can't afford to rewrite everything for every purpose everytime I need to solve anything, I'm obliged to try anyhow and hope I don't fux0r it up too badly.


### Build system

For now, all build systems and strategies are treated isomorphically. That is: They are all completely disregarded. If you are misguided enough to use this code, you can figure out how to make it work in your particular schizo codebase.
Suffer.


### And now, a list a values that Nature will allow us the luxery of holding....

#### ...some of the time


#### ...all of the time
kek... No.



### To integrate into existing frameworks?
