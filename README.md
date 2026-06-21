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
2. Ensure the VGA connector is connected to a monitor.

## Results
The following demonstrates the results of the final project demonstration.
![Profile picture](./assets/IsometricView_Setup.png)
## What is Expected
# Milestone 1
1. SW0 - Controls HEX display
2. SW1 - Displays the captured sentence, in this case a dummy sentence is hardcoded into the switch.
3. SW2 - Displays the CPU usage percentage, in this case a dummy sentence is hardcoded into the switch.
4. SW3 - The buzzer is activated, in this case a dummy noise with a specific frequency is hardcoded into the switch.
5. KEY1 - Displays the captured image into the monitor, in this case a dummy image is coded into the button.
6. Traffic LEDs - Operates as according to the sentence, in this case dummy instructions is provided to the LED.
7. Accelerometer - Measures the tilt axis of the FPGA board.
8. Speaker - Paired with SW3 to emit noise.
9. HEX Display - Display message (1st to 4th display) and CPU usage (5th and 6th display).
10. SPI - Exchange message between ESP32 board and the FPGA board.

# Milestone 2
1. Groove AI V2 Module - Converts camera input to digital data, which proceeds to transmit to the FPGA system.
2. Camera - Able to capture camera input to be sent to the Module.
3. De10-Lite Board - Able to perform multi-processor architecture.
4. SW1 - Displays captured sentence from Groove AI V2 Module.
5. SW2 - Displays the CPU usage percentage, in this case a dummy sentence is hardcoded into the switch.
6. Accelerometer - Measures tilt angle, paired with the VGA to show on board movement. Also able to detect if the board is over tilted (emergency stop).
7. VGA - 1) Displays text / image from the Groove AI V2 Module 2) Display board tilt movement
8. KEY1 - Captures the image, wait until it receives an image.
9. KEY0 - Displays the image captured.

# Milestone 3
# DE10-Lite Triple-Core AI Snake

## a) Key Features (Full Integration)

## a) Key Features

### 🏗️ Core Architecture & Processing
* **Triple-Core Nios II System:** Workloads are completely isolated across three dedicated soft-cores (Control, Image, and VGA) operating in parallel to eliminate traditional I/O bottlenecking.
* **Shared SDRAM Fabric:** Implements a decoupled memory communication layout separating active runtime variables (`0x05212000`) from a standalone debug-control mailbox (`0x06000000`) to guarantee thread safety.
* **$\mu$C/OS-II Real-Time Operating System:** Deployed on the Control processor to handle concurrent peripheral updating, event dispatch handling, and background performance tracking via deterministic priority scheduling.

### 🔌 Hardware & Peripherals
* **ESP32-C3 & Grove Vision AI V2:** Captures physical text arrays or images, executing edge-based OCR processing before piping data packets across an Avalon SPI master interface.
* **On-board ADXL345 Accelerometer:** Reads physical board tilt angles, translating telemetry changes into real-time cursor navigation and motion controls.
* **VGA Graphics Engine:** Directly outputs RGB332 8-bit color landscapes onto an external monitor at an unyielding 60 FPS frame rate.
* **Audio & Visual Indicators:** Uses direct register I/O manipulation to drive onboard 7-segment HEX modules, LEDR bars, and passive audio pins.

### 🕹️ User Controls (Master Switches & Buttons)
* **SW1:** Master enable for the 7-segment HEX displays.
* **SW2:** Enables processing and rendering of incoming scrolling text arrays parsed via OCR.
* **SW3:** Triggers a real-time CPU utilization overlay on the final two HEX modules.
* **SW4:** Hardware gate to safely isolate or silence the active speaker system.
* **KEY0 & KEY1:** Primary physical push-buttons handling menu confirmations, state retries, and live camera snapshot triggers.

## b) Summarised Game Functions

### 🎮 Multi-Game Ecosystem
* **Classic Snake:** Navigate a 32x22 wrap-around arena complete with directional input filtering, portal mappings, and a progressive score tally reflected across the onboard LEDR array.
* **Draw Pixel:** A high-density 96x96 virtual sketch canvas. Pressing `KEY0` captures a 96x96 live grayscale snapshot from the camera interface, instantly rendering it into the background layer as an interactive texture.
* **Battleship:** A tactical grid-aiming game utilizing real-time accelerometer tilt tracking to position your cursor, featuring cross and square weapon strike variations.

### 📜 Real-Time Instruction & Debug Processing
* **Python UI Synchronization:** A companion Python engine translates terminal commands into fixed-width string lines.
* **OCR Command Ingestion:** The Grove AI module reads scrolling characters and passes them to the FPGA's Image Core, which waits for a `ZZZ` execution delimiter before safely printing dynamic parameters to the debug mailbox without interrupting active graphics tasks.
* **Dynamic Performance Overlays:** Toggling `SW3` polls internal $\mu$C/OS-II timing counters, overriding the 7-segment display data to continuously track and print active processor load values during dense gameplay loops.
---

## b) Summarised Game Functions

* **Classic Snake Mechanics:** Navigate the grid, eat apples to increase your score and length, and avoid colliding with walls or your own tail.
* **Live Map Generation:** Draw a maze or obstacles on a physical piece of paper, show it to the camera, and watch the hardware translate it into playable in-game walls.
* **Motion Control Steering:** Tilt the physical DE10-Lite board up, down, left, or right to steer the snake using real-time accelerometer data.
* **Arcade Hardware Integration:** Every in-game action is tied to physical hardware. Eating an apple flashes the green LED and plays a chirp; hitting a wall locks the board into a flashing red-light "Game Over" state with a dramatic audio fanfare.

---

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
- [x] Updated NIOS II Hardware Setup (WC)
   - [x] Delegation of tasks is completed
- [x] Switches & Pushbutton interrupt completed (SEAN)
- [x] VGA connection to Accelerometer completed (LX)
- [x] Training of AI model and storing image to SDRAM completed (WC)
- [x] System Integration & Progress Checkup
   - [x] Meeting #3 at 4/5/2026
      - [x] Inclusion of GPIO toggling for latency measurement (SEAN)
      - [x] VGA display formatting (LX)
      - [x] Further training of the AI model for reliability (WC)
- [x] System Integration
   - [x] Meeting #4 at 5/5/2026
      - [x] Integration testing of control core & image core (SEAN + WC)
      - [x] VGA display is able to show SDRAM image (LX)
      - [x] Further updates to the NIOS II Hardware (WC)
- [x] System Integration successful and Tried on Meeting #5 6/5/2026

---

### Milestone 3: System Integration
- [x] Delegation of tasks is completed
- [x] Gameplay (Ships and Snakes) (WC)
   - [x] Implementation of Sound Effects and Game Effects (LX)
- [x] Implementation of RTOS for the Control Core (SEAN)

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





