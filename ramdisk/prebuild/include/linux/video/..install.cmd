cmd_usr/include/video/.install := /bin/bash /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/scripts/headers_install.sh ./usr/include/video /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/include/uapi/video edid.h sisfb.h uvesafb.h; /bin/bash /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/scripts/headers_install.sh ./usr/include/video /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/include/video ; /bin/bash /home/ethanchen/Workspace_VB/bm1880_test/linux-linaro-stable/scripts/headers_install.sh ./usr/include/video ./include/generated/uapi/video ; for F in ; do echo "\#include <asm-generic/$$F>" > ./usr/include/video/$$F; done; touch usr/include/video/.install
