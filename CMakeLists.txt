cmake_minimum_required(VERSION 3.2)
project (MilanoWaterProject)
add_subdirectory (./mylib)
add_subdirectory (./IrrigationMonitor)
add_subdirectory (./TankMonitor)
add_subdirectory (./RainbirdMon)
add_subdirectory (./Monitor)
add_subdirectory (./WellMonitor)
add_subdirectory (./HouseMonitor)
add_subdirectory (./Well3Monitor)
add_subdirectory (./SysAlert)
add_subdirectory (./SysAlertMon)
add_subdirectory (./Blynk)
add_subdirectory (./BlynkAlert)
add_custom_target(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND rm -f bin/wellmonitor
   COMMAND rm -f bin/rainbirdmon
   COMMAND rm -f bin/irrigationmonitor
   COMMAND rm -f bin/tankmonitor
   COMMAND rm -f bin/housemonitor
   COMMAND rm -f bin/well3monitor
   COMMAND rm -f bin/blynk
   COMMAND rm -f bin/monitor
   COMMAND rm -f bin/alert
   COMMAND rm -f bin/blynkalert
   # Add more commands as needed
)