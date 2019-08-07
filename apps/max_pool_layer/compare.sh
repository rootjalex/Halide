#!/bin/sh

STRIDE="--stride 4"
EXTENT="--extent 4"
WIDTH="--width 1024"
HEIGHT="--height 1024"
CHANNELS="--channels 3"
NIMAGES="--nImages 100"
ITERATIONS="--iterations 10"

make
echo "\nTENSORFLOW:"
python3 flow_max_pool.py $STRIDE $EXTENT $WIDTH $HEIGHT $CHANNELS $NIMAGES $ITERATIONS

echo "\nHALIDE:"
./bin/host/process $STRIDE $EXTENT $WIDTH $HEIGHT $CHANNELS $NIMAGES $ITERATIONS

echo "\nPYTORCH:"
python3 torch_max_pool.py $STRIDE $EXTENT $WIDTH $HEIGHT $CHANNELS $NIMAGES $ITERATIONS

