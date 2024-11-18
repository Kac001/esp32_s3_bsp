# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Administrator/esp/v5.2.2/esp-idf/components/bootloader/subproject"
  "D:/Code/esp32-s3/14-handheld/build/bootloader"
  "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix"
  "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix/tmp"
  "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix/src"
  "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Code/esp32-s3/14-handheld/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
