# Fil Pilote Radiator Controller -- ESP32-C3

A compact Wi-Fi-enabled controller for French *fil pilote* electric
radiators, built around an ESP32-C3 module.

The system uses optotriacs and simple transistor drivers to generate the
four standard *fil pilote* modes, and communicates via MQTT for seamless
integration with Home Assistant.

## Features

-   Controls **4 radiators**, each supporting the 4 official *fil
    pilote* modes:
    -   Confort (no signal)
    -   Eco (negative half-wave)
    -   Hors-gel / Frost protection (positive half-wave)
    -   Stop (full wave)
-   Uses **2 MOC3021 optotriacs per radiator** (8 total)
-   Uses optoisolation to ensure safe AC control
-   Uses *2N2222* transistor to limit the current steered from GPIOs
-   On-board status LEDs for each half-wave
-   MQTT-based command interface
-   Compact hardware using an ESP32-C3 module

## How the Fil Pilote System Works

French electric radiators accept control commands via a dedicated wire
called the *fil pilote*.
By injecting specific AC waveforms, the radiator switches modes:

```
Confort:    No signal
Eco:        Negative half-wave
Hors-gel:   Positive half-wave
Stop:       Full-wave (both half-waves)
```

## Hardware Overview

### Components

-   1 ESP32-C3 microcontroller board
-   8 MOC3021 optotriacs
-   8 Diodes for AC steering
-   8 Status LEDs
-   8 2N2222 NPN transistors
-   Resistors:
    -   120 Ω for the MOC3021 LED drive
    -   10k Ω resistors for the 2N2222 base
    -   1k - 4.7k (depends on LED color) LED resistors for the status indicators

### Electrical Principles

-   Each radiator needs **two MOC3021s** (one for each half-wave).
-   A correctly oriented diode allows the MOC to pass either the
    positive or negative AC half-cycle.
-   The ESP32-C3 GPIO activates a transistor, which drives the MOC LED
    (and indicator LED).
-   The radiator receives the correct waveform depending on which MOCs
    are activated.

**Safety note:**
You are working with 230 V AC. Ensure correct isolation, board spacing,
and a safe enclosure.

## Firmware

*Note:* The topics and messages can be modified through esp config.

```bash
$ idf.py menuconfig
```

The firmware listens to four MQTT commands:

-   `confort`
-   `eco`
-   `hors-gel`
-   `stop`

Each radiator listens on:

    <base_topic>/<id>/set

## MQTT Configuration Example

### Example topic structure

    heater-control/1/set
    heater-control/2/set
    heater-control/3/set
    heater-control/4/set

## Getting Started

### 1. Ensure ESP-IDF is installed

You must have the ESP-IDF toolchain installed.
(Installation instructions will be added later.)

### 2. Activate the ESP-IDF environment

Before building the project, activate ESP-IDF.
(Steps will be documented later.)

### 3. Set the target to ESP32-C3

From the project directory:

``` bash
idf.py set-target esp32-c3
```

This generates a `sdkconfig` file.

### 4. Open the configuration menu

``` bash
idf.py menuconfig
```

Navigate to:

    Component config →
       Heater Control →

Configure:

-   Wi-Fi SSID
-   Wi-Fi password
-   GPIO assignments
-   MQTT server settings

Save and exit.
