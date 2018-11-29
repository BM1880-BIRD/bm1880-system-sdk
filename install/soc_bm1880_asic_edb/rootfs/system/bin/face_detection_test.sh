insmod /system/data/bmnpu.ko
mdev -s 
LD_LIBRARY_PATH=/system/lib ./usb_face /dev/ttyGS0 tiny
# host side: ./videocap -d /dev/ttyACM0 -c face -i usb
