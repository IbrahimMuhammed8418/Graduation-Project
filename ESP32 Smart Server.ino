#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
  
// Replace with your network credentials
const char* ssid = "O6Unetwork";
const char* password = "NT630100";
// Passwords for each door
const char* passwordMainDoor = "123456";  // Password for the main door
const char* passwordSecondaryDoor = "654321";  // Password for the secondary door

String statusMessages[5];
int messageCount = 0;
String currentCommand = "";  // Stores the current command (e.g., "open_main")
String pendingCommand = "";  // Stores the next command to be sent to the client

// Set static IP configuration
IPAddress local_IP(192, 168, 137, 66);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);
#define FILE_PHOTO "/photo.jpg"
String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <!-- Meta and Title -->
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Data Display</title>
  <!-- CSS Styles -->
  <style>
    body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 20px; }
    h1 { text-align: center; color: #333; }
    .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
    .data { margin-top: 20px; }
    .status { font-weight: bold; color: green; }
    .message { margin-top: 10px; padding: 10px; background-color: #eef; border-radius: 5px; }
    .upload-btn { margin-top: 20px; padding: 10px 20px; background-color: #007BFF; color: white; border: none; cursor: pointer; border-radius: 5px; }
    .upload-btn:hover { background-color: #0056b3; }
    .commands { margin-top: 20px; }
    .commands button { padding: 10px 20px; margin-right: 10px; background-color: #28a745; color: white; border: none; cursor: pointer; border-radius: 5px; }
    .commands button:hover { background-color: #218838; }
    .data-log { margin-top: 20px; }
    .data-log ul { list-style-type: none; padding: 0; }
    .data-log li { background-color: #f9f9f9; padding: 10px; margin-bottom: 5px; border-radius: 4px; }
    .uploaded-photo { margin-top: 20px; }
    .status-messages ul { list-style-type: none; padding: 0; }
    .status-messages li { background-color: #dff0d8; padding: 10px; margin-bottom: 5px; border-radius: 5px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Data Display</h1>
    <div class="data">
      <h2>Sensor Data</h2>
      <p>Temperature: <span id="temperature" class="status">Loading...</span> °C</p>
      <p>Humidity: <span id="humidity" class="status">Loading...</span> %</p>
    </div>
    <div class="commands">
      <h2>Remote Commands</h2>
      <label for="passwordInput">Password:</label>
      <input type="password" id="passwordInput" placeholder="Enter password" />
      <br><br>
      <button onclick="sendCommand('open_main')">Open Main Door</button>
       <button onclick="sendCommand('toggle_secondary')">Toggle Secondary Door</button>
    </div>
    <div class="uploaded-photo">
      <h2>Latest Uploaded Photo:</h2>
      <img id="latestPhoto" src="/photo" alt="No photo uploaded yet" style="max-width: 100%; height: auto;" />
      <button class="upload-btn" onclick="triggerFileInput()">Upload Photo</button>
      <input type="file" id="fileInput" accept="image/*" style="display: none;" />
    </div>
    <div class="data-log">
      <h2>Data Log:</h2>
      <ul id="dataLog"></ul>
    </div>
    <div class="status-messages">
      <h2>Stored Status Messages</h2>
      <ul id="statusMessagesList"></ul>
    </div>
  </div>

  <script>
    // Fetch sensor data periodically
    function fetchSensorData() {
      fetch('/sensorData')
        .then(response => response.json())
        .then(data => {
          document.getElementById('temperature').textContent = data.temperature || 'N/A';
          document.getElementById('humidity').textContent = data.humidity || 'N/A';

          // Add to data log
          const log = document.getElementById("dataLog");
          const listItem = document.createElement("li");
          listItem.textContent = `Temp: ${data.temperature || 'N/A'} °C, Humidity: ${data.humidity || 'N/A'} % at ${new Date().toLocaleTimeString()}`;
          log.prepend(listItem);
        })
        .catch(error => console.error('Error fetching sensor data:', error));
    }

function sendCommand(cmd) {
  const password = document.getElementById('passwordInput').value;

  if (!password) {
    alert('Please enter a password');
    return;
  }

  fetch('/setCommand', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `command=${cmd}&password=${password}`
  })
  .then(response => response.text())
  .then(data => alert(data))
  .catch(error => console.error('Error sending command:', error));
}


    // Function to handle file upload
    function triggerFileInput() {
      document.getElementById('fileInput').click();
    }

    document.getElementById('fileInput').addEventListener('change', function (event) {
      const file = event.target.files[0];
      if (file) {
        const formData = new FormData();
        formData.append('file', file);

        fetch('/upload', { method: 'POST', body: formData })
        .then(response => response.text())
        .then(data => {
          alert(data);
          refreshPhoto();
        })
        .catch(error => console.error('Error uploading file:', error));
      }
    });

    // Function to refresh the latest photo
    function refreshPhoto() {
      const photo = document.getElementById('latestPhoto');
      photo.src = '/photo?rand=' + Math.random(); // Prevent caching
    }

    // Function to fetch stored status messages
    function fetchStatusMessages() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          const statusList = document.getElementById('statusMessagesList');
          statusList.innerHTML = ''; // Clear the list
          data.messages.forEach(message => {
            const listItem = document.createElement('li');
            listItem.textContent = message;
            statusList.appendChild(listItem);
          });
        })
        .catch(error => console.error('Error fetching status messages:', error));
    }

    // Periodically fetch status messages and sensor data
    setInterval(fetchSensorData, 5000); // Refresh sensor data every 5 seconds
    setInterval(fetchStatusMessages, 5000); // Refresh status messages every 5 seconds
    fetchSensorData(); // Initial fetch
    fetchStatusMessages(); // Initial fetch

    // Initial photo refresh
    refreshPhoto();
  </script>
</body>
</html>
)rawliteral";



String latestUID = "N/A";
float latestTemperature = 0.0;
float latestHumidity = 0.0;

void setup() {
  Serial.begin(115200);

  // Set static IP address
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.printf("Server IP Address: %s\n", WiFi.localIP().toString().c_str());

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  Serial.println("SPIFFS initialized successfully");

  // Serve HTML page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlPage);
  });

  // Handle photo upload
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (ESP.getFreeHeap() < 1000) {
        Serial.println("Insufficient memory for file upload.");
        request->send(500, "text/plain", "Insufficient memory.");
        return;
      }

      File file = SPIFFS.open(FILE_PHOTO, index == 0 ? FILE_WRITE : FILE_APPEND);
      if (!file) {
        Serial.println("Failed to open file for writing");
        request->send(500, "text/plain", "Failed to open file.");
        return;
      }

      file.write(data, len);
      file.close();

      if (index + len == total) {
        Serial.println("File uploaded successfully.");
        request->send(200, "text/plain", "Upload complete.");
      }
    });

  // Serve photo
  server.on("/photo", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpeg");
  });

  // Handle status messages (POST and GET)
  server.on("/status", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("message", true)) {
      String newMessage = request->getParam("message", true)->value();
      if (messageCount < 5) {
        statusMessages[messageCount++] = newMessage;
      } else {
        for (int i = 0; i < 4; i++) {
          statusMessages[i] = statusMessages[i + 1];
        }
        statusMessages[4] = newMessage;
      }
      request->send(200, "application/json", "{\"message\":\"Message received\"}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing message parameter\"}");
    }
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{\"messages\":[";
    for (int i = 0; i < messageCount; i++) {
      response += "\"" + statusMessages[i] + "\"";
      if (i < messageCount - 1) response += ",";
    }
    response += "]}";
    request->send(200, "application/json", response);
  });

