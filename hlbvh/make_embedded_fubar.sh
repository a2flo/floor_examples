#!/bin/sh

../etc/build_embedded_fubar.sh $(pwd)/src/hlbvh.cpp ../data/hlbvh.fubar
../etc/build_embedded_fubar.sh $(pwd)/src/radix_sort.cpp ../data/hlbvh_radix_sort.fubar --metal-restrictive-vectorization
../etc/build_embedded_fubar.sh $(pwd)/src/hlbvh_shaders.cpp ../data/hlbvh_shaders.fubar
