#!/bin/bash

PHY_DEV="ens33"
TUN_DEV="tun0"
TUN_IP="10.0.0.1"
TUN_PEER_IP="10.0.0.2"


echo 1 > /proc/sys/net/ipv4/ip_forward
echo 1 > /proc/sys/net/ipv6/conf/all/forwarding
echo 1 > /proc/sys/net/ipv6/conf/default/forwarding
ifconfig $TUN_DEV $TUN_IP pointopoint $TUN_PEER_IP up


iptables -t nat -F POSTROUTING
iptables -F FORWARD

iptables -P FORWARD ACCEPT
iptables -A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -t nat -A POSTROUTING -s $TUN_PEER_IP -o $PHY_DEV -j MASQUERADE

iptables -t mangle -A FORWARD -p tcp -m tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

echo "forward success!"