#!/bin/sh

echo "Building annotate..."
make install_inc
make "$@" depend && make "$@" && make install
