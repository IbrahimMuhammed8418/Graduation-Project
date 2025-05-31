 #include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <mbedtls/aes.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

// --- تعاريف لوحة المفاتيح ---
#define ROW_NUM 4
#define COLUMN_NUM 4

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM] = {27, 14, 12, 13};  // GPIO للأعمدة
byte pin_columns[COLUMN_NUM] = {32, 33, 25, 26};  // GPIO للصفوف
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_columns, ROW_NUM, COLUMN_NUM);

// --- تعريف محركات السيرفو ---
Servo mainDoorServo;
Servo secondaryDoorServo;

// --- أزرار ---
const int button2Pin = 23;
int button2State = HIGH;
int lastButton2State = HIGH;

// --- متغيرات أخرى ---
String enteredPassword = "";
bool isKeypadActive = true;
unsigned long doorOpenTime = 0;
const unsigned long doorCloseDelay = 3000;  // 3 ثواني

bool mainDoorState = false;
bool secondaryDoorState = false;

// --- بيانات الواي فاي ---
const char* ssid = "O6Unetwork";
const char* wifiPassword = "NT630100";

// --- مؤقت طلب الأوامر من السيرفر ---
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 5000;  // كل 5 ثواني

// --- IV ثابت لفك التشفير AES ---
byte iv[16] = {0};

// --- المفتاح الخاص بالـ RSA لفك تشفير AES Key ---
const char rsaPrivateKeyPEM[] = R"KEY(
-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDNe3C+9xwVtyxy
ILsxS91X9Vz0FjhnvT+FT6xK9blTFX31ZmFk0J2wQ4zKuQjI0XY1TTh+rK4X+FzT
R4XKfqMQMqDZX+2wppFIS5uQ+cRzU2OebHeRyb6a4cJzOSl8Ltj10mX3n2hS0xmk
0gWqUtQpT2PS+DfMlUoT5VaF4Ykj1jPmw7v36O4KOgiK18nqYr1LfS4n/mocmPqQ
iK1UvETPL9mOqx45i+kqaeHS75m5K+/5SpL2R8wlLHkP0UHd5XOB65qYehIV6uPT
0i8du9r9eYVj93ZdQCG2OyS6kRMhrn4zvJXsvntD3Et29v5p4vDFq0BbcTynDNjY
Dl6V6P5/AgMBAAECggEABCEJq9cYkI8GxoLoL3yEz1dwq52Fjkzgn5r20sczjLQE
8B+FTIq66acIHqKe3g0SGDLkGvGb6nDcBo0vGu9GT+idkO6RJ2J6N9xGkErjWZH1
G8AxOyrbiPxzpPcRjq6B7Npi+qRD56rMb3Ql4hqxx7sl0qRHuUyk/WDCReFjPSmc
xYxxNgHXk7O3Wh0pqIKPA6VCR7zKjQRMS4DCmzoPo/Zs7HLf8+5tx8CG4SZ3bQqJ
Y86+YIHXrJDtXnES+6G8vjhdDw5cgK3FMWKR5cJv4V/0G0MBKlG4YkdbxaJ3g94R
lsHhu0W0JhV2yd5kyNRDZfXT/8eN7om0PYJ2bK/eAQKBgQD9yztTp+9n5H3tn3k+
UgxvQINQ+Sxv6FL4k0liw/9AxYJHlcChFko+Z5LqgXyVu9DX7mY5AcujsB8ovYHm
FVUDuW4FL5D4Nju5grFqDTvYx1ZwCX1aSvxbpMZHxA9fzWmYLJ/Rp7jq7EeIKG8r
XdxjNwpZLt1uDku7kS/V+aCLzwKBgQDj8hIizFpuWe7DL12YzwzNluH0Lj6pq6ne
Jmk0LaO9GIrhWzjFczvqM9hK1f8oONnlr5x81cJcB03J70GiCdhNPQzdbH/N4cpt
VBl8pkh0aMehIGbdzJk6t6ObEy4ivdzGXmS9jk+6xOW4Spv+7Nq65NvpYpeY8BdZ
oPSu6zpndwKBgG0wep4PElCNkPo4vN3Vg/RqWRe4z3Lxrr4sGydkrMyqJFG+Hml8
bjVfWUS6o3QpzFl2ju7vI3id9hcMrqXGqVZrAzCuRm1RxRzRws3BGX7zP2LZlj8X
63+fyqPBNJzXyqPA4UN+8/WfnDw1DZ4r0U7rE/8y6l1VLQ8XLaMWftfJAoGAJJo9
nSQqddDHXb6B5Svj+L1zkn+DKswG4+vNY2RTGkaZZx5Ri0MJ6nrCz9Qgu6XjW2Vk
vO3IXQriLkmQeG24G+tjfk7rN3lHZvPkNl8fihk8H7HsaxnW9kpLwGuFvlXx0Gu0
dxCUOR7hvUZXT44Fxqv2syQ4Pq6e61LUh6V4nVECgYEAiIHg3RHcCOKltU1YPKgy
s+iA9L15ZcR3A6yRzNhw3BDXMxoCL9y6MKlOxXGSuBTZHkW1znX/27V4+VdlkmH8
uPX6HbsPSPecq7UDJQOZYxrj+myj3lUx6I3aS3VPhrQNXE7+4myiBu5VTxPdeTSI
b/3+WsfXtT3dkpZxxLx1PAU=
-----END PRIVATE KEY-----
)KEY";

