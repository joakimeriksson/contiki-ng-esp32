# CONTIKI_DISPLAY is a build property set from the top-level CMakeLists.txt (-DCONTIKI_DISPLAY=ON);
# it links the LCD console (components/contiki-display). The default build has no display code.
idf_build_get_property(display CONTIKI_DISPLAY)

if(display)
  idf_component_register(
      SRCS      "contiki_example_main.c"
      INCLUDE_DIRS  "."
      PRIV_REQUIRES ieee802154 driver nvs_flash contiki-ng-esp32c6 contiki-display
  )
  target_compile_definitions(${COMPONENT_LIB} PRIVATE CONTIKI_DISPLAY=1)
else()
  idf_component_register(
      SRCS      "contiki_example_main.c"
      INCLUDE_DIRS  "."
      PRIV_REQUIRES ieee802154 driver nvs_flash contiki-ng-esp32c6
  )
endif()
