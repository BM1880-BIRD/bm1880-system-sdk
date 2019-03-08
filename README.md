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

### Build SDK, you can build sdk for EDB **OR** NNM
#### Build SDK for EDB:
```bash
$ cd ../../
$ source build/envsetup_edb.sh
$ build_all
```

#### Build SDK for NNM:
```bash
cd ../../
source build/envsetup_nnm.sh
build_all
```

#### Build output:
```bash
$ tree -L 2 install/soc_bm1880_asic_edb/
install/soc_bm1880_asic_edb/

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

##### Make SD boot image in SD-Card for EDB:

    https://sophon-edge.gitbook.io/project/overview/edge-tpu-developer-board#sd-boot

##### Download eMMC boot Image for EDB:

    https://sophon-edge.gitbook.io/project/overview/edge-tpu-developer-board#emmc-boot

##### Download eMMC boot Image for NNM:

    https://sophon-edge.gitbook.io/project/overview/edge-tpu-developer-board#emmc-boot

