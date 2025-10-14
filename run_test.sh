#!/bin/bash

# Test script for MPI Average Filter

echo "=== Building MPI Average Filter ==="
make clean
make

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "=== Running with 4 processes ==="
mpirun -np 4 ./AverageFilterKernel Sample4k.pgm 30

if [ $? -eq 0 ]; then
    echo ""
    echo "=== Success! Output saved to filtered.pgm ==="
    ls -lh filtered.pgm
else
    echo ""
    echo "=== Execution failed! ==="
    exit 1
fi

