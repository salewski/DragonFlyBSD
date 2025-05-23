changequote({,})dnl
changecom(,)dnl
.\"
.\" Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
.\" All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\" $FreeBSD: src/usr.sbin/ppp/ppp.8.m4,v 1.301.2.1 2002/09/01 02:12:31 brian Exp $
.\" $DragonFly: src/usr.sbin/ppp/ppp.8.m4,v 1.3 2004/03/11 12:28:59 hmp Exp $
.\"
.Dd September 20, 1995
.Dt PPP 8
.Os
.Sh NAME
.Nm ppp
.Nd Point to Point Protocol (a.k.a. user-ppp)
.Sh SYNOPSIS
.Nm
.Op Fl Va mode
.Op Fl nat
.Op Fl quiet
.Op Fl unit Ns Ar N
.Op Ar system ...
.Sh DESCRIPTION
This is a user process
.Em PPP
software package.
Normally,
.Em PPP
is implemented as a part of the kernel (e.g., as managed by
.Xr pppd 8 )
and it's thus somewhat hard to debug and/or modify its behaviour.
However, in this implementation
.Em PPP
is done as a user process with the help of the
tunnel device driver (tun).
.Pp
The
.Fl nat
flag does the equivalent of a
.Dq nat enable yes ,
enabling
.Nm Ns No 's
network address translation features.
This allows
.Nm
to act as a NAT or masquerading engine for all machines on an internal
LAN.
ifdef({LOCALNAT},{},{Refer to
.Xr libalias 3
for details on the technical side of the NAT engine.
})dnl
Refer to the
.Sx NETWORK ADDRESS TRANSLATION (PACKET ALIASING)
section of this manual page for details on how to configure NAT in
.Nm .
.Pp
The
.Fl quiet
flag tells
.Nm
to be silent at startup rather than displaying the mode and interface
to standard output.
.Pp
The
.Fl unit
flag tells
.Nm
to only attempt to open
.Pa /dev/tun Ns Ar N .
Normally,
.Nm
will start with a value of 0 for
.Ar N ,
and keep trying to open a tunnel device by incrementing the value of
.Ar N
by one each time until it succeeds.
If it fails three times in a row
because the device file is missing, it gives up.
.Pp
The following
.Va mode Ns No s
are understood by
.Nm :
.Bl -tag -width XXX -offset XXX
.It Fl auto
.Nm
opens the tun interface, configures it then goes into the background.
The link isn't brought up until outgoing data is detected on the tun
interface at which point
.Nm
attempts to bring up the link.
Packets received (including the first one) while
.Nm
is trying to bring the link up will remain queued for a default of
2 minutes.
See the
.Dq set choked
command below.
.Pp
In
.Fl auto
mode, at least one
.Dq system
must be given on the command line (see below) and a
.Dq set ifaddr
must be done in the system profile that specifies a peer IP address to
use when configuring the interface.
Something like
.Dq 10.0.0.1/0
is usually appropriate.
See the
.Dq pmdemand
system in
.Pa /usr/share/examples/ppp/ppp.conf.sample
for an example.
.It Fl background
Here,
.Nm
attempts to establish a connection with the peer immediately.
If it succeeds,
.Nm
goes into the background and the parent process returns an exit code
of 0.
If it fails,
.Nm
exits with a non-zero result.
.It Fl foreground
In foreground mode,
.Nm
attempts to establish a connection with the peer immediately, but never
becomes a daemon.
The link is created in background mode.
This is useful if you wish to control
.Nm Ns No 's
invocation from another process.
.It Fl direct
This is used for receiving incoming connections.
.Nm
ignores the
.Dq set device
line and uses descriptor 0 as the link.
.Pp
If callback is configured,
.Nm
will use the
.Dq set device
information when dialing back.
.It Fl dedicated
This option is designed for machines connected with a dedicated
wire.
.Nm
will always keep the device open and will never use any configured
chat scripts.
.It Fl ddial
This mode is equivalent to
.Fl auto
mode except that
.Nm
will bring the link back up any time it's dropped for any reason.
.It Fl interactive
This is a no-op, and gives the same behaviour as if none of the above
modes have been specified.
.Nm
loads any sections specified on the command line then provides an
interactive prompt.
.El
.Pp
One or more configuration entries or systems
(as specified in
.Pa /etc/ppp/ppp.conf )
may also be specified on the command line.
.Nm
will read the
.Dq default
system from
.Pa /etc/ppp/ppp.conf
at startup, followed by each of the systems specified on the command line.
.Sh Major Features
.Bl -diag
.It Provides an interactive user interface.
Using its command mode, the user can
easily enter commands to establish the connection with the remote end, check
the status of connection and close the connection.
All functions can also be optionally password protected for security.
.It Supports both manual and automatic dialing.
Interactive mode has a
.Dq term
command which enables you to talk to the device directly.
When you are connected to the remote peer and it starts to talk
.Em PPP ,
.Nm
detects it and switches to packet mode automatically.
Once you have
determined the proper sequence for connecting with the remote host, you
can write a chat script to {define} the necessary dialing and login
procedure for later convenience.
.It Supports on-demand dialup capability.
By using
.Fl auto
mode,
.Nm
will act as a daemon and wait for a packet to be sent over the
.Em PPP
link.
When this happens, the daemon automatically dials and establishes the
connection.
In almost the same manner
.Fl ddial
mode (direct-dial mode) also automatically dials and establishes the
connection.
However, it differs in that it will dial the remote site
any time it detects the link is down, even if there are no packets to be
sent.
This mode is useful for full-time connections where we worry less
about line charges and more about being connected full time.
A third
.Fl dedicated
mode is also available.
This mode is targeted at a dedicated link between two machines.
.Nm
will never voluntarily quit from dedicated mode - you must send it the
.Dq quit all
command via its diagnostic socket.
A
.Dv SIGHUP
will force an LCP renegotiation, and a
.Dv SIGTERM
will force it to exit.
.It Supports client callback.
.Nm
can use either the standard LCP callback protocol or the Microsoft
CallBack Control Protocol (ftp://ftp.microsoft.com/developr/rfc/cbcp.txt).
.It Supports NAT or packet aliasing.
Packet aliasing (a.k.a. IP masquerading) allows computers on a
private, unregistered network to access the Internet.
The
.Em PPP
host acts as a masquerading gateway.
IP addresses as well as TCP and
UDP port numbers are NAT'd for outgoing packets and de-NAT'd for
returning packets.
.It Supports background PPP connections.
In background mode, if
.Nm
successfully establishes the connection, it will become a daemon.
Otherwise, it will exit with an error.
This allows the setup of
scripts that wish to execute certain commands only if the connection
is successfully established.
.It Supports server-side PPP connections.
In direct mode,
.Nm
acts as server which accepts incoming
.Em PPP
connections on stdin/stdout.
.It "Supports PAP and CHAP (rfc 1994, 2433 and 2759) authentication.
With PAP or CHAP, it is possible to skip the Unix style
.Xr login 1
procedure, and use the
.Em PPP
protocol for authentication instead.
If the peer requests Microsoft CHAP authentication and
.Nm
is compiled with DES support, an appropriate MD4/DES response will be
made.
.It Supports RADIUS (rfc 2138 & 2548) authentication.
An extension to PAP and CHAP,
.Em \&R Ns No emote
.Em \&A Ns No ccess
.Em \&D Ns No ial
.Em \&I Ns No n
.Em \&U Ns No ser
.Em \&S Ns No ervice
allows authentication information to be stored in a central or
distributed database along with various per-user framed connection
characteristics.
ifdef({LOCALRAD},{},{If
.Xr libradius 3
is available at compile time,
.Nm
will use it to make
.Em RADIUS
requests when configured to do so.
})dnl
.It Supports Proxy Arp.
.Nm
can be configured to make one or more proxy arp entries on behalf of
the peer.
This allows routing from the peer to the LAN without
configuring each machine on that LAN.
.It Supports packet filtering.
User can {define} four kinds of filters: the
.Em in
filter for incoming packets, the
.Em out
filter for outgoing packets, the
.Em dial
filter to {define} a dialing trigger packet and the
.Em alive
filter for keeping a connection alive with the trigger packet.
.It Tunnel driver supports bpf.
The user can use
.Xr tcpdump 1
to check the packet flow over the
.Em PPP
link.
.It Supports PPP over TCP and PPP over UDP.
If a device name is specified as
.Em host Ns No : Ns Em port Ns
.Xo
.Op / Ns tcp|udp ,
.Xc
.Nm
will open a TCP or UDP connection for transporting data rather than using a
conventional serial device.
UDP connections force
.Nm
into synchronous mode.
.It Supports PPP over ISDN.
If
.Nm
is given a raw B-channel i4b device to open as a link, it's able to talk
to the
.Xr isdnd 8
daemon to establish an ISDN connection.
.It Supports PPP over Ethernet (rfc 2516).
If
.Nm
is given a device specification of the format
.No PPPoE: Ns Ar iface Ns Xo
.Op \&: Ns Ar provider Ns
.Xc
and if
.Xr netgraph 4
is available,
.Nm
will attempt talk
.Em PPP
over Ethernet to
.Ar provider
using the
.Ar iface
network interface.
.Pp
On systems that do not support
.Xr netgraph 4 ,
an external program such as
.Xr pppoe 8
may be used.
.It "Supports IETF draft Predictor-1 (rfc 1978) and DEFLATE (rfc 1979) compression."
.Nm
supports not only VJ-compression but also Predictor-1 and DEFLATE compression.
Normally, a modem has built-in compression (e.g., v42.bis) and the system
may receive higher data rates from it as a result of such compression.
While this is generally a good thing in most other situations, this
higher speed data imposes a penalty on the system by increasing the
number of serial interrupts the system has to process in talking to the
modem and also increases latency.
Unlike VJ-compression, Predictor-1 and DEFLATE compression pre-compresses
.Em all
network traffic flowing through the link, thus reducing overheads to a
minimum.
.It Supports Microsoft's IPCP extensions (rfc 1877).
Name Server Addresses and NetBIOS Name Server Addresses can be negotiated
with clients using the Microsoft
.Em PPP
stack (i.e., Win95, WinNT)
.It Supports Multi-link PPP (rfc 1990)
It is possible to configure
.Nm
to open more than one physical connection to the peer, combining the
bandwidth of all links for better throughput.
.It Supports MPPE (draft-ietf-pppext-mppe)
MPPE is Microsoft Point to Point Encryption scheme.
It is possible to configure
.Nm
to participate in Microsoft's Windows VPN.
For now,
.Nm
can only get encryption keys from CHAP 81 authentication.
.Nm
must be compiled with DES for MPPE to operate.
.It Supports IPV6CP (rfc 2023).
An IPv6 connection can be made in addition to or instead of the normal
IPv4 connection.
.El
.Sh PERMISSIONS
.Nm
is installed as user
.Dv root
and group
.Dv network ,
with permissions
.Dv 04554 .
By default,
.Nm
will not run if the invoking user id is not zero.
This may be overridden by using the
.Dq allow users
command in
.Pa /etc/ppp/ppp.conf .
When running as a normal user,
.Nm
switches to user id 0 in order to alter the system routing table, set up
system lock files and read the ppp configuration files.
All external commands (executed via the "shell" or "!bg" commands) are executed
as the user id that invoked
.Nm .
Refer to the
.Sq ID0
logging facility if you're interested in what exactly is done as user id
zero.
.Sh GETTING STARTED
When you first run
.Nm
you may need to deal with some initial configuration details.
.Bl -bullet
.It
Your kernel must {include} a tunnel device (the GENERIC kernel includes
one by default).
If it doesn't, or if you require more than one tun
interface, you'll need to rebuild your kernel with the following line in
your kernel configuration file:
.Pp
.Dl pseudo-device tun N
.Pp
where
.Ar N
is the maximum number of
.Em PPP
connections you wish to support.
.It
Check your
.Pa /dev
directory for the tunnel device entries
.Pa /dev/tunN ,
where
.Sq N
represents the number of the tun device, starting at zero.
If they don't exist, you can create them by running "sh ./MAKEDEV tunN".
This will create tun devices 0 through
.Ar N .
.It
Make sure that your system has a group named
.Dq network
in the
.Pa /etc/group
file and that the group contains the names of all users expected to use
.Nm .
Refer to the
.Xr group 5
manual page for details.
Each of these users must also be given access using the
.Dq allow users
command in
.Pa /etc/ppp/ppp.conf .
.It
Create a log file.
.Nm
uses
.Xr syslog 3
to log information.
A common log file name is
.Pa /var/log/ppp.log .
To make output go to this file, put the following lines in the
.Pa /etc/syslog.conf
file:
.Bd -literal -offset indent
!ppp
*.*<TAB>/var/log/ppp.log
.Ed
.Pp
It is possible to have more than one
.Em PPP
log file by creating a link to the
.Nm
executable:
.Pp
.Dl # cd /usr/sbin
.Dl # ln ppp ppp0
.Pp
and using
.Bd -literal -offset indent
!ppp0
*.*<TAB>/var/log/ppp0.log
.Ed
.Pp
in
.Pa /etc/syslog.conf .
Don't forget to send a
.Dv HUP
signal to
.Xr syslogd 8
after altering
.Pa /etc/syslog.conf .
.It
Although not strictly relevant to
.Nm Ns No 's
operation, you should configure your resolver so that it works correctly.
This can be done by configuring a local DNS
(using
.Xr named 8 )
or by adding the correct
.Sq nameserver
lines to the file
.Pa /etc/resolv.conf .
Refer to the
.Xr resolv.conf 5
manual page for details.
.Pp
Alternatively, if the peer supports it,
.Nm
can be configured to ask the peer for the nameserver address(es) and to
update
.Pa /etc/resolv.conf
automatically.
Refer to the
.Dq enable dns
and
.Dq resolv
commands below for details.
.El
.Sh MANUAL DIALING
In the following examples, we assume that your machine name is
.Dv awfulhak .
when you invoke
.Nm
(see
.Sx PERMISSIONS
above) with no arguments, you are presented with a prompt:
.Bd -literal -offset indent
ppp ON awfulhak>
.Ed
.Pp
The
.Sq ON
part of your prompt should always be in upper case.
If it is in lower case, it means that you must supply a password using the
.Dq passwd
command.
This only ever happens if you connect to a running version of
.Nm
and have not authenticated yourself using the correct password.
.Pp
You can start by specifying the device name and speed:
.Bd -literal -offset indent
ppp ON awfulhak> set device /dev/cuaa0
ppp ON awfulhak> set speed 38400
.Ed
.Pp
Normally, hardware flow control (CTS/RTS) is used.
However, under
certain circumstances (as may happen when you are connected directly
to certain PPP-capable terminal servers), this may result in
.Nm
hanging as soon as it tries to write data to your communications link
as it is waiting for the CTS (clear to send) signal - which will never
come.
Thus, if you have a direct line and can't seem to make a
connection, try turning CTS/RTS off with
.Dq set ctsrts off .
If you need to do this, check the
.Dq set accmap
description below too - you'll probably need to
.Dq set accmap 000a0000 .
.Pp
Usually, parity is set to
.Dq none ,
and this is
.Nm Ns No 's
default.
Parity is a rather archaic error checking mechanism that is no
longer used because modern modems do their own error checking, and most
link-layer protocols (that's what
.Nm
is) use much more reliable checking mechanisms.
Parity has a relatively
huge overhead (a 12.5% increase in traffic) and as a result, it is always
disabled
(set to
.Dq none )
when
.Dv PPP
is opened.
However, some ISPs (Internet Service Providers) may use
specific parity settings at connection time (before
.Dv PPP
is opened).
Notably, Compuserve insist on even parity when logging in:
.Bd -literal -offset indent
ppp ON awfulhak> set parity even
.Ed
.Pp
You can now see what your current device settings look like:
.Bd -literal -offset indent
ppp ON awfulhak> show physical
Name: deflink
 State:           closed
 Device:          N/A
 Link Type:       interactive
 Connect Count:   0
 Queued Packets:  0
 Phone Number:    N/A

Defaults:
 Device List:     /dev/cuaa0
 Characteristics: 38400bps, cs8, even parity, CTS/RTS on

Connect time: 0 secs
0 octets in, 0 octets out
Overall 0 bytes/sec
ppp ON awfulhak>
.Ed
.Pp
The term command can now be used to talk directly to the device:
.Bd -literal -offset indent
ppp ON awfulhak> term
at
OK
atdt123456
CONNECT
login: myispusername
Password: myisppassword
Protocol: ppp
.Ed
.Pp
When the peer starts to talk in
.Em PPP ,
.Nm
detects this automatically and returns to command mode.
.Bd -literal -offset indent
ppp ON awfulhak>               # No link has been established
Ppp ON awfulhak>               # We've connected & finished LCP
PPp ON awfulhak>               # We've authenticated
PPP ON awfulhak>               # We've agreed IP numbers
.Ed
.Pp
If it does not, it's probable that the peer is waiting for your end to
start negotiating.
To force
.Nm
to start sending
.Em PPP
configuration packets to the peer, use the
.Dq ~p
command to drop out of terminal mode and enter packet mode.
.Pp
If you never even receive a login prompt, it is quite likely that the
peer wants to use PAP or CHAP authentication instead of using Unix-style
login/password authentication.
To set things up properly, drop back to
the prompt and set your authentication name and key, then reconnect:
.Bd -literal -offset indent
~.
ppp ON awfulhak> set authname myispusername
ppp ON awfulhak> set authkey myisppassword
ppp ON awfulhak> term
at
OK
atdt123456
CONNECT
.Ed
.Pp
You may need to tell ppp to initiate negotiations with the peer here too:
.Bd -literal -offset indent
~p
ppp ON awfulhak>               # No link has been established
Ppp ON awfulhak>               # We've connected & finished LCP
PPp ON awfulhak>               # We've authenticated
PPP ON awfulhak>               # We've agreed IP numbers
.Ed
.Pp
You are now connected!
Note that
.Sq PPP
in the prompt has changed to capital letters to indicate that you have
a peer connection.
If only some of the three Ps go uppercase, wait until
either everything is uppercase or lowercase.
If they revert to lowercase, it means that
.Nm
couldn't successfully negotiate with the peer.
A good first step for troubleshooting at this point would be to
.Bd -literal -offset indent
ppp ON awfulhak> set log local phase lcp ipcp
.Ed
.Pp
and try again.
Refer to the
.Dq set log
command description below for further details.
If things fail at this point,
it is quite important that you turn logging on and try again.
It is also
important that you note any prompt changes and report them to anyone trying
to help you.
.Pp
When the link is established, the show command can be used to see how
things are going:
.Bd -literal -offset indent
PPP ON awfulhak> show physical
* Modem related information is shown here *
PPP ON awfulhak> show ccp
* CCP (compression) related information is shown here *
PPP ON awfulhak> show lcp
* LCP (line control) related information is shown here *
PPP ON awfulhak> show ipcp
* IPCP (IP) related information is shown here *
PPP ON awfulhak> show ipv6cp
* IPV6CP (IPv6) related information is shown here *
PPP ON awfulhak> show link
* Link (high level) related information is shown here *
PPP ON awfulhak> show bundle
* Logical (high level) connection related information is shown here *
.Ed
.Pp
At this point, your machine has a host route to the peer.
This means
that you can only make a connection with the host on the other side
of the link.
If you want to add a default route entry (telling your
machine to send all packets without another routing entry to the other
side of the
.Em PPP
link), enter the following command:
.Bd -literal -offset indent
PPP ON awfulhak> add default HISADDR
.Ed
.Pp
The string
.Sq HISADDR
represents the IP address of the connected peer.
If the
.Dq add
command fails due to an existing route, you can overwrite the existing
route using
.Bd -literal -offset indent
PPP ON awfulhak> add! default HISADDR
.Ed
.Pp
This command can also be executed before actually making the connection.
If a new IP address is negotiated at connection time,
.Nm
will update your default route accordingly.
.Pp
You can now use your network applications (ping, telnet, ftp etc.)
in other windows or terminals on your machine.
If you wish to reuse the current terminal, you can put
.Nm
into the background using your standard shell suspend and background
commands (usually
.Dq ^Z
followed by
.Dq bg ) .
.Pp
Refer to the
.Sx PPP COMMAND LIST
section for details on all available commands.
.Sh AUTOMATIC DIALING
To use automatic dialing, you must prepare some Dial and Login chat scripts.
See the example definitions in
.Pa /usr/share/examples/ppp/ppp.conf.sample
(the format of
.Pa /etc/ppp/ppp.conf
is pretty simple).
Each line contains one comment, inclusion, label or command:
.Bl -bullet
.It
A line starting with a
.Pq Dq #
character is treated as a comment line.
Leading whitespace are ignored when identifying comment lines.
.It
An inclusion is a line beginning with the word
.Sq {!include} .
It must have one argument - the file to {include}.
You may wish to
.Dq {!include} ~/.ppp.conf
for compatibility with older versions of
.Nm .
.It
A label name starts in the first column and is followed by
a colon
.Pq Dq \&: .
.It
A command line must contain a space or tab in the first column.
.El
.Pp
The
.Pa /etc/ppp/ppp.conf
file should consist of at least a
.Dq default
section.
This section is always executed.
It should also contain
one or more sections, named according to their purpose, for example,
.Dq MyISP
would represent your ISP, and
.Dq ppp-in
would represent an incoming
.Nm
configuration.
You can now specify the destination label name when you invoke
.Nm .
Commands associated with the
.Dq default
label are executed, followed by those associated with the destination
label provided.
When
.Nm
is started with no arguments, the
.Dq default
section is still executed.
The load command can be used to manually load a section from the
.Pa /etc/ppp/ppp.conf
file:
.Bd -literal -offset indent
ppp ON awfulhak> load MyISP
.Ed
.Pp
Note, no action is taken by
.Nm
after a section is loaded, whether it's the result of passing a label on
the command line or using the
.Dq load
command.
Only the commands specified for that label in the configuration
file are executed.
However, when invoking
.Nm
with the
.Fl background ,
.Fl ddial ,
or
.Fl dedicated
switches, the link mode tells
.Nm
to establish a connection.
Refer to the
.Dq set mode
command below for further details.
.Pp
Once the connection is made, the
.Sq ppp
portion of the prompt will change to
.Sq PPP :
.Bd -literal -offset indent
# ppp MyISP
\&...
ppp ON awfulhak> dial
Ppp ON awfulhak>
PPp ON awfulhak>
PPP ON awfulhak>
.Ed
.Pp
The Ppp prompt indicates that
.Nm
has entered the authentication phase.
The PPp prompt indicates that
.Nm
has entered the network phase.
The PPP prompt indicates that
.Nm
has successfully negotiated a network layer protocol and is in
a usable state.
.Pp
If the
.Pa /etc/ppp/ppp.linkup
file is available, its contents are executed
when the
.Em PPP
connection is established.
See the provided
.Dq pmdemand
example in
.Pa /usr/share/examples/ppp/ppp.conf.sample
which runs a script in the background after the connection is established
(refer to the
.Dq shell
and
.Dq bg
commands below for a description of possible substitution strings).
Similarly, when a connection is closed, the contents of the
.Pa /etc/ppp/ppp.linkdown
file are executed.
Both of these files have the same format as
.Pa /etc/ppp/ppp.conf .
.Pp
In previous versions of
.Nm ,
it was necessary to re-add routes such as the default route in the
.Pa ppp.linkup
file.
.Nm
supports
.Sq sticky routes ,
where all routes that contain the
.Dv HISADDR ,
.Dv MYADDR ,
.Dv HISADDR6
or
.Dv MYADDR6
literals will automatically be updated when the values of these variables
change.
.Sh BACKGROUND DIALING
If you want to establish a connection using
.Nm
non-interactively (such as from a
.Xr crontab 5
entry or an
.Xr at 1
job) you should use the
.Fl background
option.
When
.Fl background
is specified,
.Nm
attempts to establish the connection immediately.
If multiple phone
numbers are specified, each phone number will be tried once.
If the attempt fails,
.Nm
exits immediately with a non-zero exit code.
If it succeeds, then
.Nm
becomes a daemon, and returns an exit status of zero to its caller.
The daemon exits automatically if the connection is dropped by the
remote system, or it receives a
.Dv TERM
signal.
.Sh DIAL ON DEMAND
Demand dialing is enabled with the
.Fl auto
or
.Fl ddial
options.
You must also specify the destination label in
.Pa /etc/ppp/ppp.conf
to use.
It must contain the
.Dq set ifaddr
command to {define} the remote peers IP address.
(refer to
.Pa /usr/share/examples/ppp/ppp.conf.sample )
.Bd -literal -offset indent
# ppp -auto pmdemand
.Ed
.Pp
When
.Fl auto
or
.Fl ddial
is specified,
.Nm
runs as a daemon but you can still configure or examine its
configuration by using the
.Dq set server
command in
.Pa /etc/ppp/ppp.conf ,
(for example,
.Dq Li "set server +3000 mypasswd" )
and connecting to the diagnostic port as follows:
.Bd -literal -offset indent
# pppctl 3000	(assuming tun0)
Password:
PPP ON awfulhak> show who
tcp (127.0.0.1:1028) *
.Ed
.Pp
The
.Dq show who
command lists users that are currently connected to
.Nm
itself.
If the diagnostic socket is closed or changed to a different
socket, all connections are immediately dropped.
.Pp
In
.Fl auto
mode, when an outgoing packet is detected,
.Nm
will perform the dialing action (chat script) and try to connect
with the peer.
In
.Fl ddial
mode, the dialing action is performed any time the line is found
to be down.
If the connect fails, the default behaviour is to wait 30 seconds
and then attempt to connect when another outgoing packet is detected.
This behaviour can be changed using the
.Dq set redial
command:
.Pp
.No set redial Ar secs Ns Xo
.Oo + Ns Ar inc Ns
.Op - Ns Ar max Ns
.Oc Ns Op . Ns Ar next
.Op Ar attempts
.Xc
.Pp
.Bl -tag -width attempts -compact
.It Ar secs
is the number of seconds to wait before attempting
to connect again.
If the argument is the literal string
.Sq Li random ,
the delay period is a random value between 1 and 30 seconds inclusive.
.It Ar inc
is the number of seconds that
.Ar secs
should be incremented each time a new dial attempt is made.
The timeout reverts to
.Ar secs
only after a successful connection is established.
The default value for
.Ar inc
is zero.
.It Ar max
is the maximum number of times
.Nm
should increment
.Ar secs .
The default value for
.Ar max
is 10.
.It Ar next
is the number of seconds to wait before attempting
to dial the next number in a list of numbers (see the
.Dq set phone
command).
The default is 3 seconds.
Again, if the argument is the literal string
.Sq Li random ,
the delay period is a random value between 1 and 30 seconds.
.It Ar attempts
is the maximum number of times to try to connect for each outgoing packet
that triggers a dial.
The previous value is unchanged if this parameter is omitted.
If a value of zero is specified for
.Ar attempts ,
.Nm
will keep trying until a connection is made.
.El
.Pp
So, for example:
.Bd -literal -offset indent
set redial 10.3 4
.Ed
.Pp
will attempt to connect 4 times for each outgoing packet that causes
a dial attempt with a 3 second delay between each number and a 10 second
delay after all numbers have been tried.
If multiple phone numbers
are specified, the total number of attempts is still 4 (it does not
attempt each number 4 times).
.Pp
Alternatively,
.Pp
.Bd -literal -offset indent
set redial 10+10-5.3 20
.Ed
.Pp
tells
.Nm
to attempt to connect 20 times.
After the first attempt,
.Nm
pauses for 10 seconds.
After the next attempt it pauses for 20 seconds
and so on until after the sixth attempt it pauses for 1 minute.
The next 14 pauses will also have a duration of one minute.
If
.Nm
connects, disconnects and fails to connect again, the timeout starts again
at 10 seconds.
.Pp
Modifying the dial delay is very useful when running
.Nm
in
.Fl auto
mode on both ends of the link.
If each end has the same timeout,
both ends wind up calling each other at the same time if the link
drops and both ends have packets queued.
At some locations, the serial link may not be reliable, and carrier
may be lost at inappropriate times.
It is possible to have
.Nm
redial should carrier be unexpectedly lost during a session.
.Bd -literal -offset indent
set reconnect timeout ntries
.Ed
.Pp
This command tells
.Nm
to re-establish the connection
.Ar ntries
times on loss of carrier with a pause of
.Ar timeout
seconds before each try.
For example,
.Bd -literal -offset indent
set reconnect 3 5
.Ed
.Pp
tells
.Nm
that on an unexpected loss of carrier, it should wait
.Ar 3
seconds before attempting to reconnect.
This may happen up to
.Ar 5
times before
.Nm
gives up.
The default value of ntries is zero (no reconnect).
Care should be taken with this option.
If the local timeout is slightly
longer than the remote timeout, the reconnect feature will always be
triggered (up to the given number of times) after the remote side
times out and hangs up.
NOTE: In this context, losing too many LQRs constitutes a loss of
carrier and will trigger a reconnect.
If the
.Fl background
flag is specified, all phone numbers are dialed at most once until
a connection is made.
The next number redial period specified with the
.Dq set redial
command is honoured, as is the reconnect tries value.
If your redial
value is less than the number of phone numbers specified, not all
the specified numbers will be tried.
To terminate the program, type
.Bd -literal -offset indent
PPP ON awfulhak> close
ppp ON awfulhak> quit all
.Ed
.Pp
A simple
.Dq quit
command will terminate the
.Xr pppctl 8
or
.Xr telnet 1
connection but not the
.Nm
program itself.
You must use
.Dq quit all
to terminate
.Nm
as well.
.Sh RECEIVING INCOMING PPP CONNECTIONS (Method 1)
To handle an incoming
.Em PPP
connection request, follow these steps:
.Bl -enum
.It
Make sure the modem and (optionally)
.Pa /etc/rc.serial
is configured correctly.
.Bl -bullet -compact
.It
Use Hardware Handshake (CTS/RTS) for flow control.
.It
Modem should be set to NO echo back (ATE0) and NO results string (ATQ1).
.El
.Pp
.It
Edit
.Pa /etc/ttys
to enable a
.Xr getty 8
on the port where the modem is attached.
For example:
.Pp
.Dl ttyd1 Qo /usr/libexec/getty std.38400 Qc dialup on secure
.Pp
Don't forget to send a
.Dv HUP
signal to the
.Xr init 8
process to start the
.Xr getty 8 :
.Pp
.Dl # kill -HUP 1
.Pp
It is usually also necessary to train your modem to the same DTR speed
as the getty:
.Bd -literal -offset indent
# ppp
ppp ON awfulhak> set device /dev/cuaa1
ppp ON awfulhak> set speed 38400
ppp ON awfulhak> term
deflink: Entering terminal mode on /dev/cuaa1
Type `~?' for help
at
OK
at
OK
atz
OK
at
OK
~.
ppp ON awfulhak> quit
.Ed
.It
Create a
.Pa /usr/local/bin/ppplogin
file with the following contents:
.Bd -literal -offset indent
#! /bin/sh
exec /usr/sbin/ppp -direct incoming
.Ed
.Pp
Direct mode
.Pq Fl direct
lets
.Nm
work with stdin and stdout.
You can also use
.Xr pppctl 8
to connect to a configured diagnostic port, in the same manner as with
client-side
.Nm .
.Pp
Here, the
.Ar incoming
section must be set up in
.Pa /etc/ppp/ppp.conf .
.Pp
Make sure that the
.Ar incoming
section contains the
.Dq allow users
command as appropriate.
.It
Prepare an account for the incoming user.
.Bd -literal
ppp:xxxx:66:66:PPP Login User:/home/ppp:/usr/local/bin/ppplogin
.Ed
.Pp
Refer to the manual entries for
.Xr adduser 8
and
.Xr vipw 8
for details.
.It
Support for IPCP Domain Name Server and NetBIOS Name Server negotiation
can be enabled using the
.Dq accept dns
and
.Dq set nbns
commands.
Refer to their descriptions below.
.El
.Sh RECEIVING INCOMING PPP CONNECTIONS (Method 2)
This method differs in that we use
.Nm
to authenticate the connection rather than
.Xr login 1 :
.Bl -enum
.It
Configure your default section in
.Pa /etc/gettytab
with automatic ppp recognition by specifying the
.Dq pp
capability:
.Bd -literal
default:\\
	:pp=/usr/local/bin/ppplogin:\\
	.....
.Ed
.It
Configure your serial device(s), enable a
.Xr getty 8
and create
.Pa /usr/local/bin/ppplogin
as in the first three steps for method 1 above.
.It
Add either
.Dq enable chap
or
.Dq enable pap
(or both)
to
.Pa /etc/ppp/ppp.conf
under the
.Sq incoming
label (or whatever label
.Pa ppplogin
uses).
.It
Create an entry in
.Pa /etc/ppp/ppp.secret
for each incoming user:
.Bd -literal
Pfred<TAB>xxxx
Pgeorge<TAB>yyyy
.Ed
.El
.Pp
Now, as soon as
.Xr getty 8
detects a ppp connection (by recognising the HDLC frame headers), it runs
.Dq /usr/local/bin/ppplogin .
.Pp
It is
.Em VITAL
that either PAP or CHAP are enabled as above.
If they are not, you are
allowing anybody to establish a ppp session with your machine
.Em without
a password, opening yourself up to all sorts of potential attacks.
.Sh AUTHENTICATING INCOMING CONNECTIONS
Normally, the receiver of a connection requires that the peer
authenticates itself.
This may be done using
.Xr login 1 ,
but alternatively, you can use PAP or CHAP.
CHAP is the more secure of the two, but some clients may not support it.
Once you decide which you wish to use, add the command
.Sq enable chap
or
.Sq enable pap
to the relevant section of
.Pa ppp.conf .
.Pp
You must then configure the
.Pa /etc/ppp/ppp.secret
file.
This file contains one line per possible client, each line
containing up to five fields:
.Pp
.Ar name Ar key Oo
.Ar hisaddr Op Ar label Op Ar callback-number
.Oc
.Pp
The
.Ar name
and
.Ar key
specify the client username and password.
If
.Ar key
is
.Dq \&*
and PAP is being used,
.Nm
will look up the password database
.Pq Xr passwd 5
when authenticating.
If the client does not offer a suitable response based on any
.Ar name Ns No / Ns Ar key
combination in
.Pa ppp.secret ,
authentication fails.
.Pp
If authentication is successful,
.Ar hisaddr
(if specified)
is used when negotiating IP numbers.
See the
.Dq set ifaddr
command for details.
.Pp
If authentication is successful and
.Ar label
is specified, the current system label is changed to match the given
.Ar label .
This will change the subsequent parsing of the
.Pa ppp.linkup
and
.Pa ppp.linkdown
files.
.Pp
If authentication is successful and
.Ar callback-number
is specified and
.Dq set callback
has been used in
.Pa ppp.conf ,
the client will be called back on the given number.
If CBCP is being used,
.Ar callback-number
may also contain a list of numbers or a
.Dq \&* ,
as if passed to the
.Dq set cbcp
command.
The value will be used in
.Nm Ns No 's
subsequent CBCP phase.
.Sh PPP OVER TCP and UDP (a.k.a Tunnelling)
Instead of running
.Nm
over a serial link, it is possible to
use a TCP connection instead by specifying the host, port and protocol as the
device:
.Pp
.Dl set device ui-gate:6669/tcp
.Pp
Instead of opening a serial device,
.Nm
will open a TCP connection to the given machine on the given
socket.
It should be noted however that
.Nm
doesn't use the telnet protocol and will be unable to negotiate
with a telnet server.
You should set up a port for receiving this
.Em PPP
connection on the receiving machine (ui-gate).
This is done by first updating
.Pa /etc/services
to name the service:
.Pp
.Dl ppp-in 6669/tcp # Incoming PPP connections over TCP
.Pp
and updating
.Pa /etc/inetd.conf
to tell
.Xr inetd 8
how to deal with incoming connections on that port:
.Pp
.Dl ppp-in stream tcp nowait root /usr/sbin/ppp ppp -direct ppp-in
.Pp
Don't forget to send a
.Dv HUP
signal to
.Xr inetd 8
after you've updated
.Pa /etc/inetd.conf .
Here, we use a label named
.Dq ppp-in .
The entry in
.Pa /etc/ppp/ppp.conf
on ui-gate (the receiver) should contain the following:
.Bd -literal -offset indent
ppp-in:
 set timeout 0
 set ifaddr 10.0.4.1 10.0.4.2
.Ed
.Pp
and the entry in
.Pa /etc/ppp/ppp.linkup
should contain:
.Bd -literal -offset indent
ppp-in:
 add 10.0.1.0/24 HISADDR
.Ed
.Pp
It is necessary to put the
.Dq add
command in
.Pa ppp.linkup
to ensure that the route is only added after
.Nm
has negotiated and assigned addresses to its interface.
.Pp
You may also want to enable PAP or CHAP for security.
To enable PAP, add the following line:
.Bd -literal -offset indent
 enable PAP
.Ed
.Pp
You'll also need to create the following entry in
.Pa /etc/ppp/ppp.secret :
.Bd -literal -offset indent
MyAuthName MyAuthPasswd
.Ed
.Pp
If
.Ar MyAuthPasswd
is a
.Dq * ,
the password is looked up in the
.Xr passwd 5
database.
.Pp
The entry in
.Pa /etc/ppp/ppp.conf
on awfulhak (the initiator) should contain the following:
.Bd -literal -offset indent
ui-gate:
 set escape 0xff
 set device ui-gate:ppp-in/tcp
 set dial
 set timeout 30
 set log Phase Chat Connect hdlc LCP IPCP IPV6CP CCP tun
 set ifaddr 10.0.4.2 10.0.4.1
.Ed
.Pp
with the route setup in
.Pa /etc/ppp/ppp.linkup :
.Bd -literal -offset indent
ui-gate:
 add 10.0.2.0/24 HISADDR
.Ed
.Pp
Again, if you're enabling PAP, you'll also need this in the
.Pa /etc/ppp/ppp.conf
profile:
.Bd -literal -offset indent
 set authname MyAuthName
 set authkey MyAuthKey
.Ed
.Pp
We're assigning the address of 10.0.4.1 to ui-gate, and the address
10.0.4.2 to awfulhak.
To open the connection, just type
.Pp
.Dl awfulhak # ppp -background ui-gate
.Pp
The result will be an additional "route" on awfulhak to the
10.0.2.0/24 network via the TCP connection, and an additional
"route" on ui-gate to the 10.0.1.0/24 network.
The networks are effectively bridged - the underlying TCP
connection may be across a public network (such as the
Internet), and the
.Em PPP
traffic is conceptually encapsulated
(although not packet by packet) inside the TCP stream between
the two gateways.
.Pp
The major disadvantage of this mechanism is that there are two
"guaranteed delivery" mechanisms in place - the underlying TCP
stream and whatever protocol is used over the
.Em PPP
link - probably TCP again.
If packets are lost, both levels will
get in each others way trying to negotiate sending of the missing
packet.
.Pp
To avoid this overhead, it is also possible to do all this using
UDP instead of TCP as the transport by simply changing the protocol
from "tcp" to "udp".
When using UDP as a transport,
.Nm
will operate in synchronous mode.
This is another gain as the incoming
data does not have to be rearranged into packets.
.Pp
Care should be taken when adding a default route through a tunneled
setup like this.
It is quite common for the default route
(added in
.Pa /etc/ppp/ppp.linkup )
to end up routing the link's TCP connection through the tunnel,
effectively garrotting the connection.
To avoid this, make sure you add a static route for the benefit of
the link:
.Bd -literal -offset indent
ui-gate:
 set escape 0xff
 set device ui-gate:ppp-in/tcp
 add ui-gate x.x.x.x
 .....
.Ed
.Pp
where
.Dq x.x.x.x
is the IP number that your route to
.Dq ui-gate
would normally use.
.Pp
When routing your connection accross a public network such as the Internet,
it is preferable to encrypt the data.
This can be done with the help of the MPPE protocol, although currently this
means that you will not be able to also compress the traffic as MPPE is
implemented as a compression layer (thank Microsoft for this).
To enable MPPE encryption, add the following lines to
.Pa /etc/ppp/ppp.conf
on the server:
.Bd -literal -offset indent
  enable MSCHAPv2
  disable deflate pred1
  deny deflate pred1
.Ed
.Pp
ensuring that you've put the requisite entry in
.Pa /etc/ppp/ppp.secret
(MSCHAPv2 is challenge based, so
.Xr passwd 5
cannot be used)
.Pp
MSCHAPv2 and MPPE are accepted by default, so the client end should work
without any additional changes (although ensure you have
.Dq set authname
and
.Dq set authkey
in your profile).
.Sh NETWORK ADDRESS TRANSLATION (PACKET ALIASING)
The
.Fl nat
command line option enables network address translation (a.k.a. packet
aliasing).
This allows the
.Nm
host to act as a masquerading gateway for other computers over
a local area network.
Outgoing IP packets are NAT'd so that they appear to come from the
.Nm
host, and incoming packets are de-NAT'd so that they are routed
to the correct machine on the local area network.
NAT allows computers on private, unregistered subnets to have Internet
access, although they are invisible from the outside world.
In general, correct
.Nm
operation should first be verified with network address translation disabled.
Then, the
.Fl nat
option should be switched on, and network applications (web browser,
.Xr telnet 1 ,
.Xr ftp 1 ,
.Xr ping 8 ,
.Xr traceroute 8 )
should be checked on the
.Nm
host.
Finally, the same or similar applications should be checked on other
computers in the LAN.
If network applications work correctly on the
.Nm
host, but not on other machines in the LAN, then the masquerading
software is working properly, but the host is either not forwarding
or possibly receiving IP packets.
Check that IP forwarding is enabled in
.Pa /etc/rc.conf
and that other machines have designated the
.Nm
host as the gateway for the LAN.
.Sh PACKET FILTERING
This implementation supports packet filtering.
There are four kinds of
filters: the
.Em in
filter, the
.Em out
filter, the
.Em dial
filter and the
.Em alive
filter.
Here are the basics:
.Bl -bullet
.It
A filter definition has the following syntax:
.Pp
set filter
.Ar name
.Ar rule-no
.Ar action
.Op !\&
.Oo
.Op host
.Ar src_addr Ns Op / Ns Ar width
.Op Ar dst_addr Ns Op / Ns Ar width
.Oc
.Ar [ proto Op src Ar cmp port
.Op dst Ar cmp port
.Op estab
.Op syn
.Op finrst
.Op timeout Ar secs ]
.Bl -enum
.It
.Ar Name
should be one of
.Sq in ,
.Sq out ,
.Sq dial
or
.Sq alive .
.It
.Ar Rule-no
is a numeric value between
.Sq 0
and
.Sq 39
specifying the rule number.
Rules are specified in numeric order according to
.Ar rule-no ,
but only if rule
.Sq 0
is defined.
.It
.Ar Action
may be specified as
.Sq permit
or
.Sq deny ,
in which case, if a given packet matches the rule, the associated action
is taken immediately.
.Ar Action
can also be specified as
.Sq clear
to clear the action associated with that particular rule, or as a new
rule number greater than the current rule.
In this case, if a given
packet matches the current rule, the packet will next be matched against
the new rule number (rather than the next rule number).
.Pp
The
.Ar action
may optionally be followed with an exclamation mark
.Pq Dq !\& ,
telling
.Nm
to reverse the sense of the following match.
.It
.Op Ar src_addr Ns Op / Ns Ar width
and
.Op Ar dst_addr Ns Op / Ns Ar width
are the source and destination IP number specifications.
If
.Op / Ns Ar width
is specified, it gives the number of relevant netmask bits,
allowing the specification of an address range.
.Pp
Either
.Ar src_addr
or
.Ar dst_addr
may be given the values
.Dv MYADDR ,
.Dv HISADDR ,
.Dv MYADDR6
or
.Dv HISADDR6
(refer to the description of the
.Dq bg
command for a description of these values).
When these values are used,
the filters will be updated any time the values change.
This is similar to the behaviour of the
.Dq add
command below.
.It
.Ar Proto
may be any protocol from
.Xr protocols 5 .
.It
.Ar Cmp
is one of
.Sq \&lt ,
.Sq \&eq
or
.Sq \&gt ,
meaning less-than, equal and greater-than respectively.
.Ar Port
can be specified as a numeric port or by service name from
.Pa /etc/services .
.It
The
.Sq estab ,
.Sq syn ,
and
.Sq finrst
flags are only allowed when
.Ar proto
is set to
.Sq tcp ,
and represent the TH_ACK, TH_SYN and TH_FIN or TH_RST TCP flags respectively.
.It
The timeout value adjusts the current idle timeout to at least
.Ar secs
seconds.
If a timeout is given in the alive filter as well as in the in/out
filter, the in/out value is used.
If no timeout is given, the default timeout (set using
.Ic set timeout
and defaulting to 180 seconds) is used.
.El
.Pp
.It
Each filter can hold up to 40 rules, starting from rule 0.
The entire rule set is not effective until rule 0 is defined,
i.e., the default is to allow everything through.
.It
If no rule in a defined set of rules matches a packet, that packet will
be discarded (blocked).
If there are no rules in a given filter, the packet will be permitted.
.It
It's possible to filter based on the payload of UDP frames where those
frames contain a
.Em PROTO_IP
.Em PPP
frame header.
See the
.Ar filter-decapsulation
option below for further details.
.It
Use
.Dq set filter Ar name No -1
to flush all rules.
.El
.Pp
See
.Pa /usr/share/examples/ppp/ppp.conf.sample .
.Sh SETTING THE IDLE TIMER
To check/set the idle timer, use the
.Dq show bundle
and
.Dq set timeout
commands:
.Bd -literal -offset indent
ppp ON awfulhak> set timeout 600
.Ed
.Pp
The timeout period is measured in seconds, the default value for which
is 180 seconds
(or 3 min).
To disable the idle timer function, use the command
.Bd -literal -offset indent
ppp ON awfulhak> set timeout 0
.Ed
.Pp
In
.Fl ddial
and
.Fl dedicated
modes, the idle timeout is ignored.
In
.Fl auto
mode, when the idle timeout causes the
.Em PPP
session to be
closed, the
.Nm
program itself remains running.
Another trigger packet will cause it to attempt to re-establish the link.
.Sh PREDICTOR-1 and DEFLATE COMPRESSION
.Nm
supports both Predictor type 1 and deflate compression.
By default,
.Nm
will attempt to use (or be willing to accept) both compression protocols
when the peer agrees
(or requests them).
The deflate protocol is preferred by
.Nm .
Refer to the
.Dq disable
and
.Dq deny
commands if you wish to disable this functionality.
.Pp
It is possible to use a different compression algorithm in each direction
by using only one of
.Dq disable deflate
and
.Dq deny deflate
(assuming that the peer supports both algorithms).
.Pp
By default, when negotiating DEFLATE,
.Nm
will use a window size of 15.
Refer to the
.Dq set deflate
command if you wish to change this behaviour.
.Pp
A special algorithm called DEFLATE24 is also available, and is disabled
and denied by default.
This is exactly the same as DEFLATE except that
it uses CCP ID 24 to negotiate.
This allows
.Nm
to successfully negotiate DEFLATE with
.Nm pppd
version 2.3.*.
.Sh CONTROLLING IP ADDRESS
For IPv4,
.Nm
uses IPCP to negotiate IP addresses.
Each side of the connection
specifies the IP address that it's willing to use, and if the requested
IP address is acceptable then
.Nm
returns an ACK to the requester.
Otherwise,
.Nm
returns NAK to suggest that the peer use a different IP address.
When
both sides of the connection agree to accept the received request (and
send an ACK), IPCP is set to the open state and a network level connection
is established.
To control this IPCP behaviour, this implementation has the
.Dq set ifaddr
command for defining the local and remote IP address:
.Bd -ragged -offset indent
.No set ifaddr Oo Ar src_addr Ns
.Op / Ns Ar \&nn
.Oo Ar dst_addr Ns Op / Ns Ar \&nn
.Oo Ar netmask
.Op Ar trigger_addr
.Oc
.Oc
.Oc
.Ed
.Pp
where,
.Sq src_addr
is the IP address that the local side is willing to use,
.Sq dst_addr
is the IP address which the remote side should use and
.Sq netmask
is the netmask that should be used.
.Sq Src_addr
defaults to the current
.Xr hostname 1 ,
.Sq dst_addr
defaults to 0.0.0.0, and
.Sq netmask
defaults to whatever mask is appropriate for
.Sq src_addr .
It is only possible to make
.Sq netmask
smaller than the default.
The usual value is 255.255.255.255, as
most kernels ignore the netmask of a POINTOPOINT interface.
.Pp
Some incorrect
.Em PPP
implementations require that the peer negotiates a specific IP
address instead of
.Sq src_addr .
If this is the case,
.Sq trigger_addr
may be used to specify this IP number.
This will not affect the
routing table unless the other side agrees with this proposed number.
.Bd -literal -offset indent
set ifaddr 192.244.177.38 192.244.177.2 255.255.255.255 0.0.0.0
.Ed
.Pp
The above specification means:
.Pp
.Bl -bullet -compact
.It
I will first suggest that my IP address should be 0.0.0.0, but I
will only accept an address of 192.244.177.38.
.It
I strongly insist that the peer uses 192.244.177.2 as his own
address and won't permit the use of any IP address but 192.244.177.2.
When the peer requests another IP address, I will always suggest that
it uses 192.244.177.2.
.It
The routing table entry will have a netmask of 0xffffffff.
.El
.Pp
This is all fine when each side has a pre-determined IP address, however
it is often the case that one side is acting as a server which controls
all IP addresses and the other side should go along with it.
In order to allow more flexible behaviour, the
.Dq set ifaddr
command allows the user to specify IP addresses more loosely:
.Pp
.Dl set ifaddr 192.244.177.38/24 192.244.177.2/20
.Pp
A number followed by a slash
.Pq Dq /
represents the number of bits significant in the IP address.
The above example means:
.Pp
.Bl -bullet -compact
.It
I'd like to use 192.244.177.38 as my address if it is possible, but I'll
also accept any IP address between 192.244.177.0 and 192.244.177.255.
.It
I'd like to make him use 192.244.177.2 as his own address, but I'll also
permit him to use any IP address between 192.244.176.0 and
192.244.191.255.
.It
As you may have already noticed, 192.244.177.2 is equivalent to saying
192.244.177.2/32.
.It
As an exception, 0 is equivalent to 0.0.0.0/0, meaning that I have no
preferred IP address and will obey the remote peers selection.
When using zero, no routing table entries will be made until a connection
is established.
.It
192.244.177.2/0 means that I'll accept/permit any IP address but I'll
suggest that 192.244.177.2 be used first.
.El
.Pp
When negotiating IPv6 addresses, no control is given to the user.
IPV6CP negotiation is fully automatic.
.Sh CONNECTING WITH YOUR INTERNET SERVICE PROVIDER
The following steps should be taken when connecting to your ISP:
.Bl -enum
.It
Describe your providers phone number(s) in the dial script using the
.Dq set phone
command.
This command allows you to set multiple phone numbers for
dialing and redialing separated by either a pipe
.Pq Dq \&|
or a colon
.Pq Dq \&: :
.Bd -ragged -offset indent
.No set phone Ar telno Ns Xo
.Oo \&| Ns Ar backupnumber
.Oc Ns ... Ns Oo : Ns Ar nextnumber
.Oc Ns ...
.Xc
.Ed
.Pp
Numbers after the first in a pipe-separated list are only used if the
previous number was used in a failed dial or login script.
Numbers
separated by a colon are used sequentially, irrespective of what happened
as a result of using the previous number.
For example:
.Bd -literal -offset indent
set phone "1234567|2345678:3456789|4567890"
.Ed
.Pp
Here, the 1234567 number is attempted.
If the dial or login script fails,
the 2345678 number is used next time, but *only* if the dial or login script
fails.
On the dial after this, the 3456789 number is used.
The 4567890
number is only used if the dial or login script using the 3456789 fails.
If the login script of the 2345678 number fails, the next number is still the
3456789 number.
As many pipes and colons can be used as are necessary
(although a given site would usually prefer to use either the pipe or the
colon, but not both).
The next number redial timeout is used between all numbers.
When the end of the list is reached, the normal redial period is
used before starting at the beginning again.
The selected phone number is substituted for the \\\\T string in the
.Dq set dial
command (see below).
.It
Set up your redial requirements using
.Dq set redial .
For example, if you have a bad telephone line or your provider is
usually engaged (not so common these days), you may want to specify
the following:
.Bd -literal -offset indent
set redial 10 4
.Ed
.Pp
This says that up to 4 phone calls should be attempted with a pause of 10
seconds before dialing the first number again.
.It
Describe your login procedure using the
.Dq set dial
and
.Dq set login
commands.
The
.Dq set dial
command is used to talk to your modem and establish a link with your
ISP, for example:
.Bd -literal -offset indent
set dial "ABORT BUSY ABORT NO\\\\sCARRIER TIMEOUT 4 \\"\\" \e
  ATZ OK-ATZ-OK ATDT\\\\T TIMEOUT 60 CONNECT"
.Ed
.Pp
This modem "chat" string means:
.Bl -bullet
.It
Abort if the string "BUSY" or "NO CARRIER" are received.
.It
Set the timeout to 4 seconds.
.It
Expect nothing.
.It
Send ATZ.
.It
Expect OK.
If that's not received within the 4 second timeout, send ATZ
and expect OK.
.It
Send ATDTxxxxxxx where xxxxxxx is the next number in the phone list from
above.
.It
Set the timeout to 60.
.It
Wait for the CONNECT string.
.El
.Pp
Once the connection is established, the login script is executed.
This script is written in the same style as the dial script, but care should
be taken to avoid having your password logged:
.Bd -literal -offset indent
set authkey MySecret
set login "TIMEOUT 15 login:-\\\\r-login: awfulhak \e
  word: \\\\P ocol: PPP HELLO"
.Ed
.Pp
This login "chat" string means:
.Bl -bullet
.It
Set the timeout to 15 seconds.
.It
Expect "login:".
If it's not received, send a carriage return and expect
"login:" again.
.It
Send "awfulhak"
.It
Expect "word:" (the tail end of a "Password:" prompt).
.It
Send whatever our current
.Ar authkey
value is set to.
.It
Expect "ocol:" (the tail end of a "Protocol:" prompt).
.It
Send "PPP".
.It
Expect "HELLO".
.El
.Pp
The
.Dq set authkey
command is logged specially.
When
.Ar command
or
.Ar chat
logging is enabled, the actual password is not logged;
.Sq ********
is logged instead.
.Pp
Login scripts vary greatly between ISPs.
If you're setting one up for the first time,
.Em ENABLE CHAT LOGGING
so that you can see if your script is behaving as you expect.
.It
Use
.Dq set device
and
.Dq set speed
to specify your serial line and speed, for example:
.Bd -literal -offset indent
set device /dev/cuaa0
set speed 115200
.Ed
.Pp
Cuaa0 is the first serial port on
.Dx .
If you're running
.Nm
on
.Ox ,
cua00 is the first.
A speed of 115200 should be specified
if you have a modem capable of bit rates of 28800 or more.
In general, the serial speed should be about four times the modem speed.
.It
Use the
.Dq set ifaddr
command to {define} the IP address.
.Bl -bullet
.It
If you know what IP address your provider uses, then use it as the remote
address (dst_addr), otherwise choose something like 10.0.0.2/0 (see below).
.It
If your provider has assigned a particular IP address to you, then use
it as your address (src_addr).
.It
If your provider assigns your address dynamically, choose a suitably
unobtrusive and unspecific IP number as your address.
10.0.0.1/0 would be appropriate.
The bit after the / specifies how many bits of the
address you consider to be important, so if you wanted to insist on
something in the class C network 1.2.3.0, you could specify 1.2.3.1/24.
.It
If you find that your ISP accepts the first IP number that you suggest,
specify third and forth arguments of
.Dq 0.0.0.0 .
This will force your ISP to assign a number.
(The third argument will
be ignored as it is less restrictive than the default mask for your
.Sq src_addr ) .
.El
.Pp
An example for a connection where you don't know your IP number or your
ISPs IP number would be:
.Bd -literal -offset indent
set ifaddr 10.0.0.1/0 10.0.0.2/0 0.0.0.0 0.0.0.0
.Ed
.Pp
.It
In most cases, your ISP will also be your default router.
If this is the case, add the line
.Bd -literal -offset indent
add default HISADDR
.Ed
.Pp
to
.Pa /etc/ppp/ppp.conf
(or to
.Pa /etc/ppp/ppp.linkup
for setups that don't use
.Fl auto
mode).
.Pp
This tells
.Nm
to add a default route to whatever the peer address is
(10.0.0.2 in this example).
This route is
.Sq sticky ,
meaning that should the value of
.Dv HISADDR
change, the route will be updated accordingly.
.It
If your provider requests that you use PAP/CHAP authentication methods, add
the next lines to your
.Pa /etc/ppp/ppp.conf
file:
.Bd -literal -offset indent
set authname MyName
set authkey MyPassword
.Ed
.Pp
Both are accepted by default, so
.Nm
will provide whatever your ISP requires.
.Pp
It should be noted that a login script is rarely (if ever) required
when PAP or CHAP are in use.
.It
Ask your ISP to authenticate your nameserver address(es) with the line
.Bd -literal -offset indent
enable dns
.Ed
.Pp
Do
.Em NOT
do this if you are running a local DNS unless you also either use
.Dq resolv readonly
or have
.Dq resolv restore
in
.Pa /etc/ppp/ppp.linkdown ,
as
.Nm
will simply circumvent its use by entering some nameserver lines in
.Pa /etc/resolv.conf .
.El
.Pp
Please refer to
.Pa /usr/share/examples/ppp/ppp.conf.sample
and
.Pa /usr/share/examples/ppp/ppp.linkup.sample
for some real examples.
The pmdemand label should be appropriate for most ISPs.
.Sh LOGGING FACILITY
.Nm
is able to generate the following log info either via
.Xr syslog 3
or directly to the screen:
.Pp
.Bl -tag -width XXXXXXXXX -offset XXX -compact
.It Li All
Enable all logging facilities.
This generates a lot of log.
The most common use of 'all' is as a basis, where you remove some facilities
after enabling 'all' ('debug' and 'timer' are usually best disabled.)
.It Li Async
Dump async level packet in hex.
.It Li CBCP
Generate CBCP (CallBack Control Protocol) logs.
.It Li CCP
Generate a CCP packet trace.
.It Li Chat
Generate
.Sq dial ,
.Sq login ,
.Sq logout
and
.Sq hangup
chat script trace logs.
.It Li Command
Log commands executed either from the command line or any of the configuration
files.
.It Li Connect
Log Chat lines containing the string "CONNECT".
.It Li Debug
Log debug information.
.It Li DNS
Log DNS QUERY packets.
.It Li Filter
Log packets permitted by the dial filter and denied by any filter.
.It Li HDLC
Dump HDLC packet in hex.
.It Li ID0
Log all function calls specifically made as user id 0.
.It Li IPCP
Generate an IPCP packet trace.
.It Li LCP
Generate an LCP packet trace.
.It Li LQM
Generate LQR reports.
.It Li Phase
Phase transition log output.
.It Li Physical
Dump physical level packet in hex.
.It Li Sync
Dump sync level packet in hex.
.It Li TCP/IP
Dump all TCP/IP packets.
.It Li Timer
Log timer manipulation.
.It Li TUN
Include the tun device on each log line.
.It Li Warning
Output to the terminal device.
If there is currently no terminal,
output is sent to the log file using syslogs
.Dv LOG_WARNING .
.It Li Error
Output to both the terminal device
and the log file using syslogs
.Dv LOG_ERROR .
.It Li Alert
Output to the log file using
.Dv LOG_ALERT .
.El
.Pp
The
.Dq set log
command allows you to set the logging output level.
Multiple levels can be specified on a single command line.
The default is equivalent to
.Dq set log Phase .
.Pp
It is also possible to log directly to the screen.
The syntax is the same except that the word
.Dq local
should immediately follow
.Dq set log .
The default is
.Dq set log local
(i.e., only the un-maskable warning, error and alert output).
.Pp
If The first argument to
.Dq set log Op local
begins with a
.Sq +
or a
.Sq -
character, the current log levels are
not cleared, for example:
.Bd -literal -offset indent
PPP ON awfulhak> set log phase
PPP ON awfulhak> show log
Log:   Phase Warning Error Alert
Local: Warning Error Alert
PPP ON awfulhak> set log +tcp/ip -warning
PPP ON awfulhak> set log local +command
PPP ON awfulhak> show log
Log:   Phase TCP/IP Warning Error Alert
Local: Command Warning Error Alert
.Ed
.Pp
Log messages of level Warning, Error and Alert are not controllable
using
.Dq set log Op local .
.Pp
The
.Ar Warning
level is special in that it will not be logged if it can be displayed
locally.
.Sh SIGNAL HANDLING
.Nm
deals with the following signals:
.Bl -tag -width "USR2"
.It INT
Receipt of this signal causes the termination of the current connection
(if any).
This will cause
.Nm
to exit unless it is in
.Fl auto
or
.Fl ddial
mode.
.It HUP, TERM & QUIT
These signals tell
.Nm
to exit.
.It USR1
This signal, tells
.Nm
to re-open any existing server socket, dropping all existing diagnostic
connections.
Sockets that couldn't previously be opened will be retried.
.It USR2
This signal, tells
.Nm
to close any existing server socket, dropping all existing diagnostic
connections.
.Dv SIGUSR1
can still be used to re-open the socket.
.El
.Sh MULTI-LINK PPP
If you wish to use more than one physical link to connect to a
.Em PPP
peer, that peer must also understand the
.Em MULTI-LINK PPP
protocol.
Refer to RFC 1990 for specification details.
.Pp
The peer is identified using a combination of his
.Dq endpoint discriminator
and his
.Dq authentication id .
Either or both of these may be specified.
It is recommended that
at least one is specified, otherwise there is no way of ensuring that
all links are actually connected to the same peer program, and some
confusing lock-ups may result.
Locally, these identification variables are specified using the
.Dq set enddisc
and
.Dq set authname
commands.
The
.Sq authname
(and
.Sq authkey )
must be agreed in advance with the peer.
.Pp
Multi-link capabilities are enabled using the
.Dq set mrru
command (set maximum reconstructed receive unit).
Once multi-link is enabled,
.Nm
will attempt to negotiate a multi-link connection with the peer.
.Pp
By default, only one
.Sq link
is available
(called
.Sq deflink ) .
To create more links, the
.Dq clone
command is used.
This command will clone existing links, where all
characteristics are the same except:
.Bl -enum
.It
The new link has its own name as specified on the
.Dq clone
command line.
.It
The new link is an
.Sq interactive
link.
Its mode may subsequently be changed using the
.Dq set mode
command.
.It
The new link is in a
.Sq closed
state.
.El
.Pp
A summary of all available links can be seen using the
.Dq show links
command.
.Pp
Once a new link has been created, command usage varies.
All link specific commands must be prefixed with the
.Dq link Ar name
command, specifying on which link the command is to be applied.
When only a single link is available,
.Nm
is smart enough not to require the
.Dq link Ar name
prefix.
.Pp
Some commands can still be used without specifying a link - resulting
in an operation at the
.Sq bundle
level.
For example, once two or more links are available, the command
.Dq show ccp
will show CCP configuration and statistics at the multi-link level, and
.Dq link deflink show ccp
will show the same information at the
.Dq deflink
link level.
.Pp
Armed with this information, the following configuration might be used:
.Pp
.Bd -literal -offset indent
mp:
 set timeout 0
 set log phase chat
 set device /dev/cuaa0 /dev/cuaa1 /dev/cuaa2
 set phone "123456789"
 set dial "ABORT BUSY ABORT NO\\sCARRIER TIMEOUT 5 \\"\\" ATZ \e
           OK-AT-OK \\\\dATDT\\\\T TIMEOUT 45 CONNECT"
 set login
 set ifaddr 10.0.0.1/0 10.0.0.2/0 0.0.0.0 0.0.0.0
 set authname ppp
 set authkey ppppassword

 set mrru 1500
 clone 1,2,3		# Create 3 new links - duplicates of the default
 link deflink remove	# Delete the default link (called ``deflink'')
.Ed
.Pp
Note how all cloning is done at the end of the configuration.
Usually, the link will be configured first, then cloned.
If you wish all links
to be up all the time, you can add the following line to the end of your
configuration.
.Pp
.Bd -literal -offset indent
  link 1,2,3 set mode ddial
.Ed
.Pp
If you want the links to dial on demand, this command could be used:
.Pp
.Bd -literal -offset indent
  link * set mode auto
.Ed
.Pp
Links may be tied to specific names by removing the
.Dq set device
line above, and specifying the following after the
.Dq clone
command:
.Pp
.Bd -literal -offset indent
 link 1 set device /dev/cuaa0
 link 2 set device /dev/cuaa1
 link 3 set device /dev/cuaa2
.Ed
.Pp
Use the
.Dq help
command to see which commands require context (using the
.Dq link
command), which have optional
context and which should not have any context.
.Pp
When
.Nm
has negotiated
.Em MULTI-LINK
mode with the peer, it creates a local domain socket in the
.Pa /var/run
directory.
This socket is used to pass link information (including
the actual link file descriptor) between different
.Nm
invocations.
This facilitates
.Nm Ns No 's
ability to be run from a
.Xr getty 8
or directly from
.Pa /etc/gettydefs
(using the
.Sq pp=
capability), without needing to have initial control of the serial
line.
Once
.Nm
negotiates multi-link mode, it will pass its open link to any
already running process.
If there is no already running process,
.Nm
will act as the master, creating the socket and listening for new
connections.
.Sh PPP COMMAND LIST
This section lists the available commands and their effect.
They are usable either from an interactive
.Nm
session, from a configuration file or from a
.Xr pppctl 8
or
.Xr telnet 1
session.
.Bl -tag -width 2n
.It accept|deny|enable|disable Ar option....
These directives tell
.Nm
how to negotiate the initial connection with the peer.
Each
.Dq option
has a default of either accept or deny and enable or disable.
.Dq Accept
means that the option will be ACK'd if the peer asks for it.
.Dq Deny
means that the option will be NAK'd if the peer asks for it.
.Dq Enable
means that the option will be requested by us.
.Dq Disable
means that the option will not be requested by us.
.Pp
.Dq Option
may be one of the following:
.Bl -tag -width 2n
.It acfcomp
Default: Enabled and Accepted.
ACFComp stands for Address and Control Field Compression.
Non LCP packets will usually have an address
field of 0xff (the All-Stations address) and a control field of
0x03 (the Unnumbered Information command).
If this option is
negotiated, these two bytes are simply not sent, thus minimising
traffic.
.Pp
See
.Pa rfc1662
for details.
.It chap Ns Op \&05
Default: Disabled and Accepted.
CHAP stands for Challenge Handshake Authentication Protocol.
Only one of CHAP and PAP (below) may be negotiated.
With CHAP, the authenticator sends a "challenge" message to its peer.
The peer uses a one-way hash function to encrypt the
challenge and sends the result back.
The authenticator does the same, and compares the results.
The advantage of this mechanism is that no
passwords are sent across the connection.
A challenge is made when the connection is first made.
Subsequent challenges may occur.
If you want to have your peer authenticate itself, you must
.Dq enable chap .
in
.Pa /etc/ppp/ppp.conf ,
and have an entry in
.Pa /etc/ppp/ppp.secret
for the peer.
.Pp
When using CHAP as the client, you need only specify
.Dq AuthName
and
.Dq AuthKey
in
.Pa /etc/ppp/ppp.conf .
CHAP is accepted by default.
Some
.Em PPP
implementations use "MS-CHAP" rather than MD5 when encrypting the
challenge.
MS-CHAP is a combination of MD4 and DES.
If
.Nm
was built on a machine with DES libraries available, it will respond
to MS-CHAP authentication requests, but will never request them.
.It deflate
Default: Enabled and Accepted.
This option decides if deflate
compression will be used by the Compression Control Protocol (CCP).
This is the same algorithm as used by the
.Xr gzip 1
program.
Note: There is a problem negotiating
.Ar deflate
capabilities with
.Xr pppd 8
- a
.Em PPP
implementation available under many operating systems.
.Nm pppd
(version 2.3.1) incorrectly attempts to negotiate
.Ar deflate
compression using type
.Em 24
as the CCP configuration type rather than type
.Em 26
as specified in
.Pa rfc1979 .
Type
.Ar 24
is actually specified as
.Dq PPP Magna-link Variable Resource Compression
in
.Pa rfc1975 !
.Nm
is capable of negotiating with
.Nm pppd ,
but only if
.Dq deflate24
is
.Ar enable Ns No d
and
.Ar accept Ns No ed .
.It deflate24
Default: Disabled and Denied.
This is a variance of the
.Ar deflate
option, allowing negotiation with the
.Xr pppd 8
program.
Refer to the
.Ar deflate
section above for details.
It is disabled by default as it violates
.Pa rfc1975 .
.It dns
Default: Disabled and Denied.
This option allows DNS negotiation.
.Pp
If
.Dq enable Ns No d,
.Nm
will request that the peer confirms the entries in
.Pa /etc/resolv.conf .
If the peer NAKs our request (suggesting new IP numbers),
.Pa /etc/resolv.conf
is updated and another request is sent to confirm the new entries.
.Pp
If
.Dq accept Ns No ed,
.Nm
will answer any DNS queries requested by the peer rather than rejecting
them.
The answer is taken from
.Pa /etc/resolv.conf
unless the
.Dq set dns
command is used as an override.
.It enddisc
Default: Enabled and Accepted.
This option allows control over whether we
negotiate an endpoint discriminator.
We only send our discriminator if
.Dq set enddisc
is used and
.Ar enddisc
is enabled.
We reject the peers discriminator if
.Ar enddisc
is denied.
.It LANMan|chap80lm
Default: Disabled and Accepted.
The use of this authentication protocol
is discouraged as it partially violates the authentication protocol by
implementing two different mechanisms (LANMan & NT) under the guise of
a single CHAP type (0x80).
.Dq LANMan
uses a simple DES encryption mechanism and is the least secure of the
CHAP alternatives (although is still more secure than PAP).
.Pp
Refer to the
.Dq MSChap
description below for more details.
.It lqr
Default: Disabled and Accepted.
This option decides if Link Quality Requests will be sent or accepted.
LQR is a protocol that allows
.Nm
to determine that the link is down without relying on the modems
carrier detect.
When LQR is enabled,
.Nm
sends the
.Em QUALPROTO
option (see
.Dq set lqrperiod
below) as part of the LCP request.
If the peer agrees, both sides will
exchange LQR packets at the agreed frequency, allowing detailed link
quality monitoring by enabling LQM logging.
If the peer doesn't agree,
.Nm
will send ECHO LQR requests instead.
These packets pass no information of interest, but they
.Em MUST
be replied to by the peer.
.Pp
Whether using LQR or ECHO LQR,
.Nm
will abruptly drop the connection if 5 unacknowledged packets have been
sent rather than sending a 6th.
A message is logged at the
.Em PHASE
level, and any appropriate
.Dq reconnect
values are honoured as if the peer were responsible for dropping the
connection.
.It mppe
Default: Enabled and Accepted.
This is Microsoft Point to Point Encryption scheme.
MPPE key size can be
40-, 56- and 128-bits.
Refer to
.Dq set mppe
command.
.It MSChapV2|chap81
Default: Disabled and Accepted.
It is very similar to standard CHAP (type 0x05)
except that it issues challenges of a fixed 16 bytes in length and uses a
combination of MD4, SHA-1 and DES to encrypt the challenge rather than using the
standard MD5 mechanism.
.It MSChap|chap80nt
Default: Disabled and Accepted.
The use of this authentication protocol
is discouraged as it partially violates the authentication protocol by
implementing two different mechanisms (LANMan & NT) under the guise of
a single CHAP type (0x80).
It is very similar to standard CHAP (type 0x05)
except that it issues challenges of a fixed 8 bytes in length and uses a
combination of MD4 and DES to encrypt the challenge rather than using the
standard MD5 mechanism.
CHAP type 0x80 for LANMan is also supported - see
.Dq enable LANMan
for details.
.Pp
Because both
.Dq LANMan
and
.Dq NT
use CHAP type 0x80, when acting as authenticator with both
.Dq enable Ns No d ,
.Nm
will rechallenge the peer up to three times if it responds using the wrong
one of the two protocols.
This gives the peer a chance to attempt using both protocols.
.Pp
Conversely, when
.Nm
acts as the authenticatee with both protocols
.Dq accept Ns No ed ,
the protocols are used alternately in response to challenges.
.Pp
Note: If only LANMan is enabled,
.Xr pppd 8
(version 2.3.5) misbehaves when acting as authenticatee.
It provides both
the NT and the LANMan answers, but also suggests that only the NT answer
should be used.
.It pap
Default: Disabled and Accepted.
PAP stands for Password Authentication Protocol.
Only one of PAP and CHAP (above) may be negotiated.
With PAP, the ID and Password are sent repeatedly to the peer until
authentication is acknowledged or the connection is terminated.
This is a rather poor security mechanism.
It is only performed when the connection is first established.
If you want to have your peer authenticate itself, you must
.Dq enable pap .
in
.Pa /etc/ppp/ppp.conf ,
and have an entry in
.Pa /etc/ppp/ppp.secret
for the peer (although see the
.Dq passwdauth
and
.Dq set radius
options below).
.Pp
When using PAP as the client, you need only specify
.Dq AuthName
and
.Dq AuthKey
in
.Pa /etc/ppp/ppp.conf .
PAP is accepted by default.
.It pred1
Default: Enabled and Accepted.
This option decides if Predictor 1
compression will be used by the Compression Control Protocol (CCP).
.It protocomp
Default: Enabled and Accepted.
This option is used to negotiate
PFC (Protocol Field Compression), a mechanism where the protocol
field number is reduced to one octet rather than two.
.It shortseq
Default: Enabled and Accepted.
This option determines if
.Nm
will request and accept requests for short
(12 bit)
sequence numbers when negotiating multi-link mode.
This is only applicable if our MRRU is set (thus enabling multi-link).
.It vjcomp
Default: Enabled and Accepted.
This option determines if Van Jacobson header compression will be used.
.El
.Pp
The following options are not actually negotiated with the peer.
Therefore, accepting or denying them makes no sense.
.Bl -tag -width 2n
.It filter-decapsulation
Default: Disabled.
When this option is enabled,
.Nm
will examine UDP frames to see if they actually contain a
.Em PPP
frame as their payload.
If this is the case, all filters will operate on the payload rather
than the actual packet.
.Pp
This is useful if you want to send PPPoUDP traffic over a
.Em PPP
link, but want that link to do smart things with the real data rather than
the UDP wrapper.
.Pp
The UDP frame payload must not be compressed in any way, otherwise
.Nm
will not be able to interpret it.
It's therefore recommended that you
.Ic disable vj pred1 deflate
and
.Ic deny vj pred1 deflate
in the configuration for the
.Nm
invocation with the udp link.
.It idcheck
Default: Enabled.
When
.Nm
exchanges low-level LCP, CCP and IPCP configuration traffic, the
.Em Identifier
field of any replies is expected to be the same as that of the request.
By default,
.Nm
drops any reply packets that do not contain the expected identifier
field, reporting the fact at the respective log level.
If
.Ar idcheck
is disabled,
.Nm
will ignore the identifier field.
.It iface-alias
Default: Enabled if
.Fl nat
is specified.
This option simply tells
.Nm
to add new interface addresses to the interface rather than replacing them.
The option can only be enabled if network address translation is enabled
.Pq Dq nat enable yes .
.Pp
With this option enabled,
.Nm
will pass traffic for old interface addresses through the NAT
ifdef({LOCALNAT},{engine,},{engine
(see
.Xr libalias 3 ) ,})
resulting in the ability (in
.Fl auto
mode) to properly connect the process that caused the PPP link to
come up in the first place.
.Pp
Disabling NAT with
.Dq nat enable no
will also disable
.Sq iface-alias .
.It ipcp
Default: Enabled.
This option allows
.Nm
to attempt to negotiate IP control protocol capabilities and if
successful to exchange IP datagrams with the peer.
.It ipv6cp
Default: Enabled.
This option allows
.Nm
to attempt to negotiate IPv6 control protocol capabilities and if
successful to exchange IPv6 datagrams with the peer.
.It keep-session
Default: Disabled.
When
.Nm
runs as a Multi-link server, a different
.Nm
instance initially receives each connection.
After determining that
the link belongs to an already existing bundle (controlled by another
.Nm
invocation),
.Nm
will transfer the link to that process.
.Pp
If the link is a tty device or if this option is enabled,
.Nm
will not exit, but will change its process name to
.Dq session owner
and wait for the controlling
.Nm
to finish with the link and deliver a signal back to the idle process.
This prevents the confusion that results from
.Nm Ns No 's
parent considering the link resource available again.
.Pp
For tty devices that have entries in
.Pa /etc/ttys ,
this is necessary to prevent another
.Xr getty 8
from being started, and for program links such as
.Xr sshd 8 ,
it prevents
.Xr sshd 8
from exiting due to the death of its child.
As
.Nm
cannot determine its parents requirements (except for the tty case), this
option must be enabled manually depending on the circumstances.
.It loopback
Default: Enabled.
When
.Ar loopback
is enabled,
.Nm
will automatically loop back packets being sent
out with a destination address equal to that of the
.Em PPP
interface.
If disabled,
.Nm
will send the packet, probably resulting in an ICMP redirect from
the other end.
It is convenient to have this option enabled when
the interface is also the default route as it avoids the necessity
of a loopback route.
.It passwdauth
Default: Disabled.
Enabling this option will tell the PAP authentication
code to use the password database (see
.Xr passwd 5 )
to authenticate the caller if they cannot be found in the
.Pa /etc/ppp/ppp.secret
file.
.Pa /etc/ppp/ppp.secret
is always checked first.
If you wish to use passwords from
.Xr passwd 5 ,
but also to specify an IP number or label for a given client, use
.Dq \&*
as the client password in
.Pa /etc/ppp/ppp.secret .
.It proxy
Default: Disabled.
Enabling this option will tell
.Nm
to proxy ARP for the peer.
This means that
.Nm
will make an entry in the ARP table using
.Dv HISADDR
and the
.Dv MAC
address of the local network in which
.Dv HISADDR
appears.
This allows other machines connecteed to the LAN to talk to
the peer as if the peer itself was connected to the LAN.
The proxy entry cannot be made unless
.Dv HISADDR
is an address from a LAN.
.It proxyall
Default: Disabled.
Enabling this will tell
.Nm
to add proxy arp entries for every IP address in all class C or
smaller subnets routed via the tun interface.
.Pp
Proxy arp entries are only made for sticky routes that are added
using the
.Dq add
command.
No proxy arp entries are made for the interface address itself
(as created by the
.Dq set ifaddr
command).
.It sroutes
Default: Enabled.
When the
.Dq add
command is used with the
.Dv HISADDR ,
.Dv MYADDR ,
.Dv HISADDR6
or
.Dv MYADDR6
values, entries are stored in the
.Sq sticky route
list.
Each time these variables change, this list is re-applied to the routing table.
.Pp
Disabling this option will prevent the re-application of sticky routes,
although the
.Sq stick route
list will still be maintained.
.It Op tcp Ns Xo
.No mssfixup
.Xc
Default: Enabled.
This option tells
.Nm
to adjust TCP SYN packets so that the maximum receive segment
size is not greater than the amount allowed by the interface MTU.
.It throughput
Default: Enabled.
This option tells
.Nm
to gather throughput statistics.
Input and output is sampled over
a rolling 5 second window, and current, best and total figures are retained.
This data is output when the relevant
.Em PPP
layer shuts down, and is also available using the
.Dq show
command.
Throughput statistics are available at the
.Dq IPCP
and
.Dq physical
levels.
.It utmp
Default: Enabled.
Normally, when a user is authenticated using PAP or CHAP, and when
.Nm
is running in
.Fl direct
mode, an entry is made in the utmp and wtmp files for that user.
Disabling this option will tell
.Nm
not to make any utmp or wtmp entries.
This is usually only necessary if
you require the user to both login and authenticate themselves.
.El
.Pp
.It add Ns Xo
.Op !\&
.Ar dest Ns Op / Ns Ar nn
.Op Ar mask
.Op Ar gateway
.Xc
.Ar Dest
is the destination IP address.
The netmask is specified either as a number of bits with
.Ar /nn
or as an IP number using
.Ar mask .
.Ar 0 0
or simply
.Ar 0
with no mask refers to the default route.
It is also possible to use the literal name
.Sq default
instead of
.Ar 0 .
.Ar Gateway
is the next hop gateway to get to the given
.Ar dest
machine/network.
Refer to the
.Xr route 8
command for further details.
.Pp
It is possible to use the symbolic names
.Sq MYADDR ,
.Sq HISADDR ,
.Sq MYADDR6
or
.Sq HISADDR6
as the destination, and
.Sq HISADDR
or
.Sq HISADDR6
as the
.Ar gateway .
.Sq MYADDR
is replaced with the interface IP address,
.Sq HISADDR
is replaced with the interface IP destination (peer) address,
.Sq MYADDR6
is replaced with the interface IPv6 address, and
.Sq HISADDR6
is replaced with the interface IPv6 destination address,
.Pp
If the
.Ar add!\&
command is used
(note the trailing
.Dq !\& ) ,
then if the route already exists, it will be updated as with the
.Sq route change
command (see
.Xr route 8
for further details).
.Pp
Routes that contain the
.Dq HISADDR ,
.Dq MYADDR ,
.Dq HISADDR6 ,
.Dq MYADDR6 ,
.Dq DNS0 ,
or
.Dq DNS1
constants are considered
.Sq sticky .
They are stored in a list (use
.Dq show ncp
to see the list), and each time the value of one of these variables
changes, the appropriate routing table entries are updated.
This facility may be disabled using
.Dq disable sroutes .
.It allow Ar command Op Ar args
This command controls access to
.Nm
and its configuration files.
It is possible to allow user-level access,
depending on the configuration file label and on the mode that
.Nm
is being run in.
For example, you may wish to configure
.Nm
so that only user
.Sq fred
may access label
.Sq fredlabel
in
.Fl background
mode.
.Pp
User id 0 is immune to these commands.
.Bl -tag -width 2n
.It allow user Ns Xo
.Op s
.Ar logname Ns No ...
.Xc
By default, only user id 0 is allowed access to
.Nm .
If this command is used, all of the listed users are allowed access to
the section in which the
.Dq allow users
command is found.
The
.Sq default
section is always checked first (even though it is only ever automatically
loaded at startup).
.Dq allow users
commands are cumulative in a given section, but users allowed in any given
section override users allowed in the default section, so it's possible to
allow users access to everything except a given label by specifying default
users in the
.Sq default
section, and then specifying a new user list for that label.
.Pp
If user
.Sq *
is specified, access is allowed to all users.
.It allow mode Ns Xo
.Op s
.Ar mode Ns No ...
.Xc
By default, access using any
.Nm
mode is possible.
If this command is used, it restricts the access
.Ar modes
allowed to load the label under which this command is specified.
Again, as with the
.Dq allow users
command, each
.Dq allow modes
command overrides any previous settings, and the
.Sq default
section is always checked first.
.Pp
Possible modes are:
.Sq interactive ,
.Sq auto ,
.Sq direct ,
.Sq dedicated ,
.Sq ddial ,
.Sq background
and
.Sq * .
.Pp
When running in multi-link mode, a section can be loaded if it allows
.Em any
of the currently existing line modes.
.El
.Pp
.It nat Ar command Op Ar args
This command allows the control of the network address translation (also
known as masquerading or IP aliasing) facilities that are built into
.Nm .
NAT is done on the external interface only, and is unlikely to make sense
if used with the
.Fl direct
flag.
.Pp
If nat is enabled on your system (it may be omitted at compile time),
the following commands are possible:
.Bl -tag -width 2n
.It nat enable yes|no
This command either switches network address translation on or turns it off.
The
.Fl nat
command line flag is synonymous with
.Dq nat enable yes .
.It nat addr Op Ar addr_local addr_alias
This command allows data for
.Ar addr_alias
to be redirected to
.Ar addr_local .
It is useful if you own a small number of real IP numbers that
you wish to map to specific machines behind your gateway.
.It nat deny_incoming yes|no
If set to yes, this command will refuse all incoming packets where an
aliasing link doesn't already exist.
ifdef({LOCALNAT},{},{Refer to the
.Sx CONCEPTUAL BACKGROUND
section of
.Xr libalias 3
for a description of what an
.Dq aliasing link
is.
})dnl
.Pp
It should be noted under what circumstances an aliasing link is
ifdef({LOCALNAT},{created.},{created by
.Xr libalias 3 .})
It may be necessary to further protect your network from outside
connections using the
.Dq set filter
or
.Dq nat target
commands.
.It nat help|?
This command gives a summary of available nat commands.
.It nat log yes|no
This option causes various NAT statistics and information to
be logged to the file
.Pa /var/log/alias.log .
.It nat port Ar proto Ar targetIP Ns Xo
.No : Ns Ar targetPort Ns
.Oo
.No - Ns Ar targetPort
.Oc Ar aliasPort Ns
.Oo
.No - Ns Ar aliasPort
.Oc Oo Ar remoteIP : Ns
.Ar remotePort Ns
.Oo
.No - Ns Ar remotePort
.Oc Ns
.Oc
.Xc
This command causes incoming
.Ar proto
connections to
.Ar aliasPort
to be redirected to
.Ar targetPort
on
.Ar targetIP .
.Ar proto
is either
.Dq tcp
or
.Dq udp .
.Pp
A range of port numbers may be specified as shown above.
The ranges must be of the same size.
.Pp
If
.Ar remoteIP
is specified, only data coming from that IP number is redirected.
.Ar remotePort
must either be
.Dq 0
(indicating any source port)
or a range of ports the same size as the other ranges.
.Pp
This option is useful if you wish to run things like Internet phone on
machines behind your gateway, but is limited in that connections to only
one interior machine per source machine and target port are possible.
.It nat proto Ar proto localIP Oo
.Ar publicIP Op Ar remoteIP
.Oc
This command tells
.Nm
to redirect packets of protocol type
.Ar proto
(see
.Xr protocols 5 )
to the internal address
.Ar localIP .
.Pp
If
.Ar publicIP
is specified, only packets destined for that address are matched,
otherwise the default alias address is used.
.Pp
If
.Ar remoteIP
is specified, only packets matching that source address are matched,
.Pp
This command is useful for redirecting tunnel endpoints to an internal machine,
for example:
.Pp
.Dl nat proto ipencap 10.0.0.1
.It "nat proxy cmd" Ar arg Ns No ...
This command tells
.Nm
to proxy certain connections, redirecting them to a given server.
ifdef({LOCALNAT},{},{Refer to the description of
.Fn PacketAliasProxyRule
in
.Xr libalias 3
for details of the available commands.
})dnl
.It nat punch_fw Op Ar base count
This command tells
.Nm
to punch holes in the firewall for FTP or IRC DCC connections.
This is done dynamically by installing termporary firewall rules which
allow a particular connection (and only that connection) to go through
the firewall.
The rules are removed once the corresponding connection terminates.
.Pp
A maximum of
.Ar count
rules starting from rule number
.Ar base
will be used for punching firewall holes.
The range will be cleared when the
.Dq nat punch_fw
command is run.
.Pp
If no arguments are given, firewall punching is disabled.
.It nat same_ports yes|no
When enabled, this command will tell the network address translation engine to
attempt to avoid changing the port number on outgoing packets.
This is useful
if you want to support protocols such as RPC and LPD which require
connections to come from a well known port.
.It nat target Op Ar address
Set the given target address or clear it if no address is given.
The target address is used
ifdef({LOCALNAT},{},{by libalias })dnl
to specify how to NAT incoming packets by default.
If a target address is not set or if
.Dq default
is given, packets are not altered and are allowed to route to the internal
network.
.Pp
The target address may be set to
.Dq MYADDR ,
in which case
ifdef({LOCALNAT},{all packets will be redirected},
{libalias will redirect all packets})
to the interface address.
.It nat use_sockets yes|no
When enabled, this option tells the network address translation engine to
create a socket so that it can guarantee a correct incoming ftp data or
IRC connection.
.It nat unregistered_only yes|no
Only alter outgoing packets with an unregistered source address.
According to RFC 1918, unregistered source addresses
are 10.0.0.0/8, 172.16.0.0/12 and 192.168.0.0/16.
.El
.Pp
These commands are also discussed in the file
.Pa README.nat
which comes with the source distribution.
.Pp
.It Op !\& Ns Xo
.No bg Ar command
.Xc
The given
.Ar command
is executed in the background with the following words replaced:
.Bl -tag -width COMPILATIONDATE
.It Li AUTHNAME
This is replaced with the local
.Ar authname
value.
See the
.Dq set authname
command below.
.It Li COMPILATIONDATE
This is replaced with the date on which
.Nm
was compiled.
.It Li DNS0 & DNS1
These are replaced with the primary and secondary nameserver IP numbers.
If nameservers are negotiated by IPCP, the values of these macros will change.
.It Li ENDDISC
This is replaced with the local endpoint discriminator value.
See the
.Dq set enddisc
command below.
.It Li HISADDR
This is replaced with the peers IP number.
.It Li HISADDR6
This is replaced with the peers IPv6 number.
.It Li INTERFACE
This is replaced with the name of the interface that's in use.
.It Li IPOCTETSIN
This is replaced with the number of IP bytes received since the connection
was established.
.It Li IPOCTETSOUT
This is replaced with the number of IP bytes sent since the connection
was established.
.It Li IPPACKETSIN
This is replaced with the number of IP packets received since the connection
was established.
.It Li IPPACKETSOUT
This is replaced with the number of IP packets sent since the connection
was established.
.It Li IPV6OCTETSIN
This is replaced with the number of IPv6 bytes received since the connection
was established.
.It Li IPV6OCTETSOUT
This is replaced with the number of IPv6 bytes sent since the connection
was established.
.It Li IPV6PACKETSIN
This is replaced with the number of IPv6 packets received since the connection
was established.
.It Li IPV6PACKETSOUT
This is replaced with the number of IPv6 packets sent since the connection
was established.
.It Li LABEL
This is replaced with the last label name used.
A label may be specified on the
.Nm
command line, via the
.Dq load
or
.Dq dial
commands and in the
.Pa ppp.secret
file.
.It Li MYADDR
This is replaced with the IP number assigned to the local interface.
.It Li MYADDR6
This is replaced with the IPv6 number assigned to the local interface.
.It Li OCTETSIN
This is replaced with the number of bytes received since the connection
was established.
.It Li OCTETSOUT
This is replaced with the number of bytes sent since the connection
was established.
.It Li PACKETSIN
This is replaced with the number of packets received since the connection
was established.
.It Li PACKETSOUT
This is replaced with the number of packets sent since the connection
was established.
.It Li PEER_ENDDISC
This is replaced with the value of the peers endpoint discriminator.
.It Li PROCESSID
This is replaced with the current process id.
.It Li SOCKNAME
This is replaced with the name of the diagnostic socket.
.It Li UPTIME
This is replaced with the bundle uptime in HH:MM:SS format.
.It Li USER
This is replaced with the username that has been authenticated with PAP or
CHAP.
Normally, this variable is assigned only in -direct mode.
This value is available irrespective of whether utmp logging is enabled.
.It Li VERSION
This is replaced with the current version number of
.Nm .
.El
.Pp
These substitutions are also done by the
.Dq set proctitle ,
.Dq ident
and
.Dq log
commands.
.Pp
If you wish to pause
.Nm
while the command executes, use the
.Dq shell
command instead.
.It clear physical|ipcp|ipv6 Op current|overall|peak...
Clear the specified throughput values at either the
.Dq physical ,
.Dq ipcp
or
.Dq ipv6cp
level.
If
.Dq physical
is specified, context must be given (see the
.Dq link
command below).
If no second argument is given, all values are cleared.
.It clone Ar name Ns Xo
.Op \&, Ns Ar name Ns
.No ...
.Xc
Clone the specified link, creating one or more new links according to the
.Ar name
argument(s).
This command must be used from the
.Dq link
command below unless you've only got a single link (in which case that
link becomes the default).
Links may be removed using the
.Dq remove
command below.
.Pp
The default link name is
.Dq deflink .
.It close Op lcp|ccp Ns Op !\&
If no arguments are given, the relevant protocol layers will be brought
down and the link will be closed.
If
.Dq lcp
is specified, the LCP layer is brought down, but
.Nm
will not bring the link offline.
It is subsequently possible to use
.Dq term
(see below)
to talk to the peer machine if, for example, something like
.Dq slirp
is being used.
If
.Dq ccp
is specified, only the relevant compression layer is closed.
If the
.Dq !\&
is used, the compression layer will remain in the closed state, otherwise
it will re-enter the STOPPED state, waiting for the peer to initiate
further CCP negotiation.
In any event, this command does not disconnect the user from
.Nm
or exit
.Nm .
See the
.Dq quit
command below.
.It delete Ns Xo
.Op !\&
.Ar dest
.Xc
This command deletes the route with the given
.Ar dest
IP address.
If
.Ar dest
is specified as
.Sq ALL ,
all non-direct entries in the routing table for the current interface,
and all
.Sq sticky route
entries are deleted.
If
.Ar dest
is specified as
.Sq default ,
the default route is deleted.
.Pp
If the
.Ar delete!\&
command is used
(note the trailing
.Dq !\& ) ,
.Nm
will not complain if the route does not already exist.
.It dial|call Op Ar label Ns Xo
.No ...
.Xc
This command is the equivalent of
.Dq load label
followed by
.Dq open ,
and is provided for backwards compatibility.
.It down Op Ar lcp|ccp
Bring the relevant layer down ungracefully, as if the underlying layer
had become unavailable.
It's not considered polite to use this command on
a Finite State Machine that's in the OPEN state.
If no arguments are
supplied, the entire link is closed (or if no context is given, all links
are terminated).
If
.Sq lcp
is specified, the
.Em LCP
layer is terminated but the device is not brought offline and the link
is not closed.
If
.Sq ccp
is specified, only the relevant compression layer(s) are terminated.
.It help|? Op Ar command
Show a list of available commands.
If
.Ar command
is specified, show the usage string for that command.
.It ident Op Ar text Ns No ...
Identify the link to the peer using
.Ar text .
If
.Ar text
is empty, link identification is disabled.
It is possible to use any of the words described for the
.Ic bg
command above.
Refer to the
.Ic sendident
command for details of when
.Nm
identifies itself to the peer.
.It iface Ar command Op args
This command is used to control the interface used by
.Nm .
.Ar Command
may be one of the following:
.Bl -tag -width 2n
.It iface add Ns Xo
.Op !\&
.Ar addr Ns Op / Ns Ar bits
.Op Ar peer
.Xc
.It iface add Ns Xo
.Op !\&
.Ar addr
.Ar mask
.Ar peer
.Xc
Add the given
.Ar addr mask peer
combination to the interface.
Instead of specifying
.Ar mask ,
.Ar /bits
can be used
(with no space between it and
.Ar addr ) .
If the given address already exists, the command fails unless the
.Dq !\&
is used - in which case the previous interface address entry is overwritten
with the new one, allowing a change of netmask or peer address.
.Pp
If only
.Ar addr
is specified,
.Ar bits
defaults to
.Dq 32
and
.Ar peer
defaults to
.Dq 255.255.255.255 .
This address (the broadcast address) is the only duplicate peer address that
.Nm
allows.
.It iface clear Op INET | INET6
If this command is used while
.Nm
is in the OPENED state or while in
.Fl auto
mode, all addresses except for the NCP negotiated address are deleted
from the interface.
If
.Nm
is not in the OPENED state and is not in
.Fl auto
mode, all interface addresses are deleted.
.Pp
If the INET or INET6 arguments are used, only addresses for that address
family are cleared.
.Pp
.It iface delete Ns Xo
.Op !\& Ns
.No |rm Ns Op !\&
.Ar addr
.Xc
This command deletes the given
.Ar addr
from the interface.
If the
.Dq !\&
is used, no error is given if the address isn't currently assigned to
the interface (and no deletion takes place).
.It iface show
Shows the current state and current addresses for the interface.
It is much the same as running
.Dq ifconfig INTERFACE .
.It iface help Op Ar sub-command
This command, when invoked without
.Ar sub-command ,
will show a list of possible
.Dq iface
sub-commands and a brief synopsis for each.
When invoked with
.Ar sub-command ,
only the synopsis for the given sub-command is shown.
.El
.It Op data Ns Xo
.No link
.Ar name Ns Op , Ns Ar name Ns
.No ... Ar command Op Ar args
.Xc
This command may prefix any other command if the user wishes to
specify which link the command should affect.
This is only applicable after multiple links have been created in Multi-link
mode using the
.Dq clone
command.
.Pp
.Ar Name
specifies the name of an existing link.
If
.Ar name
is a comma separated list,
.Ar command
is executed on each link.
If
.Ar name
is
.Dq * ,
.Ar command
is executed on all links.
.It load Op Ar label Ns Xo
.No ...
.Xc
Load the given
.Ar label Ns No (s)
from the
.Pa ppp.conf
file.
If
.Ar label
is not given, the
.Ar default
label is used.
.Pp
Unless the
.Ar label
section uses the
.Dq set mode ,
.Dq open
or
.Dq dial
commands,
.Nm
will not attempt to make an immediate connection.
.It log Ar word Ns No ...
Send the given word(s) to the log file with the prefix
.Dq LOG: .
Word substitutions are done as explained under the
.Dq !bg
command above.
.It open Op lcp|ccp|ipcp
This is the opposite of the
.Dq close
command.
All closed links are immediately brought up apart from second and subsequent
.Ar demand-dial
links - these will come up based on the
.Dq set autoload
command that has been used.
.Pp
If the
.Dq lcp
argument is used while the LCP layer is already open, LCP will be
renegotiated.
This allows various LCP options to be changed, after which
.Dq open lcp
can be used to put them into effect.
After renegotiating LCP,
any agreed authentication will also take place.
.Pp
If the
.Dq ccp
argument is used, the relevant compression layer is opened.
Again, if it is already open, it will be renegotiated.
.Pp
If the
.Dq ipcp
argument is used, the link will be brought up as normal, but if
IPCP is already open, it will be renegotiated and the network
interface will be reconfigured.
.Pp
It is probably not good practice to re-open the PPP state machines
like this as it's possible that the peer will not behave correctly.
It
.Em is
however useful as a way of forcing the CCP or VJ dictionaries to be reset.
.It passwd Ar pass
Specify the password required for access to the full
.Nm
command set.
This password is required when connecting to the diagnostic port (see the
.Dq set server
command).
.Ar Pass
is specified on the
.Dq set server
command line.
The value of
.Ar pass
is not logged when
.Ar command
logging is active, instead, the literal string
.Sq ********
is logged.
.It quit|bye Op all
If
.Dq quit
is executed from the controlling connection or from a command file,
ppp will exit after closing all connections.
Otherwise, if the user
is connected to a diagnostic socket, the connection is simply dropped.
.Pp
If the
.Ar all
argument is given,
.Nm
will exit despite the source of the command after closing all existing
connections.
.It remove|rm
This command removes the given link.
It is only really useful in multi-link mode.
A link must be in the
.Dv CLOSED
state before it is removed.
.It rename|mv Ar name
This command renames the given link to
.Ar name .
It will fail if
.Ar name
is already used by another link.
.Pp
The default link name is
.Sq deflink .
Renaming it to
.Sq modem ,
.Sq cuaa0
or
.Sq USR
may make the log file more readable.
.It resolv Ar command
This command controls
.Nm Ns No 's
manipulation of the
.Xr resolv.conf 5
file.
When
.Nm
starts up, it loads the contents of this file into memory and retains this
image for future use.
.Ar command
is one of the following:
.Bl -tag -width readonly
.It Em readonly
Treat
.Pa /etc/resolv.conf
as read only.
If
.Dq dns
is enabled,
.Nm
will still attempt to negotiate nameservers with the peer, making the results
available via the
.Dv DNS0
and
.Dv DNS1
macros.
This is the opposite of the
.Dq resolv writable
command.
.It Em reload
Reload
.Pa /etc/resolv.conf
into memory.
This may be necessary if for example a DHCP client overwrote
.Pa /etc/resolv.conf .
.It Em restore
Replace
.Pa /etc/resolv.conf
with the version originally read at startup or with the last
.Dq resolv reload
command.
This is sometimes a useful command to put in the
.Pa /etc/ppp/ppp.linkdown
file.
.It Em rewrite
Rewrite the
.Pa /etc/resolv.conf
file.
This command will work even if the
.Dq resolv readonly
command has been used.
It may be useful as a command in the
.Pa /etc/ppp/ppp.linkup
file if you wish to defer updating
.Pa /etc/resolv.conf
until after other commands have finished.
.It Em writable
Allow
.Nm
to update
.Pa /etc/resolv.conf
if
.Dq dns
is enabled and
.Nm
successfully negotiates a DNS.
This is the opposite of the
.Dq resolv readonly
command.
.El
.It save
This option is not (yet) implemented.
.It sendident
This command tells
.Nm
to identify itself to the peer.
The link must be in LCP state or higher.
If no identity has been set (via the
.Ic ident
command),
.Ic sendident
will fail.
.Pp
When an identity has been set,
.Nm
will automatically identify itself when it sends or receives a configure
reject, when negotiation fails or when LCP reaches the opened state.
.Pp
Received identification packets are logged to the LCP log (see
.Ic set log
for details) and are never responded to.
.It set Ns Xo
.Op up
.Ar var value
.Xc
This option allows the setting of any of the following variables:
.Bl -tag -width 2n
.It set accmap Ar hex-value
ACCMap stands for Asynchronous Control Character Map.
This is always
negotiated with the peer, and defaults to a value of 00000000 in hex.
This protocol is required to defeat hardware that depends on passing
certain characters from end to end (such as XON/XOFF etc).
.Pp
For the XON/XOFF scenario, use
.Dq set accmap 000a0000 .
.It set Op auth Ns Xo
.No key Ar value
.Xc
This sets the authentication key (or password) used in client mode
PAP or CHAP negotiation to the given value.
It also specifies the
password to be used in the dial or login scripts in place of the
.Sq \eP
sequence, preventing the actual password from being logged.
If
.Ar command
or
.Ar chat
logging is in effect,
.Ar value
is logged as
.Sq ********
for security reasons.
.Pp
If the first character of
.Ar value
is an exclamation mark
.Pq Dq !\& ,
.Nm
treats the remainder of the string as a program that must be executed
to determine the
.Dq authname
and
.Dq authkey
values.
.Pp
If the
.Dq !\&
is doubled up
(to
.Dq !! ) ,
it is treated as a single literal
.Dq !\& ,
otherwise, ignoring the
.Dq !\& ,
.Ar value
is parsed as a program to execute in the same was as the
.Dq !bg
command above, substituting special names in the same manner.
Once executed,
.Nm
will feed the program three lines of input, each terminated by a newline
character:
.Bl -bullet
.It
The host name as sent in the CHAP challenge.
.It
The challenge string as sent in the CHAP challenge.
.It
The locally defined
.Dq authname .
.El
.Pp
Two lines of output are expected:
.Bl -bullet
.It
The
.Dq authname
to be sent with the CHAP response.
.It
The
.Dq authkey ,
which is encrypted with the challenge and request id, the answer being sent
in the CHAP response packet.
.El
.Pp
When configuring
.Nm
in this manner, it's expected that the host challenge is a series of ASCII
digits or characters.
An encryption device or Secure ID card is usually
required to calculate the secret appropriate for the given challenge.
.It set authname Ar id
This sets the authentication id used in client mode PAP or CHAP negotiation.
.Pp
If used in
.Fl direct
mode with CHAP enabled,
.Ar id
is used in the initial authentication challenge and should normally be set to
the local machine name.
.It set autoload Xo
.Ar min-percent max-percent period
.Xc
These settings apply only in multi-link mode and default to zero, zero and
five respectively.
When more than one
.Ar demand-dial
(also known as
.Fl auto )
mode link is available, only the first link is made active when
.Nm
first reads data from the tun device.
The next
.Ar demand-dial
link will be opened only when the current bundle throughput is at least
.Ar max-percent
percent of the total bundle bandwidth for
.Ar period
seconds.
When the current bundle throughput decreases to
.Ar min-percent
percent or less of the total bundle bandwidth for
.Ar period
seconds, a
.Ar demand-dial
link will be brought down as long as it's not the last active link.
.Pp
Bundle throughput is measured as the maximum of inbound and outbound
traffic.
.Pp
The default values cause
.Ar demand-dial
links to simply come up one at a time.
.Pp
Certain devices cannot determine their physical bandwidth, so it
is sometimes necessary to use the
.Dq set bandwidth
command (described below) to make
.Dq set autoload
work correctly.
.It set bandwidth Ar value
This command sets the connection bandwidth in bits per second.
.Ar value
must be greater than zero.
It is currently only used by the
.Dq set autoload
command above.
.It set callback Ar option Ns No ...
If no arguments are given, callback is disabled, otherwise,
.Nm
will request (or in
.Fl direct
mode, will accept) one of the given
.Ar option Ns No s .
In client mode, if an
.Ar option
is NAK'd
.Nm
will request a different
.Ar option ,
until no options remain at which point
.Nm
will terminate negotiations (unless
.Dq none
is one of the specified
.Ar option ) .
In server mode,
.Nm
will accept any of the given protocols - but the client
.Em must
request one of them.
If you wish callback to be optional, you must {include}
.Ar none
as an option.
.Pp
The
.Ar option Ns No s
are as follows (in this order of preference):
.Pp
.Bl -tag -width Ds
.It auth
The callee is expected to decide the callback number based on
authentication.
If
.Nm
is the callee, the number should be specified as the fifth field of
the peers entry in
.Pa /etc/ppp/ppp.secret .
.It cbcp
Microsoft's callback control protocol is used.
See
.Dq set cbcp
below.
.Pp
If you wish to negotiate
.Ar cbcp
in client mode but also wish to allow the server to request no callback at
CBCP negotiation time, you must specify both
.Ar cbcp
and
.Ar none
as callback options.
.It E.164 *| Ns Xo
.Ar number Ns Op , Ns Ar number Ns
.No ...
.Xc
The caller specifies the
.Ar number .
If
.Nm
is the callee,
.Ar number
should be either a comma separated list of allowable numbers or a
.Dq \&* ,
meaning any number is permitted.
If
.Nm
is the caller, only a single number should be specified.
.Pp
Note, this option is very unsafe when used with a
.Dq \&*
as a malicious caller can tell
.Nm
to call any (possibly international) number without first authenticating
themselves.
.It none
If the peer does not wish to do callback at all,
.Nm
will accept the fact and continue without callback rather than terminating
the connection.
This is required (in addition to one or more other callback
options) if you wish callback to be optional.
.El
.Pp
.It set cbcp Oo
.No *| Ns Ar number Ns Oo
.No , Ns Ar number Ns ...\& Oc
.Op Ar delay Op Ar retry
.Oc
If no arguments are given, CBCP (Microsoft's CallBack Control Protocol)
is disabled - ie, configuring CBCP in the
.Dq set callback
command will result in
.Nm
requesting no callback in the CBCP phase.
Otherwise,
.Nm
attempts to use the given phone
.Ar number Ns No (s).
.Pp
In server mode
.Pq Fl direct ,
.Nm
will insist that the client uses one of these numbers, unless
.Dq \&*
is used in which case the client is expected to specify the number.
.Pp
In client mode,
.Nm
will attempt to use one of the given numbers (whichever it finds to
be agreeable with the peer), or if
.Dq \&*
is specified,
.Nm
will expect the peer to specify the number.
.It set cd Oo
.No off| Ns Ar seconds Ns Op !\&
.Oc
Normally,
.Nm
checks for the existence of carrier depending on the type of device
that has been opened:
.Bl -tag -width XXX -offset XXX
.It Terminal Devices
Carrier is checked one second after the login script is complete.
If it's not set,
.Nm
assumes that this is because the device doesn't support carrier (which
is true for most
.Dq laplink
NULL-modem cables), logs the fact and stops checking
for carrier.
.Pp
As ptys don't support the TIOCMGET ioctl, the tty device will switch all
carrier detection off when it detects that the device is a pty.
.It ISDN (i4b) Devices
Carrier is checked once per second for 6 seconds.
If it's not set after
the sixth second, the connection attempt is considered to have failed and
the device is closed.
Carrier is always required for i4b devices.
.It PPPoE (netgraph) Devices
Carrier is checked once per second for 5 seconds.
If it's not set after
the fifth second, the connection attempt is considered to have failed and
the device is closed.
Carrier is always required for PPPoE devices.
.El
.Pp
All other device types don't support carrier.
Setting a carrier value will
result in a warning when the device is opened.
.Pp
Some modems take more than one second after connecting to assert the carrier
signal.
If this delay isn't increased, this will result in
.Nm Ns No 's
inability to detect when the link is dropped, as
.Nm
assumes that the device isn't asserting carrier.
.Pp
The
.Dq set cd
command overrides the default carrier behaviour.
.Ar seconds
specifies the maximum number of seconds that
.Nm
should wait after the dial script has finished before deciding if
carrier is available or not.
.Pp
If
.Dq off
is specified,
.Nm
will not check for carrier on the device, otherwise
.Nm
will not proceed to the login script until either carrier is detected
or until
.Ar seconds
has elapsed, at which point
.Nm
assumes that the device will not set carrier.
.Pp
If no arguments are given, carrier settings will go back to their default
values.
.Pp
If
.Ar seconds
is followed immediately by an exclamation mark
.Pq Dq !\& ,
.Nm
will
.Em require
carrier.
If carrier is not detected after
.Ar seconds
seconds, the link will be disconnected.
.It set choked Op Ar timeout
This sets the number of seconds that
.Nm
will keep a choked output queue before dropping all pending output packets.
If
.Ar timeout
is less than or equal to zero or if
.Ar timeout
isn't specified, it is set to the default value of
.Em 120 seconds .
.Pp
A choked output queue occurs when
.Nm
has read a certain number of packets from the local network for transmission,
but cannot send the data due to link failure (the peer is busy etc.).
.Nm
will not read packets indefinitely.
Instead, it reads up to
.Em 30
packets (or
.Em 30 No +
.Em nlinks No *
.Em 2
packets in multi-link mode), then stops reading the network interface
until either
.Ar timeout
seconds have passed or at least one packet has been sent.
.Pp
If
.Ar timeout
seconds pass, all pending output packets are dropped.
.It set ctsrts|crtscts on|off
This sets hardware flow control.
Hardware flow control is
.Ar on
by default.
.It set deflate Ar out-winsize Op Ar in-winsize
This sets the DEFLATE algorithms default outgoing and incoming window
sizes.
Both
.Ar out-winsize
and
.Ar in-winsize
must be values between
.Em 8
and
.Em 15 .
If
.Ar in-winsize
is specified,
.Nm
will insist that this window size is used and will not accept any other
values from the peer.
.It set dns Op Ar primary Op Ar secondary
This command specifies DNS overrides for the
.Dq accept dns
command.
Refer to the
.Dq accept
command description above for details.
This command does not affect the IP numbers requested using
.Dq enable dns .
.It set device|line Xo
.Ar value Ns No ...
.Xc
This sets the device(s) to which
.Nm
will talk to the given
.Dq value .
.Pp
All ISDN and serial device names are expected to begin with
.Pa /dev/ .
ISDN devices are usually called
.Pa i4brbchX
and serial devices are usually called
.Pa cuaXX .
.Pp
If
.Dq value
does not begin with
.Pa /dev/ ,
it must either begin with an exclamation mark
.Pq Dq !\& ,
be of the format
.No PPPoE: Ns Ar iface Ns Xo
.Op \&: Ns Ar provider Ns
.Xc
(on
.Xr netgraph 4
enabled systems), or be of the format
.Sm off
.Ar host : port Op /tcp|udp .
.Sm on
.Pp
If it begins with an exclamation mark, the rest of the device name is
treated as a program name, and that program is executed when the device
is opened.
Standard input, output and error are fed back to
.Nm
and are read and written as if they were a regular device.
.Pp
If a
.No PPPoE: Ns Ar iface Ns Xo
.Op \&: Ns Ar provider Ns
.Xc
specification is given,
.Nm
will attempt to create a
.Em PPP
over Ethernet connection using the given
.Ar iface
interface by using
.Xr netgraph 4 .
If
.Xr netgraph 4
is not available,
.Nm
will attempt to load it using
.Xr kldload 2 .
If this fails, an external program must be used such as the
.Xr pppoe 8
program available under
.Ox .
The given
.Ar provider
is passed as the service name in the PPPoE Discovery Initiation (PADI)
packet.
If no provider is given, an empty value will be used.
.Pp
When a PPPoE connection is established,
.Nm
will place the name of the Access Concentrator in the environment variable
.Ev ACNAME .
.Pp
Refer to
.Xr netgraph 4
and
.Xr ng_pppoe 4
for further details.
.Pp
If a
.Ar host Ns No : Ns Ar port Ns Oo
.No /tcp|udp
.Oc
specification is given,
.Nm
will attempt to connect to the given
.Ar host
on the given
.Ar port .
If a
.Dq /tcp
or
.Dq /udp
suffix is not provided, the default is
.Dq /tcp .
Refer to the section on
.Em PPP OVER TCP and UDP
above for further details.
.Pp
If multiple
.Dq values
are specified,
.Nm
will attempt to open each one in turn until it succeeds or runs out of
devices.
.It set dial Ar chat-script
This specifies the chat script that will be used to dial the other
side.
See also the
.Dq set login
command below.
Refer to
.Xr chat 8
and to the example configuration files for details of the chat script
format.
It is possible to specify some special
.Sq values
in your chat script as follows:
.Bl -tag -width 2n
.It Li \ec
When used as the last character in a
.Sq send
string, this indicates that a newline should not be appended.
.It Li \ed
When the chat script encounters this sequence, it delays two seconds.
.It Li \ep
When the chat script encounters this sequence, it delays for one quarter of
a second.
.It Li \en
This is replaced with a newline character.
.It Li \er
This is replaced with a carriage return character.
.It Li \es
This is replaced with a space character.
.It Li \et
This is replaced with a tab character.
.It Li \eT
This is replaced by the current phone number (see
.Dq set phone
below).
.It Li \eP
This is replaced by the current
.Ar authkey
value (see
.Dq set authkey
above).
.It Li \eU
This is replaced by the current
.Ar authname
value (see
.Dq set authname
above).
.El
.Pp
Note that two parsers will examine these escape sequences, so in order to
have the
.Sq chat parser
see the escape character, it is necessary to escape it from the
.Sq command parser .
This means that in practice you should use two escapes, for example:
.Bd -literal -offset indent
set dial "... ATDT\\\\T CONNECT"
.Ed
.Pp
It is also possible to execute external commands from the chat script.
To do this, the first character of the expect or send string is an
exclamation mark
.Pq Dq !\& .
If a literal exclamation mark is required, double it up to
.Dq !!\&
and it will be treated as a single literal
.Dq !\& .
When the command is executed, standard input and standard output are
directed to the open device (see the
.Dq set device
command), and standard error is read by
.Nm
and substituted as the expect or send string.
If
.Nm
is running in interactive mode, file descriptor 3 is attached to
.Pa /dev/tty .
.Pp
For example (wrapped for readability):
.Bd -literal -offset indent
set login "TIMEOUT 5 \\"\\" \\"\\" login:--login: ppp \e
word: ppp \\"!sh \\\\-c \\\\\\"echo \\\\-n label: >&2\\\\\\"\\" \e
\\"!/bin/echo in\\" HELLO"
.Ed
.Pp
would result in the following chat sequence (output using the
.Sq set log local chat
command before dialing):
.Bd -literal -offset indent
Dial attempt 1 of 1
dial OK!
Chat: Expecting:
Chat: Sending:
Chat: Expecting: login:--login:
Chat: Wait for (5): login:
Chat: Sending: ppp
Chat: Expecting: word:
Chat: Wait for (5): word:
Chat: Sending: ppp
Chat: Expecting: !sh \\-c "echo \\-n label: >&2"
Chat: Exec: sh -c "echo -n label: >&2"
Chat: Wait for (5): !sh \\-c "echo \\-n label: >&2" --> label:
Chat: Exec: /bin/echo in
Chat: Sending:
Chat: Expecting: HELLO
Chat: Wait for (5): HELLO
login OK!
.Ed
.Pp
Note (again) the use of the escape character, allowing many levels of
nesting.
Here, there are four parsers at work.
The first parses the original line, reading it as three arguments.
The second parses the third argument, reading it as 11 arguments.
At this point, it is
important that the
.Dq \&-
signs are escaped, otherwise this parser will see them as constituting
an expect-send-expect sequence.
When the
.Dq !\&
character is seen, the execution parser reads the first command as three
arguments, and then
.Xr sh 1
itself expands the argument after the
.Fl c .
As we wish to send the output back to the modem, in the first example
we redirect our output to file descriptor 2 (stderr) so that
.Nm
itself sends and logs it, and in the second example, we just output to stdout,
which is attached directly to the modem.
.Pp
This, of course means that it is possible to execute an entirely external
.Dq chat
command rather than using the internal one.
See
.Xr chat 8
for a good alternative.
.Pp
The external command that is executed is subjected to the same special
word expansions as the
.Dq !bg
command.
.It set enddisc Op label|IP|MAC|magic|psn value
This command sets our local endpoint discriminator.
If set prior to LCP negotiation, and if no
.Dq disable enddisc
command has been used,
.Nm
will send the information to the peer using the LCP endpoint discriminator
option.
The following discriminators may be set:
.Bl -tag -width indent
.It Li label
The current label is used.
.It Li IP
Our local IP number is used.
As LCP is negotiated prior to IPCP, it is
possible that the IPCP layer will subsequently change this value.
If
it does, the endpoint discriminator stays at the old value unless manually
reset.
.It Li MAC
This is similar to the
.Ar IP
option above, except that the MAC address associated with the local IP
number is used.
If the local IP number is not resident on any Ethernet
interface, the command will fail.
.Pp
As the local IP number defaults to whatever the machine host name is,
.Dq set enddisc mac
is usually done prior to any
.Dq set ifaddr
commands.
.It Li magic
A 20 digit random number is used.
Care should be taken when using magic numbers as restarting
.Nm
or creating a link using a different
.Nm
invocation will also use a different magic number and will therefore not
be recognised by the peer as belonging to the same bundle.
This makes it unsuitable for
.Fl direct
connections.
.It Li psn Ar value
The given
.Ar value
is used.
.Ar Value
should be set to an absolute public switched network number with the
country code first.
.El
.Pp
If no arguments are given, the endpoint discriminator is reset.
.It set escape Ar value...
This option is similar to the
.Dq set accmap
option above.
It allows the user to specify a set of characters that will be
.Sq escaped
as they travel across the link.
.It set filter dial|alive|in|out Ar rule-no Xo
.No permit|deny|clear| Ns Ar rule-no
.Op !\&
.Oo Op host
.Ar src_addr Ns Op / Ns Ar width
.Op Ar dst_addr Ns Op / Ns Ar width
.Oc [ Ns Ar proto
.Op src lt|eq|gt Ar port
.Op dst lt|eq|gt Ar port
.Op estab
.Op syn
.Op finrst
.Op timeout Ar secs ]
.Xc
.Nm
supports four filter sets.
The
.Em alive
filter specifies packets that keep the connection alive - resetting the
idle timer.
The
.Em dial
filter specifies packets that cause
.Nm
to dial when in
.Fl auto
mode.
The
.Em in
filter specifies packets that are allowed to travel
into the machine and the
.Em out
filter specifies packets that are allowed out of the machine.
.Pp
Filtering is done prior to any IP alterations that might be done by the
NAT engine on outgoing packets and after any IP alterations that might
be done by the NAT engine on incoming packets.
By default all empty filter sets allow all packets to pass.
Rules are processed in order according to
.Ar rule-no
(unless skipped by specifying a rule number as the
.Ar action ) .
Up to 40 rules may be given for each set.
If a packet doesn't match
any of the rules in a given set, it is discarded.
In the case of
.Em in
and
.Em out
filters, this means that the packet is dropped.
In the case of
.Em alive
filters it means that the packet will not reset the idle timer (even if
the
.Ar in Ns No / Ns Ar out
filter has a
.Dq timeout
value) and in the case of
.Em dial
filters it means that the packet will not trigger a dial.
A packet failing to trigger a dial will be dropped rather than queued.
Refer to the
section on
.Sx PACKET FILTERING
above for further details.
.It set hangup Ar chat-script
This specifies the chat script that will be used to reset the device
before it is closed.
It should not normally be necessary, but can
be used for devices that fail to reset themselves properly on close.
.It set help|? Op Ar command
This command gives a summary of available set commands, or if
.Ar command
is specified, the command usage is shown.
.It set ifaddr Oo Ar myaddr Ns
.Op / Ns Ar \&nn
.Oo Ar hisaddr Ns Op / Ns Ar \&nn
.Oo Ar netmask
.Op Ar triggeraddr
.Oc Oc
.Oc
This command specifies the IP addresses that will be used during
IPCP negotiation.
Addresses are specified using the format
.Pp
.Dl a.b.c.d/nn
.Pp
Where
.Dq a.b.c.d
is the preferred IP, but
.Ar nn
specifies how many bits of the address we will insist on.
If
.No / Ns Ar nn
is omitted, it defaults to
.Dq /32
unless the IP address is 0.0.0.0 in which case it defaults to
.Dq /0 .
.Pp
If you wish to assign a dynamic IP number to the peer,
.Ar hisaddr
may also be specified as a range of IP numbers in the format
.Bd -ragged -offset indent
.Ar \&IP Ns Oo \&- Ns Ar \&IP Ns Xo
.Oc Ns Oo , Ns Ar \&IP Ns
.Op \&- Ns Ar \&IP Ns
.Oc Ns ...
.Xc
.Ed
.Pp
for example:
.Pp
.Dl set ifaddr 10.0.0.1 10.0.1.2-10.0.1.10,10.0.1.20
.Pp
will only negotiate
.Dq 10.0.0.1
as the local IP number, but may assign any of the given 10 IP
numbers to the peer.
If the peer requests one of these numbers,
and that number is not already in use,
.Nm
will grant the peers request.
This is useful if the peer wants
to re-establish a link using the same IP number as was previously
allocated (thus maintaining any existing tcp or udp connections).
.Pp
If the peer requests an IP number that's either outside
of this range or is already in use,
.Nm
will suggest a random unused IP number from the range.
.Pp
If
.Ar triggeraddr
is specified, it is used in place of
.Ar myaddr
in the initial IPCP negotiation.
However, only an address in the
.Ar myaddr
range will be accepted.
This is useful when negotiating with some
.Dv PPP
implementations that will not assign an IP number unless their peer
requests
.Dq 0.0.0.0 .
.Pp
It should be noted that in
.Fl auto
mode,
.Nm
will configure the interface immediately upon reading the
.Dq set ifaddr
line in the config file.
In any other mode, these values are just
used for IPCP negotiations, and the interface isn't configured
until the IPCP layer is up.
.Pp
Note that the
.Ar HISADDR
argument may be overridden by the third field in the
.Pa ppp.secret
file once the client has authenticated itself
(if PAP or CHAP are
.Dq enabled ) .
Refer to the
.Sx AUTHENTICATING INCOMING CONNECTIONS
section for details.
.Pp
In all cases, if the interface is already configured,
.Nm
will try to maintain the interface IP numbers so that any existing
bound sockets will remain valid.
.It set ifqueue Ar packets
Set the maximum number of packets that
.Nm
will read from the tunnel interface while data cannot be sent to any of
the available links.
This queue limit is necessary to flow control outgoing data as the tunnel
interface is likely to be far faster than the combined links available to
.Nm .
.Pp
If
.Ar packets
is set to a value less than the number of links,
.Nm
will read up to that value regardless.
This prevents any possible latency problems.
.Pp
The default value for
.Ar packets
is
.Dq 30 .
.It set ccpretry|ccpretries Oo Ar timeout
.Op Ar reqtries Op Ar trmtries
.Oc
.It set chapretry|chapretries Oo Ar timeout
.Op Ar reqtries
.Oc
.It set ipcpretry|ipcpretries Oo Ar timeout
.Op Ar reqtries Op Ar trmtries
.Oc
.It set ipv6cpretry|ipv6cpretries Oo Ar timeout
.Op Ar reqtries Op Ar trmtries
.Oc
.It set lcpretry|lcpretries Oo Ar timeout
.Op Ar reqtries Op Ar trmtries
.Oc
.It set papretry|papretries Oo Ar timeout
.Op Ar reqtries
.Oc
These commands set the number of seconds that
.Nm
will wait before resending Finite State Machine (FSM) Request packets.
The default
.Ar timeout
for all FSMs is 3 seconds (which should suffice in most cases).
.Pp
If
.Ar reqtries
is specified, it tells
.Nm
how many configuration request attempts it should make while receiving
no reply from the peer before giving up.
The default is 5 attempts for
CCP, LCP and IPCP and 3 attempts for PAP and CHAP.
.Pp
If
.Ar trmtries
is specified, it tells
.Nm
how many terminate requests should be sent before giving up waiting for the
peers response.
The default is 3 attempts.
Authentication protocols are
not terminated and it is therefore invalid to specify
.Ar trmtries
for PAP or CHAP.
.Pp
In order to avoid negotiations with the peer that will never converge,
.Nm
will only send at most 3 times the configured number of
.Ar reqtries
in any given negotiation session before giving up and closing that layer.
.It set log Xo
.Op local
.Op +|- Ns
.Ar value Ns No ...
.Xc
This command allows the adjustment of the current log level.
Refer to the Logging Facility section for further details.
.It set login Ar chat-script
This
.Ar chat-script
compliments the dial-script.
If both are specified, the login
script will be executed after the dial script.
Escape sequences available in the dial script are also available here.
.It set logout Ar chat-script
This specifies the chat script that will be used to logout
before the hangup script is called.
It should not normally be necessary.
.It set lqrperiod Ar frequency
This command sets the
.Ar frequency
in seconds at which
.Em LQR
or
.Em ECHO LQR
packets are sent.
The default is 30 seconds.
You must also use the
.Dq enable lqr
command if you wish to send LQR requests to the peer.
.It set mode Ar interactive|auto|ddial|background
This command allows you to change the
.Sq mode
of the specified link.
This is normally only useful in multi-link mode,
but may also be used in uni-link mode.
.Pp
It is not possible to change a link that is
.Sq direct
or
.Sq dedicated .
.Pp
Note: If you issue the command
.Dq set mode auto ,
and have network address translation enabled, it may be useful to
.Dq enable iface-alias
afterwards.
This will allow
.Nm
to do the necessary address translations to enable the process that
triggers the connection to connect once the link is up despite the
peer assigning us a new (dynamic) IP address.
.It set mppe Op 40|56|128|* Op stateless|stateful|*
This option selects the encryption parameters used when negotiation
MPPE.
MPPE can be disabled entirely with the
.Dq disable mppe
command.
If no arguments are given,
.Nm
will attempt to negotiate a stateful link with a 128 bit key, but
will agree to whatever the peer requests (including no encryption
at all).
.Pp
If any arguments are given,
.Nm
will
.Em insist
on using MPPE and will close the link if it's rejected by the peer (Note;
this behaviour can be overridden by a configured RADIUS server).
.Pp
The first argument specifies the number of bits that
.Nm
should insist on during negotiations and the second specifies whether
.Nm
should insist on stateful or stateless mode.
In stateless mode, the
encryption dictionary is re-initialised with every packet according to
an encryption key that is changed with every packet.
In stateful mode,
the encryption dictionary is re-initialised every 256 packets or after
the loss of any data and the key is changed every 256 packets.
Stateless mode is less efficient but is better for unreliable transport
layers.
.It set mrru Op Ar value
Setting this option enables Multi-link PPP negotiations, also known as
Multi-link Protocol or MP.
There is no default MRRU (Maximum Reconstructed Receive Unit) value.
If no argument is given, multi-link mode is disabled.
.It set mru Xo
.Op max Ns Op imum
.Op Ar value
.Xc
The default MRU (Maximum Receive Unit) is 1500.
If it is increased, the other side *may* increase its MTU.
In theory there is no point in decreasing the MRU to below the default as the
.Em PPP
protocol says implementations *must* be able to accept packets of at
least 1500 octets.
.Pp
If the
.Dq maximum
keyword is used,
.Nm
will refuse to negotiate a higher value.
The maximum MRU can be set to 2048 at most.
Setting a maximum of less than 1500 violates the
.Em PPP
rfc, but may sometimes be necessary.
For example,
.Em PPPoE
imposes a maximum of 1492 due to hardware limitations.
.Pp
If no argument is given, 1500 is assumed.
A value must be given when
.Dq maximum
is specified.
.It set mtu Xo
.Op max Ns Op imum
.Op Ar value
.Xc
The default MTU is 1500.
At negotiation time,
.Nm
will accept whatever MRU the peer requests (assuming it's
not less than 296 bytes or greater than the assigned maximum).
If the MTU is set,
.Nm
will not accept MRU values less than
.Ar value .
When negotiations are complete, the MTU is used when writing to the
interface, even if the peer requested a higher value MRU.
This can be useful for
limiting your packet size (giving better bandwidth sharing at the expense
of more header data).
.Pp
If the
.Dq maximum
keyword is used,
.Nm
will refuse to negotiate a higher value.
The maximum MTU can be set to 2048 at most.
.Pp
If no
.Ar value
is given, 1500, or whatever the peer asks for is used.
A value must be given when
.Dq maximum
is specified.
.It set nbns Op Ar x.x.x.x Op Ar y.y.y.y
This option allows the setting of the Microsoft NetBIOS name server
values to be returned at the peers request.
If no values are given,
.Nm
will reject any such requests.
.It set openmode active|passive Op Ar delay
By default,
.Ar openmode
is always
.Ar active
with a one second
.Ar delay .
That is,
.Nm
will always initiate LCP/IPCP/CCP negotiation one second after the line
comes up.
If you want to wait for the peer to initiate negotiations, you
can use the value
.Ar passive .
If you want to initiate negotiations immediately or after more than one
second, the appropriate
.Ar delay
may be specified here in seconds.
.It set parity odd|even|none|mark
This allows the line parity to be set.
The default value is
.Ar none .
.It set phone Ar telno Ns Xo
.Oo \&| Ns Ar backupnumber
.Oc Ns ... Ns Oo : Ns Ar nextnumber
.Oc Ns ...
.Xc
This allows the specification of the phone number to be used in
place of the \\\\T string in the dial and login chat scripts.
Multiple phone numbers may be given separated either by a pipe
.Pq Dq \&|
or a colon
.Pq Dq \&: .
.Pp
Numbers after the pipe are only dialed if the dial or login
script for the previous number failed.
.Pp
Numbers after the colon are tried sequentially, irrespective of
the reason the line was dropped.
.Pp
If multiple numbers are given,
.Nm
will dial them according to these rules until a connection is made, retrying
the maximum number of times specified by
.Dq set redial
below.
In
.Fl background
mode, each number is attempted at most once.
.It set Op proc Ns Xo
.No title Op Ar value
.Xc
The current process title as displayed by
.Xr ps 1
is changed according to
.Ar value .
If
.Ar value
is not specified, the original process title is restored.
All the
word replacements done by the shell commands (see the
.Dq bg
command above) are done here too.
.Pp
Note, if USER is required in the process title, the
.Dq set proctitle
command must appear in
.Pa ppp.linkup ,
as it is not known when the commands in
.Pa ppp.conf
are executed.
.It set radius Op Ar config-file
This command enables RADIUS support (if it's compiled in).
.Ar config-file
refers to the radius client configuration file as described in
.Xr radius.conf 5 .
If PAP, CHAP, MSCHAP or MSCHAPv2 are
.Dq enable Ns No d ,
.Nm
behaves as a
.Em \&N Ns No etwork
.Em \&A Ns No ccess
.Em \&S Ns No erver
and uses the configured RADIUS server to authenticate rather than
authenticating from the
.Pa ppp.secret
file or from the passwd database.
.Pp
If none of PAP, CHAP, MSCHAP or MSCHAPv2 are enabled,
.Dq set radius
will do nothing.
.Pp
.Nm
uses the following attributes from the RADIUS reply:
.Bl -tag -width XXX -offset XXX
.It RAD_FRAMED_IP_ADDRESS
The peer IP address is set to the given value.
.It RAD_FRAMED_IP_NETMASK
The tun interface netmask is set to the given value.
.It RAD_FRAMED_MTU
If the given MTU is less than the peers MRU as agreed during LCP
negotiation, *and* it is less that any configured MTU (see the
.Dq set mru
command), the tun interface MTU is set to the given value.
.It RAD_FRAMED_COMPRESSION
If the received compression type is
.Dq 1 ,
.Nm
will request VJ compression during IPCP negotiations despite any
.Dq disable vj
configuration command.
.It RAD_FILTER_ID
If this attribute is supplied,
.Nm
will attempt to use it as an additional label to load from the
.Pa ppp.linkup
and
.Pa ppp.linkdown
files.
The load will be attempted before (and in addition to) the normal
label search.
If the label doesn't exist, no action is taken and
.Nm
proceeds to the normal load using the current label.
.It RAD_FRAMED_ROUTE
The received string is expected to be in the format
.Ar dest Ns Op / Ns Ar bits
.Ar gw
.Op Ar metrics .
Any specified metrics are ignored.
.Dv MYADDR
and
.Dv HISADDR
are understood as valid values for
.Ar dest
and
.Ar gw ,
.Dq default
can be used for
.Ar dest
to sepcify the default route, and
.Dq 0.0.0.0
is understood to be the same as
.Dq default
for
.Ar dest
and
.Dv HISADDR
for
.Ar gw .
.Pp
For example, a returned value of
.Dq 1.2.3.4/24 0.0.0.0 1 2 -1 3 400
would result in a routing table entry to the 1.2.3.0/24 network via
.Dv HISADDR
and a returned value of
.Dq 0.0.0.0 0.0.0.0
or
.Dq default HISADDR
would result in a default route to
.Dv HISADDR .
.Pp
All RADIUS routes are applied after any sticky routes are applied, making
RADIUS routes override configured routes.
This also applies for RADIUS routes that don't {include} the
.Dv MYADDR
or
.Dv HISADDR
keywords.
.Pp
.It RAD_SESSION_TIMEOUT
If supplied, the client connection is closed after the given number of
seconds.
.It RAD_REPLY_MESSAGE
If supplied, this message is passed back to the peer as the authentication
SUCCESS text.
.It RAD_MICROSOFT_MS_CHAP_ERROR
If this
.Dv RAD_VENDOR_MICROSOFT
vendor specific attribute is supplied, it is passed back to the peer as the
authentication FAILURE text.
.It RAD_MICROSOFT_MS_CHAP2_SUCCESS
If this
.Dv RAD_VENDOR_MICROSOFT
vendor specific attribute is supplied and if MS-CHAPv2 authentication is
being used, it is passed back to the peer as the authentication SUCCESS text.
.It RAD_MICROSOFT_MS_MPPE_ENCRYPTION_POLICY
If this
.Dv RAD_VENDOR_MICROSOFT
vendor specific attribute is supplied and has a value of 2 (Required),
.Nm
will insist that MPPE encryption is used (even if no
.Dq set mppe
configuration command has been given with arguments).
If it is supplied with a value of 1 (Allowed), encryption is made optional
(despite any
.Dq set mppe
configuration commands with arguments).
.It RAD_MICROSOFT_MS_MPPE_ENCRYPTION_TYPES
If this
.Dv RAD_VENDOR_MICROSOFT
vendor specific attribute is supplied, bits 1 and 2 are examined.
If either or both are set, 40 bit and/or 128 bit (respectively) encryption
options are set, overriding any given first argument to the
.Dq set mppe
command.
Note, it is not currently possible for the RADIUS server to specify 56 bit
encryption.
.It RAD_MICROSOFT_MS_MPPE_RECV_KEY
If this
.Dv RAD_VENDOR_MICROSOFT
vendor specific attribute is supplied, it's value is used as the master
key for decryption of incoming data.  When clients are authenticated using
MSCHAPv2, the RADIUS server MUST provide this attribute if inbound MPPE is
to function.
.It RAD_MICROSOFT_MS_MPPE_SEND_KEY
If this
.Dv RAD_VENDOR_MICROSOFT
vendor specific attribute is supplied, it's value is used as the master
key for encryption of outgoing data.  When clients are authenticated using
MSCHAPv2, the RADIUS server MUST provide this attribute if outbound MPPE is
to function.
.El
.Pp
Values received from the RADIUS server may be viewed using
.Dq show bundle .
.It set reconnect Ar timeout ntries
Should the line drop unexpectedly (due to loss of CD or LQR
failure), a connection will be re-established after the given
.Ar timeout .
The line will be re-connected at most
.Ar ntries
times.
.Ar Ntries
defaults to zero.
A value of
.Ar random
for
.Ar timeout
will result in a variable pause, somewhere between 1 and 30 seconds.
.It set recvpipe Op Ar value
This sets the routing table RECVPIPE value.
The optimum value is just over twice the MTU value.
If
.Ar value
is unspecified or zero, the default kernel controlled value is used.
.It set redial Ar secs Ns Xo
.Oo + Ns Ar inc Ns
.Op - Ns Ar max Ns
.Oc Ns Op . Ns Ar next
.Op Ar attempts
.Xc
.Nm
can be instructed to attempt to redial
.Ar attempts
times.
If more than one phone number is specified (see
.Dq set phone
above), a pause of
.Ar next
is taken before dialing each number.
A pause of
.Ar secs
is taken before starting at the first number again.
A literal value of
.Dq Li random
may be used here in place of
.Ar secs
and
.Ar next ,
causing a random delay of between 1 and 30 seconds.
.Pp
If
.Ar inc
is specified, its value is added onto
.Ar secs
each time
.Nm
tries a new number.
.Ar secs
will only be incremented at most
.Ar max
times.
.Ar max
defaults to 10.
.Pp
Note, the
.Ar secs
delay will be effective, even after
.Ar attempts
has been exceeded, so an immediate manual dial may appear to have
done nothing.
If an immediate dial is required, a
.Dq !\&
should immediately follow the
.Dq open
keyword.
See the
.Dq open
description above for further details.
.It set sendpipe Op Ar value
This sets the routing table SENDPIPE value.
The optimum value is just over twice the MTU value.
If
.Ar value
is unspecified or zero, the default kernel controlled value is used.
.It "set server|socket" Ar TcpPort Ns No \&| Ns Xo
.Ar LocalName Ns No |none|open|closed
.Op password Op Ar mask
.Xc
This command tells
.Nm
to listen on the given socket or
.Sq diagnostic port
for incoming command connections.
.Pp
The word
.Dq none
instructs
.Nm
to close any existing socket and clear the socket configuration.
The word
.Dq open
instructs
.Nm
to attempt to re-open the port.
The word
.Dq closed
instructs
.Nm
to close the open port.
.Pp
If you wish to specify a local domain socket,
.Ar LocalName
must be specified as an absolute file name, otherwise it is assumed
to be the name or number of a TCP port.
You may specify the octal umask to be used with a local domain socket.
Refer to
.Xr umask 2
for umask details.
Refer to
.Xr services 5
for details of how to translate TCP port names.
.Pp
You must also specify the password that must be entered by the client
(using the
.Dq passwd
variable above) when connecting to this socket.
If the password is
specified as an empty string, no password is required for connecting clients.
.Pp
When specifying a local domain socket, the first
.Dq %d
sequence found in the socket name will be replaced with the current
interface unit number.
This is useful when you wish to use the same
profile for more than one connection.
.Pp
In a similar manner TCP sockets may be prefixed with the
.Dq +
character, in which case the current interface unit number is added to
the port number.
.Pp
When using
.Nm
with a server socket, the
.Xr pppctl 8
command is the preferred mechanism of communications.
Currently,
.Xr telnet 1
can also be used, but link encryption may be implemented in the future, so
.Xr telnet 1
should be avoided.
.Pp
Note;
.Dv SIGUSR1
and
.Dv SIGUSR2
interact with the diagnostic socket.
.It set speed Ar value
This sets the speed of the serial device.
If speed is specified as
.Dq sync ,
.Nm
treats the device as a synchronous device.
.Pp
Certain device types will know whether they should be specified as
synchronous or asynchronous.
These devices will override incorrect
settings and log a warning to this effect.
.It set stopped Op Ar LCPseconds Op Ar CCPseconds
If this option is set,
.Nm
will time out after the given FSM (Finite State Machine) has been in
the stopped state for the given number of
.Dq seconds .
This option may be useful if the peer sends a terminate request,
but never actually closes the connection despite our sending a terminate
acknowledgement.
This is also useful if you wish to
.Dq set openmode passive
and time out if the peer doesn't send a Configure Request within the
given time.
Use
.Dq set log +lcp +ccp
to make
.Nm
log the appropriate state transitions.
.Pp
The default value is zero, where
.Nm
doesn't time out in the stopped state.
.Pp
This value should not be set to less than the openmode delay (see
.Dq set openmode
above).
.It set timeout Ar idleseconds Op Ar mintimeout
This command allows the setting of the idle timer.
Refer to the section titled
.Sx SETTING THE IDLE TIMER
for further details.
.Pp
If
.Ar mintimeout
is specified,
.Nm
will never idle out before the link has been up for at least that number
of seconds.
.It set urgent Xo
.Op tcp|udp|none
.Oo Op +|- Ns
.Ar port
.Oc No ...
.Xc
This command controls the ports that
.Nm
prioritizes when transmitting data.
The default priority TCP ports
are ports 21 (ftp control), 22 (ssh), 23 (telnet), 513 (login), 514 (shell),
543 (klogin) and 544 (kshell).
There are no priority UDP ports by default.
See
.Xr services 5
for details.
.Pp
If neither
.Dq tcp
or
.Dq udp
are specified,
.Dq tcp
is assumed.
.Pp
If no
.Ar port Ns No s
are given, the priority port lists are cleared (although if
.Dq tcp
or
.Dq udp
is specified, only that list is cleared).
If the first
.Ar port
argument is prefixed with a plus
.Pq Dq \&+
or a minus
.Pq Dq \&- ,
the current list is adjusted, otherwise the list is reassigned.
.Ar port Ns No s
prefixed with a plus or not prefixed at all are added to the list and
.Ar port Ns No s
prefixed with a minus are removed from the list.
.Pp
If
.Dq none
is specified, all priority port lists are disabled and even
.Dv IPTOS_LOWDELAY
packets are not prioritised.
.It set vj slotcomp on|off
This command tells
.Nm
whether it should attempt to negotiate VJ slot compression.
By default, slot compression is turned
.Ar on .
.It set vj slots Ar nslots
This command sets the initial number of slots that
.Nm
will try to negotiate with the peer when VJ compression is enabled (see the
.Sq enable
command above).
It defaults to a value of 16.
.Ar Nslots
must be between
.Ar 4
and
.Ar 16
inclusive.
.El
.Pp
.It shell|! Op Ar command
If
.Ar command
is not specified a shell is invoked according to the
.Dv SHELL
environment variable.
Otherwise, the given
.Ar command
is executed.
Word replacement is done in the same way as for the
.Dq !bg
command as described above.
.Pp
Use of the ! character
requires a following space as with any of the other commands.
You should note that this command is executed in the foreground;
.Nm
will not continue running until this process has exited.
Use the
.Dv bg
command if you wish processing to happen in the background.
.It show Ar var
This command allows the user to examine the following:
.Bl -tag -width 2n
.It show bundle
Show the current bundle settings.
.It show ccp
Show the current CCP compression statistics.
.It show compress
Show the current VJ compression statistics.
.It show escape
Show the current escape characters.
.It show filter Op Ar name
List the current rules for the given filter.
If
.Ar name
is not specified, all filters are shown.
.It show hdlc
Show the current HDLC statistics.
.It show help|?
Give a summary of available show commands.
.It show iface
Show the current interface information
(the same as
.Dq iface show ) .
.It show ipcp
Show the current IPCP statistics.
.It show layers
Show the protocol layers currently in use.
.It show lcp
Show the current LCP statistics.
.It show Op data Ns Xo
.No link
.Xc
Show high level link information.
.It show links
Show a list of available logical links.
.It show log
Show the current log values.
.It show mem
Show current memory statistics.
.It show ncp
Show the current NCP statistics.
.It show physical
Show low level link information.
.It show mp
Show Multi-link information.
.It show proto
Show current protocol totals.
.It show route
Show the current routing tables.
.It show stopped
Show the current stopped timeouts.
.It show timer
Show the active alarm timers.
.It show version
Show the current version number of
.Nm .
.El
.Pp
.It term
Go into terminal mode.
Characters typed at the keyboard are sent to the device.
Characters read from the device are displayed on the screen.
When a remote
.Em PPP
peer is detected,
.Nm
automatically enables Packet Mode and goes back into command mode.
.El
.Sh MORE DETAILS
.Bl -bullet
.It
Read the example configuration files.
They are a good source of information.
.It
Use
.Dq help ,
.Dq nat \&? ,
.Dq enable \&? ,
.Dq set ?\&
and
.Dq show ?\&
to get online information about what's available.
.It
The following URLs contain useful information:
.Bl -bullet -compact
.It
http://www.FreeBSD.org/doc/en_US.ISO8859-1/books/faq/ppp.html
.It
http://www.FreeBSD.org/doc/handbook/userppp.html
.El
.Pp
.El
.Sh FILES
.Nm
refers to four files:
.Pa ppp.conf ,
.Pa ppp.linkup ,
.Pa ppp.linkdown
and
.Pa ppp.secret .
These files are placed in the
.Pa /etc/ppp
directory.
.Bl -tag -width 2n
.It Pa /etc/ppp/ppp.conf
System default configuration file.
.It Pa /etc/ppp/ppp.secret
An authorisation file for each system.
.It Pa /etc/ppp/ppp.linkup
A file to check when
.Nm
establishes a network level connection.
.It Pa /etc/ppp/ppp.linkdown
A file to check when
.Nm
closes a network level connection.
.It Pa /var/log/ppp.log
Logging and debugging information file.
Note, this name is specified in
.Pa /etc/syslog.conf .
See
.Xr syslog.conf 5
for further details.
.It Pa /var/spool/lock/LCK..*
tty port locking file.
Refer to
.Xr uucplock 3
for further details.
.It Pa /var/run/tunN.pid
The process id (pid) of the
.Nm
program connected to the tunN device, where
.Sq N
is the number of the device.
.It Pa /var/run/ttyXX.if
The tun interface used by this port.
Again, this file is only created in
.Fl background ,
.Fl auto
and
.Fl ddial
modes.
.It Pa /etc/services
Get port number if port number is using service name.
.It Pa /var/run/ppp-authname-class-value
In multi-link mode, local domain sockets are created using the peer
authentication name
.Pq Sq authname ,
the peer endpoint discriminator class
.Pq Sq class
and the peer endpoint discriminator value
.Pq Sq value .
As the endpoint discriminator value may be a binary value, it is turned
to HEX to determine the actual file name.
.Pp
This socket is used to pass links between different instances of
.Nm .
.El
.Sh SEE ALSO
.Xr at 1 ,
.Xr ftp 1 ,
.Xr gzip 1 ,
.Xr hostname 1 ,
.Xr login 1 ,
.Xr tcpdump 1 ,
.Xr telnet 1 ,
.Xr kldload 2 ,
ifdef({LOCALNAT},{},{.Xr libalias 3 ,
})dnl
ifdef({LOCALRAD},{},{.Xr libradius 3 ,
})dnl
.Xr syslog 3 ,
.Xr uucplock 3 ,
.Xr netgraph 4 ,
.Xr ng_pppoe 4 ,
.Xr crontab 5 ,
.Xr group 5 ,
.Xr passwd 5 ,
.Xr protocols 5 ,
.Xr radius.conf 5 ,
.Xr resolv.conf 5 ,
.Xr syslog.conf 5 ,
.Xr adduser 8 ,
.Xr chat 8 ,
.Xr getty 8 ,
.Xr inetd 8 ,
.Xr init 8 ,
.Xr isdn 8 ,
.Xr named 8 ,
.Xr ping 8 ,
.Xr pppctl 8 ,
.Xr pppd 8 ,
.Xr pppoe 8 ,
.Xr route 8 ,
.Xr sshd 8 ,
.Xr syslogd 8 ,
.Xr traceroute 8 ,
.Xr vipw 8
.Sh HISTORY
This program was originally written by
.An Toshiharu OHNO Aq tony-o@iij.ad.jp ,
and was submitted to
.Fx 2.0.5
by
.An Atsushi Murai Aq amurai@spec.co.jp .
.Pp
It was substantially modified during 1997 by
.An Brian Somers Aq brian@Awfulhak.org ,
and was ported to
.Ox
in November that year
(just after the 2.2 release).
.Pp
Most of the code was rewritten by
.An Brian Somers
in early 1998 when multi-link ppp support was added.
