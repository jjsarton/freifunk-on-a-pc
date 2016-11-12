#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "You must run this script as root."
   exit 1
fi

# prepend the search path whith or location.
# we expect all files and directories to be relative to thia $DIR

b=$(basename $0)
w=$(which $b 2>/dev/null)
if [ "$?" = "0" ]
then
	DIR=$(dirname $w)
else
	cd "$(dirname $0)"
	DIR="$(pwd)"
fi
unset b
unset w

DIR="$(dirname $DIR)";

PATH=$DIR/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin/

source $DIR/bin/conf.sh
source $DIR/bin/functions.sh

start1()
{
	# test if USE_IPV4 an USE_IPV6 are set as expected
	case "$USE_IPV4$USE_IPV6" in
	01|10|11) : ;;
	*)	{
		echo Variable USE_IPv4 and USE_IPv6 must be set to 0 or 1
		echo at leas one mus have the value 1.
		} 2> /dev/tty
		exit 1
		;;
	esac

	rm -f $DIR/tmp/*

	if [ "$USE_IPV6" = "1" ]
	then
		DNS=$(sed -n 's/nameserver[ \t]*\(.*:.*\)/\1/p' /etc/resolv.conf | tail -1 )
	else
		DNS=$(sed -n 's/nameserver[ \t]*\(.*\...*\)/\1/p' /etc/resolv.conf | tail -1 )
	fi
	
	modprobe batman_adv
	# create veth device and configure the  services for
	# the normal environment
	if [ "$USE_IPV4" = "1" ]
	then
		sysctl net.ipv4.ip_forward=1 >/dev/null
	fi

	if [ "$USE_IPV6" = "1" ]
	then
		sysctl net.ipv6.conf.all.forwarding=1 >/dev/null
		PREFIX=$(dhclient -6 -P $EXTIF -pf $DIR/tmp/dhclient-$NETNS-$EXTIF.pid 2> /dev/null | sed 's@.*=\(.*\)@\1@')
		if [ "$PREFIX" = "" ]
		then
			# duclient allread running ?
			PREFIX=$(grep iaprefix /var/lib/dhclient/dhclient6.leases |tail -1 | sed 's/.* \(.*\) .*/\1/')
		fi
		if [ "$PREFIX" != "" ]
		then
			# I get a /62 prefix from my fritzbox so and want the second availabe prefix
			PREFIX=$(selectPrefix  $PREFIX 1)
		fi
	fi

	MAC=$(ethtool -P $MAIN_IF | awk '{print $3}')
	PHY=$(getWifiPhy $WIFI)

	MAC_BRW=$(calc_mac  $MAC 1 0) 
	MAC_ETH=$MAC
	MAC_BRC=$MAC
	MAC_BAT0=$MAC
	MAC_VPN=$(calc_mac  $MAC 4 0) 
	MAC_MP=$(calc_mac   $MAC 5 1)
	MAC_ETHM=$(calc_mac $MAC 5 2)
	MAC_AP=$(calc_mac   $MAC 2 1)
	MAC_LAN=$(calc_mac  $MAC 1 1)
	
	cat > $DIR/bin/glob.sh <<!
export IPV4_SUBNET=$IPV4_SUBNET
export USE_IPV6=$USE_IPV6
export USE_IPV4=$USE_IPV4
export USE_BATCTL=$USE_BATCTL
export PROVIDE_HTTP=$PROVIDE_HTTP
export MAIN_IF=$MAIN_IF
export SSID=$SSID
export MESHID=$MESHID
export WIFI=$WIFI
export HOSTNAME=$HOSTNAME
export DIR=$DIR
export MESH_ON_LAN=$MESH_ON_LAN
export PATH=$PATH
export PREFIX=$PREFIX
export MAC=$MAC
export PHY=$PHY
export MAC_BRW=$MAC_BRW
export MAC_ETH=$MAC_ETH
export MAC_ETHM=$MAC_ETHM
export MAC_BRC=$MAC_BRC
export MAC_BAT0=$MAC_BAT0
export MAC_VPN=$MAC_VPN
export MAC_MP=$MAC_MP
export MAC_AP=$MAC_AP
export MAC_LAN=$MAC_LAN
export USE_DHCP=$USE_DHCP
export ETH=$ETH
export LOCAL_IPV4=$LOCAL_IPV4
export LOCAL_IPV6=$LOCAL_IPV6
export FFRO_IPV6=$FFRO_IPV6
export MESHID=$MESHID
export SSID=$SSID
export WIFI_CHANNEL=$WIFI_CHANNEL
export NETNS=$NETNS
export ROLE=$ROLE
export SITE=$SITE
!

	chmod +x $DIR/bin/glob.sh

	if [ "$PREFIX" != "" ]
	then
		buildRadvdConfLow veth0 ::/0 $DNS $DIR/etc/radvd-veth0.conf
	fi

	ip link add dev veth0 type veth peer name eth0
	ip link set dev veth0 address ce:26:c9:34:ae:90 up
	
	if [ "$PREFIX" != "" ]
	then
		ip a a ${PREFIX}1/64 dev veth0
	fi
	ip a a $IPV4_SUBNET.1/24 dev veth0

	radvd -C $DIR/etc/radvd-veth0.conf -u radvd -p $DIR/tmp/radvd-veth0.pid

	if [ "$USE_IPV4" = "1" ]
	then
		myIP=$(getMyIp $EXTIF)
		iptables -t nat -A POSTROUTING -s $IPV4_SUBNET.0/24 -o $EXTIF -j SNAT --to $myIP
	fi
	if [ "$PROVIDE_HTTP" = "1" ]
	then
		echo "# BEWARE : No empty lines are allowed!
