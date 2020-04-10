#!/bin/sh

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


start 3

# Precondition checks
qemu-img --version >/dev/null || bail "missing qemu-img"
[ -x ./catqcow2 ] || bail "missing catqcow2"

tmp=${TMPDIR:-/tmp}/check

# An "empty" 4MB qcow file looks like 4MB of nuls
qemu-img create -q -f qcow2 $tmp.zero4M.qcow2 4194304
dd if=/dev/zero bs=1024 count=4096 of=$tmp.zero4M.raw status=none
./catqcow2 <$tmp.zero4M.qcow2 | cmp -s - $tmp.zero4M.raw
result $? "empty 4M"
rm -f $tmp.zero4M.qcow2 $tmp.zero4M.raw

# Convert a "random" 8MB+ input into qcow2, and then
# check that what we extract is what we put in
cat catqcow2 >$tmp.blah8M.raw
while [ $(wc -c <$tmp.blah8M.raw) -lt 8388608 ]; do
	# making semi-random input
	cat $tmp.blah8M.raw $tmp.blah8M.raw > $tmp.blah8M.raw2
	mv $tmp.blah8M.raw2 $tmp.blah8M.raw
done
qemu-img convert -q -f raw -O qcow2 $tmp.blah8M.raw $tmp.blah8M.qcow2
./catqcow2 $tmp.blah8M.qcow2 | cmp -s - $tmp.blah8M.raw
result $? "random 8M identical"

# Repeat but with a smaller cluster size.
# (This may fail with "too fine" on some systems)
qemu-img convert -q -f raw -O qcow2 -o cluster_size=4096 \
	$tmp.blah8M.raw $tmp.blah8M.qcow2
./catqcow2 $tmp.blah8M.qcow2 | cmp -s - $tmp.blah8M.raw
result $? "random 8M with cluster_size 4096 identical"

rm -f $tmp.blah8M.raw $tmp.blah8M.qcow2

