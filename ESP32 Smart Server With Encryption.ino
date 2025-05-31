 #include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <mbedtls/aes.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

// Replace with your network credentials
const char* ssid = "O6Unetwork";
const char* password = "NT630100";

// Passwords for each door (original passwords)
const String passwordMainDoor = "123456";
const String passwordSecondaryDoor = "654321";

// RSA Public Key PEM of client (for encrypting AES key)
const char rsaClientPublicKeyPEM[] = R"KEY(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzXtwvvccFbcssiC7MUvd
V/Vc9BY4Z70/hU+sSvW5UxV99WZhZNCdsEOMyrkIyNF2NU04fqyuF/hc00eFyn6j
EDKg2V/tsKaRSEubkPnEc1Njnmx3kcm+muHCczkpfC7Y9dJl959oUtMZpNIFqlLU
KU9j0vg3zJVKE+VWheGJI9Yz5sO79+juCjoIitfJ6mK9S30uJ/5qHJj6kIitVLxE
zy/ZjqseOYvpKmnh0u+ZuSvv+UqS9kfMJSx5D9FB3eVzg eummHoSFerv09IvHbv
2v15hWP3dl1AIbY7JLqREyGufjO8ley+e0PcS3b2/mni8MWrQFtxPKcMY2NgOXpX
o/n8CAwEAAQ==
-----END PUBLIC KEY-----
)KEY";

// AES Key (32 bytes for AES-256)
byte aesKey[32] = { 
  0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18,
  0x29, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x90,
  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
  0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
};

byte iv[16] = {0}; // Initialization vector (fixed)

String statusMessages[5];
int messageCount = 0;
String currentCommand = "";
String pendingCommand = "";

IPAddress local_IP(192, 168, 137, 66);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

#define FILE_PHOTO "/photo.jpg"

AsyncWebServer server(80);

String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Data Display</title>
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
    function fetchSensorData() {
      fetch('/sensorData')
        .then(response => response.json())
        .then(data => {
          document.getElementById('temperature').textContent = data.temperature || 'N/A';
          document.getElementById('humidity').textContent = data.humidity || 'N/A';

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

    function refreshPhoto() {
      const photo = document.getElementById('latestPhoto');
      photo.src = '/photo?rand=' + Math.random();
    }

    function fetchStatusMessages() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          const statusList = document.getElementById('statusMessagesList');
          statusList.innerHTML = '';
          data.messages.forEach(message => {
            const listItem = document.createElement('li');
            listItem.textContent = message;
            statusList.appendChild(listItem);
          });
        })
        .catch(error => console.error('Error fetching status messages:', error));
    }

    setInterval(fetchSensorData, 5000);
    setInterval(fetchStatusMessages, 5000);
    fetchSensorData();
    fetchStatusMessages();
    refreshPhoto();
  </script>
</body>
</html>
)rawliteral";

float latestTemperature = 0.0;
float latestHumidity = 0.0;

String encryptAES(const String& input, const byte key[32]) {
  mbedtls_aes_context aes;
  byte inputBytes[16] = {0};
  byte outputBytes[16] = {0};
  input.getBytes(inputBytes, 16);

  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 256);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 16, iv, inputBytes, outputBytes);
  mbedtls_aes_free(&aes);

  char hexStr[33];
  for(int i=0; i<16; i++) {
    sprintf(hexStr + i*2, "%02x", outputBytes[i]);
  }
  return String(hexStr);
}

String encryptRSA_AESKey() {
  int ret;
  mbedtls_pk_context pk;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  const char *pers = "rsa_encrypt";

  mbedtls_pk_init(&pk);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)pers, strlen(pers));
  if(ret != 0) {
    Serial.println("Failed to seed RNG");
    return "";
  }

  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)rsaClientPublicKeyPEM, strlen(rsaClientPublicKeyPEM)+1);
  if(ret != 0) {
    Serial.println("Failed to parse client public key");
    return "";
  }

  size_t olen = 0;
  byte encrypted[512] = {0};

  ret = mbedtls_pk_encrypt(&pk, aesKey, sizeof(aesKey),
                           encrypted, &olen, sizeof(encrypted),
                           mbedtls_ctr_drbg_random, &ctr_drbg);
  if(ret != 0) {
    Serial.println("RSA encryption of AES key failed");
    return "";
  }

  char hexEncrypted[1025];
  for(size_t i=0; i<olen; i++) {
    sprintf(hexEncrypted + i*2, "%02x", encrypted[i]);
  }
  hexEncrypted[olen*2] = 0;

  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  return String(hexEncrypted);
}

void setup() {
  Serial.begin(115200);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.printf("Server IP Address: %s\n", WiFi.localIP().toString().c_str());

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  Serial.println("SPIFFS initialized successfully");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlPage);
  });

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

  server.on("/photo", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpeg");
  });

  server.on("/status", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("message", true)) {
      String newMessage = request->getParam("message", true)->value();
      if (messageCount < 5) {
        statusMessages[messageCount++] = newMessage;
      } else {
        for (int i = 0; i < 4; i++) statusMessages[i] = statusMessages[i + 1];
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
      request->send(200, "text/plain", pendingCommand);
      Serial.println("Command sent to client: " + pendingCommand);
      pendingCommand = "";
    } else {
      request->send(200, "text/plain", "");
      Serial.println("No command pending");
    }
  });

  server.on("/setCommand", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("command", true) && request->hasParam("door", true)) {
      String command = request->getParam("command", true)->value();
      String door = request->getParam("door", true)->value();

      String originalPass = "";
      if (door == "main") originalPass = passwordMainDoor;
      else if (door == "secondary") originalPass = passwordSecondaryDoor;
      else {
        request->send(400, "text/plain", "Invalid door parameter");
        return;
      }

      String encryptedPassword = encryptAES(originalPass, aesKey);
      String encryptedAESKey = encryptRSA_AESKey();

      // Send JSON response with encrypted password and AES key
      String jsonResp = "{\"command\":\"" + command + "\",\"encryptedPassword\":\"" + encryptedPassword + "\",\"encryptedAESKey\":\"" + encryptedAESKey + "\"}";
      request->send(200, "application/json", jsonResp);

      Serial.printf("Encrypted Password: %s\nEncrypted AES Key: %s\n", encryptedPassword.c_str(), encryptedAESKey.c_str());

    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });

  server.begin();
  Serial.println("Server started");
}

void loop() {
  // No code needed here, async server handles requests
}
