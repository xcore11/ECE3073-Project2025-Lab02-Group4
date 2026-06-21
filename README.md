# ECE3073 DE10-Lite Triple-Core AI Snake

A real-time embedded vision and FPGA gaming project developed for the ECE3073 mini-project. The system combines a DE10-Lite FPGA, multiple Nios II soft-core processors, Grove Vision AI V2, an ESP32-C3, camera input, VGA graphics, accelerometer motion control, LEDs, HEX displays, and audio feedback.

The final system runs a multi-game interface with **Snake**, **Draw Pixel**, and **Battleship**, while also demonstrating real-time OCR/image capture, shared-memory communication, hardware multitasking, and VGA rendering.

---

## About This Project

This project explores **real-time embedded vision using FPGA-based multitasking systems**. It integrates an edge-AI vision module with a multicore Nios II system on the DE10-Lite board to capture, process, and display visual data on a VGA monitor.

The final implementation uses a **triple-core Nios II architecture** where major workloads are separated into dedicated processing roles:

- **Control Core**: handles user input, switches, push-buttons, audio, LEDs, HEX displays, and RTOS-based task scheduling.
- **Image Core**: receives camera/OCR data from the Grove Vision AI V2 and ESP32-C3 communication path.
- **VGA Core**: renders menus, game screens, captured images, and graphical overlays to an external monitor.

The system demonstrates how hardware peripherals and software tasks can work together in real time. Users interact with the games through the DE10-Lite switches, push-buttons, and accelerometer-based tilt controls.

---

## Project Showcase

> Make sure these image files are placed inside an `assets/` folder in your repository. If your files use `.jpg` or `.png`, update the file extensions below.

<p align="center">
  <img src="./assets/IsometricView_Setup.jpeg" alt="Isometric view of the full FPGA setup" width="360">
  <img src="./assets/BirdEyeView_Setup.jpeg" alt="Bird-eye view of the hardware setup" width="360">
</p>

<p align="center">
  <img src="./assets/MainMenu.jpeg" alt="Main menu displayed on VGA monitor" width="360">
  <img src="./assets/SnakeGame_Menu.jpeg" alt="Snake game menu displayed on VGA monitor" width="360">
</p>

<p align="center">
  <img src="./assets/BattleShip_Menu.jpeg" alt="Battleship game menu displayed on VGA monitor" width="360">
  <img src="./assets/DrawPixel_Menu.jpeg" alt="Draw Pixel menu displayed on VGA monitor" width="360">
</p>

<p align="center">
  <img src="./assets/3DprintedMount.jpeg" alt="3D printed camera or hardware mount" width="300">
</p>

---

## Key Features

### Embedded System Architecture

- Triple-core Nios II processor system on FPGA.
- Shared SDRAM communication between processing cores.
- RTOS-based scheduling using μC/OS-II on the control processor.
- Separation of control, image-processing, and VGA-rendering workloads.

### Vision and Communication

- Grove Vision AI V2 and camera module for image/OCR capture.
- ESP32-C3 interface for transferring processed vision data.
- SPI/UART communication between hardware modules.
- OCR command handling for text-based instructions and debug output.

### VGA and Game Rendering

- VGA output for menus, gameplay, captured images, and graphics overlays.
- RGB332 8-bit colour rendering.
- Hardware-rendered game screens for Snake, Draw Pixel, and Battleship.
- Accelerometer-controlled cursor and movement input.

### Hardware Interaction

- DE10-Lite switches and push-buttons for game control.
- LEDs and HEX displays for status, score, CPU usage, and feedback.
- Speaker/buzzer output for sound effects.
- Accelerometer-based motion steering and tilt detection.

---

## Games Implemented

### Classic Snake

A hardware-controlled Snake game displayed through VGA. The player controls movement using the DE10-Lite accelerometer. The game includes scoring, apple collection, LED feedback, sound effects, and game-over handling.

### Draw Pixel

A 96x96 drawing canvas where users can create pixel-style graphics. The camera can capture a grayscale image and render it as a background layer for interaction.

