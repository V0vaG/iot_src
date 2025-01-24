/*
  ardu <id> set id <id>, c <channel> r <r_pipe> w <w_pipe>
  ardu <id> set <gpio> on
  ardu <id> set <gpio> off
  ardu <id> set <gpio> <value>

  ardu <id> get <gpio>
  ardu <id> get A<gpio>
  
  ardu <id> pu <gpio>
  ardu <id> upu <gpio>
*/

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>

// CE and CSN pins for nRF24L01
#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

// EEPROM addresses for pipe names, channel, and ID
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_LISTENING_PIPE_ADDR 1
#define EEPROM_WRITING_PIPE_ADDR 7
#define EEPROM_CHANNEL_ADDR 13
#define EEPROM_ID_ADDR 14

// Magic number to check EEPROM validity
#define EEPROM_MAGIC_NUMBER 0x42

// Buffers to hold the pipe names, channel, and ID
char listeningPipe[6] = {0};
char writingPipe[6] = {0};
uint8_t channel;
uint8_t deviceID;

void setDefaults() {
  // Default pipe names, channel, and ID
  const char defaultListeningPipe[] = "2Node";
  const char defaultWritingPipe[] = "1Node";
  uint8_t defaultChannel = 77;
  uint8_t defaultID = 0;

  // Save defaults to EEPROM
  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_NUMBER);  // Set magic number
  EEPROM.put(EEPROM_LISTENING_PIPE_ADDR, defaultListeningPipe);
  EEPROM.put(EEPROM_WRITING_PIPE_ADDR, defaultWritingPipe);
  EEPROM.put(EEPROM_CHANNEL_ADDR, defaultChannel);
  EEPROM.update(EEPROM_ID_ADDR, defaultID);

  Serial.println("Defaults set in EEPROM.");
  radio.stopListening();
  radio.write("Defaults set in EEPROM.", strlen("Defaults set in EEPROM."));
  radio.startListening();
}

void resetEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);  // Reset all EEPROM addresses to 0xFF
  }
  Serial.println("EEPROM reset to factory state.");
  radio.stopListening();
  radio.write("EEPROM reset to factory state.", strlen("EEPROM reset to factory state."));
  radio.startListening();
}

bool isEEPROMEmpty() {
  // Check if the magic number is present
  return EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_NUMBER;
}

void loadSettings() {
  if (isEEPROMEmpty()) {
    Serial.println("EEPROM is empty, setting defaults.");
    radio.stopListening();
    radio.write("EEPROM is empty, setting defaults.", strlen("EEPROM is empty, setting defaults."));
    radio.startListening();
    setDefaults();
  }

  // Load pipe names, channel, and ID from EEPROM
  EEPROM.get(EEPROM_LISTENING_PIPE_ADDR, listeningPipe);
  EEPROM.get(EEPROM_WRITING_PIPE_ADDR, writingPipe);
  EEPROM.get(EEPROM_CHANNEL_ADDR, channel);
  deviceID = EEPROM.read(EEPROM_ID_ADDR);

  // Ensure null termination for strings
  listeningPipe[5] = '\0';
  writingPipe[5] = '\0';
}

void saveSettings(uint8_t newID, uint8_t newChannel, const char* newListeningPipe, const char* newWritingPipe) {
  // Update settings
  deviceID = newID;
  channel = newChannel;
  strncpy(listeningPipe, newListeningPipe, 5);
  strncpy(writingPipe, newWritingPipe, 5);
  listeningPipe[5] = '\0';
  writingPipe[5] = '\0';

  // Save settings to EEPROM
  EEPROM.update(EEPROM_ID_ADDR, deviceID);
  EEPROM.update(EEPROM_CHANNEL_ADDR, channel);
  EEPROM.put(EEPROM_LISTENING_PIPE_ADDR, listeningPipe);
  EEPROM.put(EEPROM_WRITING_PIPE_ADDR, writingPipe);

  // Apply new settings to the radio
  radio.setChannel(channel);
  uint64_t listeningPipeAddr = *(uint64_t*)listeningPipe;
  uint64_t writingPipeAddr = *(uint64_t*)writingPipe;
  radio.openReadingPipe(1, listeningPipeAddr);
  radio.openWritingPipe(writingPipeAddr);

  Serial.println("New settings saved to EEPROM and applied to the radio:");
  Serial.print("Device ID: "); Serial.println(deviceID);
  Serial.print("Channel: "); Serial.println(channel);
  Serial.print("Listening Pipe: "); Serial.println(listeningPipe);
  Serial.print("Writing Pipe: "); Serial.println(writingPipe);

  radio.stopListening();
  radio.write("Settings updated in EEPROM and applied.", strlen("Settings updated in EEPROM and applied."));
  radio.startListening();
}


