## Project 

This repository contains the core project structure, templates, and supporting files for the ECE3073 mini-project on **Real-Time Embedded Vision using ARM and FPGA-based multitasking systems**.

The project focuses on integrating an edge-AI vision module with a NIOS II multicore system on FPGA to capture, process, and display real-time visual data. The system performs text recognition using a vision module and executes embedded instructions through hardware peripherals such as VGA, LEDs, HEX displays, and audio output.

The provided files are designed to guide development across multiple milestones, from basic hardware interfacing to full system integration and optimization.

### Key Features Covered
- FPGA-based embedded system using DE10-Lite
- Multicore NIOS II processor architecture
- Real-time image/text processing via Grove AI Vision module
- SPI/UART communication between modules
- VGA display output and graphical rendering
- Hardware interaction (LEDs, switches, HEX display, speaker)
- Accelerometer-based orientation control
- Optional RTOS for task scheduling and multitasking

## Structure
```
ECE3073-Project2025-Lab02-Group4/
│-- M1/              # Contains VGA controller skeleton
│-- M2/              # Skeleton files for image convolution and SPI-Gyro communication skeleton
|-- project/         # Main project file, currently for M1
│-- .gitignore       # Git ignore file to be used with your Quartus project
│-- README.md        # Documentation and/or instructions

```
## Getting Started
# What You'll Need:
1. A DE10-Lite board with cable
2. Breadboard
3. Groove AI Vision 2
4. 5MP OV5647 Camera Module for Raspberry Pi with camera ribbon
5. Crow-tail speaker
6. LED Module

# Ensure you have the software downloaded as well
1. Nios II Software Build Tools for Eclipse [Instruction Guide here](https://www.terasic.com.tw/wiki/Getting_Start_Install_Eclipse_IDE_into_Nios_EDS)
2. Quartus Programmer Prime Lite [Installation Here](https://www.intel.com/content/www/us/en/collections/products/fpga/software/downloads.html)

## Steps to operate the device
# Software
1. Clone the repository:
   ```sh
   git clone https://github.com/ece3073-monash/project-templates.git
   ```
2. Copy files .gitignore and template files in your project directory.
3. M1 and M2 are template files, the current Milestone is updated and stashed on their respective files after demo under 'project' file. A new project file is created after each demo.

# Hardware
1. Follow the pinout connection as shown below:
   ![Wiring Diagram](https://i.imgur.com/L8OQDLU.png)

## Roadmap
### Milestone 1: VGA Controller
- [x] Initial NIOS II Hardware Setup (WC)
   - [x] Delegation of tasks is completed
- [x] Switches & HEX dispay completed (SEAN)
- [x] VGA & Speaker Completed (LX)
- [x] SPI Trial & Accelerometer Completed (WC)
- [x] System Integration
   - [x] Meeting #1 at 20/4/2026
      - [x] Integration of Speaker and LED Module (SEAN)
      - [x] Attempt at 2 processor VGA (LX)
      - [x] Integration of SPI (WC)
- [x] System Integration successful and Tried on Meeting #2 22/4/2026

---

### Milestone 2: Image Convolution & SPI-Gyro

---

### Milestone 3: System Integration

---

### Final Deliverables


## Contributing

The following contributors are involved in this project:

| Name            | Student ID | Email                          |
|-----------------|-----------|--------------------------------|
| Chin Wei Chun   | 33520569  | wchi0051@student.monash.edu    |
| Sean Loh Kim Fook | 34640509  | sloh0020@student.monash.edu    |
| Ooi Li Xiang    | 33070040  | looi0005@student.monash.edu    |

## License
Copyright © 2026 Copyright, Monash University





