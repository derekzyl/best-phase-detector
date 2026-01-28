# Best Phase Detector System

A comprehensive 3-phase power monitoring and automatic phase selection system using ESP32, with WiFi control via Flutter mobile app.

## Features

- **Real-time Voltage Monitoring**: Monitors 3 phases using ZMPT101 voltage sensors
- **Automatic Phase Selection**: Automatically selects the best phase based on voltage trends and stability
- **Manual Control**: Manual phase selection via buttons or mobile app
- **LCD Display**: 16x2 LCD shows real-time voltage readings and system status
- **Button Control**: 
  - Short press: Navigate menu
  - Long press: Enter menu / Select option
- **WiFi Control**: Control system remotely via Flutter mobile app
- **Trend Analysis**: Analyzes voltage history to determine the most stable phase

## Hardware Components

- ESP32 Development Board
- ZMPT101 Voltage Sensor Modules (x3)
- 16x2 LCD Display (I2C)
- Relays (x3) - for phase switching
- Push Buttons (x2) - with pull-up resistors
- Resistors for pull-up configuration

## Pin Connections

### ESP32 Pinout

| Component | Pin | Notes |
|-----------|-----|-------|
| Button 1 | GPIO 13 | Pull-up resistor |
| Button 2 | GPIO 14 | Pull-up resistor |
| Relay 1 (Phase 1) | GPIO 18 | Active LOW (adjust if needed) |
| Relay 2 (Phase 2) | GPIO 4 | Active LOW (adjust if needed) |
| Relay 3 (Phase 3) | GPIO 23 | Active LOW (adjust if needed) |
| ZMPT101 Phase 1 | GPIO 32 | ADC input |
| ZMPT101 Phase 2 | GPIO 35 | ADC input |
| ZMPT101 Phase 3 | GPIO 34 | ADC input |
| LCD SDA | GPIO 21 | I2C (default) |
| LCD SCL | GPIO 22 | I2C (default) |

**Note**: 
- LCD I2C address is typically 0x27 or 0x3F. Update in code if different.
- Relay logic: Code assumes active LOW relays. If your relays are active HIGH, change `digitalWrite(RELAY_PIN, LOW)` to `digitalWrite(RELAY_PIN, HIGH)` in the code.

## Software Setup

### ESP32 Firmware (PlatformIO)

1. Install PlatformIO IDE or PlatformIO CLI
2. Open the `best_phase_detector` folder in PlatformIO
3. Update WiFi credentials in `src/main.cpp`:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
4. Adjust LCD I2C address if needed (default: 0x27)
5. Calibrate voltage sensors by adjusting `SENSITIVITY` constant if readings are inaccurate
6. Upload to ESP32:
   ```bash
   pio run -t upload
   ```
7. Monitor serial output to get the ESP32 IP address:
   ```bash
   pio device monitor
   ```

### Flutter Mobile App

1. Navigate to `best_phase_detector_app` directory
2. Install dependencies:
   ```bash
   flutter pub get
   ```
3. Run the app:
   ```bash
   flutter run
   ```
4. On first launch, enter the ESP32 IP address (shown in serial monitor)
5. The app will connect and display real-time phase data

## Usage

### Button Controls

- **Button 1 (Short Press)**: Navigate menu / Decrement selection
- **Button 1 (Long Press)**: Enter/Exit menu
- **Button 2 (Short Press)**: Navigate menu / Increment selection
- **Button 2 (Long Press)**: Select/Confirm menu item

### Menu Navigation

1. **Main Screen**: Shows all phase voltages and current mode
2. **Select Phase Menu**: Navigate with short presses, select with long press on Button 2
3. **Settings Menu**: Toggle between Automatic and Manual mode

### Mobile App Controls

1. **Connect**: Tap WiFi icon to enter device IP address
2. **Refresh**: Pull down to refresh or tap refresh button
3. **Mode Selection**: Toggle between Automatic and Manual modes
4. **Manual Phase Selection**: In Manual mode, tap "Select Phase" button on any phase card
5. **Real-time Monitoring**: View current, average, min, and max voltages for each phase

## API Endpoints

The ESP32 exposes a REST API on port 80:

- `GET /api/status` - Get current system status and phase data
- `POST /api/setPhase` - Set active phase (body: `{"phase": 0-2}`)
- `POST /api/setMode` - Set operation mode (body: `{"mode": "auto"|"manual"}`)
- `GET /` - Web interface for browser control

## Calibration

### Voltage Sensor Calibration

The ZMPT101 sensors may need calibration. Adjust these constants in `main.cpp`:

```cpp
const float SENSITIVITY = 0.066;  // Adjust based on your module
```

To calibrate:
1. Connect a known voltage source (e.g., 220V AC)
2. Measure the reading
3. Adjust `SENSITIVITY` until the reading matches the actual voltage

### Relay Configuration

If your relays are active HIGH instead of active LOW:

1. In `main.cpp`, change all relay control lines:
   - From: `digitalWrite(RELAY_PIN, LOW);`
   - To: `digitalWrite(RELAY_PIN, HIGH);`
   - And vice versa for turning off

## Automatic Phase Selection Algorithm

The system selects the best phase based on:
- **Voltage Level** (40% weight): Higher voltage is preferred
- **Stability** (30% weight): Lower voltage variation is preferred
- **Target Voltage** (30% weight): Closer to 220V is preferred

The system maintains a history of voltage readings and analyzes trends over time.

## Troubleshooting

### WiFi Connection Failed
- Check SSID and password in code
- Ensure ESP32 is within WiFi range
- Check router settings (some routers block new devices)

### LCD Not Displaying
- Check I2C address (try 0x27 or 0x3F)
- Verify I2C connections (SDA/SCL)
- Check LCD power supply

### Voltage Readings Incorrect
- Calibrate `SENSITIVITY` constant
- Verify sensor connections
- Check sensor power supply (5V)

### Relays Not Switching
- Verify relay connections
- Check if relays are active HIGH or LOW
- Test relays independently
- Verify relay power supply

### App Cannot Connect
- Verify ESP32 IP address
- Ensure phone and ESP32 are on same WiFi network
- Check firewall settings
- Verify ESP32 web server is running (check serial monitor)

## Safety Notes

⚠️ **WARNING**: This system works with high voltage AC power. 
- Always disconnect power before making connections
- Use proper isolation and safety measures
- Double-check all connections before powering on
- Ensure proper grounding
- Use appropriate relay ratings for your load
- Follow local electrical codes and regulations

## License

This project is provided as-is for educational and personal use.

## Contributing

Feel free to submit issues and enhancement requests!

