#!/bin/sh

STRIDE="--stride 2"
EXTENT="--extent 2"
WIDTH="--width 5048"
HEIGHT="--height 5048"
CHANNELS="--channels 3"
NIMAGES="--nImages 10"
ITERATIONS="--iterations 10"

make
echo "\nTENSORFLOW:"
python3 flow_max_pool.py $STRIDE $EXTENT $WIDTH $HEIGHT $CHANNELS $NIMAGES $ITERATIONS

echo "\nHALIDE:"
./bin/host/process $STRIDE $EXTENT $WIDTH $HEIGHT $CHANNELS $NIMAGES $ITERATIONS

echo "\nPYTORCH:"
python3 torch_max_pool.py $STRIDE $EXTENT $WIDTH $HEIGHT $CHANNELS $NIMAGES $ITERATIONS