// --- دوال فك التشفير ---
bool decryptRSA_AESKey(const String& encryptedHex, byte* outputKey, size_t& outputLen) {
  int ret;
  mbedtls_pk_context pk;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  const char *pers = "rsa_decrypt";

  mbedtls_pk_init(&pk);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)pers, strlen(pers));
  if(ret != 0) {
    Serial.println("Failed to seed the RNG");
    return false;
  }

  ret = mbedtls_pk_parse_key(&pk, (const unsigned char*)rsaPrivateKeyPEM, strlen(rsaPrivateKeyPEM)+1, NULL, 0);
  if(ret != 0) {
    Serial.println("Failed to parse private key");
    return false;
  }

  size_t encryptedLen = encryptedHex.length() / 2;
  byte encryptedBytes[512] = {0};
  for (size_t i = 0; i < encryptedLen; i++) {
    sscanf(encryptedHex.c_str() + i*2, "%2hhx", &encryptedBytes[i]);
  }

  ret = mbedtls_pk_decrypt(&pk, encryptedBytes, encryptedLen,
                           outputKey, &outputLen, 512,
                           mbedtls_ctr_drbg_random, &ctr_drbg);
  if(ret != 0) {
    Serial.printf("RSA decryption failed with error %d\n", ret);
    return false;
  }

  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  return true;
}

String decryptAESPassword(const String& encryptedHex, byte* aesKey, size_t keyLen) {
  mbedtls_aes_context aes;
  byte iv_local[16] = {0};
  byte input[16] = {0};
  byte output[16] = {0};

  for(int i=0; i<16; i++) {
    sscanf(encryptedHex.c_str() + i*2, "%2hhx", &input[i]);
  }

  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, aesKey, keyLen * 8);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv_local, input, output);
  mbedtls_aes_free(&aes);

  output[15] = 0;
  return String((char*)output);
}

// --- وظائف فتح وغلق الأبواب ---
void openMainDoor() {
  if (!mainDoorState) {
    mainDoorServo.write(80);
    mainDoorState = true;
    doorOpenTime = millis();
    Serial.println("Main door opened");
  }
}

void closeMainDoor() {
  if (mainDoorState) {
    mainDoorServo.write(0);
    mainDoorState = false;
    Serial.println("Main door closed");
  }
}

void openSecondaryDoor() {
  if (!secondaryDoorState) {
    secondaryDoorServo.write(80);
    secondaryDoorState = true;
    doorOpenTime = millis();
    Serial.println("Secondary door opened");
  }
}

