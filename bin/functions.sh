#!/bin/sh

getMyIp()
{
	ip -4 a sh $1 | sed -n -e 's/.*inet \(.*\)\/.*/\1/p' 
}

GetWifiMac()
{
	iw dev $1 info | sed -n -e 's/.*addr \(.*\)/\1/p'
}

getWifiPhy()
{
	iw dev | tr '#' ' ' | \
	awk  'BEGIN {
		phy="";
		mac="";
		interface = 0;
	}
	{
		if ( $1 == "phy" ) {
			phy=$2;
			interface = 0;
		}
		if ($1 == "Interface" && $2 =="'$1'" ) {
			interface = 1;
		}
		if (interface == 1 && $1 == "addr" ) {
			printf("phy%d\n", phy);
			exit;
		}
	}'
}
#getPrefix()
#{
#        # Auf eine FritzBox muss ein Prefix Delegation 
#	# angefordert werden!
#	dhclient -6 -P $1 -pf /tmp/dhclient-$1.pid | sed 's@.*=\(.*\)/.*@\1@'
#}

buildRadvdConfLow()
{
	# Rarameter:
	# $1 If name
	# $2 route prefix
	# $3 name server
	# $4 name of configuration file
	echo "interface $1 {
    IgnoreIfMissing on;
    AdvSendAdvert on;
    MinRtrAdvInterval 30;
    MaxRtrAdvInterval 60;
    AdvDefaultPreference low;
    UnicastOnly on;
    prefix ::/64
    {
	   AdvOnLink on;
	   AdvAutonomous on;
	   AdvRouterAddr on;
    };

    route $2
    {
        AdvRoutePreference low;
    };
    RDNSS $3 {};
};" > $4
chmod 640 $4
chown root:radvd $4

}

createBridge()
{
	name=$1
	macb=$2
	bip4=$3
	bip6=$4
	shift 4
	ip l add $name type bridge
	if [ "$macb" != "" ]; then ip l set $name address $macb; fi
	while [ $# -ge 1 ]
	do
		slave=$1
		macs=$2
		ip l set $slave down
		ip l set $slave address $macs
		if [ "$mac" != "" ]
		then
			ip l set $slave address $mac
		fi
		ip l set $slave master $name
		ip l set dev $slave up
		shift
		shift
	done
	
	ip l set dev $name up
	if [ "$bip4" != "" ]
	then
		ip a a $bip4 dev $name
	fi 
	if [ "$bip6" != "" ]
	then
		ip a a $bip6 dev $name
	fi 
	ip l set dev $name up
}

delBridge()
{
	name=$1
	ip l set dev $name down
	ip link show master $name | grep  $name | while read x dev rest
	do
		dev=`echo $dev | sed 's/@.*//'`
		if [ "$dev" != "" ]
		then
			ip l set dev $dev down
			ip l set dev $dev nomaster
		fi
	done
	ip l del $name type bridge
}

createHostMesh()
{
	host=$1
	mesh=$2
	meshid=$3
	mmac=$4

	iw dev $WIFI interface add mesh0 type mesh mesh_id $MESHID
	ip l set dev mesh0 addr $MAC_WIF mtu 1532
	iw dev mesh0 set channel 11 HT20
	iw dev mesh0 set meshid $MESHID

	hostapd -dd -B -P /tmp/hostad-$WIFI.pid /tmp/hostapd-$WIFI.conf 

	ip l set dev mesh0 up
	iw dev mesh0 set mesh_param mesh_fwding=0
	iw dev mesh0 set mesh_param mesh_hwmp_rootmode=1
	iw dev mesh0 set mesh_param mesh_gate_announcements=1
	
}

delMesh()
{
	mesh=$1
	iw dev $mesh del
}

case $1 in
createBridge) echo $@;
	func=$1
	shift 
	$func "$@"
;;
esac
