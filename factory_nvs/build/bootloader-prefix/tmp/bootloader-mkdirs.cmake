# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Program_Files/esp-idf-510-offline/frameworks/esp-idf-v5.1/components/bootloader/subproject"
  "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader"
  "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix"
  "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix/tmp"
  "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix/src"
  "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Program_Files/esp-box-master/examples/chatgpt_wgdemo/factory_nvs/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
