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
