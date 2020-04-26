#!/bin/sh
# Integration tests for qcow2.c via catqcow2 using the output from qemu-img.

bail () { echo "Bail out! $*"; exit 1; }
result () { # [0|1] <desc>
	counter=$(expr ${counter:-0} + 1)
	if [ $1 -eq 0 ]; then echo "ok $counter - $2"
	else echo "not ok $counter - $2"; failed=1; fi
	if [ $counter -gt $countermax ]; then
	    echo "too many tests!" >&2; failed=1
	fi
}
start () { # number of tests
	countermax=$1 counter=0 failed=
	echo 1..$1
	trap '[ $failed ] && exit 1' 0
}


# Thin wrapper program around qcow2.c
SUT=./catqcow2

start 3

# Precondition checks
qemu-img --version >/dev/null || bail "missing qemu-img"
[ -x $SUT ] || bail "missing $SUT"

tmp=${TMPDIR:-/tmp}/check


# 1. An "empty" 4MB qcow file dumps to 4MB of NULs
#  - use qemu-img to make an empty 4MB qcow2 file
qemu-img create -q -f qcow2 $tmp.zero4M.qcow2 4194304
#  - use dd to create a 4MB file of NULs
dd if=/dev/zero bs=1024 count=4096 of=$tmp.zero4M.raw status=none
#  - catqcow2 the .qcow2 file and compare it against the raw
$SUT <$tmp.zero4M.qcow2 | cmp -s - $tmp.zero4M.raw
#  - exit code should be success
result $? "empty 4M .qcow2 dumps to 4M of NULs"


# 2. A "random" 8MB input file converted will dump to its original data.
#  - create the start of the random 8MB file
cat catqcow2 README.md >$tmp.blah8M.raw
#  - keep growing the file by doubling it until it exceeds 8MB in size
while [ $(wc -c <$tmp.blah8M.raw) -lt 8388608 ]; do
	# making semi-random input
	cat $tmp.blah8M.raw $tmp.blah8M.raw > $tmp.blah8M.raw2
	mv $tmp.blah8M.raw2 $tmp.blah8M.raw
done
#  - trim the file back to 8MB
dd bs=1048576 count=8 status=none <$tmp.blah8M.raw >$tmp.blah8M.raw2
mv $tmp.blah8M.raw2 $tmp.blah8M.raw
#  - convert it to .qcow2 using qemu-img
qemu-img convert -q -f raw -O qcow2 $tmp.blah8M.raw $tmp.blah8M.qcow2
#  - check that what we extract using catqcow2 is what we put in
$SUT $tmp.blah8M.qcow2 | cmp -s - $tmp.blah8M.raw
result $? "random 8M as .qcow2 dumps to same input"


# 3. Same again, but with a smaller cluster size.
# (This may fail with "too fine" on some systems)
#  - convert random input to .qcow2 with 4MB cluster size
qemu-img convert -q -f raw -O qcow2 -o cluster_size=4096 \
	$tmp.blah8M.raw $tmp.blah8M.qcow2
#  - check that what we extract using catqcow2 is what we put in
$SUT $tmp.blah8M.qcow2 | cmp -s - $tmp.blah8M.raw
result $? "random 8M as .qcow2 dumps to same input, cluster_size=4M"


# Clean up temporary files
rm -f $tmp.zero4M.raw
rm -f $tmp.zero4M.qcow2
rm -f $tmp.blah8M.raw
rm -f $tmp.blah8M.qcow2
