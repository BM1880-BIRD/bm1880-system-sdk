cmd_usr/include/linux/iio/.install := /bin/bash /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/scripts/headers_install.sh ./usr/include/linux/iio /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/include/uapi/linux/iio events.h types.h; /bin/bash /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/scripts/headers_install.sh ./usr/include/linux/iio /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/include/linux/iio ; /bin/bash /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/scripts/headers_install.sh ./usr/include/linux/iio ./include/generated/uapi/linux/iio ; for F in ; do echo "\#include <asm-generic/$$F>" > ./usr/include/linux/iio/$$F; done; touch usr/include/linux/iio/.install
