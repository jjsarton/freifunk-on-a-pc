#!/bin/bash

PATH=$DIR/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin/
cd $DIR

source ./bin/conf.sh
source ./bin/functions.sh
source ./bin/glob.sh

net_config()
{
	sysctl net.ipv4.conf.all.forwarding=1
	sysctl net.ipv4.ip_forward=1
	sysctl net.ipv6.conf.br-wan.autoconf=0
	sysctl net.ipv6.conf.local-node.autoconf=0
	sysctl net.ipv6.conf.default.use_tempaddr=0
	sysctl net.ipv6.conf.all.use_tempaddr=0
}

makeHostapdConf()
{
cat <<!
ctrl_interface=/var/run/hostapd
ctrl_interface_group=wheel
macaddr_acl=0
auth_algs=1
driver=nl80211
preamble=1
ignore_broadcast_ssid=0
wpa=0
interface=$WIFI
bridge=br-client
country_code=DE
hw_mode=g
ieee80211n=1
wmm_enabled=1
ieee80211d=1
ht_capab=[HT20][SHORT-GI-20]
channel=$WIFI_CHANNEL
ssid=$SSID
!
}

filter()
{
	ebtables -t filter -N IN_ONLY
	ebtables -t filter -N OUT_ONLY
	ebtables -t filter -N MULTICAST_OUT
	ebtables -t filter -A INPUT -p IPv4 --ip-proto udp --ip-dport 68 -j IN_ONLY
	ebtables -t filter -A INPUT -p IPv6 --ip6-proto udp --ip6-dport 546 -j IN_ONLY
	ebtables -t filter -A INPUT -p IPv6 --ip6-proto ipv6-icmp --ip6-icmp-type router-advertisement -j IN_ONLY
	ebtables -t filter -A INPUT -p IPv6 -i bat0 --ip6-proto ipv6-icmp --ip6-icmp-type router-solicitation -j DROP 
	ebtables -t filter -A FORWARD -p IPv4 --ip-proto udp --ip-dport 67 -j OUT_ONLY
	ebtables -t filter -A FORWARD -p IPv4 --ip-proto udp --ip-dport 68 -j IN_ONLY
	ebtables -t filter -A FORWARD -p IPv6 --ip6-proto udp --ip6-dport 547 -j OUT_ONLY
	ebtables -t filter -A FORWARD -p IPv6 --ip6-proto udp --ip6-dport 546 -j IN_ONLY
	ebtables -t filter -A FORWARD -p IPv6 --ip6-proto ipv6-icmp --ip6-icmp-type router-solicitation -j OUT_ONLY
	ebtables -t filter -A FORWARD -p IPv6 --ip6-proto ipv6-icmp --ip6-icmp-type router-advertisement -j IN_ONLY
	ebtables -t filter -A FORWARD -p ARP --logical-in br-client --arp-ip-src $LOCAL_IPV4 -j DROP 
	ebtables -t filter -A FORWARD -p ARP --logical-in br-client --arp-ip-dst $LOCAL_IPV4 -j DROP 
	ebtables -t filter -A FORWARD -d 8:b8:7b:cb:ff:2 --logical-out br-client -o bat0 -j DROP 
	ebtables -t filter -A FORWARD -s 8:b8:7b:cb:ff:2 --logical-out br-client -o bat0 -j DROP 
	ebtables -t filter -A FORWARD -p IPv4 --logical-out br-client -o bat0 --ip-dst $LOCAL_IPV4 -j DROP 
	ebtables -t filter -A FORWARD -p IPv4 --logical-out br-client -o bat0 --ip-src $LOCAL_IPV4 -j DROP 
	ebtables -t filter -A FORWARD -p IPv6 --logical-out br-client -o bat0 --ip6-dst $LOCAL_IPV6 -j DROP 
	ebtables -t filter -A FORWARD -p IPv6 --logical-out br-client -o bat0 --ip6-src $LOCAL_IPV6 -j DROP 
	ebtables -t filter -A FORWARD -d Multicast --logical-out br-client -o bat0 -j MULTICAST_OUT
	ebtables -t filter -A OUTPUT -p IPv4 --ip-proto udp --ip-dport 67 -j OUT_ONLY
	ebtables -t filter -A OUTPUT -p IPv6 --ip6-proto udp --ip6-dport 547 -j OUT_ONLY
	ebtables -t filter -A OUTPUT -p IPv6 --ip6-proto ipv6-icmp --ip6-icmp-type router-solicitation -j OUT_ONLY
	ebtables -t filter -A OUTPUT -d 8:b8:7b:cb:ff:2 --logical-out br-client -o bat0 -j DROP 
	ebtables -t filter -A OUTPUT -s 8:b8:7b:cb:ff:2 --logical-out br-client -o bat0 -j DROP 
	ebtables -t filter -A OUTPUT -p IPv4 --logical-out br-client -o bat0 --ip-dst $LOCAL_IPV4 -j DROP 
	ebtables -t filter -A OUTPUT -p IPv4 --logical-out br-client -o bat0 --ip-src $LOCAL_IPV4 -j DROP 
	ebtables -t filter -A OUTPUT -p IPv6 --logical-out br-client -o bat0 --ip6-dst $LOCAL_IPV6 -j DROP 
	ebtables -t filter -A OUTPUT -p IPv6 --logical-out br-client -o bat0 --ip6-src $LOCAL_IPV6 -j DROP 
	ebtables -t filter -A OUTPUT -d Multicast --logical-out br-client -o bat0 -j MULTICAST_OUT
	ebtables -t filter -A OUTPUT -p IPv6 -o bat0 --ip6-proto ipv6-icmp --ip6-icmp-type router-advertisement -j DROP 
	ebtables -t filter -P IN_ONLY RETURN
	ebtables -t filter -A IN_ONLY -i ! bat0 --logical-in br-client -j DROP 
	ebtables -t filter -P OUT_ONLY RETURN
	ebtables -t filter -A OUT_ONLY --logical-out br-client -o ! bat0 -j DROP 
	ebtables -t filter -P MULTICAST_OUT DROP
	ebtables -t filter -A MULTICAST_OUT -p ARP --arp-op Reply --arp-ip-src 0.0.0.0 -j DROP 
	ebtables -t filter -A MULTICAST_OUT -p ARP --arp-op Request --arp-ip-dst 0.0.0.0 -j DROP 
	ebtables -t filter -A MULTICAST_OUT -p ARP -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto udp --ip6-dport 6696 -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv4 --ip-dst 239.192.152.143 --ip-proto udp --ip-dport 6771 -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv4 --ip-proto udp --ip-dport 67 -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto udp --ip6-dport 547 -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto ipv6-icmp --ip6-icmp-type echo-request -j DROP 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto ipv6-icmp --ip6-icmp-type 139/0:255 -j DROP 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto ipv6-icmp -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto ip -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv4 --ip-proto igmp -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv4 --ip-proto ospf -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-proto ospf -j RETURN 
	ebtables -t filter -A MULTICAST_OUT -p IPv6 --ip6-dst ff02::9/128 --ip6-proto udp --ip6-dport 521 -j RETURN 
	
	iptables -P INPUT ACCEPT
	iptables -P FORWARD DROP
	iptables -P OUTPUT ACCEPT
	iptables -N delegate_forward
	iptables -N delegate_input
	iptables -N delegate_output
	iptables -N forwarding_client_rule
	iptables -N forwarding_lan_rule
	iptables -N forwarding_local_node_rule
	iptables -N forwarding_rule
	iptables -N forwarding_wan_rule
	iptables -N input_client_rule
	iptables -N input_lan_rule
	iptables -N input_local_node_rule
	iptables -N input_rule
	iptables -N input_wan_rule
	iptables -N output_client_rule
	iptables -N output_lan_rule
	iptables -N output_local_node_rule
	iptables -N output_rule
	iptables -N output_wan_rule
	iptables -N reject
	iptables -N syn_flood
	iptables -N zone_client_dest_ACCEPT
	iptables -N zone_client_dest_REJECT
	iptables -N zone_client_forward
	iptables -N zone_client_input
	iptables -N zone_client_output
	iptables -N zone_client_src_ACCEPT
	iptables -N zone_lan_dest_ACCEPT
	iptables -N zone_lan_forward
	iptables -N zone_lan_input
	iptables -N zone_lan_output
	iptables -N zone_lan_src_ACCEPT
	iptables -N zone_local_node_dest_ACCEPT
	iptables -N zone_local_node_dest_REJECT
	iptables -N zone_local_node_forward
	iptables -N zone_local_node_input
	iptables -N zone_local_node_output
	iptables -N zone_local_node_src_ACCEPT
	iptables -N zone_wan_dest_ACCEPT
	iptables -N zone_wan_dest_REJECT
	iptables -N zone_wan_forward
	iptables -N zone_wan_input
	iptables -N zone_wan_output
	iptables -N zone_wan_src_REJECT
	iptables -A INPUT -j delegate_input
	iptables -A FORWARD -j delegate_forward
	iptables -A OUTPUT -j delegate_output
	iptables -A delegate_forward -m comment --comment "user chain for forwarding" -j forwarding_rule
	iptables -A delegate_forward -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
	iptables -A delegate_forward -i br-wan -j zone_wan_forward
	iptables -A delegate_forward -i br-client -j zone_client_forward
	iptables -A delegate_forward -i local-node -j zone_local_node_forward
	iptables -A delegate_forward -j reject
	iptables -A delegate_input -i lo -j ACCEPT
	iptables -A delegate_input -m comment --comment "user chain for input" -j input_rule
	iptables -A delegate_input -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
	iptables -A delegate_input -p tcp -m tcp --tcp-flags FIN,SYN,RST,ACK SYN -j syn_flood
	iptables -A delegate_input -i br-wan -j zone_wan_input
	iptables -A delegate_input -i br-client -j zone_client_input
	iptables -A delegate_input -i local-node -j zone_local_node_input
	iptables -A delegate_output -o lo -j ACCEPT
	iptables -A delegate_output -m comment --comment "user chain for output" -j output_rule
	iptables -A delegate_output -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
	iptables -A delegate_output -o br-wan -j zone_wan_output
	iptables -A delegate_output -o br-client -j zone_client_output
	iptables -A delegate_output -o local-node -j zone_local_node_output
	iptables -A reject -p tcp -j REJECT --reject-with tcp-reset
	iptables -A reject -j REJECT --reject-with icmp-port-unreachable
	iptables -A syn_flood -p tcp -m tcp --tcp-flags FIN,SYN,RST,ACK SYN -m limit --limit 25/sec --limit-burst 50 -j RETURN
	iptables -A syn_flood -j DROP
	iptables -A zone_client_dest_ACCEPT -o br-client -j ACCEPT
	iptables -A zone_client_dest_REJECT -o br-client -j reject
	iptables -A zone_client_forward -m comment --comment "user chain for forwarding" -j forwarding_client_rule
	iptables -A zone_client_forward -m conntrack --ctstate DNAT -m comment --comment "Accept port forwards" -j ACCEPT
	iptables -A zone_client_forward -j zone_client_dest_REJECT
	iptables -A zone_client_input -m comment --comment "user chain for input" -j input_client_rule
	iptables -A zone_client_input -p tcp -m tcp --dport 53 -m comment --comment client_dns -j reject
	iptables -A zone_client_input -p udp -m udp --dport 53 -m comment --comment client_dns -j reject
	iptables -A zone_client_input -m conntrack --ctstate DNAT -m comment --comment "Accept port redirections" -j ACCEPT
	iptables -A zone_client_input -j zone_client_src_ACCEPT
	iptables -A zone_client_output -m comment --comment "user chain for output" -j output_client_rule
	iptables -A zone_client_output -j zone_client_dest_ACCEPT
	iptables -A zone_client_src_ACCEPT -i br-client -j ACCEPT
	iptables -A zone_lan_forward -m comment --comment "user chain for forwarding" -j forwarding_lan_rule
	iptables -A zone_lan_forward -m comment --comment "forwarding lan -> wan" -j zone_wan_dest_ACCEPT
	iptables -A zone_lan_forward -m conntrack --ctstate DNAT -m comment --comment "Accept port forwards" -j ACCEPT
	iptables -A zone_lan_forward -j zone_lan_dest_ACCEPT
	iptables -A zone_lan_input -m comment --comment "user chain for input" -j input_lan_rule
	iptables -A zone_lan_input -m conntrack --ctstate DNAT -m comment --comment "Accept port redirections" -j ACCEPT
	iptables -A zone_lan_input -j zone_lan_src_ACCEPT
	iptables -A zone_lan_output -m comment --comment "user chain for output" -j output_lan_rule
	iptables -A zone_lan_output -j zone_lan_dest_ACCEPT
	iptables -A zone_local_node_dest_ACCEPT -o local-node -j ACCEPT
	iptables -A zone_local_node_dest_REJECT -o local-node -j reject
	iptables -A zone_local_node_forward -m comment --comment "user chain for forwarding" -j forwarding_local_node_rule
	iptables -A zone_local_node_forward -m conntrack --ctstate DNAT -m comment --comment "Accept port forwards" -j ACCEPT
	iptables -A zone_local_node_forward -j zone_local_node_dest_REJECT
	iptables -A zone_local_node_input -m comment --comment "user chain for input" -j input_local_node_rule
	iptables -A zone_local_node_input -m conntrack --ctstate DNAT -m comment --comment "Accept port redirections" -j ACCEPT
	iptables -A zone_local_node_input -j zone_local_node_src_ACCEPT
	iptables -A zone_local_node_output -m comment --comment "user chain for output" -j output_local_node_rule
	iptables -A zone_local_node_output -j zone_local_node_dest_ACCEPT
	iptables -A zone_local_node_src_ACCEPT -i local-node -j ACCEPT
	iptables -A zone_wan_dest_ACCEPT -o br-wan -j ACCEPT
	iptables -A zone_wan_dest_REJECT -o br-wan -j reject
	iptables -A zone_wan_forward -m comment --comment "user chain for forwarding" -j forwarding_wan_rule
	iptables -A zone_wan_forward -m conntrack --ctstate DNAT -m comment --comment "Accept port forwards" -j ACCEPT
	iptables -A zone_wan_forward -j zone_wan_dest_REJECT
	iptables -A zone_wan_input -m comment --comment "user chain for input" -j input_wan_rule
	iptables -A zone_wan_input -p udp -m udp --dport 68 -m comment --comment Allow-DHCP-Renew -j ACCEPT
	iptables -A zone_wan_input -p icmp -m icmp --icmp-type 8 -m comment --comment Allow-Ping -j ACCEPT
	iptables -A zone_wan_input -p tcp -m tcp --dport 22 -m comment --comment wan_ssh -j ACCEPT
	iptables -A zone_wan_input -m conntrack --ctstate DNAT -m comment --comment "Accept port redirections" -j ACCEPT
	iptables -A zone_wan_input -j zone_wan_src_REJECT
	iptables -A zone_wan_output -m comment --comment "user chain for output" -j output_wan_rule
	iptables -A zone_wan_output -j zone_wan_dest_ACCEPT
	iptables -A zone_wan_src_REJECT -i br-wan -j reject

	ip6tables -P INPUT ACCEPT
	ip6tables -P FORWARD DROP
	ip6tables -P OUTPUT ACCEPT
	ip6tables -N delegate_forward
	ip6tables -N delegate_input
	ip6tables -N delegate_output
	ip6tables -N forwarding_client_rule
	ip6tables -N forwarding_lan_rule
	ip6tables -N forwarding_local_node_rule
	ip6tables -N forwarding_rule
	ip6tables -N forwarding_wan_rule
	ip6tables -N input_client_rule
	ip6tables -N input_lan_rule
	ip6tables -N input_local_node_rule
	ip6tables -N input_rule
	ip6tables -N input_wan_rule
	ip6tables -N output_client_rule
	ip6tables -N output_lan_rule
	ip6tables -N output_local_node_rule
	ip6tables -N output_rule
	ip6tables -N output_wan_rule
	ip6tables -N reject
	ip6tables -N syn_flood
	ip6tables -N zone_client_dest_ACCEPT
	ip6tables -N zone_client_dest_REJECT
	ip6tables -N zone_client_forward
	ip6tables -N zone_client_input
	ip6tables -N zone_client_output
	ip6tables -N zone_client_src_ACCEPT
	ip6tables -N zone_lan_dest_ACCEPT
	ip6tables -N zone_lan_forward
	ip6tables -N zone_lan_input
	ip6tables -N zone_lan_output
	ip6tables -N zone_lan_src_ACCEPT
	ip6tables -N zone_local_node_dest_ACCEPT
	ip6tables -N zone_local_node_dest_REJECT
	ip6tables -N zone_local_node_forward
	ip6tables -N zone_local_node_input
	ip6tables -N zone_local_node_output
	ip6tables -N zone_local_node_src_ACCEPT
	ip6tables -N zone_wan_dest_ACCEPT
	ip6tables -N zone_wan_dest_REJECT
	ip6tables -N zone_wan_forward
	ip6tables -N zone_wan_input
	ip6tables -N zone_wan_output
	ip6tables -N zone_wan_src_REJECT
	ip6tables -A INPUT -j delegate_input
	ip6tables -A FORWARD -j delegate_forward
	ip6tables -A OUTPUT -j delegate_output
	ip6tables -A delegate_forward -m comment --comment "user chain for forwarding" -j forwarding_rule
	ip6tables -A delegate_forward -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
	ip6tables -A delegate_forward -i br-wan -j zone_wan_forward
	ip6tables -A delegate_forward -i br-client -j zone_client_forward
	ip6tables -A delegate_forward -i local-node -j zone_local_node_forward
	ip6tables -A delegate_forward -j reject
	ip6tables -A delegate_input -i lo -j ACCEPT
	ip6tables -A delegate_input -m comment --comment "user chain for input" -j input_rule
	ip6tables -A delegate_input -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
	ip6tables -A delegate_input -p tcp -m tcp --tcp-flags FIN,SYN,RST,ACK SYN -j syn_flood
	ip6tables -A delegate_input -i br-wan -j zone_wan_input
	ip6tables -A delegate_input -i br-client -j zone_client_input
	ip6tables -A delegate_input -i local-node -j zone_local_node_input
	ip6tables -A delegate_output -o lo -j ACCEPT
	ip6tables -A delegate_output -m comment --comment "user chain for output" -j output_rule
	ip6tables -A delegate_output -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
	ip6tables -A delegate_output -o br-wan -j zone_wan_output
	ip6tables -A delegate_output -o br-client -j zone_client_output
	ip6tables -A delegate_output -o local-node -j zone_local_node_output
	ip6tables -A reject -p tcp -j REJECT --reject-with tcp-reset
	ip6tables -A reject -j REJECT --reject-with icmp6-port-unreachable
	ip6tables -A syn_flood -p tcp -m tcp --tcp-flags FIN,SYN,RST,ACK SYN -m limit --limit 25/sec --limit-burst 50 -j RETURN
	ip6tables -A syn_flood -j DROP
	ip6tables -A zone_client_dest_ACCEPT -o br-client -j ACCEPT
	ip6tables -A zone_client_dest_REJECT -o br-client -j reject
	ip6tables -A zone_client_forward -m comment --comment "user chain for forwarding" -j forwarding_client_rule
	ip6tables -A zone_client_forward -j zone_client_dest_REJECT
	ip6tables -A zone_client_input -m comment --comment "user chain for input" -j input_client_rule
	ip6tables -A zone_client_input -p tcp -m tcp --dport 53 -m comment --comment client_dns -j reject
	ip6tables -A zone_client_input -p udp -m udp --dport 53 -m comment --comment client_dns -j reject
	ip6tables -A zone_client_input ! -s fe80::/64 -p udp -m udp --dport 1001 -m comment --comment client_respondd -j reject
	ip6tables -A zone_client_input -j zone_client_src_ACCEPT
	ip6tables -A zone_client_output -m comment --comment "user chain for output" -j output_client_rule
	ip6tables -A zone_client_output -j zone_client_dest_ACCEPT
	ip6tables -A zone_client_src_ACCEPT -i br-client -j ACCEPT
	ip6tables -A zone_lan_forward -m comment --comment "user chain for forwarding" -j forwarding_lan_rule
	ip6tables -A zone_lan_forward -m comment --comment "forwarding lan -> wan" -j zone_wan_dest_ACCEPT
	ip6tables -A zone_lan_forward -j zone_lan_dest_ACCEPT
	ip6tables -A zone_lan_input -m comment --comment "user chain for input" -j input_lan_rule
	ip6tables -A zone_lan_input -j zone_lan_src_ACCEPT
	ip6tables -A zone_lan_output -m comment --comment "user chain for output" -j output_lan_rule
	ip6tables -A zone_lan_output -j zone_lan_dest_ACCEPT
	ip6tables -A zone_local_node_dest_ACCEPT -o local-node -j ACCEPT
	ip6tables -A zone_local_node_dest_REJECT -o local-node -j reject
	ip6tables -A zone_local_node_forward -m comment --comment "user chain for forwarding" -j forwarding_local_node_rule
	ip6tables -A zone_local_node_forward -j zone_local_node_dest_REJECT
	ip6tables -A zone_local_node_input -m comment --comment "user chain for input" -j input_local_node_rule
	ip6tables -A zone_local_node_input -j zone_local_node_src_ACCEPT
	ip6tables -A zone_local_node_output -m comment --comment "user chain for output" -j output_local_node_rule
	ip6tables -A zone_local_node_output -j zone_local_node_dest_ACCEPT
	ip6tables -A zone_local_node_src_ACCEPT -i local-node -j ACCEPT
	ip6tables -A zone_wan_dest_ACCEPT -o br-wan -j ACCEPT
	ip6tables -A zone_wan_dest_REJECT -o br-wan -j reject
	ip6tables -A zone_wan_forward -m comment --comment "user chain for forwarding" -j forwarding_wan_rule
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 128 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 129 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 1 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 2 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 3 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 4/0 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -p ipv6-icmp -m icmp6 --icmpv6-type 4/1 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Forward -j ACCEPT
	ip6tables -A zone_wan_forward -j zone_wan_dest_REJECT
	ip6tables -A zone_wan_input -m comment --comment "user chain for input" -j input_wan_rule
	ip6tables -A zone_wan_input -s fe80::/10 -d fe80::/10 -p udp -m udp --sport 547 --dport 546 -m comment --comment Allow-DHCPv6 -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 128 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 129 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 1 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 2 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 3 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 4/0 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 4/1 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 133 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 135 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 134 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p ipv6-icmp -m icmp6 --icmpv6-type 136 -m limit --limit 1000/sec -m comment --comment Allow-ICMPv6-Input -j ACCEPT
	ip6tables -A zone_wan_input -p tcp -m tcp --dport 22 -m comment --comment wan_ssh -j ACCEPT
	ip6tables -A zone_wan_input -s fe80::/64 -p udp -m udp --sport 1001 --dport 32768:61000 -m comment --comment wan_respondd_reply -j ACCEPT
	ip6tables -A zone_wan_input -s fe80::/64 -p udp -m udp --dport 1001 -m comment --comment wan_respondd -j ACCEPT
	ip6tables -A zone_wan_input -j zone_wan_src_REJECT
	ip6tables -A zone_wan_output -m comment --comment "user chain for output" -j output_wan_rule
	ip6tables -A zone_wan_output -j zone_wan_dest_ACCEPT
	ip6tables -A zone_wan_src_REJECT -i br-wan -j reject
	
}

