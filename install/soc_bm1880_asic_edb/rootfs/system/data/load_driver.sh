#! /bin/sh

path_ko="/system/data"

echo "load_driver for install ko"

do_start () {
        module="vpu"
        device="vpu"
        mode="664"

        vpu=`/sbin/lsmod | awk "\\$1==\"$module\" {print \\$1}"`

        if [ "$vpu" != "$module" ]; then
            echo "insmod $module.ko"
            if grep '^staff:' /etc/group > /dev/null; then
                group="staff"
            else
                group="wheel"
            fi

            # invoke insmod with all arguments we got
            # and use a pathname, as newer modutils don't look in . by default
            /sbin/insmod $path_ko/$module.ko $* || exit -1

            major=`cat /proc/devices | awk "\\$2==\"$module\" {print \\$1}"`

            # Remove stale nodes and replace them, then give gid and perms
            # Usually the script is shorter, it's simple that has several devices in it.
            rm -f /dev/${device}
              mknod /dev/${device} c $major 0
            chgrp $group /dev/${device}
            chmod $mode  /dev/${device}
            #chown $SUDO_USER.$SUDO_USER /dev/${device}
              chown root.root /dev/${device}
        else
            echo "$module.ko already installed"
        fi

        module="jpu"
        device="jpu"
        mode="664"

        jpu=`/sbin/lsmod | awk "\\$1==\"$module\" {print \\$1}"`
        if [ "$jpu" != "$module" ]; then
            echo "insmod $module.ko"
            # Group: since distributions do it differently, look for wheel or use staff
            if grep '^staff:' /etc/group > /dev/null; then
                  group="staff"
                else
                    group="wheel"
                fi
              # invoke insmod with all arguments we got
            # and use a pathname, as newer modutils don't look in . by default
            /sbin/insmod $path_ko/$module.ko $* || exit -1
            major=`cat /proc/devices | awk "\\$2==\"$module\" {print \\$1}"`
            # Remove stale nodes and replace them, then give gid and perms
              # Usually the script is shorter, it's simple that has several devices in it.
              rm -f /dev/${device}
              mknod /dev/${device} c $major 0
              chgrp $group /dev/${device}
              chmod $mode  /dev/${device}
              chown root.root /dev/${device}
        else
            echo "$module.ko already installed"
        fi

	#load npu ko
        bmnpu=`/sbin/lsmod | awk "\\$1==\"bmnpu\" {print \\$1}"`
        if [ "$bmnpu" != "bmnpu" ]; then
            /sbin/insmod $path_ko/bmnpu.ko || exit -1
        else
            echo "bmnpu.ko already installed"
        fi
	
        [ $? = 0 ] && echo "OK" || echo "FAIL" 
}
do_stop () {

        module="bmnpu"
        device="bmnpu"

        # invoke rmmod with all	arguments we got
        /sbin/rmmod $module $* || exit 1

        # Remove stale nodes
        rm -f /dev/${device} 

        module="vpu"
        device="vpu"
        
        rm -f /dev/shm/*
        # invoke rmmod with all	arguments we got
        /sbin/rmmod $module $* || exit 1

        # Remove stale nodes
        rm -f /dev/${device} 

        module="jpu"
        device="jpu"

        # invoke rmmod with all	arguments we got
        /sbin/rmmod $module $* || exit 1

        # Remove stale nodes
        rm -f /dev/${device} 

}


case "$1" in

  "")
	printf "Starting load driver: "
    hostn=$(sed -n 1p /etc/hostname)

    avahi-set-host-name $hostn 
        
	do_start
	  ;;	  
  start)
	printf "Starting load driver...: "
    do_start
	;;
  stop)
	printf "Stopping driver: "
    do_stop
	echo "OK"
	;;
  restart|reload)
	printf "restarting driver: "
    do_stop
	do_start	
	;;
  *)
	echo "Usage: $0 {start|stop|restart|reload}"
	exit 1
esac

exit $?
