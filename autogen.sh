#!/bin/sh

# check if pkg-config is installed, otherwise ./configure will fail.
pkg-config --version > /dev/null 2>&1
if [ $? -ne 0 ]
then
	echo "Required package \"pkg-config\" not found."
	exit 1
fi

autoreconf -fiv
