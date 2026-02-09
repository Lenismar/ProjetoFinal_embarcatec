# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/italo/.pico-sdk/sdk/1.5.1/tools/pioasm")
  file(MAKE_DIRECTORY "/home/italo/.pico-sdk/sdk/1.5.1/tools/pioasm")
endif()
file(MAKE_DIRECTORY
  "/home/italo/Downloads/projetofinal-main/build/pioasm"
  "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm"
  "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/PioasmBuild-stamp"
  "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/PioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/PioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/italo/Downloads/projetofinal-main/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/PioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
