# 🌱 AgriGuard — Smart Irrigation & Water Management System

AgriGuard is a smart irrigation system designed to help small-scale farmers **use water more efficiently while maintaining suitable soil conditions for crop growth**.

The system combines soil-moisture sensing, environmental monitoring, automated irrigation, weather information, and historical irrigation data to make better decisions about **when, where, and how much water should be supplied**.

Rather than relying on a simple fixed irrigation schedule, AgriGuard is being developed toward an **adaptive, zone-based irrigation system** that can learn the behaviour of individual areas of a farm and use that information to improve future irrigation decisions.

---

## 🎯 Project Objective

The main objective of AgriGuard is:

> **To reduce unnecessary water consumption while providing crops with the water they need.**

Traditional automated irrigation systems can operate using fixed timers or simple moisture thresholds. While this works, it does not account well for differences between zones, soil types, environmental conditions, or available water.

AgriGuard addresses this by building an irrigation profile for each zone.

The system considers factors such as:

* Soil moisture
* Soil type
* Temperature
* Humidity
* Rainfall/weather conditions
* Soil drying rate
* Irrigation duration
* Water consumption
* Available tank water
* Historical irrigation behaviour

These factors are used to make more informed irrigation decisions.

---

# 🧠 How the Solution Works

At a high level, AgriGuard follows this process:

```text
             ┌─────────────────────┐
             │      Sensors        │
             │                     │
             │ Soil Moisture       │
             │ Temperature         │
             │ Humidity            │
             │ Water Level         │
             │ Flow Rate           │
             └──────────┬──────────┘
                        │
                        ▼
             ┌─────────────────────┐
             │       ESP32         │
             │   Control System    │
             └──────────┬──────────┘
                        │
          ┌─────────────┼─────────────┐
          ▼             ▼             ▼
   Soil Analysis   Environment    Water Status
          │             │             │
          └─────────────┼─────────────┘
                        ▼
             ┌─────────────────────┐
             │ Irrigation Decision │
             │      Algorithm       │
             └──────────┬──────────┘
                        │
                        ▼
             ┌─────────────────────┐
             │   Zone Control      │
             │                     │
             │ Valve / Pump        │
             └──────────┬──────────┘
                        │
                        ▼
                 Irrigation Cycle
                        │
                        ▼
             ┌─────────────────────┐
             │ Historical Data     │
             │ & Zone Profile      │
             └──────────┬──────────┘
                        │
                        ▼
             Improve Future Decisions
```

---

# ⚙️ System Architecture

AgriGuard is divided into several major sections.

### 1. Soil Monitoring

Capacitive soil-moisture sensors are used to monitor the moisture condition of different irrigation zones.

The system does not treat the raw sensor reading as a direct percentage. Instead, calibration values are used to convert sensor readings into a more meaningful moisture representation.

The calibration process considers conditions such as:

* Dry soil
* Wet soil
* Soil type
* Sensor behaviour

Multiple sensors can be deployed across different zones to provide a more representative view of the farm.

---

### 2. Environmental Monitoring

Environmental conditions can affect how quickly soil loses moisture.

AgriGuard therefore incorporates environmental data such as:

* Temperature
* Relative humidity
* Precipitation
* Weather forecasts

Weather data is treated as an **additional decision-making factor rather than a single point of failure**.

If weather data is unavailable, the system can continue operating using the locally measured sensor data.

---

### 3. Zone-Based Irrigation

The farm is divided into irrigation zones.

Each zone can have its own:

* Soil type
* Moisture readings
* Drying behaviour
* Irrigation history
* Valve status
* Irrigation duration
* Water consumption
* Calibration values

This allows AgriGuard to recognise that different areas of a farm may require different irrigation strategies.

For example:

```text
Zone 1 → Sandy soil → dries quickly
Zone 2 → Loamy soil → moderate drying
Zone 3 → Clay soil  → dries slowly
```

Instead of treating the entire farm identically, the controller can make decisions based on the behaviour of each zone.

---

# 💧 Adaptive Irrigation

One of the main developments of AgriGuard is moving beyond a simple:

```text
IF soil is dry
    irrigate
```

approach.

The system is being developed to consider both **current conditions and historical behaviour**.

A simplified decision can be represented as:

```text
Current Soil Condition
          +
Environmental Conditions
          +
Zone Drying Behaviour
          +
Expected Weather
          +
Available Water
          ↓
   Irrigation Decision
```

This creates the foundation for adaptive irrigation.

---

# 📈 Zone Irrigation Profile

AgriGuard records information about how each irrigation zone behaves over time.

The zone profile can include:

* Moisture readings
* Moisture change over time
* Drying rate
* Temperature during drying
* Irrigation duration
* Irrigation frequency
* Water used
* Previous irrigation cycles

The purpose is to determine patterns such as:

> "How quickly does this zone dry after irrigation?"

and:

