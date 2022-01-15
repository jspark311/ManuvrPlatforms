# ManuvrArduino

A collection of shims for running ManuvrDrivers and CppPotpourri on top of Arduino.

Because "Arduino" now covers so many weird arrangements of both good and bad
code (usually in the same binary), no effort is here made to make these shims
work under all environments.

The environments I _have_ used it under are:

  * Arduino Uno
  * Teensy 3.x (Teensyduino)
  * Teensy 4.1 (Teensyduino)

Generally my advice here would be to hard-fork the parts you want into your own
tree, and build it with the rest of your top-level code. If you are in a classic
Arduino environment, that would mean copying the relevant files into your sketch
folder.

------------------------

### Dependencies

This class relies on [CppPotpourri](https://github.com/jspark311/CppPotpourri).
