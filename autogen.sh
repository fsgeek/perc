#!/bin/bash

set -e

if [ -z "$AUTOMAKE" ]
then
	AUTOMAKE=automake
fi

AUTOMAKE="$AUTOMAKE --foreign" autoreconf -vif

echo
echo "To build here, run:"
echo "  ./configure"
echo "  make"
echo
