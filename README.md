# AIoMT-HypoGuard 🩺📡

**An ESP32-based real-time vital signs monitoring system that combines classical medical thresholds with a lightweight, knowledge-informed AI model to estimate hypoglycemia risk.**

> Academic project — AI in Medicine coursework. Not validated for clinical use. See [Disclaimer](#-disclaimer--limitations).

---

## 📖 Overview

This project implements an embedded health-monitoring system built around the **ESP32** microcontroller and two biomedical I2C sensors. It continuously measures:

- **Body temperature** (MAX30205)
- **Heart rate (HR)** (MAX30102)
- **Blood oxygen saturation (SpO₂)** (MAX30102)

Beyond simple rule-based thresholds, the system runs a lightweight **Logistic Regression** model directly on the microcontroller to estimate the probability of **hypoglycemia** (low blood sugar) from the combined physiological signal. All readings and the AI's risk score are streamed live to **ThingSpeak** for visualization and remote monitoring.

This is a hands-on example of applying **Embedded AI** to an **IoT medical monitoring** use case — sensor fusion, edge inference, and cloud telemetry running on a single low-power microcontroller.

---

## ✨ Key Features

- 🔌 **Auto-detecting sensors** — I2C bus scan at boot; the system runs safely even if a sensor is disconnected (no crash, graceful degradation)
- ❤️ **Dual vital-sign acquisition** — heart rate & SpO₂ via the industry-standard Maxim algorithm, body temperature via a medical-grade digital sensor
- 🧠 **On-device AI inference** — a knowledge-informed Logistic Regression model estimates hypoglycemia risk probability (0–1), running entirely on the ESP32
- 🚨 **Two-layer alert system** — a fast rule-based safety net (physiological threshold checks) combined with a probabilistic AI layer
- ☁️ **Cloud telemetry** — live data streaming to ThingSpeak across 5 channels, viewable as time-series charts from anywhere

---

## 🏗️ System Architecture

```
   ┌─────────────┐      I2C       ┌──────────────────┐
   │  MAX30205   │◄──────────────►│                  │
   │ (Temp Sensor)│                │                  │
   └─────────────┘                │      ESP32       │
                                   │                  │──── WiFi ────► ThingSpeak
   ┌─────────────┐      I2C       │  • Sensor drivers│      Cloud
   │  MAX30102   │◄──────────────►│  • Rule engine   │
   │ (HR / SpO2) │                │  • Logistic Reg. │
   └─────────────┘                └──────────────────┘
```

**Decision flow per cycle:**
1. Read temperature, heart rate, and SpO₂
2. Check each value against physiological safety thresholds (rule-based alert)
3. Feed normalized readings into the on-device Logistic Regression model (AI alert)
4. Combine both signals into a final alert flag
5. Push all 5 fields to ThingSpeak (rate-limited to the platform's update interval)

---

## 🔧 Hardware

| Component | Role | Interface |
|---|---|---|
| **ESP32** | Main controller — WiFi + I2C + on-device inference | — |
| **MAX30102** | Optical PPG sensor for Heart Rate & SpO₂ | I2C (`0x57`) |
| **MAX30205** | Digital body temperature sensor | I2C (`0x48`) |

### Wiring

<details>
<summary><b>MAX30102 → ESP32</b></summary>

| ESP32 Pin | MAX30102 Pin |
|---|---|
| 3.3V *(never 5V!)* | VIN |
| GND | GND |
| GPIO 22 (SCL) | SCL |
| GPIO 21 (SDA) | SDA |
| *(not connected — optional)* | INT |

</details>

<details>
<summary><b>MAX30205 → ESP32</b></summary>

| ESP32 Pin | MAX30205 Pin |
|---|---|
| 3.3V | VCC |
| GND | GND |
| GPIO 21 | SDA |
| GPIO 22 | SCL |
| GND | A0 |
| GND | A1 |
| GND | A2 |

</details>

Full wiring diagrams are available in [`docs/wiring/`](docs/wiring).

---

## 🧠 The AI Model: Knowledge-Informed Logistic Regression

Rather than a simple `if/else` threshold check, this project uses a **Logistic Regression** classifier to estimate hypoglycemia risk as a continuous probability rather than a hard binary decision.

**Why Logistic Regression for an embedded system?**
- Output is a probability (0–1), not just a binary flag
- Extremely lightweight — well suited to a resource-constrained microcontroller
- Interpretable / explainable (each weight has a clear physiological meaning)
- Widely used pattern in medical alerting systems
- Naturally combines multiple features with weighted contributions

**Important distinction:** the model's weights were **not trained on a dataset**. They were set manually based on physiological/medical domain knowledge — this is what makes it "knowledge-informed" rather than "data-driven." Mathematically it is identical to a standard Logistic Regression; only the *method of determining the weights* differs.

```cpp
// Normalized inputs → weighted sum → sigmoid → probability
z = W_TEMP * temp_n + W_HR * hr_n + W_SPO2 * spo2_n + BIAS
risk_probability = sigmoid(z)
```

| Weight | Sign | Physiological interpretation |
|---|---|---|
| `W_TEMP` | Negative | Dropping body temperature (hypothermia) can signal hypoglycemia |
| `W_HR` | Positive | Elevated heart rate is often linked to metabolic stress |
| `W_SPO2` | Negative | Falling SpO₂ can accompany a critical physiological state |
| `BIAS` | Negative | Keeps baseline risk low under normal conditions |

If the resulting probability exceeds a configurable threshold (default `0.7`), an AI-triggered alert fires — independent of, and in addition to, the rule-based threshold checks.

---

## ☁️ ThingSpeak Fields

| Field | Description |
|---|---|
| 1 | Body Temperature (°C) |
| 2 | Heart Rate (BPM) |
| 3 | SpO₂ (%) |
| 4 | Alert Status — `0` normal / `1` alert (rule-based OR AI-triggered) |
| 5 | AI Hypoglycemia Risk Probability (0–1) |

---

## 🚀 Getting Started

### Requirements
- ESP32 development board
- MAX30102 and MAX30205 sensor modules
- Arduino IDE (or PlatformIO) with the ESP32 board package
- Libraries: `WiFi`, `Wire`, `ThingSpeak`, `MAX30105`, `heartRate`, `spo2_algorithm`, `ClosedCube_MAX30205`

### Setup
1. Clone this repository
2. Copy `firmware/config.example.h` to `firmware/config.h` and fill in your own WiFi credentials and ThingSpeak channel/API key
3. Open `firmware/vital_signs_monitor.ino` in the Arduino IDE
4. Wire the sensors as described in [Hardware](#-hardware)
5. Flash the ESP32 and open the Serial Monitor at `115200` baud
6. Watch your data appear on your ThingSpeak channel dashboard

> ⚠️ **Never commit `config.h` with real credentials.** It's excluded via `.gitignore` — always use `config.example.h` as your template.

---

## ⚠️ Disclaimer & Limitations

This project was built as part of an **AI in Medicine** university course and is a **proof-of-concept, not a certified medical device**.

- The AI model's weights are hand-tuned from domain knowledge, **not trained or validated on real patient data**
- Thresholds and risk estimates have **not** undergone clinical validation or regulatory review
- **Do not use this system to make real medical decisions.** It is not a substitute for professional medical equipment or advice

### Potential future improvements
- Train the Logistic Regression weights (or a small neural net) on a real, labeled physiological dataset
- Add sensor calibration and motion-artifact filtering for the PPG signal
- Move credentials to secure storage (e.g., ESP32 NVS / Preferences) instead of source code
- Add unit tests and a simulation mode for offline development
- Expand to a mobile/web dashboard beyond ThingSpeak's default charts

---

## 📁 Repository Structure

```
AIoMT-HypoGuard/
├── .gitignore
├── LICENSE
├── README.md
├── firmware/
│   └── vital_signs_monitor/
│       ├── vital_signs_monitor.ino
│       ├── config.example.h
│       └── libraries.txt
├── docs/
│   ├── architecture.md
│   ├── ai_model.md
│   └── wiring/
│       ├── max30102_connections.png
│       └── max30205_connections.png
└── media/
```

---

## 📄 License

This project is released under the [MIT License](LICENSE).

---

## 🙋 Author

Built as part of an AI in Medicine graduate program coursework project.
Feel free to open an issue or reach out with questions or suggestions!
