# 📘 S32K144 Embedded Systems Study: Smart Wiper Project

This repository documents my journey from Linux-based C programming (42 Gyeongsan) to professional Automotive Embedded Software Engineering. It covers everything from basic MCU register control to building a high-level **Smart Wiper System** using the NXP S32K144EVB.

---

## 📂 Project Structure

```text
.
├── assets/                     # Datasheets, Schematics, and Reference Manuals (S32K-RM)
├── board_notes/                # Step-by-step study logs and hardware theory
│   ├── 00_gpio_output_led/
│   ├── 01_freemaster_s32k144/
│   ├── 02_RGB_color_mixer/
│   └── 03_smart_wiper/         # Core Project: Current Development
├── docs/                       # Environmental setup and prerequisite knowledge
└── s32k_projects/              # S32 Design Studio (S32DS) Project Files
    ├── LED_Blink_Red/          # Basic GPIO output
    ├── freemaster_s32k144/     # FreeMASTER integration for debugging
    ├── RGB_color_mixer/        # PWM control via FTM (FlexTimer)
    └── smart_wiper/            # MAIN: ADC + PWM + Non-blocking logic

```

---

## 🏎️ Main Project: Smart Wiper System

The goal of this project is to simulate a real-world automotive wiper system that adjusts its speed based on rain intensity (simulated via ADC/Potentiometer).

### Key Features

* **Non-blocking Logic**: Implemented using a **State Machine** (STOP, INT, LOW, HIGH) instead of `delay()` functions, allowing the MCU to handle multiple tasks concurrently.
* **Hardware Integration**:
* **ADC**: Reads rain intensity from the potentiometer.
* **FTM (PWM)**: Controls the servo motor (wiper) speed and position.
* **LPUART**: Communicates system status to the PC.


* **Real-time Monitoring**: Integrated with **FreeMASTER** to visualize ADC values and state transitions in real-time.

---

## 📈 Engineering Roadmap (Industry-Focused)

Designed to meet the technical requirements of Tier-1 automotive suppliers like **Valeo**.

### Phase 1: Feature Implementation (Current)

* Functional Smart Wiper using NXP S32 SDK.
* Non-blocking state management.

### Phase 2: Hardware Deep-Dive (Bare-metal)

* Re-implementing SDK drivers by directly manipulating **Registers** (PCC, PORT, ADC, FTM).
* Optimization using **Interrupts** and **DMA (Direct Memory Access)** to reduce CPU load.

### Phase 3: Automotive Communication

* Implementing **LIN (Local Interconnect Network)** for wiper/door control simulation.
* Implementing **CAN (Controller Area Network)** for high-speed vehicle data exchange.

### Phase 4: Embedded OS (RTOS)

* Porting **FreeRTOS** to manage multiple automotive tasks (Control, Communication, Diagnosis).
* Understanding AUTOSAR-like layered architecture (MCAL-BSW-RTE-ASW).

---

## 🛠 Tech Stack

* **Hardware**: NXP S32K144EVB-Q100 (Cortex-M4F)
* **IDE**: S32 Design Studio (S32DS) for ARM v2.2
* **Debugger**: FreeMASTER, OpenSDA
* **Language**: Embedded C

---

## 📝 Study Philosophy (From 42 Gyeongsan to Embedded)

* **Understanding MMIO**: Connecting C pointers to physical memory addresses (Registers).
* **Manual Reference**: Prioritizing the **S32K1xx Reference Manual** over generic tutorials.
* **Data-Driven**: Every system state must be visible and measurable via FreeMASTER.

---

## 📎 References

* [S32K1xx Series Reference Manual](https://www.google.com/search?q=assets/S32K-RM.pdf)
* [S32K144EVB Schematic](https://www.google.com/search?q=assets/NXP_S32K144EVB_SCH.pdf)
* [AN5413: S32K1xx Series Cookbook](https://www.google.com/search?q=assets/AN5413.pdf)
