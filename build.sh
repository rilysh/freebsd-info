#!/bin/sh

FLAGS="-Wall -Wextra -O2 -s"
PROGRAM=freebsd-info

build() {
	[ ! "$(command -v cc)" ] && {
		echo "Error: No C compiler was found"
		exit 1
	}

	cc $FLAGS "$PROGRAM.c" -o $PROGRAM
	sed -i "" "s/@@#REVISION#@@/0.1/" "$PROGRAM.c"
}

build "$@"
