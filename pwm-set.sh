#!/bin/zsh

channel=$1
value=$2

if [ -z "$channel" ] || [ -z "$value" ]; then
    echo "Usage $0 channel value" > /dev/stderr
    exit 1
fi

i2cset -y 1 0x40 $((6 + 4*$channel)) 0 0 $(($value & 0xff)) $(($value >> 8)) i
