# generate_alarms.py

This Python script generates C code for the alarm engine based on alarm definitions in a CSV file.

## Prerequisites

- Python 3
- pandas library (`pip3 install pandas`)

## Usage

1. Activate your Python environment (if using a virtual environment):

   ```bash
   source /path/to/your/venv/bin/activate
   ```

2. Ensure `alarms.csv` is in the `scripts` directory with the alarm definitions.
3. Run the script from the project root:

   ```bash
   python scripts/generate_alarms.py
   ```

## Input

- `scripts/alarms.csv`: CSV file containing alarm definitions (columns: Alarm ID, Alarm Type, Label, Event Message, etc.)

## Output

- `alarm_engine/alarm_config.h`: Header file with alarm types and constants.
- `alarm_engine/alarm_logic.c.inc`: Include file with alarm evaluation logic.

The script will overwrite existing files. Do not edit the generated files manually.

## Notes

- The script sorts alarms by Alarm ID.
- Ensure the CSV is valid to avoid errors.