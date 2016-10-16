#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

const char* ssid = ".IDEAL";
const char* wifiPassword = "";

const char* user = "esp8266-lcd-display";
const char* matrixPassword = "esp8266-lcd-display";
const char* roomId = "!ZUEZXBBVjWJjQeXgbY:matrix.org";

#define SERIAL_RX 12
#define SERIAL_TX 13

SoftwareSerial serial2(SERIAL_RX, SERIAL_TX);

HTTPClient http;
String accessToken;
String lastMessageToken;

void createLoginBody(char* buffer, int bufferLen, String user, String password) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "m.login.password";
  root["user"] = user;
  root["password"] = password;
  root.printTo(buffer, bufferLen);
}

void createMessageBody(char* buffer, int bufferLen, String message) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["msgtype"] = "m.text";
  root["body"] = message;
  root.printTo(buffer, bufferLen);
}

bool login(String user, String password) {
  bool success = false;
  
  char buffer[512];
  createLoginBody(buffer, 512, user, password);

  String url = "http://matrix.org/_matrix/client/r0/login";
// Serial.printf("POST %s\n", url.c_str());

  http.begin("http://matrix.org/_matrix/client/r0/login");
  http.addHeader("Content-Type", "application/json");
  int rc = http.POST(buffer);
  if (rc > 0) {
//    Serial.printf("%d\n", rc);
    if (rc == HTTP_CODE_OK) {
      String body = http.getString();
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(body);
      String myAccessToken = root["access_token"];
      accessToken = String(myAccessToken.c_str());
      Serial.println(accessToken);
      success = true;
    }
  } else {
    Serial.printf("Error: %s\n", http.errorToString(rc).c_str());
  }

  return success;
}

bool getMessages(String roomId) {
  bool success = false;

  String url = "http://matrix.org/_matrix/client/r0/rooms/" + roomId + "/messages?access_token=" + accessToken + "&limit=1";
  if (lastMessageToken == "") {
    url += "&dir=b";
  } else {
    url += "&dir=f&from=" + lastMessageToken;
  }
  Serial.printf("GET %s\n", url.c_str());

  http.begin(url);
  int rc = http.GET();
  if (rc > 0) {
    Serial.printf("%d\n", rc);
    if (rc == HTTP_CODE_OK) {
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(http.getString());
      if (lastMessageToken != "") {
        JsonArray& chunks = root["chunk"];
        JsonObject& chunk = chunks[0];
        String sender = chunk["sender"];
        JsonObject& content = chunk["content"];
        if (content.containsKey("body")) {
          String body = content["body"];
          Serial.println(body);
          serial2.println(body);
        }
      }
      String myLastMessageToken = root["end"];
      lastMessageToken = String(myLastMessageToken.c_str());
      Serial.println(lastMessageToken);
      success = true;
    }
  } else {
    Serial.printf("Error: %s\n", http.errorToString(rc).c_str());
  }

  return success;
}

bool sendMessage(String roomId, String message) {
  bool success = false;

  char buffer[512];
  createMessageBody(buffer, 512, message);

  String url = "http://matrix.org/_matrix/client/r0/rooms/!ZUEZXBBVjWJjQeXgbY:matrix.org/send/m.room.message/" + String(millis()) + "?access_token=" + accessToken + "&limit=1";
  Serial.printf("PUT %s\n", url.c_str());

  http.begin(url);
  int rc = http.sendRequest("PUT", buffer);
  if (rc > 0) {
//    Serial.printf("%d\n", rc);
    if (rc == HTTP_CODE_OK) {
      success = true;
    }
  } else {
    Serial.printf("Error: %s\n", http.errorToString(rc).c_str());
  }
  return success;  
}


void setup(void) {
  pinMode(SERIAL_RX, INPUT);
  pinMode(SERIAL_TX, OUTPUT);
  serial2.begin(9600);
  serial2.println("Connecting to");
  serial2.println(ssid);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(9600);
  WiFi.begin(ssid, wifiPassword);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  serial2.println("Connected as");
  serial2.println(WiFi.localIP());

  http.setReuse(true);
  if (login(user, matrixPassword)) {
    getMessages(roomId);
//    sendMessage(roomId, "Hello from ESP8266");
  }
}

int nextMillis = 0;
String input;
void loop(void){
  if (serial2.available() > 0) {
    char c = serial2.read();
    Serial.print(c);
    if ((c == '\r') || (c == '\n')) {
      sendMessage(roomId, input);
      input = "";
    } else {
      input += String(c);
    }
  }  
//  serial2.print("Hello\n");
  if (millis() > nextMillis) {
    getMessages(roomId);    
    nextMillis += 5000;
  }
//  getMessages();
}