> "How much water does this zone normally require?"

This information can then be used to improve future irrigation decisions.

---

# 💦 Water Budget

Another major component of AgriGuard is the **water budget**.

The system considers:

```text
Available Water
       ↓
Water Required by Zones
       ↓
Zone Priority
       ↓
Water Allocation
       ↓
Irrigation
```

This is particularly important when the available water supply is limited.

Instead of simply irrigating every zone whenever its moisture falls below a threshold, AgriGuard can determine how available water should be allocated.

The long-term goal is to answer:

> **"Given the amount of water currently available, where will using that water provide the greatest benefit?"**

---

# 🧠 Learning Irrigation Cycles

AgriGuard is being developed toward a system that can learn from previous irrigation cycles.

After an irrigation event, the system can monitor:

```text
Irrigation starts
       ↓
Water is supplied
       ↓
Irrigation stops
       ↓
Soil moisture is monitored
       ↓
Drying rate is calculated
       ↓
Data is stored
       ↓
Future irrigation decisions improve
```

This allows the system to gradually build an understanding of each zone rather than relying entirely on manually configured values.

---

# 🚨 Safety System

AgriGuard also includes an emergency-stop mechanism.

The emergency stop is designed to provide a physical safety mechanism capable of shutting down the irrigation system when activated.

The emergency function is treated separately from the normal irrigation decision logic so that safety takes priority.

Conceptually:

```text
              Emergency Stop
                    │
                    ▼
          ┌──────────────────┐
          │ System Shutdown  │
          └────────┬─────────┘
                   │
          Normal operation
             is disabled
```

The design also considers the interaction between the physical latching switch, ESP32 control logic, and irrigation power-control circuitry.

---

# 🔌 Hardware

The system has been designed around an **ESP32 development board** as the main controller.

Major hardware components include:

* ESP32 development board
* Capacitive soil-moisture sensors
* Temperature/humidity sensor
* Water-level sensor
* Water-flow sensor
* Solenoid/flow valves
* Pump/control circuitry
* LCD display
* Emergency-stop switch
* Power supply
* Protective sensor enclosures

The exact hardware configuration may evolve as the prototype is developed and tested.

---

# 💻 Software

The embedded software is being developed primarily using **C++ for the Arduino/ESP32 environment**.

The software is structured into separate functional sections so that each subsystem can be developed and tested independently before being integrated into the main program.

Major software components include:

```text
Sensor acquisition
      ↓
Data processing
      ↓
Calibration
      ↓
Zone management
      ↓
Drying-rate analysis
      ↓
Weather processing
      ↓
Irrigation decision
      ↓
Valve control
      ↓
Water-budget management
      ↓
Historical data
```

Data structures such as `enum` and `struct` are used to represent things such as:

* Soil types
* Irrigation zones
* Water tanks
* Sensor data
* Valve states
* Calibration parameters

This makes the system easier to expand as additional zones and sensors are introduced.

---

# 🌦️ Weather Integration

AgriGuard can retrieve weather information through an online weather API.

The weather subsystem has been designed to provide information such as:

* Temperature
* Relative humidity
* Precipitation
* Probability of precipitation

This information can be incorporated into irrigation decisions.

For example, if a zone is approaching its irrigation threshold but significant rainfall is expected, the system can consider reducing or delaying irrigation.

Weather information therefore acts as a **decision-enhancement layer**, rather than being required for the system to function.

---

# 🛠️ Development Approach

The system has been developed incrementally rather than attempting to build the complete system at once.

The development process has broadly followed:

```text
1. Identify the irrigation problem
            ↓
2. Select sensors and hardware
            ↓
3. Develop individual sensor modules
            ↓
4. Develop ESP32 control logic
            ↓
5. Develop soil calibration
            ↓
6. Introduce multiple irrigation zones
            ↓
7. Add environmental data
            ↓
8. Develop irrigation thresholds
            ↓
9. Implement water monitoring
            ↓
10. Measure irrigation cycles
            ↓
11. Build zone irrigation profiles
            ↓
12. Develop water budgeting
            ↓
13. Introduce adaptive/learning behaviour
            ↓
14. Integrate safety systems
            ↓
15. Package the system into physical hardware
```

The project therefore combines **embedded systems, electronics, software engineering, data processing, control systems, and physical product design**.


# 🚀 Long-Term Vision

The long-term goal of AgriGuard is not simply to automate irrigation.

It is to create a system that can **understand how a farm uses water and continuously improve how that water is allocated.**

The intended progression is:

```text
Automated Irrigation
        ↓
Zone-Based Irrigation
        ↓
Data-Driven Irrigation
        ↓
Adaptive Irrigation
        ↓
Water-Budget Optimisation
        ↓
Self-Improving Irrigation System
```

AgriGuard is therefore being developed as a platform where **sensing, control, data and physical infrastructure work together** to make irrigation more efficient.

---