### Battleship

A grid-based aiming game that uses accelerometer tilt tracking to move the cursor and select strike positions. The game includes multiple attack patterns and VGA-rendered feedback.

---

## Hardware Requirements

- DE10-Lite FPGA board with USB cable
- Breadboard and jumper wires
- Grove Vision AI V2 module
- 5MP OV5647 camera module with ribbon cable
- ESP32-C3 module
- VGA monitor and VGA connector
- Crow-tail speaker or buzzer
- LED module
- 3D-printed camera/hardware mount

---

## Software Requirements

- Intel Quartus Prime Lite
- Nios II Software Build Tools for Eclipse
- DE10-Lite board support files
- ESP32/Grove Vision AI development environment, if modifying the vision module code
- Python, if using the companion UI or OCR command-generation tools

---

## Repository Structure

```text
ECE3073-Project2025-Lab02-Group4/
│-- M1/              # Milestone 1 files: VGA controller and basic hardware interface
│-- M2/              # Milestone 2 files: image processing and SPI/gyro communication
│-- project/         # Main integrated project files
│-- assets/          # Images used in this README
│-- .gitignore       # Git ignore rules for Quartus/project-generated files
│-- README.md        # Project documentation
```

---

## Getting Started

1. Clone the repository:

   ```sh
   git clone https://github.com/ece3073-monash/project-templates.git
   ```

2. Open the project in Quartus Prime Lite.
3. Compile the FPGA hardware design.
4. Program the DE10-Lite board.
5. Open the Nios II software workspace in Eclipse.
6. Build and run the software application on the correct Nios II core.
7. Connect the VGA monitor, Grove Vision AI V2 module, camera, speaker, and required hardware peripherals.

---

## Device Operation

### Main Controls

| Control | Function |
|---|---|
| `SW1` | Enables the 7-segment HEX displays |
| `SW2` | Enables rendering of captured OCR/text data |
| `SW3` | Displays CPU utilisation on the HEX display |
| `SW4` | Enables or disables speaker output |
| `KEY0` | Menu confirmation, retry, or image display action |
| `KEY1` | Camera capture or game-specific action |
| Accelerometer | Controls movement, cursor direction, or tilt-based interaction |

### Output Devices

| Output | Function |
|---|---|
| VGA monitor | Displays menus, games, captured images, and overlays |
| LEDs | Shows game events, score feedback, and status indicators |
| HEX displays | Shows text, score, CPU usage, or system state |
| Speaker | Plays audio feedback and sound effects |

---

## Milestones

### Milestone 1: VGA Controller and Basic Hardware Interface

- Implemented initial Nios II hardware setup.
- Added switch, HEX display, VGA, speaker, LED, SPI, and accelerometer testing.
- Integrated basic FPGA peripheral control.

### Milestone 2: Image Processing and SPI/Gyro Integration

- Added Grove Vision AI V2 and camera pipeline.
- Implemented image transfer/storage into SDRAM.
- Integrated push-button interrupts, accelerometer-to-VGA movement, and latency measurement.
- Tested communication between control and image-processing components.

### Milestone 3: Full System Integration

- Implemented triple-core system design.
- Added Snake, Draw Pixel, and Battleship gameplay.
- Integrated RTOS scheduling on the control core.
- Added sound effects, visual feedback, menu navigation, and real-time VGA rendering.

---

## Final Outcome

The completed system demonstrates a full FPGA-based embedded vision and gaming platform. It combines camera input, AI-assisted OCR/image capture, real-time graphics, accelerometer motion control, audio feedback, and multiple hardware peripherals into one integrated DE10-Lite project.

---

## Contributors

| Name | Student ID | Email |
|---|---:|---|
| Chin Wei Chun | 33520569 | wchi0051@student.monash.edu |
| Sean Loh Kim Fook | 34640509 | sloh0020@student.monash.edu |
| Ooi Li Xiang | 33070040 | looi0005@student.monash.edu |

---

## License

Copyright © 2026 Monash University.
