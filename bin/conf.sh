# for our connection to the internet
EXTIF=wlp5s0

# If we have DOC-SYS we have not really an IPV4 Address so we
# shall not use IPV4 for connection to the
USE_IPV4=1                     # allow IPV4 for connection to the internet

# We may havr internal IPV6 but not atrue IPv6 connnection
USE_IPV6=1                     # allow IPV6 for connection to the internet 
IPV6_PREFIX_NUMBER=1

# the subnet we use for connection to the internet via NAT
IPV4_SUBNET=172.16.1

MAIN=WIFI

# Name of the networ space we use
NETNS=ffnet

# set the following to 1 if you want
# to be able to reach the internet within
# the freifunk name space on this pc.
USE_DHCP=0
PROVIDE_HTTP=1

# The FF Node interfaces we use
WIFI=wlp0s20u1
ETH=enp0s20u4

if [ "$MAIN" == WIFI ]
then
	MAIN_IF=$WIFI
	HOSTNAME=True-Linux
	ROLE=node
else
	MAIN_IF=$ETH
	HOSTNAME=Linux-Offloader
	ROLE=offloader
fi

# configuration for the router/offloader itself

MESH_ON_LAN=0
MESHID=04:ff:37:d0:5f:5b
SSID=suedpfalz.freifunk.net  # network
SITE=ffld
LOCAL_IPV4=10.215.0.1/32
LOCAL_IPV6=2a03:2260:100a::1/128
WIFI_CHANNEL=11

GW_LIST="0.queich.net 1.queich.net"

# We can use dhcp on the client.
USE_DHCP=0
# force using of batctl
USE_BACTL=true

#MESHID=04:ff:37:d0:5f:5c     # no collision with normal mesh
#SSID=suedpfalz.freifunkB.net  # allow client connection to test
