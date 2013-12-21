#!/bin/sh
#
# creates still-mpegs from pictures, to be able to display them
# using a hardware video decoder. Does about the same as pic2m2v.c
# (C) 2013 Stefan Seyfried
# License: GPLv2+
#
if grep -q TRIPLEDRAGON /proc/cpuinfo; then
	RES=704x576
else
	RES=1280x720
fi

while true; do
	IN=$1
	test -z "$IN" && break
	shift
	OUT="/var/cache/`echo ${IN#/}|sed 's@/@.@g'`"
	MD5=${OUT}.md5
	M2V=${OUT}.m2v
	# $MD5 not existing => return code != 0
	if [ -s $M2V ] && md5sum -c -s $MD5 > /dev/null 2>&1; then
		echo "$IN is unchanged"
		touch -r $IN $M2V
		continue
	fi
	if ! [ -e $IN ]; then
		echo "$IN does not exist!"
		continue
	fi
	echo "converting $IN -> $M2V"
	ffmpeg -v 31 -y -f mjpeg -i $IN -s $RES $M2V < /dev/null
	if ! [ -s $M2V ]; then
		echo "$M2V does not exist - conversion error?"
		continue
	fi
	# set the time identical to input file
	touch -r $IN $M2V
	md5sum $IN > $MD5
done
