#!/usr/local/bin/python

from bm_usb_util.bm_usb_pyserial import bm_usb_pyserial
from bm_usb_util.bm_usb_libusb import bm_usb_libusb
import os
import sys
import time
import bm_usb_util.bm_usb_pkt as pkt
from array import array

if __name__ == '__main__':
    print ("bm1880_emmc_download v1.1")
    total_time = time.time()

    bm_usb_serial = bm_usb_pyserial()

    if len(sys.argv) > 1:
        bm_usb_serial.parse_arg()

    print ("Connecting to ROM code...")
    bm_usb_serial.serial_query([pkt.rom_vidpid, pkt.prg_vidpid])
    # delay 20ms to prevent write timeout 
    time.sleep(0.02)
    bm_usb_serial.usb_send_file('bm_dl_magic.bin', 0x4003000, 0)
    print ("Done")

    print ("Sending prg.bin...")
    # if the target address is SRAM, send it by rom usb
    bm_usb_serial.usb_send_file('prg.bin', 0x4003000, 0)

    # Set SRAM flag for prg.bin
    flag = array('B')
    flag.append(ord('M'))
    flag.append(ord('B'))
    flag.append(ord('M'))
    flag.append(ord('B'))
    bm_usb_serial.usb_send_req_data(pkt.BM_USB_NONE, 0x04000388, 12, flag)
    print ("Done")

    print ("Jumping to prg.bin...")
    bm_usb_serial.usb_send_req_data(pkt.BM_USB_JUMP, 0x04003000, 0, None)
    # Delay 1s because rom and prg use the same vid/pid with different speed.
    time.sleep(1)
    bm_usb_serial.serial_query([pkt.prg_vidpid], 3)
    print ("Done")

    print ("Sending FIP.bin...")
    bm_usb_serial.usb_send_file('fip.bin', 0x08003000, 0)

    # Set SRAM flag for uboot
    flag = array('B')
    flag.append(ord('b'))
    flag.append(ord('r'))
    flag.append(ord('m'))
    flag.append(ord('b'))
    bm_usb_serial.usb_send_req_data(pkt.BM_USB_NONE, 0x04000384, 12, flag)
    print ("Done")

    # prg write FIP.bin to eMMC
    bm_usb_serial.usb_send_req_data(pkt.BM_USB_BREAK, 0x04003000, 0, None)
    print ("Downloading FIP.bin to eMMC then reboot to uboot...")
    bm_usb_serial.serial_query([pkt.uboot_vidpid])
    print ("Done")

    print ("Sending ramboot_mini.itb...")
    bm_usb_serial.protocol_send_file('ramboot_mini.itb', 0x010F100000)
    print ("Done")

    print ("Booting linux in ramboot_mini.itb...")
    bm_usb_serial.usb_send_req_data(pkt.BM_USB_BREAK, 0x04003000, 0, None)

    bm_usb_libusb = bm_usb_libusb()
    if len(sys.argv) > 1:
        bm_usb_libusb.parse_arg()

    device = bm_usb_libusb.libusb_query([pkt.kernel_libusb_vidpid], 30)
    print ("Done")

    filename = 'emmc.tar.gz'
    file_size = os.path.getsize(filename)
    file_path = array('B')
    file_path = '/data/emmc.tar.gz'
    print ("Sending %s..." % filename)
    bm_usb_libusb.usb_send_req_kernel(pkt.BM_USB_NONE, file_size, file_path, 0)
    dest_addr = 0x4003000
    bm_usb_libusb.usb_send_file(filename, dest_addr, 0)
    print ("Done")

    print ("Processing %s and then reboot..." % filename)
    print ("It takes about 250 seconds, please wait...")
    time.sleep(5);

    bm_usb_serial.serial_query([pkt.rom_vidpid, pkt.prg_vidpid], 250)

    bm_usb_serial.usb_emmc_dl_verify([pkt.kernel_libusb_vidpid], 30)
    print ("Done")
    print ("Download complete")
    print ("--- %s Seconds ---\n" % round(time.time() - total_time, 2))
