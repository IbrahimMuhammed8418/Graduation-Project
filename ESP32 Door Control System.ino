#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define ROW_NUM 4
#define COLUMN_NUM 4

// Keypad setup
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
// Recommended GPIO pins for ESP32 (avoid flash/clk pins)
byte pin_rows[ROW_NUM]={27, 14, 12, 13};  // C1-C4
byte pin_columns[COLUMN_NUM]={32, 33, 25, 26};  // R1-R4 
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_columns, ROW_NUM, COLUMN_NUM);

// Servo objects for the doors
Servo mainDoorServo;
Servo secondaryDoorServo;

// Button pins
const int button2Pin = 23;



 
// Other variables
int button2State = 0;
int lastButton2State = HIGH;
String password = "2800";
String password1 = "2900";

String enteredPassword = "";
bool isKeypadActive = true;
unsigned long doorOpenTime = 0;
const unsigned long doorCloseDelay = 3000;

// Door states (true = open, false = closed)
bool mainDoorState = false;
bool secondaryDoorState = false;

// Wi-Fi credentials
const char* ssid = "O6Unetwork";
const char* wifiPassword = "NT630100";

// Server URL
String serverUrl = "http://192.168.137.66/status";

// Timers for periodic tasks
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 5000;

void connectToWiFi() {
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(ssid, wifiPassword);
    delay(5000);
    retries++;
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to Wi-Fi.");
      return;
    } else {
      Serial.println("Retrying...");
    }
  }
}
const int ledPin = 22;  // Choose an available GPIO pin

void setup() {
  Serial.begin(9600);
  Serial.println("System Initialized...");

  connectToWiFi();
  pinMode(ledPin, OUTPUT);  // Set LED as OUTPUT on GPIO 14
  digitalWrite(ledPin, LOW); // Turn LED OFF initially

  mainDoorServo.attach(5);
  secondaryDoorServo.attach(18);

  pinMode(button2Pin, INPUT_PULLUP);
 
  mainDoorServo.write(0);
  secondaryDoorServo.write(0);
}

void loop() {
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchRemoteCommands();
    lastFetchTime = millis();
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "OPEN") {
      Serial.println("Received OPEN command from Arduino");
      openMainDoor();
    }
  }

  if (isKeypadActive) {
    handleKeypad();
  }

  handleButtons();
  closeDoors();

  delay(10);
}
void handleKeypad() {
  char key = keypad.getKey();
  if (key) {
    // Flash LED when a key is pressed
    digitalWrite(ledPin, HIGH);  
    delay(100);  // Flash duration (100ms)
    digitalWrite(ledPin, LOW);

    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key == '#') {
      Serial.print("Checking entered password: ");
      Serial.println(enteredPassword);

      if (enteredPassword == password) {
        Serial.println("Correct password for secondary door! Opening secondary door.");
        openSecondaryDoor();
      } else if (enteredPassword == password1) {
        Serial.println("Correct password for main door! Opening main door.");
        openMainDoor();
      } else {
        Serial.println("Incorrect password.");
        sendStatusUpdate("Incorrect password");
      }

      enteredPassword = "";
      isKeypadActive = true;  
    } else if (key == '*') {
      enteredPassword = "";
      Serial.println("Password reset.");
      sendStatusUpdate("Password reset");
    } else {
      enteredPassword += key;
    }
  }
}

void handleButtons() {
   
  button2State = digitalRead(button2Pin);
  if (button2State == LOW && lastButton2State == HIGH) {
    Serial.println("Button 2 pressed! Toggling secondary door.");
    if (secondaryDoorState) {
      closeSecondaryDoor();
    } else {
      openSecondaryDoor();
    }
    delay(200);
  }
  lastButton2State = button2State;
}

void openMainDoor() {
  if (!mainDoorState) {
    mainDoorServo.write(80);
    mainDoorState = true;
    doorOpenTime = millis();
    sendStatusUpdate("Main door opened");
  }
}

void closeMainDoor() {
  if (mainDoorState) {
    mainDoorServo.write(0);
    mainDoorState = false;
    sendStatusUpdate("Main door closed");
  }
}

void openSecondaryDoor() {
  if (!secondaryDoorState) {
    secondaryDoorServo.write(80);
    secondaryDoorState = true;
    doorOpenTime = millis();
    sendStatusUpdate("Secondary door opened");
  }
}

void closeSecondaryDoor() {
  if (secondaryDoorState) {
    secondaryDoorServo.write(0);
    secondaryDoorState = false;
    sendStatusUpdate("Secondary door closed");
  }
}

void sendStatusUpdate(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://192.168.137.66/status";
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String payload = "message=" + message;
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
      Serial.printf("HTTP POST request sent. Response code: %d\n", httpCode);
    } else {
      Serial.printf("Failed to send HTTP request. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    delay(100); // Prevent ESP32 from overloading with HTTP requests
  } else {
    Serial.println("WiFi not connected, unable to send status update.");
  }
}

void closeDoors() {
  if (mainDoorState && (millis() - doorOpenTime >= doorCloseDelay)) {
    closeMainDoor();
  }
  if (secondaryDoorState && (millis() - doorOpenTime >= doorCloseDelay)) {
    closeSecondaryDoor();
  }
}

void fetchRemoteCommands() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.137.66/commands");
    int httpCode = http.GET(); 
    if (httpCode == 200) {
      String command = http.getString();
      Serial.print(command);
      if (command == "open_main123456") {
        openMainDoor();
      } else if (command == "toggle_secondary654321") {
        if (secondaryDoorState) {
          closeSecondaryDoor();
        } else {
          openSecondaryDoor();
        }
      }
    }
    http.end();
  }
}









