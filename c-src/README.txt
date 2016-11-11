These File are for my tests with an offloader or a freifunk router
working on a normal Linux distribution.

Some file used within gluon work well, other are specific to the
gluon worlfs and can not be easely compiled un der a normal Linux
system.

As far as possible I used the files from gluon. The respondd and
signal file from gluon need the special gluon environment, so I
have wrote my own counterpart.

Signal call the iw utility and build the data stream needed for
the meshviewerr communication with the browser.

In order to build the various json files send via alfred or via
the other components involved in the communication, I use a very
different approach. The datas are build according to a template file
which is basically a modified json file. 

Some parts from the data's are static other are to be calculated
and put into the file. This occur by calling dedicated functions.
For example a statistic template file look as follow:

-----------------------------------
{
    "traffic": {
    	@traffic(bat0)
    },
    "memory": {
    	@memory()
    },
    "uptime": @uptime(),
    "loadavg": @loadavg(),
    "processes": {
    	@processes()
    }
}
------------------------------------

This approach allow me to make a new layout with few changes.
Adding some datas is easier as with fully hardcoded software.
My test environemt is build via shell scripts. In order to
get the wanded configuration with correct values and to
start the Freifunk part within an own network name space
a configurations script is necessary. This script may also
be read bei mkjson/respondd.  The configurationsscrip may
conaint comment line (beginning with '#', Assignement
as VAR = 'this is the value' # and this a cpmment or
VAR="value" or VAR=value # this is an other comment.
The assignement may also be preceded by the magic word export
which may be relevant for subprocesses. 

The functions are recognized by the "@" character. Adding comment
to the template is very easy, yout can insert for example:
@comment( This is an important comment ).
 
If a called function is unknown, this part will be eliminated.
          
The predecessor of respondd ist mkjson which is used for the development.
In order to work as respondd I have simply borrowed 4 functions from the
original sources and modified them a little bit.

---------------------------------------------------------------------------
[user@pc mkjson]$ mkjson -h
Usage: mkjson -t <json-template> [-c <conf-file>] [-d <path>] [-b]

	-t json template
	-c configuration file (shell script
	-d path where the templates and scripts are located
	-b human readable output

	Buildin functions:

		Output    function
		--------- -----------------------------------------------
		number    kern(file)
		delimited dkern(file)
		number    nproc()
		delimited uname(sysname|nodename|release|version|machine)
		delimited mac(interface)
		list      macList(interface interface...)
		list      allIPv6(interface)
		list      stringList(word1 word2 ...)
		delimited nodeid(interface)
		number    var(Variable_name ...)
		delimited dvar(Variable_name ...)
		no output addVar(variable_name value)
		no output readConf(filename)
		number    uptime()
		number    exec(script_name)
		number    idletime()
		number    loadavg()
		number    fsusage()
		block     memory()
		block     processes()
		delimited lsbRelease(-i|-r|...)
		number    include(filename)
		block     fastdConnection(socket_name)
		delimited gateway(interface)
		block     batadv(interface)
		block     wifi(interface interface...)
		block     traffic(interface)
		block     printVars()

---------------------------------------------------------------------------

This output show the build in functions and the expected parameters.
One function seem not to be very usefull stringList(word1 word2) will
return "word1","word2". This can be writted directly within the template.
The reason for the function is that variable can be passed to all functions
stringList("$VAR1 $VAR2) will produce "value for variable VAR1","value2"
The function printVars() produce a json output from the known variable.

The offloader / router I use are build with shell scripts. Some Variables
are needed in order to get a working system. Accordingly mkjson can
read the shell script defining all needed variable. printVars will
translate them to a json representation.

With the function @readConf() allow you to get the variable contained
within the configurations shell script (usefull while working as
respondd).

@include(file) can include an optional block. If the file exist, his
content will simply be inserted within the output. If we have
-----------------------------------
@include(file),
"nodeid": "may_node_id"
-----------------------------------

we will have a missformed json output. The solitions ist to put the
final ',' within the file or to mark within the template that a comma
shall be added if the file what found.

------------------------------------
@,include(file)
"nodeid": "may_node_id"
------------------------------------

Further provided files

calc_mac     This is a C-file and I use it for calculating the Mac-Adress
             for the various network devices.
	  
queryHost    This is an old little binary which return the IPv6 global Address.
             it print only the Ip Adresse for the queries host.

sations      This file is a replacement for the gluon counterpart.

selectPrefix A little helper for setting a IPv6 prefix

callscript   My answer to sudo. This little program allow to run scripts or
             binaries as root, Protection is done by very restrictif right
	     for the script/programm which shall work as "root" 

The sources include some modified gluon components:
---------------------------------------------------

sse-multiplex
sse-multiplexd
batman-adv-visdata
gluon-neighbour-info
neighbours-batadv
nodeinfo


For licence and author see the corresponding sources
on github.com

BUGS:
If the router/offloader is started, shutting down the PC or
notebook will end in a kernel crash for an unpatched
batman-adv 2016.3!
Please stop the router/offloader before shutting down!
