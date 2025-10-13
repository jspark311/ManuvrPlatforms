#!/bin/bash

# This script is meant to go fetch the most recent versions of various libraries that
#   this program has been written against.
mkdir -p lib

# CppPotpourri
rm -rf lib/CppPotpourri
git clone https://github.com/jspark311/CppPotpourri.git lib/CppPotpourri
