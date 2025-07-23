#!/bin/bash

STUFF=("300" "400" "500")
for i in "${STUFF[@]}"; do
    ./ep4.sh $i 5 10 15 20 25 30 35
done
