#!/bin/bash
#esp-idf v5.3
source /home/ian/esp-idf/export.sh
idf.py build size
idf.py flash monitor