server.on("/clien2", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("temperature", true) && request->hasParam("humidity", true)) {
        latestTemperature = request->getParam("temperature", true)->value().toFloat();
        latestHumidity = request->getParam("humidity", true)->value().toFloat();
        request->send(200, "application/json", "{\"status\":\"success\"}");
        Serial.printf("Received Temperature: %.2f °C, Humidity: %.2f %%\n", latestTemperature, latestHumidity);
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
        Serial.println("Invalid request: Missing temperature or humidity parameters");
    }
});
  server.on("/sensorData", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{\"temperature\":" + String(latestTemperature) + ", \"humidity\":" + String(latestHumidity) + "}";
    request->send(200, "application/json", response);
});

server.on("/commands", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (!pendingCommand.isEmpty()) {
    request->send(200, "text/plain", pendingCommand);  // Send the pending command
    Serial.println("Command sent to client: " + pendingCommand);
    pendingCommand = "";  // Clear the command after sending
  } else {
    request->send(200, "text/plain", "");  // No command available
    Serial.println("No command pending");
  }
});
  server.on("/setCommand", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("command", true) && request->hasParam("password", true)) {
      String command = request->getParam("command", true)->value();
      String providedPassword = request->getParam("password", true)->value();

      bool isValid = false;

      if ((command == "open_main" || command == "close_main") && providedPassword == passwordMainDoor) {
        isValid = true;
      } else if (command == "toggle_secondary" && providedPassword == passwordSecondaryDoor) {
        isValid = true;
      }

      if (isValid) {
        pendingCommand = command + providedPassword;
  String responseMsg = "Command: " + command + "9879";
        request->send(200, "text/plain", "Command received and set");
      } else {
        request->send(403, "text/plain", "Invalid password for the command");
      }
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });


  // Start the server
  server.begin();
  Serial.println("Server started");
}

void loop() {
  // No loop code needed; server runs asynchronously
}    

