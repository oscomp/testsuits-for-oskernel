#!/bin/bash
echo "$2/lmbench_all $1 \"\$@\"" > $2/$1
chmod +x $2/$1
