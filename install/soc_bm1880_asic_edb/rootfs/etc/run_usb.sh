CLASS=acm
VID=0x30b1
PID=0x1003
MANUFACTURER="Bitmain"
PRODUCT="USB Com Port"
SERIAL="0123456789"
MSC_FILE=$3
BM_DIR=/tmp/usb
BM_GADGET=$BM_DIR/usb_gadget/g_1880
FUNC_NUM=0
MAX_EP_NUM=4
TMP_NUM=0
EP_IN=0
EP_OUT=0

case "$2" in
  acm)
	CLASS=acm
	;;
  msc)
	CLASS=mass_storage
	;;
  bcm)
	CLASS=bcm
	;;
  *)
	if [ "$1" = "probe" ] ; then
	  echo "Usage: $0 probe {acm|msc|bcm}"
	  exit 1
	fi
esac

calc_func() {
  FUNC_NUM=$(ls $BM_GADGET/functions -l | grep ^d | wc -l)
  echo "$FUNC_NUM file(s)"
}

res_check() {
  TMP_NUM=$(find $BM_GADGET/functions/ -name acm* | wc -l)
  EP_OUT=$(($EP_OUT+$TMP_NUM))
  TMP_NUM=$(($TMP_NUM * 2))
  EP_IN=$(($EP_IN+$TMP_NUM))
  TMP_NUM=$(find $BM_GADGET/functions/ -name mass_storage* | wc -l)
  EP_IN=$(($EP_IN+$TMP_NUM))
  EP_OUT=$(($EP_OUT+$TMP_NUM))
  TMP_NUM=$(find $BM_GADGET/functions/ -name bcm* | wc -l)
  EP_IN=$(($EP_IN+$TMP_NUM))
  EP_OUT=$(($EP_OUT+$TMP_NUM))
  if [ "$CLASS" = "acm" ] ; then
    EP_IN=$(($EP_IN+2))
    EP_OUT=$(($EP_OUT+1))
  fi
  if [ "$CLASS" = "mass_storage" ] ; then
    EP_IN=$(($EP_IN+1))
    EP_OUT=$(($EP_OUT+1))
  fi
  if [ "$CLASS" = "bcm" ] ; then
    EP_IN=$(($EP_IN+1))
    EP_OUT=$(($EP_OUT+1))
  fi
  echo "$EP_IN in ep"
  echo "$EP_OUT out ep"
  if [ $EP_IN -gt $MAX_EP_NUM ]; then
    echo "reach maximum resource"
    exit 1
  fi
  if [ $EP_OUT -gt $MAX_EP_NUM ]; then
    echo "reach maximum resource"
    exit 1
  fi
}

probe() {
  if [ ! -d $BM_DIR ]; then
    mkdir $BM_DIR
  fi
  if [ ! -d $BM_DIR/usb_gadget ]; then
    # Enale USB ConfigFS
    mount none $BM_DIR -t configfs
    # Create gadget dev
    mkdir $BM_GADGET
    # Set the VID and PID
    echo $VID >$BM_GADGET/idVendor
    echo $PID >$BM_GADGET/idProduct
    # Set the product information string
    mkdir $BM_GADGET/strings/0x409
    echo $MANUFACTURER>$BM_GADGET/strings/0x409/manufacturer
    echo $PRODUCT>$BM_GADGET/strings/0x409/product
    echo $SERIAL>$BM_GADGET/strings/0x409/serialnumber
    # Set the USB configuration
    mkdir $BM_GADGET/configs/c.1
    mkdir $BM_GADGET/configs/c.1/strings/0x409
    echo "config1">$BM_GADGET/configs/c.1/strings/0x409/configuration
    # Set the MaxPower of USB descriptor
    echo 120 >$BM_GADGET/configs/c.1/MaxPower  
  fi
  # get current functions number
  calc_func
  # assign the class code for composite device
  if [ ! $FUNC_NUM -eq 0 ]; then
    echo 0xEF >$BM_GADGET/bDeviceClass
    echo 0x02 >$BM_GADGET/bDeviceSubClass
    echo 0x01 >$BM_GADGET/bDeviceProtocol
    if [ "$CLASS" = "bcm" ] ; then
        echo "BCM must be the 1st and only function!"
        exit 1
    fi
  fi
  # resource check
  res_check
  # create the desired function
  mkdir $BM_GADGET/functions/$CLASS.usb$FUNC_NUM
  if [ "$CLASS" = "mass_storage" ] ; then
    echo $MSC_FILE >$BM_GADGET/functions/$CLASS.usb$FUNC_NUM/lun.0/file
  fi
}

start() {
  # link this function to the configuration
  calc_func
  if [ $FUNC_NUM -eq 0 ]; then
    echo "Functions Empty!"
    exit 1
  fi
  for i in `seq 0 $(($FUNC_NUM-1))`;
  do
    find $BM_GADGET/functions/ -name *.usb$i | xargs -I % ln -s % $BM_GADGET/configs/c.1
  done
  # Start the gadget driver
  echo 500a0000.bm-usb-dev >$BM_GADGET/UDC
}

stop() {
  echo "" >$BM_GADGET/UDC
  rm $BM_GADGET/configs/c.1/*.usb*
  rmdir $BM_GADGET/configs/c.1/strings/0x409/
  rmdir $BM_GADGET/configs/c.1/
  rmdir $BM_GADGET/functions/*
  rmdir $BM_GADGET/strings/0x409/
  rmdir $BM_GADGET
  umount $BM_DIR
  rmdir $BM_DIR
}

case "$1" in
  start)
	start
	;;
  stop)
	stop
	;;
  probe)
	probe
	;;
  *)
	echo "Usage: $0 probe {acm|msc|bcm} {file (msc)}"
	echo "Usage: $0 start"
	echo "Usage: $0 stop"
	exit 1
esac
exit $?
