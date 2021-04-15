#!/bin/sh

brctl showstp virbr0 > /dev/null 2>&1 || brctl addbr virbr0
ifconfig virbr0 up
ifconfig virbr0 192.168.100.1
brctl addif virbr0 $1
ifconfig $1 up
