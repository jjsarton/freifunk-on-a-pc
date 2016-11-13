# set fastd interface
ip l set address $MAC_VPN dev mesh-vpn;
batctl if add mesh-vpn;
ip l set up dev mesh-vpn;
