# 📘 S32K144 Embedded Systems: Smart Wiper Evolution
> **From 42 Gyeongsan (Software Fundamentals) to Automotive System Architecture.**

This repository documents the development of a Smart Wiper System using the NXP S32K144EVB. It captures the complete engineering journey—evolving from basic control logic to hardware acceleration (DMA), digital signal processing (DSP), and eventually to a **standard automotive network (CAN)** and **AUTOSAR architecture**.

---

## 🏎️ Project Roadmap: The Evolution of Architecture

This is not just about adding features; it is an architectural leap to ensure **Real-time processing** and absolute **Reliability**, crucial for automotive electronic control units (ECUs).

### [Phase 1] Foundation (v1 ~ v4)
- **Core**: Finite State Machine (FSM) based wiper mode control (STOP, INT, LOW, HIGH).
- **Hard Real-time**: Strict 10ms periodic control guaranteed by LPIT timer interrupts.
- **Efficiency**: "Zero-overhead sensing" achieved by mitigating CPU load through eDMA for ADC data acquisition.

### [Phase 2] Reliability & DSP (v5)
- **Signal Integrity**: Implementation of a **Moving Average Filter (DSP)** to remove sensor noise and ensure control stability.
- **Failsafe Design**: Built-in diagnostic logic for hardware faults (open/short circuits) and sudden abnormal signal variations (Delta Check).
- **Layered Refactoring**: Increased code reusability by strictly separating top-level application logic from the Hardware Abstraction Layer (HAL).

### [Phase 3] Distributed System & Optimization (v6 ~ v7)
- **Architectural Shift (v6)**: Introduction of the **Foreground-Background Scheduling**. High-overhead control logic was migrated from the ISR to the Main Loop to maximize system responsiveness.
  - *Why?* To minimize interrupt latency in preparation for asynchronous CAN communication and large-scale data processing in the future.
- **Network Implementation (v7)**: Construction of a Master-Slave distributed control environment via a physical CAN bus.
  - **Fail-safe Over Network**: Slave nodes are programmed to revert to a Safe-state (Wiper Home Position) upon detection of communication loss or CAN bus off events.

### [Phase 4] Standard & Future (Current) 🚀
- **Transition to AUTOSAR**: Platform standardization using NXP RTD and **EB tresos** ecosystem.
- **High-Performance Control**: Implementation of a robust PID Controller for precise closed-loop DC Motor + Encoder actuation.
- **Physical AI Integration**: Building an intelligent decision-making system by integrating Embedded Linux (Raspberry Pi Vision) with the S32K144 VCU.

---

## 🔌 Technical Deep Dive

### 1. Foreground-Background Scheduling (v6 Innovation)
We detached the heavy computational logic from the ISRs, moving it to the `while(1)` Main Loop. Now, the 10ms ISR strictly serves to generate execution **Flags**. This architectural design guarantees that the system will never stall and can gracefully handle critical high-priority asynchronous events (e.g., CAN Rx/Tx) without jitter.

> 📊 **Slack Time Comparison Graph (Pre- vs Post-Architecture Shift)**
> 
> *<img src="assets/images/loop_count_comparison.png" alt="v6 Loop Count Slack Time Comparison" width="600"/>*
> 
> *(This graph illustrates the dramatic increase in idle slack time achieved by relieving the CPU from heavily packed ISRs, leaving ample bandwidth for future AUTOSAR BSW tasks.)*

### 2. DMA & DSP Pipeline
Upon completion of ADC conversion, the eDMA instantly transfers the data into memory. The CPU then reads and processes only the filtered values during its schedule. By utilizing bitwise shift operations (`adcSum >> 3U`) over standard division, mathematical overhead was deeply optimized for the Cortex-M4F environment.

### 3. Failsafe Strategy
For mission-critical automotive software, 'Safety' is the highest priority. If the filtered sensor variations exceed physical limits (`MAX_DELTA`) or electrical anomalies are flagged, the system inherently triggers a Safe-state strategy, locking into `MODE_OFF` and returning the wiper to its home position immediately.

---

## 🛠 Tech Stack & Tools

- **MCU**: NXP S32K144 (ARM Cortex-M4F)
- **IDE/Tools**: S32 Design Studio v3.5 (RTD-based), **EB tresos Studio**, FreeMASTER
- **Protocol**: CAN 2.0B, LPUART, SPI
- **Architecture**: Foreground-Background, AUTOSAR Classic (In Progress)
- **Language**: Embedded C (MISRA-C compliance oriented)

---

## 📝 Engineering Philosophy
- **Manual First**: Direct register manipulation (MMIO) powered by the **S32K1xx Reference Manual** and **Datasheets**, rather than relying solely on generic tutorials or black-box SDKs.
- **Data Driven**: Tuning and verification are performed empirically by visualizing live memory variables via real-time **FreeMASTER DAQ graphs**.
- **System Thinking**: Prioritizing the overall system data flow, task priority, and real-time execution constraints over the isolated operation of individual peripherals.

---

## 📎 References
- [S32K1xx Series Reference Manual](https://www.nxp.com/webapp/Download?colCode=S32K1XXRM)
- [AN5413: S32K144 Series Cookbook](https://www.nxp.com/webapp/Download?colCode=AN5413)
