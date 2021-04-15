#!/bin/sh

ifconfig $1 down
brctl delif virbr0 $1
ifconfig virbr0 down
