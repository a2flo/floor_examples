#!/bin/sh

# get args
if [[ $# < 2 ]]; then
	echo "insufficient args: need src and out file name"
	exit -1
fi
src_file="$1"
out_file="$2"
shift
shift

# get occ binary
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
	exit -2
fi

# build
rm "${out_file}" 2>/dev/null
targets_json=$(dirname "$0")"/targets.json"
${OCC} --src "${src_file}" --fubar "${targets_json}" --out "${out_file}" --fubar-compress --no-double -vv --warnings "$@" -- -fdiscard-value-names
if [ ! -f "${out_file}" ]; then
	echo "failed to build ${src_file}"
	exit -3
fi
