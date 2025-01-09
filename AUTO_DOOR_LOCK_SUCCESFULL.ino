#define BLYNK_TEMPLATE_ID "TMPL6V9pbEat4"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "JKLm1O-DeYMMpqe7Txrf2vWdEYIBKx0N"
#define BLYNK_PRINT Serial

#include <ESP32Servo.h>  // Use ESP32-specific Servo library
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <EEPROM.h>            // Include EEPROM library
#include <WiFi.h>              // ESP32 WiFi
#include <BlynkSimpleEsp32.h>  // Blynk library
#include <HTTPClient.h>

// Your WiFi credentials
char auth[] = BLYNK_AUTH_TOKEN;  // Blynk authentication token
char ssid[] = "test";            // WiFi SSID
char pass[] = "12345678";        // WiFi password

// Google Apps Script URL (replace this with the URL from your deployment)
String googleScriptURL = "https://script.google.com/macros/s/AKfycbzcW_FOnO2wWimVttU9E2Wh2vF3Rl17meOb-Dbdbi9Cv5z-R7QFnfI7gBgwrpLn4W4I/exec";

// Define pin for SPI slave select (SS)
MFRC522DriverPinSimple ss_pin(5);

// Create SPI driver instance
MFRC522DriverSPI driver{ ss_pin };
MFRC522 mfrc522{ driver };

// Servo motor pin
Servo lockServo;  // ESP32-specific servo instance
const int servoPin = 14;

// LED pins
const int ledPin1 = 16;            // LED1 for successful access and registration mode
const int ledPin2 = 17;            // LED2 for denied access and removal mode
const int ledPin3 = 32;            // LED3 for successful registration feedback
const int magneticSwitchPin = 22;  // Magnetic switch connected to GPIO 22

// Define maximum number of registered cards
#define MAX_CARDS 10
// #define EEPROM_SIZE 40                      // 10 cards * 4 bytes per card
byte registeredUIDs[MAX_CARDS][4] = { 0 };  // Array to hold registered UIDs
int registeredCount = 0;

#define MAX_USERNAME_LENGTH 16  // Maximum length for usernames
#define EEPROM_ENTRY_SIZE 20    // 4 bytes for UID + 16 bytes for username
#define EEPROM_SIZE (MAX_CARDS * EEPROM_ENTRY_SIZE)
#define DOOR_SWITCH_PIN 22  // Magnetic door switch connected to pin 22


// Flags to indicate modes
bool registerMode = false;
bool removeMode = false;
bool normalMode = true;  // Start in normal mode
bool adminMode = false;  // Flag for Admin Mode


// Virtual pins for Blynk button
#define VIRTUAL_REGISTER_PIN V1  // Register mode
#define VIRTUAL_REMOVE_PIN V2    // Remove mode
#define VIRTUAL_NORMAL_PIN V3    // Normal mode
#define VIRTUAL_USERNAME_PIN V5
#define VIRTUAL_TERMINAL_PIN V6


// Define a struct to store UIDs and corresponding user names
struct User {
  byte uid[4];                     // UID (4-byte array)
  char name[MAX_USERNAME_LENGTH];  // Corresponding user name (max 16 characters)
  bool inside = false;             // State: true = inside, false = outside
};

User registeredUsers[MAX_CARDS];


// Variable to store the input username from Blynk
String inputUsername = "";



// Function to capture username input from the Blynk app
BLYNK_WRITE(VIRTUAL_USERNAME_PIN) {
  inputUsername = param.asStr();  // Store the username from Blynk Text Input
  Serial.print("Username received: ");
  Serial.println(inputUsername);  // Debugging print
}



