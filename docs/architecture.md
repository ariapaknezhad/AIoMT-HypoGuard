# System Architecture — AIoMT-HypoGuard

This document describes the internal architecture of the vital-signs monitoring system: how data flows from the sensors to the cloud, and the design decisions behind each layer.

## 1. High-Level Data Flow

```
┌──────────────┐        ┌──────────────┐
│  MAX30205    │        │  MAX30102    │
│  (Temp)      │        │  (HR / SpO2) │
└──────┬───────┘        └──────┬───────┘
       │  I2C (0x48)           │  I2C (0x57)
       └───────────┬───────────┘
                    ▼
           ┌─────────────────┐
           │   ESP32 (I2C     │
           │   bus @ GPIO21/22)│
           └────────┬─────────┘
                     │
      ┌──────────────┼──────────────┐
      ▼              ▼              ▼
┌───────────┐  ┌────────────┐ ┌──────────────┐
│ Rule-based│  │  Logistic  │ │  Data        │
│ Threshold │  │  Regression│ │  Aggregation │
│ Checks    │  │  (AI layer)│ │              │
└─────┬─────┘  └─────┬──────┘ └──────┬───────┘
      │              │               │
      └──────┬───────┘               │
             ▼                       ▼
      ┌─────────────┐        ┌──────────────┐
      │ Final Alert │───────▶│  ThingSpeak  │
      │ Decision    │        │  (5 fields)  │
      └─────────────┘        └──────────────┘
```

## 2. Boot Sequence

On power-up, the ESP32 runs a full I2C bus scan (addresses `1`–`126`) and logs every device it finds. It then specifically checks for:

- **`0x57`** → MAX30102 (heart rate / SpO₂)
- **`0x48`** → MAX30205 (body temperature)

Each sensor sets a boolean presence flag (`found_MAX30102`, `found_MAX30205`) rather than assuming both are always attached. This is a deliberate **fault-tolerance** decision: if one sensor is disconnected, damaged, or not yet wired, the firmware still boots, connects to WiFi, and reports whatever data it *can* collect — instead of hanging or crashing.

After the sensor scan, the device connects to WiFi and initializes the ThingSpeak client.

## 3. Sensing Layer

### 3.1 Body Temperature (MAX30205)
A single blocking call (`tempSensor.readTemperature()`) returns the body temperature in °C per loop iteration.

### 3.2 Heart Rate & SpO₂ (MAX30102)
This sensor requires more work than a simple register read:

1. A buffer of **100 samples** is collected from both the red and infrared LED channels
2. The buffer is passed to Maxim's standard `maxim_heart_rate_and_oxygen_saturation()` algorithm, which extracts:
   - Heart rate (BPM) + a validity flag
   - SpO₂ (%) + a validity flag
3. If a reading is flagged invalid, it is reset to `-1` (sentinel value for "no valid data")

Using the vendor-standard Maxim algorithm (rather than a custom peak-detection routine) was a deliberate choice for **signal reliability** — PPG signals are noisy and this algorithm is a well-tested reference implementation for this exact sensor family.

## 4. Decision Layer — Two Independent Alert Paths

The system does **not** rely on a single decision mechanism. Two independent paths can each trigger an alert, and the final status is their logical OR:

```cpp
bool finalAlert = ruleAlert || aiAlert;
```

### 4.1 Rule-Based Layer (safety net)
Simple threshold checks against physiological bounds:

| Parameter | Alert condition |
|---|---|
| Heart Rate | `< 40 bpm` or `> 140 bpm` |
| SpO₂ | `< 50%` |
| Body Temp | `< 30°C` or `> 44°C` |

These bounds are intentionally wide — they exist mainly to catch **implausible readings** (e.g., a sensor not actually touching skin, or a hardware fault), not to serve as fine-grained clinical thresholds. This layer runs independently of the AI model, so it keeps working even if the AI logic were ever disabled or misbehaving.

### 4.2 AI Layer (Logistic Regression)
A lightweight, on-device Logistic Regression model estimates the probability of hypoglycemia from the combination of all three vitals. See [`ai_model.md`](ai_model.md) for the full mathematical explanation and design rationale.

## 5. Telemetry Layer (ThingSpeak)

Every cycle, five fields are pushed to a ThingSpeak channel:

| Field | Content |
|---|---|
| 1 | Body Temperature (°C) |
| 2 | Heart Rate (BPM) |
| 3 | SpO₂ (%) |
| 4 | Final alert flag (0/1) |
| 5 | AI risk probability (0–1) |

The loop waits **20 seconds** between cycles (`delay(20000)`), which respects ThingSpeak's free-tier rate limit for channel updates.

## 6. Why This Architecture?

- **Separation of concerns** — sensing, rule-based safety checks, AI inference, and telemetry are distinct stages, making each easy to reason about or replace independently
- **Graceful degradation** — presence flags mean a missing sensor doesn't take down the whole system
- **Defense in depth** — two independent alert mechanisms (rules + AI) reduce the chance that a single point of failure causes a missed alert
- **Edge-first** — all inference happens on the ESP32 itself; the cloud (ThingSpeak) is used only for visualization/logging, not for decision-making, which keeps the alert latency low and avoids a network dependency for safety-critical logic