# This section overrides defaults
dir=$DIR/www
user=root
logfile=/var/log/thttpd.log
pidfile=/var/run/thttpd.pid
cgipat=/cgi-bin/**
charset=utf-8" > "$DIR/etc/thttpd.conf"
	fi
}

stop1()
{
	if [ "$USE_IPV4" = "1" ]
	then
		myIP=$(getMyIp $EXTIF)
		iptables -t nat -D POSTROUTING -s $IPV4_SUBNET.0/24 -o $EXTIF -j SNAT --to $myIP
		sysctl net.ipv4.ip_forward=1
	fi
	if [ "$USE_IPV6" = "1" ]
	then
		: #sysctl net.ipv6.conf.all.forwarding=0
		kill $(cat $DIR/tmp/radvd-veth0.pid)
	fi

	if [ -e $DIR/tmp/dhclient-$NETNS-$EXTIF.pid ]
	then
		# an other stuff is running !
		kill $(cat $DIR/tmp/dhclient-$NETNS-$EXTIF.pid)
		rm $DIR/tmp/dhclient-$NETNS-$EXTIF.pid
	fi
	ip link set dev veth0 down
	ip link del dev veth0 type veth peer name eth0
	sleep 1
	if [ "$WIFI" != "" ]
	then
		orgMac=$(ethtool -P $WIFI | awk '{print $3}')
		ip l set $WIFI down
		ip l set $WIFI address $orgMac
	fi
	if [ "$ETH" != "" ]
	then
		orgMac=$(ethtool -P $ETH | awk '{print $3}')
		ip l set $ETH down
		ip l set $ETH address $orgMac
	fi	
}

start2()
{
	# set route for fastd
	> $DIR/bin/setFastdRoute.sh
	if [ "$USE_IPV4" = "1" ]
	then
		for gw in $GW_LIST
		do
			echo "ip r a $(queryHost 4 $gw)/32 via $IPV4_SUBNET.1 dev br-wan"
		done >> $DIR/bin/setFastdRoute.sh
	fi

	if [ "$PREFIX" != "" ]
	then
		for gw in $GW_LIST
		do
			echo "ip r a $(queryHost 6 0.queich.net)/128 dev br-wan"
		done >> $DIR/bin/setFastdRoute.sh
	fi
	chmod +x $DIR/bin/setFastdRoute.sh

	# create  resolv.conf
	mkdir -p /etc/netns/$NETNS/
	rm  -f /etc/netns/$NETNS/resolv.conf
	cp /etc/resolv.conf /etc/netns/$NETNS/resolv.conf

	# create the ff net space
	ip netns add $NETNS

	# move some device to the network space
	ip link set eth0 netns $NETNS
	ip link set $ETH netns $NETNS
	ip link set $WIFI down
	iw phy $PHY set netns name $NETNS
	ip netns exec $NETNS ip link set lo up
	
	ip netns exec $NETNS ip link set eth0 up
	if [ "$MESH_ON_LAN" = "1" ]
	then
		ip netns exec $NETNS ip link set $ETH up
	fi
	ip netns exec $NETNS bash bldff.sh start
}

stop2()
{
	ip netns pids $NETNS | while read pid;do kill $pid;done
	ip netns exec $NETNS bash bldff.sh stop
	ip netns del $NETNS
}

case $1 in
sh)
	shift
	if ! ip netns | grep $NETNS >/dev/null
	then
		echo $NETNS not available
		exit 1
	else
		source ./glob.sh
		PS1="$NETNS # " ip netns exec $NETNS bash
	fi
;;
start)
	start1
	sleep 2
	start2
;;
stop)
	stop2
	stop1
;;
esac