void closeSecondaryDoor() {
  if (secondaryDoorState) {
    secondaryDoorServo.write(0);
    secondaryDoorState = false;
    Serial.println("Secondary door closed");
  }
}

// --- التعامل مع لوحة المفاتيح ---
void handleKeypad() {
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key == '#') {
      Serial.print("Entered password: ");
      Serial.println(enteredPassword);

      if (enteredPassword == "123456") {
        Serial.println("Correct password for main door. Opening...");
        openMainDoor();
      } else if (enteredPassword == "654321") {
        Serial.println("Correct password for secondary door. Opening...");
        openSecondaryDoor();
      } else {
        Serial.println("Incorrect password");
      }
      enteredPassword = "";
    } else if (key == '*') {
      enteredPassword = "";
      Serial.println("Password reset.");
    } else {
      enteredPassword += key;
    }
  }
}

// --- التعامل مع الأزرار ---
void handleButtons() {
  button2State = digitalRead(button2Pin);
  if (button2State == LOW && lastButton2State == HIGH) {
    Serial.println("Button 2 pressed! Toggling secondary door.");
    if (secondaryDoorState) closeSecondaryDoor();
    else openSecondaryDoor();
    delay(200);
  }
  lastButton2State = button2State;
}

// --- إغلاق الأبواب تلقائياً بعد فترة ---
void closeDoors() {
  if (mainDoorState && (millis() - doorOpenTime >= doorCloseDelay)) {
    closeMainDoor();
  }
  if (secondaryDoorState && (millis() - doorOpenTime >= doorCloseDelay)) {
    closeSecondaryDoor();
  }
}

// --- الاتصال بالواي فاي ---
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

void fetchRemoteCommands() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.137.66/setCommand");
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();

      // فك JSON يدوي (يمكن استخدام ArduinoJson لتحسين)
      int cmdStart = payload.indexOf("\"command\":\"") + 10;
      int cmdEnd = payload.indexOf("\"", cmdStart);
      String command = payload.substring(cmdStart, cmdEnd);

      int passStart = payload.indexOf("\"encryptedPassword\":\"") + 21;
      int passEnd = payload.indexOf("\"", passStart);
      String encryptedPassword = payload.substring(passStart, passEnd);

      int keyStart = payload.indexOf("\"encryptedAESKey\":\"") + 18;
      int keyEnd = payload.indexOf("\"", keyStart);
      String encryptedAESKey = payload.substring(keyStart, keyEnd);

      Serial.println("Received command: " + command);
      Serial.println("Encrypted AES Key: " + encryptedAESKey);
      Serial.println("Encrypted Password: " + encryptedPassword);

      byte decryptedAESKey[512];
      size_t decryptedAESKeyLen = 0;

      if (decryptRSA_AESKey(encryptedAESKey, decryptedAESKey, decryptedAESKeyLen)) {
        Serial.println("AES Key decrypted successfully.");

        String decryptedPassword = decryptAESPassword(encryptedPassword, decryptedAESKey, decryptedAESKeyLen);
        Serial.println("Decrypted password: " + decryptedPassword);

        // التحقق من كلمة السر وتنفيذ الأمر
        if (command == "open_main" && decryptedPassword == "123456") {
          openMainDoor();
        } else if (command == "toggle_secondary" && decryptedPassword == "654321") {
          if (secondaryDoorState) closeSecondaryDoor();
          else openSecondaryDoor();
        } else {
          Serial.println("Invalid command or password");
        }

      } else {
        Serial.println("Failed to decrypt AES Key");
      }

    } else {
      Serial.printf("HTTP GET failed, code: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();

  pinMode(button2Pin, INPUT_PULLUP);

  mainDoorServo.attach(5);
  secondaryDoorServo.attach(18);

  mainDoorServo.write(0);
  secondaryDoorServo.write(0);

  Serial.println("Setup complete");
}

void loop() {
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchRemoteCommands();
    lastFetchTime = millis();
  }

  handleKeypad();
  handleButtons();
  closeDoors();

  delay(10);
}
