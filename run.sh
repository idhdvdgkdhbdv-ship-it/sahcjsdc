#!/bin/bash
rm -f p1.log p2.log p3.log

./generate_random_sets $1 $2 &

sleep 1

./P1_private_set_intersection > p1.log &
./P2_private_set_intersection > p2.log &
./Q_private_set_intersection > p3.log &

wait

echo "=== P1 Log ==="
cat p1.log
echo ""
echo "=== P2 Log ==="
cat p2.log
echo ""
echo "=== Q Log ==="
cat p3.log
echo ""
echo "Done"
