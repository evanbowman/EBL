#!/bin/bash

for filename in ebl/*.test.ebl; do
    echo $filename
    if ! ./ebl-dofile $filename; then
        exit 1
    fi
done

if ! ./ebl-dofile "ebl/mandelbrot.ebl"; then
    exit 1
fi
