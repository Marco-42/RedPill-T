# RedPill - TT&C

The TT&C (Telemetry, Tracking & Control) subteam of the J2050 group is responsible for the telecommunication, tracking, and control operations of the RedPill PocketQube satellite. This repository provides all the source code and tools required for communication experiments, telemetry, and control between ground stations and CubeSat modules.

## Features

- **LoRa Protocol**: Our communication system is based on the LoRa protocol and managed using RadioLib libraries, ensuring compatibility with the TinyGS network. The software is currently designed to run on LilyGO LoRa32 v1.6.1 modules.

- **FreeRTOS Integration**: FreeRTOS is implemented to guarantee real-time operations, efficient task management, and reduced execution time.

- **Graphical User Interface (GUI)**: A user-friendly interface is included for testing procedures and future ground station operations. The GUI manages serial communication and provides full control over the communication process.

- **Database Management**: Acquired data is managed and stored using an SQLite database, which can be integrated directly into any communication interface. For ensure accessibility, a dedicated interface for database management is also provided.

- **Telemetry & Control**: Satellite telemetry data, stored in the database, are directly visualized using a Grafana dashboard. An orbit simulation software based on the SGP4 model has also been developed to support future satellite tracking operations.

## Repository Structure

- **esp32/**  
  Source code for ESP32-based LoRa communication and state machines.

- **python/**  
  Python scripts for database management, GUI visualization, and data analysis.

- **grafana/**  
  Grafana dashboards for telemetry visualization.

- **UPC codes/**  
  Additional C code utilities.
