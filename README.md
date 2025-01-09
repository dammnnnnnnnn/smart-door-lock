# SMART-DOOR-LOCK
The Smart Access Lock is an RFID-based security system that offers remote control, real-time monitoring, and user management via a mobile app. It’s perfect for securing homes and offices with ease and reliability.

## Features

- **RFID Card Access**: Register and manage up to 10 RFID cards with unique usernames.
- **Blynk Integration**: Control access modes (register, remove, normal) and receive notifications for unauthorized access attempts.
- **Google Sheets Logging**: Log entry and exit events with timestamps to Google Sheets for easy tracking.
- **Admin Mode**: Temporarily override the magnetic switch for manual control.
- **Visual Feedback**: LED indicators for successful registration, removal, and access attempts.

## Hardware Requirements

- ESP32 Development Board
- RFID Module (MFRC522)
- Servo Motor
- LEDs (3x)
- Magnetic Door Switch
- WiFi Network

## Software Requirements

- Arduino IDE
- Blynk Library
- ESP32Servo Library
- MFRC522 Library
- EEPROM Library
- WiFi Library
- HTTPClient Library

## Setup Instructions

1. **Hardware Connections**:
   - Connect the RFID module to the ESP32 using SPI pins.
   - Connect the servo motor to the designated GPIO pin.
   - Connect LEDs and the magnetic switch to their respective GPIO pins.

2. **Software Configuration**:
   - Clone this repository to your local machine.
   - Open the `AUTO_DOOR_LOCK_SUCCESFULL.ino` file in Arduino IDE.
   - Update the WiFi credentials and Blynk authentication token in the code.
   - Deploy the Google Apps Script and update the `googleScriptURL` in the code.

3. **Upload and Run**:
   - Connect your ESP32 to the computer.
   - Upload the code to the ESP32 using Arduino IDE.
   - Monitor the Serial output for debugging and status updates.

## Usage

- **Register Mode**: Use the Blynk app to enter register mode and scan new cards to register them with a username.
- **Remove Mode**: Enter remove mode to delete registered cards.
- **Normal Mode**: Operate the system in normal mode to allow access to registered cards.
- **Admin Mode**: Use the Blynk app to manually control the lock.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request for any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## Acknowledgments

- Special thanks to the developers of the libraries used in this project.
- Thanks to the Blynk community for their support and resources.

## Contact

For any questions or feedback, please contact dammm at dammnn06@gmail.com
