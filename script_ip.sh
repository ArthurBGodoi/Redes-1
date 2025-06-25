#!/bin/bash

sudo ip addr flush dev enp1s0
sudo ip addr add 10.0.0.$1/24 dev enp1s0
sudo ip link set enp1s0 up

echo "IP configurado com sucesso"
