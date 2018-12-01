cmake_minimum_required(VERSION 3.2.2)
project(demo_example)
include(CMakeForceCompiler)

#set(INSTALL_LIBS "INSTALL_PATH/lib/")
#set(INSTALL_INC "INSTALL_PATH/include")

set(INSTALL_LIBS "/home/install/soc_bm1880_asic/lib/")
set(INSTALL_INC "/home/install/soc_bm1880_asic/include")

# Set a toolchain path. You only need to set this if the toolchain isn't in
# your system path. Don't forget a trailing path separator!
set( TC_PATH "" )

# The toolchain prefix for all toolchain executables
set( CROSS_COMPILE aarch64-linux-gnu- )
set( ARCH arm64 )

# specify the cross compiler. We force the compiler so that CMake doesn't
# attempt to build a simple test program as this will fail without us using
# the -nostartfiles option on the command line
CMAKE_FORCE_C_COMPILER( ${TC_PATH}${CROSS_COMPILE}gcc GNU )
CMAKE_FORCE_CXX_COMPILER( ${TC_PATH}${CROSS_COMPILE}g++ GNU )

# To build the tests, we need to set where the target environment containing
# the required library is. On Debian-like systems, this is
# /usr/aarch64-linux-gnu.
SET(CMAKE_FIND_ROOT_PATH $ENV{TOOLCHAIN_TOPDIR})
# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# We must set the OBJCOPY setting into cache so that it's available to the
# whole project. Otherwise, this does not get set into the CACHE and therefore
# the build doesn't know what the OBJCOPY filepath is
set( CMAKE_OBJCOPY      ${TC_PATH}${CROSS_COMPILE}objcopy
            CACHE FILEPATH "The toolchain objcopy command " FORCE )

# Set the CMAKE C flags (which should also be used by the assembler!
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -std=gnu11" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-to-int-cast" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char" )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsigned-char" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "" )
set( CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char" )


include_directories("${INSTALL_INC}")
link_directories("${INSTALL_LIBS}")

set(BM_LIBS bmiva_engine bmiva_face bmiva_utils)
set(BM_LIBS ${BM_LIBS} bmkernel bmruntime protobuf bmodel)
set(EXTRA_LIBS ${EXTRA_LIBS} pthread)
set(OPENCV_LIBS opencv_imgproc opencv_core opencv_highgui opencv_imgcodecs opencv_videoio video_bm)

add_executable(test_bmodel_face_detector test_bmodel_face_detector.cpp)
add_executable(test_bmodel_face_ssh test_bmodel_face_ssh.cpp)

target_link_libraries(test_bmodel_face_detector ${BM_LIBS} ${OPENCV_LIBS} ${EXTRA_LIBS})
target_link_libraries(test_bmodel_face_ssh ${BM_LIBS} ${OPENCV_LIBS} ${EXTRA_LIBS})
