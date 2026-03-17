# Monitor Block Canonical Name Map (Superset)

This map keeps all current `MONITOR_` fields from `include/water.h` and assigns each one a canonical snake_case name.

- No existing field is dropped.
- The canonical schema is a superset for ThingsBoard.
- Primary telemetry keys from the ThingsBoard canonical spec are preserved exactly.
- Additional monitor/Blynk operational fields are retained as canonical extension keys.

## Source

- File: `include/water.h`
- Block: `union MONITOR_ { struct monitor { ... } }`
- Length: `MONITOR_LEN = 38`

## Superset Field Map (Grouped by Function)

### Tank Level & Storage

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 0 | `Tank_Water_Height` | `tank_height_ft` | Canonical TB key |
| 1 | `Tank_Gallons` | `tank_gallons` | Canonical TB key |
| 2 | `Tank_Percent_Full` | `tank_percent_full` | Canonical TB key |

### Pressure & Flow Telemetry

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 3 | `House_Pressure` | `house_pressure_psi` | Canonical TB key |
| 4 | `Well3_Pressure` | `well3_pressure_psi` | Canonical TB key |
| 5 | `Irrigation_Pressure` | `irrigation_pressure_psi` | Canonical TB key |
| 6 | `House_Gallons_Minute` | `house_gpm` | Canonical TB key |
| 7 | `Well3_Gallons_Minute` | `well3_gpm` | Canonical TB key |
| 8 | `Irrigation_Gallons_Minute` | `irrigation_gpm` | Canonical TB key |
| 9 | `House_Gallons_Day` | `house_gallons_day` | Canonical TB key |
| 10 | `Well3_Gallons_Day` | `well3_gallons_day` | Canonical TB key |
| 11 | `Irrigation_Gallons_Day` | `irrigation_gallons_day` | Canonical TB key |

### Temperature Telemetry

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 12 | `System_Temp` | `system_temp_f` | Canonical TB key |
| 13 | `House_Water_Temp` | `house_temp_f` | Canonical TB key |
| 14 | `Irrigation_Pump_Temp` | `irrigation_temp_f` | Canonical TB key |
| 15 | `Air_Temp` | `air_temp_f` | Canonical TB key |
| 38 | `well3Mon_.well3.temperatureF` | `well3_temp_f` | Additional current monitor-message key (runtime merge) |

### LED Status & Indicators

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 18 | `Well_1_LED_Bright` | `well_1_led_brightness` | Canonical extension |
| 19 | `Well_2_LED_Bright` | `well_2_led_brightness` | Canonical extension |
| 20 | `Well_3_LED_Bright` | `well_3_led_brightness` | Canonical extension |
| 21 | `Irrig_4_LED_Bright` | `irrigation_4_led_brightness` | Canonical extension |
| 22 | `Spare1_LED_Bright` | `spare_1_led_brightness` | Canonical extension |
| 23 | `Spare2_LED_Bright` | `spare_2_led_brightness` | Canonical extension |
| 24 | `Well_1_LED_Color` | `well_1_led_color` | Canonical extension |
| 25 | `Well_2_LED_Color` | `well_2_led_color` | Canonical extension |
| 26 | `Well_3_LED_Color` | `well_3_led_color` | Canonical extension |
| 27 | `Irrig_4_LED_Color` | `irrigation_4_led_color` | Canonical extension |

### Irrigation Control Context

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 28 | `Controller` | `irrigation_controller` | Canonical extension |
| 29 | `Zone` | `irrigation_zone` | Canonical extension |

### Pump Runtime Metrics

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 30 | `Pump_1_RunCount` | `pump_1_run_count` | Canonical extension |
| 31 | `Pump_1_RunTime` | `pump_1_run_time_sec` | Canonical extension |
| 32 | `Pump_2_RunCount` | `pump_2_run_count` | Canonical extension |
| 33 | `Pump_2_RunTime` | `pump_2_run_time_sec` | Canonical extension |
| 34 | `Pump_3_RunCount` | `pump_3_run_count` | Canonical extension |
| 35 | `Pump_3_RunTime` | `pump_3_run_time_sec` | Canonical extension |
| 36 | `Pump_4_RunCount` | `pump_4_run_count` | Canonical extension |
| 37 | `Pump_4_RunTime` | `pump_4_run_time_sec` | Canonical extension |

### Pump/Well State Flags

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 39 | `wellMon_.well.well_pump_1_on` | `well_pump_1_on` | Additional current monitor-message key (runtime merge) |
| 40 | `wellMon_.well.well_pump_2_on` | `well_pump_2_on` | Additional current monitor-message key (runtime merge) |
| 41 | `wellMon_.well.well_pump_3_on` | `well_pump_3_on` | Additional current monitor-message key (runtime merge) |
| 42 | `wellMon_.well.irrigation_pump_on` | `irrigation_pump_on` | Additional current monitor-message key (runtime merge) |

### Safety, Switches & Metadata

| Index | Current Name | Future Canonical Name | Notes |
|---|---|---|---|
| 16 | `Spare1` | `monitor_spare_1` | Canonical extension |
| 17 | `Spare2` | `monitor_spare_2` | Canonical extension |
| 43 | `wellMon_.well.House_tank_pressure_switch_on` | `house_tank_pressure_switch_on` | Additional current monitor-message key (runtime merge) |
| 44 | `wellMon_.well.septic_alert_on` | `septic_alert_on` | Additional current monitor-message key (runtime merge) |
| 45 | `publisher runtime timestamp` | `ts` | Additional current monitor-message key (runtime merge) |

## Superset Summary

- Base monitor payload keys from `MONITOR_`: 38
- Additional merged keys for superset message: 8
- Total superset keys: 46

## Migration Note

When renaming via spreadsheet/macro generation, keep field order stable for binary compatibility (`MONITOR_LEN`, `monitor_.data_payload[]`) and update both:

- struct field names in `MONITOR_`
- character names in `monitor_ClientData_var_name[]`

This preserves binary payload shape while moving to canonical JSON names.

## Spreadsheet Paste List (Canonical Keys in Displayed Order)

tank_height_ft,
tank_gallons,
tank_percent_full,
house_pressure_psi,
well3_pressure_psi,
irrigation_pressure_psi,
house_gpm,
well3_gpm,
irrigation_gpm,
house_gallons_day,
well3_gallons_day,
irrigation_gallons_day,
system_temp_f,
house_temp_f,
irrigation_temp_f,
air_temp_f,
well3_temp_f,
well_1_led_brightness,
well_2_led_brightness,
well_3_led_brightness,
irrigation_4_led_brightness,
spare_1_led_brightness,
spare_2_led_brightness,
well_1_led_color,
well_2_led_color,
well_3_led_color,
irrigation_4_led_color,
irrigation_controller,
irrigation_zone,
pump_1_run_count,
pump_1_run_time_sec,
pump_2_run_count,
pump_2_run_time_sec,
pump_3_run_count,
pump_3_run_time_sec,
pump_4_run_count,
pump_4_run_time_sec,
well_pump_1_on,
well_pump_2_on,
well_pump_3_on,
irrigation_pump_on,
monitor_spare_1,
monitor_spare_2,
house_tank_pressure_switch_on,
septic_alert_on,
ts,
