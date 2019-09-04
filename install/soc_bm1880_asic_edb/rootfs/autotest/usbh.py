#!/usr/bin/env python

import os, sys

USBH_DEV_NO_EXIST = -1
USBH_DEV_TYPE_FAIL = -2
USBH_DeV_DN_GRADE = -3

DEV_NAME = "/dev/sda"
TARGET = 52428800/24.327759
LOG_PATH = "//auto_host.log"
DD_CMD = "dd if=/dev/zero of=./largefile bs=512K count=1024 2>"

def get_device_name(device):
    return os.path.basename(device)

def get_media_path(device):
    return "/media/" + get_device_name(device)


def is_mounted(device):
    return os.path.ismount(get_media_path(device))

def get_throughput(log_name):
    with open(log_name, "r") as f:
        data = f.read()
        data_size = data.split("\n")[-2].split()[0].strip()
        total_time = data.split("\n")[-2].split()[4].strip()
        throughput = int(data_size)/float(total_time)
        sys.stdout.write("target is %f Bytes/s, measure is %f Bytes/s\n" %(TARGET, throughput))
        sys.stdout.flush()
        if throughput >= TARGET:
            sys.stdout.write("usb host: passed\n")
            sys.stdout.flush()
        else:
            sys.stdout.write("usb host: failed ERROR=%d\n" % USBH_DeV_DN_GRADE)
            sys.stdout.flush()
            unmount(DEV_NAME)
            sys.exit(USBH_DeV_DN_GRADE)


def mount_partition(partition, name="usb"):
    path = get_media_path(name)
    if not is_mounted(path):
        os.system("mkdir -p " + path)
        if os.system("mount %s %s" % (partition, path)) != 0:
            sys.stdout.write("usb host: failed ERROR=%d\n" % USBH_DEV_TYPE_FAIL)
            sys.stdout.flush()
            sys.exit(USBH_DEV_TYPE_FAIL)
        os.system("cd " + path)
        os.system(DD_CMD + LOG_PATH)

def unmount_partition(name="usb"):
    path = get_media_path(name)
    if is_mounted(path):
        os.system("umount " + path)
        #os.system("rm -rf " + path)


def mount(device, name=None):
    if not name:
        name = get_device_name(device)
        mount_partition(device, name)

def unmount(device, name=None):
    if not name:
        name = get_device_name(device)
        unmount_partition(name)

def show_usage():
    sys.stdout.write("usage- python [script] dev=[dev_path] traget=[target_throughput]\n")
    sys.stdout.flush()


def parse_arg():
    global TARGET
    global DEV_NAME
    dev = ''
    target = ''
    for i in range(1, len(sys.argv)):
        if 'dev' in sys.argv[i]:
            dev = sys.argv[i]
            dev = dev.replace('dev=', '')
        if 'target' in sys.argv[i]:
            target = sys.argv[i]
            target = target.replace('target=', '')
        if 'usage' in sys.argv[i]:
            show_usage()
        if 'help' in sys.argv[i]:
            show_usage()
    DEV_NAME = dev
    TARGET = float(target)


if __name__ == "__main__":
    parse_arg()
    
    if os.system("ls " + DEV_NAME) != 0:
        sys.stdout.write("usb host: failed ERROR=%d\n" % USBH_DEV_NO_EXIST)
        sys.stdout.flush()
        sys.exit(USBH_DEV_NO_EXIST)

    mount(DEV_NAME)
    get_throughput(LOG_PATH)
    unmount(DEV_NAME)