void setup() {
  Serial.begin(115200);

  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println(F("Failed to initialize EEPROM"));
    while (1)
      ;  // Stop if EEPROM initialization fails
  }

  // Initialize RFID module
  mfrc522.PCD_Init();
  Serial.println(F("Ready to scan PICCs. Please present a card..."));

  // Set up and initialize servo motor
  lockServo.attach(servoPin);
  lockServo.write(180);  // Start in locked position (adjust according to your setup)

  // Set up LEDs
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);  // Set pin 32 as output for LED3

  // Ensure LEDs are off initially
  digitalWrite(ledPin1, LOW);
  digitalWrite(ledPin2, LOW);
  digitalWrite(ledPin3, LOW);  // Ensure LED3 is off initially

  // Load registered cards from EEPROM
  loadRegisteredCardsFromEEPROM();

  printRegisteredCards();  //see registered card

  // Connect to WiFi and Blynk
  WiFi.begin(ssid, pass);
  Blynk.begin(auth, ssid, pass);

  Serial.println(F("System is ready."));
  // Start Serial Monitor for debugging
  Serial.begin(115200);
  Serial.println("Starting setup...");

  // Initialize the magnetic switch pin as input
  pinMode(magneticSwitchPin, INPUT_PULLUP);

  // Attach the servo to the pin and check if it's successful
  bool attached = lockServo.attach(servoPin);
  if (attached) {
    Serial.println("Servo attached successfully.");
  } else {
    Serial.println("Servo failed to attach. Check wiring or pin compatibility.");
  }

  // Start with the servo at 0 degrees (Unlocked)
  lockServo.write(180);
  Serial.println("Servo initialized to 0 degrees (Unlocked).");
}

void loop() {

  // Run Blynk connection
  Blynk.run();

    // Check if Admin Mode is active
  if (!adminMode) {
    // Read the state of the magnetic switch
    int switchState = digitalRead(magneticSwitchPin);

    // Debugging print to show the state of the switch
    Serial.print("Magnetic Switch State: ");
    Serial.println(switchState);

    if (switchState == LOW) {
      // If the magnetic switch is in contact (door closed), move the servo to 180 degrees
      lockServo.write(180);
      Serial.println("Door is closed, locking (servo to 180 degrees).");
    }
  } else {
    Serial.println("Admin Mode active, ignoring magnetic door switch.");
  }

  // Small delay to avoid spamming the Serial Monitor
  delay(2000);

  // Check if a new card is present and read it
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    sendCardInfoToBlynk(mfrc522.uid.uidByte, mfrc522.uid.size);

    if (registerMode) {
      // Register the card with Blynk-provided username
      if (!isCardRegistered(mfrc522.uid.uidByte, mfrc522.uid.size)) {
        if (registeredCount < MAX_CARDS) {
          if (inputUsername.length() > 0) {
            registerCardWithUsername(mfrc522.uid.uidByte, inputUsername.c_str());
            Serial.println(F("New card registered with username."));

            // Light up LED3 for successful registration
            digitalWrite(ledPin3, HIGH);
            delay(2000);
            digitalWrite(ledPin3, LOW);

            inputUsername = "";  // Reset the username input after registration
          } else {
            Serial.println(F("Username is empty. Please enter a username."));
          }
        } else {
          Serial.println(F("Maximum number of cards reached. Cannot register more."));
        }
      } else {
        Serial.println(F("Card is already registered."));
      }

      digitalWrite(ledPin1, LOW);  // Turn off LED1
    } else if (removeMode) {

      // Remove the card
      removeCard(mfrc522.uid.uidByte);

      // Light up LED3 for successful removal
      digitalWrite(ledPin3, HIGH);  // Turn on LED3
      delay(2000);                  // Keep LED3 on for 2 seconds
      digitalWrite(ledPin3, LOW);   // Turn off LED3

      removeMode = false;          // Exit removal mode after removing the card
      digitalWrite(ledPin2, LOW);  // Turn off LED2

    } else if (normalMode) {
      if (isCardRegistered(mfrc522.uid.uidByte, mfrc522.uid.size)) {
        const char *userName = getUserName(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.println(F("Access granted: Authorized card detected."));
        digitalWrite(ledPin1, HIGH);
        unlockDoor();
        digitalWrite(ledPin1, LOW);

      } else {
        Serial.println(F("Access denied: Unauthorized card."));
        digitalWrite(ledPin2, HIGH);
        delay(5000);
        digitalWrite(ledPin2, LOW);

        // Use logEvent to trigger notification in Blynk
        Blynk.logEvent("unauthorized_access", "Unauthorized access attempt detected!");

        // Log this attempt in the Blynk terminal or table
        String logMessage = "Unauthorized access attempt detected!";
        Blynk.virtualWrite(VIRTUAL_TERMINAL_PIN, logMessage + "\n");


        Serial.println(logMessage);
      }
    }

    // Halt PICC to stop reading more cards
    mfrc522.PICC_HaltA();
  }
}

