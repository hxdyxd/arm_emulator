#!/bin/bash

PHY_DEV="ens33"
TAP_DEV="tap0"
TAP_IP="10.0.0.1"
TAP_NET="10.0.0.0/24"


echo 1 > /proc/sys/net/ipv4/ip_forward
echo 1 > /proc/sys/net/ipv6/conf/all/forwarding
echo 1 > /proc/sys/net/ipv6/conf/default/forwarding
ifconfig $TAP_DEV $TAP_IP netmask 255.255.255.0


iptables -t nat -F POSTROUTING
iptables -F FORWARD

iptables -P FORWARD ACCEPT
iptables -A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -t nat -A POSTROUTING -s $TAP_NET -o $PHY_DEV -j MASQUERADE

iptables -t mangle -A FORWARD -p tcp -m tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

echo "forward success!"