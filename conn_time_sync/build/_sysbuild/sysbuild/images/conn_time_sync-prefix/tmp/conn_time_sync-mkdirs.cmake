# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync")
  file(MAKE_DIRECTORY "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync")
endif()
file(MAKE_DIRECTORY
  "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/conn_time_sync"
  "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix"
  "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix/tmp"
  "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix/src/conn_time_sync-stamp"
  "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix/src"
  "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix/src/conn_time_sync-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix/src/conn_time_sync-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/m4rch0/marco/master/master_thesis/nrf5340_sync/conn_time_sync/build/_sysbuild/sysbuild/images/conn_time_sync-prefix/src/conn_time_sync-stamp${cfgdir}") # cfgdir has leading slash
endif()