void registerCardWithUsername(byte *uid, const char *username) {
  Serial.println(F("Registering card with username..."));

  // Check if the card is already registered
  if (isCardRegistered(uid, 4)) {
    Serial.println(F("This card is already registered."));
    return;  // Exit the function without registering the card again
  }

  // Check if there is space to register the card
  if (registeredCount < MAX_CARDS) {
    // Copy the UID and username to the registered list
    memcpy(registeredUsers[registeredCount].uid, uid, 4);
    strncpy(registeredUsers[registeredCount].name, username, MAX_USERNAME_LENGTH);
    registeredCount++;

    // Save the card and username to EEPROM
    saveRegisteredCardToEEPROM(uid, username);

    Serial.println(F("Card and username registered successfully."));
  } else {
    Serial.println(F("Maximum number of cards reached. Cannot register more cards."));
  }
}



void saveRegisteredCardToEEPROM(byte *uid, const char *username) {
  int eepromAddress = (registeredCount - 1) * EEPROM_ENTRY_SIZE;  // Calculate the start position in EEPROM

  // Store the UID (4 bytes)
  for (int i = 0; i < 4; i++) {
    EEPROM.write(eepromAddress + i, uid[i]);
  }

  // Store the username (16 bytes, padded with null characters if shorter)
  for (int i = 0; i < MAX_USERNAME_LENGTH; i++) {
    if (i < strlen(username)) {
      EEPROM.write(eepromAddress + 4 + i, username[i]);
    } else {
      EEPROM.write(eepromAddress + 4 + i, 0);  // Pad with null characters
    }
  }

  EEPROM.commit();  // Ensure data is saved
  Serial.println(F("UID and username saved to EEPROM."));
}



// Function to find the user name by matching the scanned UID
const char *getUserName(byte *uid, byte size) {
  for (int i = 0; i < registeredCount; i++) {
    if (memcmp(registeredUsers[i].uid, uid, size) == 0) {
      return registeredUsers[i].name;  // Return the name directly since it's a C-string (char array)
    }
  }
  return "Unknown User";  // Return default if no match found
}



// Function to send the user name associated with the card to Blynk
void sendCardInfoToBlynk(byte *uid, byte size) {
  const char *userName = getUserName(uid, size);  // Get the user name for the scanned card

  // Format UID as a string
  String uidString = "";
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) uidString += "0";  // Add leading zero for single-digit hex values
    uidString += String(uid[i], HEX);
  }

  // Send the user name and UID to Blynk
  Blynk.virtualWrite(V4, userName);   // Send user name to Blynk
  Blynk.virtualWrite(V7, uidString);  // Send UID to a new virtual pin V7

  // Print to serial monitor for debugging
  Serial.print(F("Card scanned by: "));
  Serial.println(userName);
  Serial.print(F("UID: "));
  Serial.println(uidString);
}


/// Function to print all registered cards and usernames
void printRegisteredCards() {
  Serial.println(F("Registered cards:"));

  if (registeredCount == 0) {
    Serial.println(F("No registered cards found."));
  }

  for (int i = 0; i < registeredCount; i++) {
    Serial.print(F("Card "));
    Serial.print(i + 1);
    Serial.print(F(": "));
    for (int j = 0; j < 4; j++) {
      Serial.print(registeredUsers[i].uid[j] < 0x10 ? " 0" : " ");
      Serial.print(registeredUsers[i].uid[j], HEX);
    }
    Serial.print(F(" - Username: "));
    Serial.println(registeredUsers[i].name);
  }
}

BLYNK_WRITE(V0) {
  int pinValue = param.asInt();  // Get the value from the Blynk button (0 or 1)

  if (pinValue == 1) {
    // Button ON: Unlock or perform some action (move servo)
    lockServo.write(0);  // Servo moves to 0 degrees (unlock position)
    adminMode = true;     // Enter Admin Mode
    Serial.println(F("Admin control: Servo unlocked. Admin Mode ON."));
  } else {
    // Button OFF: Lock or perform some other action (move servo back)
    lockServo.write(180);  // Servo moves to 180 degrees (lock position)
    adminMode = false;     // Exit Admin Mode
    Serial.println(F("Admin control: Servo locked. Admin Mode OFF."));
  }
}


