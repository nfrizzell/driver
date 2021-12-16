#!/bin/sh
module="scull"
device="scull"
mode="666"

# invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
/sbin/insmod ./$module.ko $* || exit 1

# remove stale nodes
rm -f /dev/${device}

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
echo $major

mknod /dev/${device} c $major 0

group="root"

chgrp $group /dev/${device}
chmod $mode /dev/${device}
