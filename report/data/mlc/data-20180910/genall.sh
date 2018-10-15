#!/bin/bash
for f in "$@"
do
	./generate_plot.sh "$f" &
done
