# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# -DCONTIKI_DISPLAY=ON: LCD console + status view on the Waveshare ESP32-C6-LCD-1.47
# (components/contiki-display). Exposed as a build property because component
# requirements are evaluated in a separate cmake pass that cannot see cache options.
option(CONTIKI_DISPLAY "Show the console and node status on the on-board LCD" OFF)
if(CONTIKI_DISPLAY)
  idf_build_set_property(CONTIKI_DISPLAY 1)
endif()

project(esp32-blink)
