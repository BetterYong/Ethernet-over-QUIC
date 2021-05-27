#!/bin/bash

tunctl
tunctl

brctl addbr br0
brctl addif br0 tap0
brctl addif br0 enp2s0

brctl addbr br1
brctl addif br1 tap1
brctl addif br1 enp3s0
brctl show

ifconfig tap0 up
ifconfig tap1 up
ifconfig br1 up 
ifconfig br0 up
ifconfig
