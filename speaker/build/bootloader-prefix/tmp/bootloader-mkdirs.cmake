# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/nix/store/fmn799jqhyhrvpsqwv0g9irf31iw820h-esp-idf-v5.4/components/bootloader/subproject"
  "/home/david/Downloads/git/sms/speaker/build/bootloader"
  "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix"
  "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix/tmp"
  "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix/src/bootloader-stamp"
  "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix/src"
  "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/david/Downloads/git/sms/speaker/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
