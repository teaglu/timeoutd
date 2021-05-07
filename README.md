# timeoutd

timeoutd is a daemon that listens for keep-alive packets on a UDP port.
Each packet includes a string or "key" that names the resource to be
monitoried, and a number of seconds after which that resource should
expire and trigger a notification.

If another packet arrives with the same resource key, the resource is
deferred again and notification is put off.  The packet can also specify
zero seconds, which instructs timeoutd to remove the resource without
notification.

Keys are limited to alphanumeric characters and the dot (.) character.

## Self Monitoring

The daemon can also function as a sender of the same protocol, so that
multiple computers running timeoutd will send a notification if one of
it's peers goes down.

The default key sent by timeoutd is "node." followed by the short form
of the hostname.  This can be set by the -K flag.

By default the sender sends a keepalive packet every two seconds, and
indicates that it should time out after ten seconds.  These can be
overridden with the -F and -T flags.

Multiple peers can be added with the -p flag.

## Multicast

If you enable multicast with the -m flag, then the program will
listen on a multicast address, and notify the kernel of the multicast
address for IGMP.

If multicast is enabled, timeoutd will also send an announcement for itself
on the multicast group to all peers.  This functionally creates a
full-mesh within the broadcast domain.

The default TTL on multicast packets is set to 5 to allow forwarding
between broadcast domains via PIM.  This value can be overridden with the
-M flag.

## Multi-Home Hosts

Multicast on multi-homed hosts requires timeoutd to bind to a particular
interface for both sending and receiving.

To find the correct interface, timeoutd examines each network interface
looking for one with a valid RFC1918 IP4 address.  In the common case of
cloud hosts with a direct internet interface and an internal interface,
this will locate the correct interface.

Sites which use publically routed addresses on private networks will have
problems with this strategy, as will sites with hosts that have more than
one interface with an RFC1918 address.

Interfaces whose name starts with "lo" or "docker" are excluded.

## Notification

Notification is done via calling the external program timeoutd-notify,
which is commonly a shell script.  The first argument is the key
that went offline, and the second argument is the IP address the
key was last received from.

By default the shell is located in /usr/local/libexec/timeoutd-notify.
This can be overridden with the -s flag.

Example scripts are included in the "sample" directory.  If you include
API details in the script be sure to remove "other" permission.

## Simple Wire Protocol

For simple messages received on port 2952, the packet is just the name
of the resource key, optionally followed by a colon and the number of
seconds before it should expire.

## Secured Wire Protocol

If the environment variable SIGNATURE_KEY is present, it will be used as
a pre-shared key for an HMAC digest on the message packets.  The program
will then listen on port 2953 for secured packets.

The secured format is the same as the simple format, except it has a header
with a timestamp (to prevent replay) and a message digest.  The full form
of the header is in the file protocol.h.

If the secured protocol is enabled, the program will still listen on port
2952 for simple packets.

## Flags

Configuration is done by command line flags:

These flags can be used:

| Flag            | Setting                                 |
| --------------- | --------------------------------------- |
| -v              | Show additional debugging               |
| -p {peer}       | Add a peer by dns name or IP            |
| -K {key}        | Set key for local sender                |
| -T {seconds}    | Set timeout for local sender            |
| -F {seconds}    | Set send frequency for local sender     |
| -m              | Enable multicast send/receive           |
| -M {#}          | Set multicast TTL (default 5)           |
| -s {path}       | Set notification script path            |
| -l {#}          | Set entry limit (max number of keys)    |


