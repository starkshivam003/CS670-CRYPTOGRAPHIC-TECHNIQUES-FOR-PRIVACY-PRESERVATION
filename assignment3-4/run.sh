#!/bin/bash
set -e

echo "[1] Building..."
make clean
make

echo "[2] Running unit test..."
./test_protocol

echo "[3] Running benchmark..."
./bench --items 1024 --dim 4 --runs 10 > outputs.csv

echo "Done. Results saved to outputs.csv"
