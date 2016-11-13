Freifunk-on-a-pc

This is a small footprint environment which allow using your Linux PC or notebook as Freifunk router or offloader.

This allow using your system as normally and to provide Freifunk, or to develop and test some features or new components as for example batman-adv, alfred and so on.

Inspecting the network traffic easy, you can launch wireshark within the network space and look at the traffic on te different devices. As root enter the name space with “ip netns exec <name space> bash” and then launch wireshark with “su <user> -c wireshark &”. 

The router / offloader run within a network name space and is therefor separated from the normal working space.  Within the name space all services which are normally announced are unknown, utilities as avahi and so on have no access to the network interfaces pushed or creates within the network name space.

The router / offloader network layout is very similar as the environment found on the router our community. The minor difference is that we don’t use “ip rules” in order to allows fastd to get his DNS and route information from the PC / notebook environment.

Batman-adv and network name space:

Support for name space what introduced partially with the version 2016.3. This version has a little bug, if you shutdown your system while the router / offloader is running, you will get a kernel crash  which end in a loop. There are fixes for but there are not official and you must download the proper revisions from the open-mesh.org site.
2016.4, the next version is corrected but all features are not well tested. With the revision I use, I had not problems.

Some little parts of the environment consists of c programs picked from the gluon sources, other are little utilities from my own which help the scripts spawning the router / offloader. One of my program is an implementation of respondd which is a more flexible as the original and allows configuring the json streams as wanted.

WIFI:

A host AP and an ieee802.11s mesh device will be build if you configure this feature. All WIFI device are not suitable, Atheros based device shall probably work. 

Further Requirement:

You can provide run a dhcp client within the name space, I have chosen dhcpcd. A modified copy of the script 2o-resolv.conf is provided within the directory dhcpcd-hook. The http server I use within the name space is thttpd.
If these programs are not available for your distribution, you must download the sources and compile.
dhccpd: http://roy.marples.name/projects/dhcpcd/home
thttpd: http://www.acme.com/software/thttpd/

The dhclient from isc program is also needed. This package is provided by all distributions.

The choice is based on an easy configuration within the name space.

We use fastd for our communication with the gateways. This suite expect a fastd installation. See: https://fastd.readthedocs.io/en/v17/

Installation:

You may install this anywhere on your PC or notebook, within the home directory or for example under /opt.
Go to the main directory, call “make install”, the binaries will be compiled and installed within our working environment.

Configuration:
 
go to the bin directory and correct the script conf.sh. This script contains the declaration of some variable for connection to the internet and for the community settings.

IPv4 / IPv6 access is dependent of your ISP. You may have only IPv4, IPv4 and IPv6 or only IPv6 and natting for IPv4 on the ISP side (access via television cable).
If you use IPv6, the scripts will launch dhclient in order to get a prefix delegation. I have a FritzBox router which return alway a /62 prefix. The script try to get a prefix delegation and select one of the subnet via the utility program selectPrefix. 

Provide a fastd configuration according to your community requirement. The fast configuration shall be put to the etc directory (not /etc). The on up entry shall not be provided.

Launching:

Go to the bin directory and as root call “./freifunk start”.

Stopping:

Call “./freifunk.sh stop”. All specific services will be killed and the generated virtual network devices will be deleted.

You have also the ability (if you have enough network hardware devices) to launch more router / offloader. Each instance must be started on it own copy of this suite.

lauching a shell within the netns:

./freifunk sh

BUGS:
A lot, no man pages and documentation. Some features are not provided.
If you use the ability to display the node information's on meshviewer, thttpd must be run root. The name of the neighbor station is not displayed, the rest shall work as expected.
