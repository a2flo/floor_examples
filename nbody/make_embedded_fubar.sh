#!/bin/sh

OCC=
if [[ -n $(command -v occ) ]]; then
	OCC=occ
elif [[ -n $(command -v /opt/floor/toolchain/bin/occ) ]]; then
	OCC=/opt/floor/toolchain/bin/occ
elif [[ -n $(command -v occd) ]]; then
	OCC=occd
elif [[ -n $(command -v /opt/floor/toolchain/bin/occd) ]]; then
	OCC=/opt/floor/toolchain/bin/occd
else
	echo "failed to find the libfloor offline compiler binary"
	exit -1
fi

# nbody
rm ../data/nbody.fubar 2>/dev/null
${OCC} --src src/nbody.cpp --fubar targets_nbody.json --out ../data/nbody.fubar --no-double -vv --warnings
if [ ! -f ../data/nbody.fubar ]; then
	echo "failed to build nbody.fubar"
	exit -2
fi

xxd -i ../data/nbody.fubar > src/nbody_fubar.hpp

# mip map minify
rm ../data/mmm.fubar 2>/dev/null
${OCC} --src /opt/floor/include/floor/compute/device/mip_map_minify.hpp --fubar targets_mmm.json --out ../data/mmm.fubar --no-double -vv
if [ ! -f ../data/mmm.fubar ]; then
	echo "failed to build mmm.fubar"
	exit -3
fi

xxd -i ../data/mmm.fubar > src/nbody_mmm_fubar.hpp
