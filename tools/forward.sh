#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: $0 [vlab hostname]"
	exit 1
fi

PORT=8148
sed "s/vlab00/$1/g" <<< "ssh -L $PORT:vlab00:$PORT stargate"
ssh -L $PORT:$1:$PORT stargate
