cmake_minimum_required(VERSION 3.10)
project(SysAlertMon C)

# Find the json-c library
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED json-c)

add_executable(SysAlertMon sysAlertMon.c)

target_link_directories(SysAlertMon PRIVATE /home/pi/paho.mqtt.c/build/output/lib  ../mylib)
target_include_directories(SysAlertMon PRIVATE ${JSONC_INCLUDE_DIRS}) # Add the include directories for json-c
target_link_libraries(SysAlertMon libpaho-mqtt3c.so libpaho-mqtt3a.so libpaho-mqtt3as.so libpaho-mqtt3cs.so libmylib.a ${JSONC_LIBRARIES})
install(TARGETS SysAlertMon DESTINATION /home/pi/MWPCore/bin)
