#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: $0 [vlab hostname]"
	exit 1
fi

PORT=8148
ssh -L $PORT:$1:$PORT stargate
