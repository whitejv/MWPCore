# Setup for Alarm Code Generator

This document outlines the one-time setup process for the Python environment needed to auto-generate the C alarm logic from a CSV file.

This process uses a Python virtual environment (`venv`) to ensure that all dependencies are isolated to this project and will **not** interfere with the system's global Python installation or other applications like `pyrainbird`.

## One-Time Environment Setup

These steps only need to be performed once. They set up the `scripts/` directory, create the virtual environment, and install the necessary `pandas` library.

Execute these commands from the project root directory (`/home/pi/MWPCore`).

1.  **Create the `scripts` Directory**

    ```bash
    mkdir -p scripts
    ```

2.  **Navigate into the `scripts` Directory**

    ```bash
    cd scripts
    ```

3.  **Create the Python Virtual Environment**

    This creates a self-contained environment in a new folder named `venv`.

    ```bash
    python3 -m venv venv
    ```

4.  **Activate the Virtual Environment**

    Your command prompt will change to indicate that the environment is active (e.g., it will start with `(venv)`).

    ```bash
    source venv/bin/activate
    ```

5.  **Install the Required Python Library**

    While the environment is active, use `pip` to install the `pandas` library.

    ```bash
    pip install pandas
    ```

6.  **Create a `requirements.txt` File (Good Practice)**

    This saves a list of the installed libraries for documentation and easy re-installation if needed.

    ```bash
    pip freeze > requirements.txt
    ```

7.  **Deactivate the Environment**

    You can now "step out" of the virtual environment. Your command prompt will return to normal.

    ```bash
    deactivate
    ```

The setup is now complete. The `scripts` directory should contain your `alarms.csv`, your future `generate_alarms.py` script, the `requirements.txt` file, and the `venv/` directory.

---

## Usage Workflow (Generating C Code)

After you have modified your `alarms.csv` file, follow these steps to regenerate the `alarm_engine.c` and `alarm_config.h` files:

1.  **Navigate to the `scripts` directory:**
    ```bash
    cd /home/pi/MWPCore/scripts
    ```

2.  **Activate the virtual environment:**
    ```bash
    source venv/bin/activate
    ```

3.  **Run the generator script:**
    ```bash
    python3 generate_alarms.py
    ```

4.  **Deactivate the environment:**
    ```bash
    deactivate
    ```

5.  **Navigate back to the project root and compile:**
    ```bash
    cd ..
    make
    ``` 