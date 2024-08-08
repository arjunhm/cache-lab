#!/bin/bash

make format && make cache
./csim-ref -s 1 -E 1 -b 1 -t traces/yi2.trace
./csim -s 1 -E 1 -b 1 -t traces/yi2.trace
