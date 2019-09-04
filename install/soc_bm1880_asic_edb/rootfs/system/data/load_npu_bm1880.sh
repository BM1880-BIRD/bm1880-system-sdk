#!/bin/sh

module="bmnpu"
devices="bm-npu"
npudev="bm-npu0"
mode="664"

# Group: since distributions do	it differently,	look for wheel or use staff
if grep	'^staff:' /etc/group > /dev/null; then
    group="staff"
else
    group="wheel"
fi

# invoke insmod	with all arguments we got
# and use a pathname, as newer modutils	don't look in .	by default
/sbin/insmod ./$module.ko $*	|| exit	1

major=`cat /proc/devices | awk "\\$2==\"$devices\" {print \\$1}"`

# Remove stale nodes and replace them, then give gid and perms
# Usually the script is	shorter, it's simple that has several devices in it.

rm -f /dev/${npudev}
mknod /dev/${npudev} c $major 0
chgrp $group /dev/${npudev}
chmod $mode  /dev/${npudev}
chown $SUDO_USER.$SUDO_USER /dev/${npudev}