void setup() {
  Serial.begin(9600);

  resetEEPROM();

  // Load settings from EEPROM
  loadSettings();

  Serial.print("Listening Pipe: ");
  Serial.println(listeningPipe);
  Serial.print("Writing Pipe: ");
  Serial.println(writingPipe);
  Serial.print("Channel: ");
  Serial.println(channel);
  Serial.print("Device ID: ");
  Serial.println(deviceID);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(channel);

  // Convert pipe names to uint64_t
  uint64_t listeningPipeAddr = *(uint64_t*)listeningPipe;
  uint64_t writingPipeAddr = *(uint64_t*)writingPipe;

  radio.openWritingPipe(writingPipeAddr);  // Send to Raspberry Pi
  radio.openReadingPipe(1, listeningPipeAddr);  // Listen to Raspberry Pi

  radio.setAutoAck(true);             // Enable acknowledgment
  radio.enableDynamicPayloads();      // Allow varying payload size
  radio.startListening();             // Always start in listening mode

  Serial.println("Arduino is now always listening.");
  radio.stopListening();
  radio.write("Hi, my name is Ardu", strlen("Hi, my name is Ardu"));
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    char receivedMessage[32] = {0};  // Buffer to store the received message

    // Get the size of the incoming message
    uint8_t length = radio.getDynamicPayloadSize();

    // Read the incoming message
    radio.read(&receivedMessage, length);
    receivedMessage[length] = '\0';  // Null-terminate the string

    Serial.print("Received message: ");
    Serial.println(receivedMessage);
    radio.stopListening();
    radio.write(receivedMessage, strlen(receivedMessage));
    radio.startListening();

    // Parse message for commands
    char commandID[5] = {0};
    uint8_t gpioPin;
    char gpioState[4] = {0};
    int pwmValue = -1;

    // Check for "ardu <id> set id <id>, channel <channel>, r_pipe <r_pipe>, w_pipe <w_pipe>"
    uint8_t newID, newChannel;
    char newListeningPipe[6] = {0};
    char newWritingPipe[6] = {0};
    if (sscanf(receivedMessage, "ardu %4s set id %hhu c %hhu r %5s w %5s", commandID, &newID, &newChannel, newListeningPipe, newWritingPipe) == 5) {
      uint8_t receivedID = atoi(commandID);
      if (receivedID == deviceID) {
        saveSettings(newID, newChannel, newListeningPipe, newWritingPipe);
      } else {
        Serial.println("Mismatched device ID.");
        radio.stopListening();
        radio.write("Mismatched device ID.", strlen("Mismatched device ID."));
        radio.startListening();
      }
    } else if (sscanf(receivedMessage, "ardu %4s set %hhu %3s", commandID, &gpioPin, gpioState) == 3) {
      // Check for "ardu <id> set <gpio> on/off"
      uint8_t receivedID = atoi(commandID);

      if (receivedID == deviceID) {
        pinMode(gpioPin, OUTPUT);

        if (strcmp(gpioState, "on") == 0) {
          digitalWrite(gpioPin, HIGH);
          Serial.print("GPIO ");
          Serial.print(gpioPin);
          Serial.println(" set to HIGH.");
          radio.stopListening();
          radio.write("GPIO set to HIGH.", strlen("GPIO set to HIGH."));
          radio.startListening();
        } else if (strcmp(gpioState, "off") == 0) {
          digitalWrite(gpioPin, LOW);
          Serial.print("GPIO ");
          Serial.print(gpioPin);
          Serial.println(" set to LOW.");
          radio.stopListening();
          radio.write("GPIO set to LOW.", strlen("GPIO set to LOW."));
          radio.startListening();
        } else if (sscanf(gpioState, "%d", &pwmValue) == 1 && pwmValue >= 0 && pwmValue <= 255) {
          // Handle PWM command
          if (gpioPin == 3 || gpioPin == 5 || gpioPin == 6 || gpioPin == 9 || gpioPin == 10 || gpioPin == 11) {
            analogWrite(gpioPin, pwmValue);
            Serial.print("GPIO ");
            Serial.print(gpioPin);
            Serial.print(" set to PWM value: ");
            Serial.println(pwmValue);
            radio.stopListening();
            char pwmResponse[50];
            snprintf(pwmResponse, sizeof(pwmResponse), "GPIO %d set to PWM value: %d", gpioPin, pwmValue);
            radio.write(pwmResponse, strlen(pwmResponse));
            radio.startListening();
          } else {
            Serial.println("Invalid PWM pin.");
            radio.stopListening();
            radio.write("Invalid PWM pin.", strlen("Invalid PWM pin."));
            radio.startListening();
          }
        } else {
          Serial.println("Invalid GPIO state or PWM value.");
          radio.stopListening();
          radio.write("Invalid GPIO state or PWM value.", strlen("Invalid GPIO state or PWM value."));
          radio.startListening();
        }
      } else {
        Serial.println("Mismatched device ID.");
        radio.stopListening();
        radio.write("Mismatched device ID.", strlen("Mismatched device ID."));
        radio.startListening();
      }
    } else if (sscanf(receivedMessage, "ardu %4s get %hhu", commandID, &gpioPin) == 2) {
      uint8_t receivedID = atoi(commandID);

      if (receivedID == deviceID) {
        if (gpioPin >= 2 && gpioPin <= 6) { // Digital pin range
          pinMode(gpioPin, INPUT);
          int value = digitalRead(gpioPin);
          Serial.print("Digital pin ");
          Serial.print(gpioPin);
          Serial.print(" read value: ");
          Serial.println(value == HIGH ? "HIGH" : "LOW");
          radio.stopListening();
          char response[50];
          snprintf(response, sizeof(response), "Digital pin %d read value: %s", gpioPin, value == HIGH ? "HIGH" : "LOW");
          radio.write(response, strlen(response));
          radio.startListening();
        } else {
          Serial.println("Invalid digital pin for reading.");
          radio.stopListening();
          radio.write("Invalid digital pin for reading.", strlen("Invalid digital pin for reading."));
          radio.startListening();
        }
      }
    } else if (sscanf(receivedMessage, "ardu %4s get A%hhu", commandID, &gpioPin) == 2) {
      uint8_t receivedID = atoi(commandID);

      if (receivedID == deviceID) {
        if (gpioPin >= 1 && gpioPin <= 7) { // Analog pin range A1-A7
          int value = analogRead(gpioPin + 14); // Adjust for A1 = 15, A2 = 16, etc.
          Serial.print("Analog pin A");
          Serial.print(gpioPin);
          Serial.print(" read value: ");
          Serial.println(value);
          radio.stopListening();
          char response[50];
          snprintf(response, sizeof(response), "Analog pin A%d read value: %d", gpioPin, value);
          radio.write(response, strlen(response));
          radio.startListening();
        } else {
          Serial.println("Invalid analog pin for reading.");
          radio.stopListening();
          radio.write("Invalid analog pin for reading.", strlen("Invalid analog pin for reading."));
          radio.startListening();
        }
      }
    } else if (sscanf(receivedMessage, "ardu %4s pu %hhu", commandID, &gpioPin) == 2) {
      uint8_t receivedID = atoi(commandID);

      if (receivedID == deviceID) {
        if (gpioPin >= 2 && gpioPin <= 6) { // Digital pin range
          pinMode(gpioPin, INPUT_PULLUP);
          Serial.print("Digital pin ");
          Serial.print(gpioPin);
          Serial.println(" set to INPUT_PULLUP.");
          radio.stopListening();
          char response[50];
          snprintf(response, sizeof(response), "Digital pin %d set to INPUT_PULLUP.", gpioPin);
          radio.write(response, strlen(response));
          radio.startListening();
        } else {
          Serial.println("Invalid digital pin for INPUT_PULLUP.");
          radio.stopListening();
          radio.write("Invalid digital pin for INPUT_PULLUP.", strlen("Invalid digital pin for INPUT_PULLUP."));
          radio.startListening();
        }
      }
    } else {
      Serial.println("Invalid message format.");
      radio.stopListening();
      radio.write("Invalid message format.", strlen("Invalid message format."));
      radio.startListening();
    }
  }
}