// Function to handle Blynk button switch for modes (register/remove/normal)
BLYNK_WRITE(VIRTUAL_REGISTER_PIN) {
  int buttonValue = param.asInt();

  if (buttonValue == 1) {
    Serial.println(F("Blynk switch: Entering card registration mode."));
    registerMode = true;
    removeMode = false;
    normalMode = false;
    digitalWrite(ledPin1, HIGH);  // Turn on LED1 for registration mode
    digitalWrite(ledPin2, LOW);   // Turn off LED2
  }
}

BLYNK_WRITE(VIRTUAL_REMOVE_PIN) {
  int buttonValue = param.asInt();

  if (buttonValue == 1) {
    Serial.println(F("Blynk switch: Entering card removal mode."));
    registerMode = false;
    removeMode = true;
    normalMode = false;
    digitalWrite(ledPin1, LOW);   // Turn off LED1
    digitalWrite(ledPin2, HIGH);  // Turn on LED2 for removal mode
  }
}

BLYNK_WRITE(VIRTUAL_NORMAL_PIN) {
  int buttonValue = param.asInt();

  if (buttonValue == 1) {
    Serial.println(F("Blynk switch: Entering normal mode."));
    registerMode = false;
    removeMode = false;
    normalMode = true;
    digitalWrite(ledPin1, LOW);  // Turn off LED1
    digitalWrite(ledPin2, LOW);  // Turn off LED2
  }
}

// Function to unlock the door briefly and then lock again
void unlockDoor() {
  lockServo.write(0);  // Unlock position (adjust as per your lock mechanism)
  Serial.println(F("Door unlocked."));

  // Send the username and UID of the cardholder to Google Sheets
  const char *username = getUserName(mfrc522.uid.uidByte, mfrc522.uid.size);

  // Find the user and toggle their state
  for (int i = 0; i < registeredCount; i++) {
    if (memcmp(registeredUsers[i].uid, mfrc522.uid.uidByte, 4) == 0) {

      if (!registeredUsers[i].inside) {
        // User is outside, logging entry
        Serial.println(String(username) + " entered the door.");
        Blynk.virtualWrite(V4, String(username) + " entered the door.");
        sendDataToGoogleSheets(username, "entered", mfrc522.uid.uidByte, mfrc522.uid.size);  // Log to Google Sheets

        registeredUsers[i].inside = true;  // Update state to inside
      } else {
        // User is inside, logging exit
        Serial.println(String(username) + " left the door.");
        Blynk.virtualWrite(V4, String(username) + " left the door.");
        sendDataToGoogleSheets(username, "left", mfrc522.uid.uidByte, mfrc522.uid.size);  // Log to Google Sheets

        registeredUsers[i].inside = false;  // Update state to outside
      }

      break;
    }
  }
}


void sendDataToGoogleSheets(const char *username, const char *action, byte *uid, byte size) {
  if (WiFi.status() == WL_CONNECTED) {  // Check WiFi connection
    HTTPClient http;

    // Construct the UID as a string
    String uidString = "";
    for (byte i = 0; i < size; i++) {
      if (uid[i] < 0x10) uidString += "0";  // Add leading zero for single-digit hex values
      uidString += String(uid[i], HEX);
    }

    // Construct URL with parameters (username, UID, action, and timestamp)
    String url = googleScriptURL + "?username=" + String(username) + "&uid=" + uidString + "&action=" + String(action);

    // Send the request to Google Sheets
    http.begin(url);                    // Start the HTTP request
    int httpResponseCode = http.GET();  // Send the GET request

    // Check response code
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Data sent successfully to Google Sheets: " + response);
    } else {
      Serial.println("Error sending data to Google Sheets. HTTP response code: " + String(httpResponseCode));
    }

    http.end();  // End the HTTP request
  } else {
    Serial.println("WiFi not connected. Cannot send data to Google Sheets.");
  }
}


// Function to check if a card is already registered
bool isCardRegistered(byte *uid, byte size) {
  for (int i = 0; i < registeredCount; i++) {
    // Compare the scanned UID with the stored UIDs
    if (memcmp(registeredUsers[i].uid, uid, size) == 0) {
      return true;  // Card is registered
    }
  }
  return false;  // Card not found
}


