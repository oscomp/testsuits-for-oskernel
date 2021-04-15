#!/bin/sh

echo '========== START test_yield =========='

./yield_A &
./yield_B &
./yield_C &
wait

echo '========== END test_yield =========='