advert()
{
	sleep 5
	while [ true ]
	do
		gluon-neighbour-info -d ::1 -p 1001 -t 1 -c 1 -r nodeinfo   | gzip | alfred -s 158
		gluon-neighbour-info -d ::1 -p 1001 -t 1 -c 1 -r statistics | gzip | alfred -s 159
		gluon-neighbour-info -d ::1 -p 1001 -t 1 -c 1 -r neighbours | gzip | alfred -s 160
		sleep 1200
	done
}

start()
{
	# Create our dridge
	createBridge br-wan $MAC_BRW "$IPV4_SUBNET.2/24" "${PREFIX}2/64 " eth0 $MAC_ETH
	# set route for fastd
	$DIR/bin/setFastdRoute.sh

	# build iptables and ebtables
	filter
	createBridge br-client $MAC_BRC "" ""

	# build hostapd.conf
	makeHostapdConf > $DIR/etc/hostapd.conf

	# Create the MESH interface
	if [ "$WIFI" != "" ]
	then
		iw dev $WIFI interface add mesh0 type mesh mesh_id $MESHID
		ip l set dev $WIFI addr $MAC_AP mtu 1532
		ip l set dev mesh0 addr $MAC_MP mtu 1532
		iw dev mesh0 set channel 11 HT20
		iw dev mesh0 set meshid $MESHID

      	 	# Start the AP and add our client to the vr-client vridge
		hostapd -dd -B -P $DIR/tmp/hostad-$WIFI.pid $DIR/etc/hostapd.conf 

       		# finish configuration for our mesh interface
		ip l set dev mesh0 up
		iw dev mesh0 set mesh_param mesh_fwding=0
		iw dev mesh0 set mesh_param mesh_hwmp_rootmode=1
		iw dev mesh0 set mesh_param mesh_gate_announcements=1
	fi > /dev/null 2>&1

	if [ "$ETH" != "" ]
	then
		if [ "$MESH_ON_LAN" = "1" ]
		then
			ip l set dev $ETH address $MAC_LAN
		else
			ip l set dev $ETH address $MAC
		fi
	fi

        # Start fastd	
	fastd -c $DIR/etc/fastd.conf --on-up $DIR/bin/setMeshVpnUp.sh -d 

        # Set BAT0
	if [ "$WIFI" != "" ]
	then
		batctl if add mesh0
	fi
	if [ "$ETH" != "" ]
	then
		if [ "$MESH_ON_LAN" = "1" ]
		then
			batctl if add $ETH
		else
			ip l set $ETH master br-client
		fi
		ip l set dev $ETH up
	fi
	batctl gw client
	batctl multicast_mode disable
	batctl it 5000
	
	ip l set dev bat0 address $MAC_BAT0
	ip l set dev bat0 master br-client

	# create macvlan not necessary but our node have it
	ip link add  link br-client local-node type macvlan
	net_config >/dev/null


	ip link set local-node address 08:b8:7b:cb:ff:02
	ip a a $LOCAL_IPV4 dev local-node
	ip a a $LOCAL_IPV6 dev local-node
	ip r a $FFRO_IPV6 dev br-client


	ip l set dev br-client up
	ip link set local-node up
	
	ip l set dev bat0 up
	sleep 3
	# mount debugfs, this is not done by ip netns
	mount -t debugfs | grep debugfs &> /dev/null || mount -t debugfs none /sys/kernel/debug

	if [ "$USE_DHCP" = "1" ]
	then
		dhcpcd -h $HOSTNAME -L -b -e NETNS=$NETNS br-client 
	fi

	alfred -i br-client -b bat0 -u /var/run/alfred.sock > /dev/null 2>&1 &
	sleep 3
	chmod 666 /var/run/alfred.sock

 	batadv-vis -i bat0 -s -u /var/run/alfred.sock > /dev/null 2>&1 &
	if [ "$PROVIDE_HTTP" = "1" ]
	then
		thttpd -C $DIR/etc/thttpd.conf -nor 
	fi
	$DIR/bin/sse-multiplexd &
	$DIR/bin/respondd -d $DIR/etc/respondd.d -p 1001 -g ff02::2:1001 -i br-client -i mesh-vpn -i mesh0 &
	sleep 1
	chmod 766 /var/run/sse-multiplex.sock
	advert &
}

stop()
{
	ip link del link br-client local-node type macvlan
	delBridge br-wan
	delBridge br-client

	if [ "$WIFI" != "" ]
	then
		ip l set $WIFI down
		ip l set mesh0 down
		iw dev mesh0 del
	fi	
	if [ "$ETH" != "" ]
	then
		ip l set $ETH down
	fi
	rm /var/run/alfred.sock
}

case $1 in
start)
	start
;;
stop)
	stop
;;
esac
