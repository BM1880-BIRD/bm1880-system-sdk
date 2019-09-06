# bm1880-system-sdk
## SDK for bm1880 Edge Developement Board(EDB) and Neural Network Module(NNM)

#### Get code:

```bash
$ mkdir bm1880 && cd bm1880
$ git clone https://github.com/BM1880-BIRD/bm1880-system-sdk.git
```

#### Get cross-compile toolchains:

```bash
$ mkdir -p ./bm1880-system-sdk/host-tools/gcc && cd ./bm1880-system-sdk/host-tools/gcc
$ wget https://sophon-file.bitmain.com.cn/sophon-prod/drive/18/11/08/11/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu.tar.xz.zip
$ unzip gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu.tar.xz.zip && xz -d gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu.tar.xz
$ tar -xvf gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu.tar
```

#### preparation:  

update protobuf to version 3.6 with libprotobuf.so.17  
  

### Build SDK, you can build sdk for EDB **OR** NNM
#### Build SDK for EDB:

```bash
$ sudo apt-get install device-tree-compiler

```
```bash
$ cd ../../
$ source build/envsetup_edb.sh
$ build_all
```

#### Build SDK for NNM:
```bash
$ cd ../../
$ source build/envsetup_nnm.sh
$ build_all
```

#### Build output:
###### For EDB:
```bash
$ tree -L 2 install/soc_bm1880_asic_**edb**/
install/soc_bm1880_asic_**edb**/

//Images for eMMC boot
├── bm1880_emmc_dl_v1p1
│   ├── bm1880_emmc_download.py
│   ├── bm_dl_magic.bin
│   ├── bm_usb_util
│   ├── emmc.tar.gz
│   ├── fip.bin
│   ├── prg.bin
│   ├── ramboot_mini.itb

//Images for SD card boot
├── fip.bin
├── rootfs
├── sdboot.itb
```
###### For NNM:
```bash
$ tree -L 2 install/soc_bm1880_asic_**nnm**/
install/soc_bm1880_asic_**nnm**/

//Images for eMMC boot
├── bm1880_emmc_dl_v1p1
│   ├── bm1880_emmc_download.py
│   ├── bm_dl_magic.bin
│   ├── bm_usb_util
│   ├── emmc.tar.gz
│   ├── fip.bin
│   ├── prg.bin
│   ├── ramboot_mini.itb

```

##### Make SD boot image in SD-Card for EDB:

https://sophon-edge.gitbook.io/project/overview/edge-tpu-developer-board#sd-boot

##### Download eMMC boot Image for EDB:

https://sophon-edge.gitbook.io/project/overview/edge-tpu-developer-board#emmc-boot

##### Download eMMC boot Image for NNM:

https://sophon-edge.gitbook.io/project/overview/edge-tpu-developer-board#emmc-boot



### v1.1.4 Major change list:

    	1. 新增USB ConfigFS功能(可以想更新U盘一样更新emmc rootfs分区，提高开效率)
    	2. 新增以太网TFTP更新软件功能
    	3. 新增thermal and CCF framework
    	4. 解決RTSP长时间测试随机断线问题
    	5. 解決USB async mode不稳定问题
    	6. 优化OpenCV API: VPP硬件resize加速, 多合一功能加速

