# Install script for directory: /home/m4rch0/ncs/v3.4.1/zephyr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/m4rch0/ncs/toolchains/8285d8ad56/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/arch/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/lib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/soc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/boards/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/subsys/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/drivers/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/nrf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/mcuboot/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/mbedtls/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/trusted-firmware-m/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/cjson/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/azure-sdk-for-c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/cirrus-logic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/openthread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/nrf_wifi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/hal_nordic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/memfault-firmware-sdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/hostap/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/canopennode/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/chre/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/cmsis/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/cmsis-dsp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/cmsis-nn/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/cmsis_6/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/dhara/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/fatfs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/hal_st/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/hal_tdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/hal_wurthelektronik/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/liblc3/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/libmetal/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/libsbc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/littlefs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/loramac-node/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/lvgl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/mipi-sys-t/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/nanopb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/open-amp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/percepio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/picolibc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/segger/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/uoscore-uedhoc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/zcbor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/nrfxlib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/nrf_hw_models/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/modules/connectedhomeip/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/kernel/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/cmake/flash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/cmake/usage/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/cmake/reports/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync/zephyr/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