// Function to register a new card
void registerCard(byte *uid) {
  Serial.println(F("Registering card..."));

  // Check if there is space to register the card
  if (registeredCount < MAX_CARDS) {
    // Copy the UID to the registered list
    memcpy(registeredUIDs[registeredCount], uid, 4);
    registeredCount++;

    // Save the card to EEPROM
    saveRegisteredCardToEEPROM(uid);

    Serial.println(F("Card registered successfully."));
  } else {
    Serial.println(F("Maximum number of cards reached. Cannot register more cards."));
  }
}

// Function to remove a card
void removeCard(byte *uid) {
  Serial.println(F("Removing card..."));

  bool cardFound = false;

  // Loop through registered cards to find the matching UID
  for (int i = 0; i < registeredCount; i++) {
    if (memcmp(registeredUsers[i].uid, uid, 4) == 0) {
      cardFound = true;

      Serial.print(F("Card found for removal: "));
      for (int j = 0; j < 4; j++) {
        Serial.print(uid[j], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // Shift the remaining UIDs and usernames down to remove the card
      for (int j = i; j < registeredCount - 1; j++) {
        memcpy(registeredUsers[j].uid, registeredUsers[j + 1].uid, 4);
        strncpy(registeredUsers[j].name, registeredUsers[j + 1].name, MAX_USERNAME_LENGTH);
      }

      registeredCount--;  // Decrease the count of registered cards

      // Remove the card from EEPROM
      removeCardFromEEPROM(i);

      Serial.println(F("Card removed successfully."));
      break;  // Exit loop once the card is found and removed
    }
  }

  if (!cardFound) {
    Serial.println(F("Card not found in the registered list."));
  }
}


// Function to save a registered card to EEPROM
void saveRegisteredCardToEEPROM(byte *uid) {
  int eepromAddress = (registeredCount - 1) * 4;  // Ensure writing at the correct position
  for (int i = 0; i < 4; i++) {
    EEPROM.write(eepromAddress + i, uid[i]);
  }
  EEPROM.commit();  // Ensure data is committed to EEPROM
  Serial.println(F("Card saved to EEPROM."));
}

// Function to remove a card from EEPROM
void removeCardFromEEPROM(int index) {
  Serial.println(F("Removing card from EEPROM..."));

  // Shift the remaining UIDs and usernames down in EEPROM
  for (int i = index; i < registeredCount; i++) {
    for (int j = 0; j < 4; j++) {
      EEPROM.write(i * EEPROM_ENTRY_SIZE + j, EEPROM.read((i + 1) * EEPROM_ENTRY_SIZE + j));
    }
    for (int j = 0; j < MAX_USERNAME_LENGTH; j++) {
      EEPROM.write(i * EEPROM_ENTRY_SIZE + 4 + j, EEPROM.read((i + 1) * EEPROM_ENTRY_SIZE + 4 + j));
    }
  }

  // Overwrite the last card position with 0xFF (indicating invalid data)
  for (int i = 0; i < EEPROM_ENTRY_SIZE; i++) {
    EEPROM.write(registeredCount * EEPROM_ENTRY_SIZE + i, 0xFF);
  }

  EEPROM.commit();  // Ensure changes are committed to EEPROM
  Serial.println(F("Card removed from EEPROM."));
}


// Make sure this function is defined only once:
void loadRegisteredCardsFromEEPROM() {
  registeredCount = 0;
  for (int i = 0; i < MAX_CARDS; i++) {
    byte uid[4];
    char username[MAX_USERNAME_LENGTH];
    bool validUID = false;

    int eepromAddress = i * EEPROM_ENTRY_SIZE;  // Calculate EEPROM address

    // Read UID (4 bytes)
    for (int j = 0; j < 4; j++) {
      uid[j] = EEPROM.read(eepromAddress + j);
      if (uid[j] != 0xFF) {  // If UID is not 0xFF (uninitialized)
        validUID = true;
      }
    }

    // Read username (16 bytes)
    for (int j = 0; j < MAX_USERNAME_LENGTH; j++) {
      username[j] = EEPROM.read(eepromAddress + 4 + j);
    }
    username[MAX_USERNAME_LENGTH - 1] = '\0';  // Ensure the username is null-terminated

    if (validUID) {
      // Copy the UID and username into the registered users array
      memcpy(registeredUsers[registeredCount].uid, uid, 4);
      strncpy(registeredUsers[registeredCount].name, username, MAX_USERNAME_LENGTH);
      registeredCount++;
    }
  }

  Serial.println(F("Registered cards and usernames loaded from EEPROM."));
}
