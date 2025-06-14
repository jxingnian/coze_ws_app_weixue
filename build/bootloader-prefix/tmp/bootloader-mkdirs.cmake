# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif5.3/frameworks/esp-idf-v5.3/components/bootloader/subproject"
  "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader"
  "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix"
  "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix/tmp"
  "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix/src/bootloader-stamp"
  "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix/src"
  "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/esp-adf/examples/ai_agent/coze_ws_app/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
