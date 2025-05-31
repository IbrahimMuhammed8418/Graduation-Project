#include <WiFi.h>  // Wi-Fi library for ESP32
#include <HTTPClient.h>  // HTTP client library for ESP32
#include <DHT.h>  // DHT sensor library

// Wi-Fi credentials
const char* ssid = "O6Unetwo"rk;         // Replace with your Wi-Fi SSID
const char* password = "NT630100";  // Replace with your Wi-Fi password

// DHT11 configuration
#define DHTPIN 4         // GPIO pin where the DHT11 is connected (for ESP32, GPIO4 is used)
#define DHTTYPE DHT11     // DHT11 sensor type
DHT dht(DHTPIN, DHTTYPE); // Create DHT object

// Server URL
String serverUrl = "http://192.168.137.66/clien2"; // Replace with your server's URL

// Timing variables
unsigned long previousMillis = 0;
const unsigned long interval = 9000; // Interval to send data (9 seconds)

void setup() {
  // Initialize Serial communication
  Serial.begin(115200);
  Serial.println("DHT11 Sensor with Wi-Fi Data Sender (ESP32)");

  // Initialize the DHT sensor
  dht.begin();

  // Connect to Wi-Fi
  connectToWiFi();
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if the interval has passed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read temperature and humidity from the DHT11 sensor
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();  // In Celsius

    // Check if the readings are valid
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      // Print temperature and humidity
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      // Send data to the server
      sendDataToServer(temperature, humidity);
    }
  }
}

// Function to connect to Wi-Fi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Function to send data to the server
void sendDataToServer(float temperature, float humidity) {
  if (WiFi.status() == WL_CONNECTED) { // Ensure Wi-Fi is connected
    HTTPClient http;

    // Prepare the POST request
    http.begin(serverUrl); // URL of the server
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Prepare the data payload
    String payload = "temperature=" + String(temperature, 2); // 2 decimal places
    payload += "&humidity=" + String(humidity, 2);           // 2 decimal places

    // Send the POST request
    int httpCode = http.POST(payload);

    // Check the result
    if (httpCode > 0) {
      Serial.print("HTTP POST request sent, response code: ");
      Serial.println(httpCode);
      String response = http.getString();
      Serial.println("Server Response: " + response);
    } else {
      Serial.print("Error in HTTP request, code: ");
      Serial.println(httpCode);
    }

    // End the HTTP request
    http.end();
  } else {
    Serial.println("Wi-Fi not connected. Unable to send data.");
  }
}
