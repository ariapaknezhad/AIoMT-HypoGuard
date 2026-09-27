# The AI Model — Knowledge-Informed Logistic Regression

## 1. Goal

Estimate the probability that a person is experiencing **hypoglycemia** (low blood sugar) by combining three physiological signals measured simultaneously:

- Body temperature
- Heart rate
- SpO₂ (blood oxygen saturation)

Instead of a hard `if/else` rule, the system outputs a **continuous probability between 0 and 1**, giving a graded sense of risk rather than a binary yes/no.

## 2. Why Logistic Regression?

Logistic Regression was chosen specifically because it fits the constraints of an embedded medical-monitoring use case:

| Property | Why it matters here |
|---|---|
| Output is a probability, not a hard decision | Supports graded risk rather than "arbitrary" binary logic |
| Extremely cheap to compute | Runs comfortably on an ESP32 with no ML accelerator |
| Explainable | Every weight has a direct physiological interpretation (see below) — this matters for a medical-adjacent system where "why did it alert?" needs an answer |
| Common pattern in medical alerting | Widely used as an interpretable baseline for risk scoring in clinical and pre-clinical tooling |
| Naturally combines multiple features | Each vital sign contributes a weighted term to a single combined score |

## 3. "Knowledge-Informed" vs. "Trained" Logistic Regression

This is the most important design decision to understand about this project, and the reason for the explicit disclaimer in the main README.

| | **Trained LR (classical)** | **Knowledge-informed LR (this project)** |
|---|---|---|
| Mathematical form | Logistic Regression ✅ | Logistic Regression ✅ |
| Sigmoid activation | ✅ | ✅ |
| Weights | Learned from a labeled dataset | Set manually, based on physiological/medical domain knowledge |
| Needs a dataset | Yes | No |
| Fit for embedded / no-training-data scenario | Not directly | ✅ |
| Validated accuracy on real patients | Depends on dataset quality | **Not validated** — this is a heuristic |
| Explainability | Medium | Very high — every coefficient was hand-picked for an intuitive medical reason |

**Both are mathematically the same model.** The only difference is *how the weights were obtained*. This project deliberately traded "learned accuracy" for "zero-dataset, fully explainable, resource-light" — which is a reasonable choice for a class project and an embedded proof-of-concept, but it means the risk scores should be treated as **illustrative, not diagnostic**.

## 4. The Math

### 4.1 Normalization
Each raw input is normalized against a typical physiological reference range before being combined, so that no single feature dominates purely due to its numeric scale:

```cpp
temp_n = (temp - 37.0) / 5.0   // centered on normal body temp
hr_n   = (hr   - 75.0) / 40.0  // centered on a typical resting HR
spo2_n = (spo2 - 95.0) / 10.0  // centered on normal SpO2
```

### 4.2 Weighted Sum
```cpp
z = W_TEMP * temp_n + W_HR * hr_n + W_SPO2 * spo2_n + BIAS
```

### 4.3 Sigmoid
```cpp
probability = 1 / (1 + e^(-z))   // squashes z into [0, 1]
```

### 4.4 Threshold Decision
```cpp
if (probability > 0.7) → AI alert triggered
```

## 5. Weight Values & Medical Interpretation

```cpp
const float W_TEMP = -1.2;
const float W_HR   =  1.5;
const float W_SPO2 = -0.6;
const float BIAS   = -5.0;
```

| Weight | Sign | Interpretation |
|---|---|---|
| `W_TEMP` | Negative | A **drop** in body temperature (hypothermia) can be an early sign of hypoglycemia — so lower temperature pushes the score *up* |
| `W_HR` | Positive | **Elevated** heart rate is often associated with the metabolic/adrenergic stress response the body produces during a hypoglycemic episode |
| `W_SPO2` | Negative | A **drop** in blood oxygen saturation can accompany a broader critical physiological state |
| `BIAS` | Negative | Keeps the baseline (resting, normal-vitals) probability low, so the model doesn't alert under everyday conditions |

## 6. Why Not Just `if/else`?

| | `if/else` (hard rules) | Logistic Regression (this project) |
|---|---|---|
| Decision type | Hard / binary | Continuous probability |
| Combines multiple signals | Awkward (nested conditionals) | Natural (weighted sum) |
| Sensitivity to sensor noise | High — a single borderline reading can flip the decision | Lower — one noisy input is smoothed by the combined weighted sum |
| Mathematical analyzability | None | Well understood (standard ML formulation) |
| Tunability | Requires rewriting logic | Just adjust weights / threshold |

The rule-based thresholds are still present in this project (see `architecture.md`) — they serve as an independent **safety net** for implausible readings, while the Logistic Regression layer provides the more nuanced, combined risk estimate.

## 7. Known Limitations

- Weights are **not derived from data** — they are a reasonable-but-unverified approximation based on general physiological knowledge, not a clinically validated model
- No consideration of patient-specific baselines (e.g., a naturally low resting heart rate would be treated the same as anyone else's)
- No temporal modeling — each cycle's prediction is independent; trends over time are not considered
- The `0.7` alert threshold is a static, hand-picked value, not calibrated against outcome data

## 8. Natural Next Step

If this project were to move beyond a classroom proof-of-concept, the highest-value improvement would be collecting or sourcing a real, labeled physiological dataset and re-fitting the same Logistic Regression structure with **trained** weights — the inference code would barely need to change, since the mathematical form is already identical to a classically trained model.
