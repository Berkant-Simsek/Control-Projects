#include <WiFi.h>
// #include <ESPAsyncWebServer.h>
// #include <AsyncTCP.h>

#define STEP_PIN 14
#define DIR_PIN  12
#define POWER_LED_PIN  2
#define MS1 25
#define MS2 26
#define MS3 27

// declerations
void initializePins();
void startHotspot();
void startTCPServer();
// void webSocketSettings();
// void startHTTPServer();
void runUntilStop();
void waitUntilConnection();
void operateClient();
void closeConnection();
// void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void runOneCycle();
void operateIncomingData();
void parseMessage();
void operateMessage();
bool isInt(String str);
bool isFloat(String str);
void updateMod();
void updateVelocity();
void refresh();


// const char index_html[] PROGMEM = R"rawliteral(
// <!DOCTYPE html>
// <html>
// <head><meta charset="utf-8"><title>Motor Control</title></head>
// <body>
// <h1>Motor Control Panel</h1>
// <button id="powerButton" onclick="togglePower()">Power OFF</button>
// <p id="status">No connection...</p>

// <script>
// var ws = new WebSocket("ws://" + location.host + "/ws");
// var power = false;

// const powerButton = document.getElementById("powerButton");

// ws.onopen = () => {
// document.getElementById("status").innerText = "WebSocket connected.";
// };

// ws.onmessage = (e) => {
// document.getElementById("status").innerText = e.data;
// };

// function togglePower() {
// power = !power;
// if (power) {
// ws.send("config 1 -1 1000 -1");
// powerButton.innerText = "Power ON";
// }
// else {
// ws.send("config 0 -1 1000 -1");
// powerButton.innerText = "Power OFF";
// }
// }
// </script>
// </body>
// </html>
// )rawliteral";

// WiFi Settings
const char *ssid = "ESP32_HOTSPOT";
const char *password = "12345678";

// TCP Server Settings
WiFiServer tcpServer(5000);
WiFiClient tcpClient;

// WebSocket and HTTP server
// AsyncWebServer httpServer(80);
// AsyncWebSocket ws("/ws");

unsigned long startTime = 0;
unsigned long endTime = 0;
unsigned long duration = 100;

int mod = 0;
float timeConstant = 250000.0;
float frequency = 0.0;
float multipliers[] = {1.0, 1.0, 1.0, 1.0, 1.0};
int desiredSpeed = 0;
int desiredVelocity = 0;
int actualVelocity = 0;
int acceleration = 50;
int desiredDirection = 1;
int currentDirection = 1;
int previousDirection = 0;
int power = 0;
String command;
int dataInt[4];
float dataFloat[5];
bool isValidFormat = false;
String incomingMessage;
char clientType = '-';

String httpMessage;
bool httpConnected;
bool httpNewMessage;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("setup runs!");
  initializePins();

  startHotspot();

  startTCPServer();

  // webSocketSettings();
  // startHTTPServer();
}

void loop() {
  Serial.println("Loop runs!");
  runUntilStop();
  waitUntilConnection();
  Serial.println("Connected!");
  operateClient();
  closeConnection();
}

void initializePins(){
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(POWER_LED_PIN, OUTPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(MS3, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);
  digitalWrite(POWER_LED_PIN, LOW);
  digitalWrite(MS1, LOW);
  digitalWrite(MS2, LOW);
  digitalWrite(MS3, LOW);
}

// void startHTTPServer() {
//   httpServer.begin();
//   Serial.println("HTTP Server started.");
// }

// void webSocketSettings() {
//   ws.onEvent(onWsEvent);
//   httpServer.addHandler(&ws);

//   // Basic HTML endpoint
//   httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//     request->send(200, "text/html", index_html);
//   });
// }

// void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
//   if (type == WS_EVT_CONNECT) {
//     httpConnected = true;
//   }
//   else if (type == WS_EVT_DISCONNECT) {
//     httpConnected = false;
//   }
//   else if (type == WS_EVT_DATA) {
//     String message = "";
//     for (size_t i = 0; i < len; i++){
//       message += (char)data[i];
//     }
//     httpMessage = message;
//     httpNewMessage = true;
//   }
// }

void updateSpeedSmoothly(){
  endTime = millis();
    if (endTime - startTime >= duration){
    int difference = desiredVelocity - actualVelocity;
    if (difference < -acceleration){
      difference = -acceleration;
    } else if (difference > acceleration){
      difference = acceleration;
    }
    actualVelocity = actualVelocity + difference;
    if(actualVelocity >= 0){
      currentDirection = 1;
    } else {
      currentDirection = 0;
    }
    if (previousDirection != currentDirection){
      digitalWrite(DIR_PIN, currentDirection);
      previousDirection = currentDirection;
    }
    frequency = abs(actualVelocity)/timeConstant;
    startTime = millis();
  }
}

void startHotspot(){
  WiFi.softAP(ssid, password);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

void startTCPServer(){
  tcpServer.begin();
  Serial.println("TCP Server started.");
}

void waitUntilConnection(){
  while (true){
    // if (httpConnected) {
      // clientType = 'H';
      // return;
    // }
    if (tcpServer.hasClient()){
      tcpClient = tcpServer.available();
      clientType = 'T';
      return;
    }
  }
}

void closeConnection(){
  if (clientType == 'T'){
    if (tcpClient) {
      tcpClient.stop();
    }
  }
  // else if (clientType == 'H') {
    // clientType = '-';
  // }
  clientType = '-';
}

void runUntilNewData(){
  while ((clientType == 'T' && tcpClient.connected() && !tcpClient.available())){ // || (clientType == 'H' && httpConnected && !httpNewMessage)) {
    updateSpeedSmoothly();
    runOneCycle();
  }
}

void runUntilStop(){
  desiredVelocity = 0;
  while (actualVelocity) {
    updateSpeedSmoothly();
    runOneCycle();
  }
}

void runOneCycle(){
  if (frequency){
    float halfPeriod = 1/(2*frequency);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(halfPeriod);
  }
}

void operateClient(){
  if (clientType == 'T'){
    if (tcpClient) {
      while (tcpClient.connected()) {
        runUntilNewData();
        if (tcpClient.available()) {
          operateIncomingData();
        }
      }
    }
  }
  // else if (clientType == 'H') {
    // while (httpConnected) {
      // runUntilNewData();
      // if (httpNewMessage) {
      //  operateIncomingData();
      // }
    // }
  // }
}

void operateIncomingData(){
  if (clientType == 'T') {
    incomingMessage = tcpClient.readStringUntil('\n');
    Serial.print("Message: ");
    Serial.println(incomingMessage);
  }
  // else if (clientType == 'H') {
  //  incomingMessage = httpMessage;
  // }
  parseMessage();
  operateMessage();
}

void parseMessage(){
  isValidFormat = false;
  int spaceIndexes[7];
  spaceIndexes[0] = -1;
  String temp = incomingMessage;
  int wordCount = 1;
  while (wordCount < 7){
    int index = temp.indexOf(' ');
    if (index == -1) {
      break;
    }
    spaceIndexes[wordCount] = spaceIndexes[wordCount-1] + 1 + index;
    temp = temp.substring(index + 1); // remaining
    Serial.print("temp: ");
    Serial.println(temp);
    Serial.print("Index: ");
    Serial.println(spaceIndexes[wordCount]);
    wordCount++;
  }
  Serial.println("parse after word count");
  Serial.print("word count: ");
  Serial.println(wordCount);

  if (wordCount == 1) {
    if (strcmp(incomingMessage.c_str(), "refresh") == 0){
      command = "refresh";
      isValidFormat = true;
    }
    return;
  }

  command = incomingMessage.substring(0, spaceIndexes[1]);
  if (wordCount == 5 && strcmp(command.c_str(), "config") == 0) {
    Serial.println("parse config");
    spaceIndexes[wordCount] = incomingMessage.length();
    for (int i = 0; i < wordCount - 1; i++){
      String dataStr = incomingMessage.substring(spaceIndexes[i+1] + 1, spaceIndexes[i+2]);
      if (isInt(dataStr)){
        dataInt[i] = dataStr.toInt();
      }
      else {
        return;
      }
    }
    isValidFormat = true;
    return;
  }

  if (wordCount == 6 && strcmp(command.c_str(), "multipliers") == 0) {
    spaceIndexes[wordCount] = incomingMessage.length();
    for (int i = 0; i < wordCount - 1; i++){
      String dataStr = incomingMessage.substring(spaceIndexes[i+1] + 1, spaceIndexes[i+2]);
      if (isFloat(dataStr)){
        dataFloat[i] = dataStr.toFloat();
        } else {
          return;
      }
    }
    isValidFormat = true;
    return;
  }
}

bool isInt(String str){
  int size = str.length();
  if (size == 0) {
    return false;
  }

  for (int i = 0; i < size; i++) {
    char ch = str.charAt(i);
    if (!isDigit(ch) && !(ch == '-' && i == 0)) {
      return false;
    }
  }
  return true;
}

bool isFloat(String str){
  int size = str.length();
  if (size == 0) {
    return false;
  }

  int floatingPointCount = 0;
  for (int i = 0; i < size; i++) {
    char ch = str.charAt(i);
    if (!isDigit(ch) && !(ch == '-' && i == 0) && !(ch == '.' && floatingPointCount++ == 0)) {
      return false;
    }
  }
  return true;
}

void operateMessage(){
  Serial.println("operateMessage runs!");
  if (isValidFormat){
    Serial.println("is valid.");
    isValidFormat = false;
    if (strcmp(command.c_str(), "refresh") == 0 ){
      refresh();
    } else if (strcmp(command.c_str(), "config") == 0){
      if (dataInt[0] != -1){ // power
        if (dataInt[0] == 0 || dataInt[0] == 1) {
          power = dataInt[0];
          digitalWrite(POWER_LED_PIN, power);
          Serial.print("Power: ");
          Serial.println(power);
        }
      }
      if (dataInt[1] != -1){ // direction
        if (dataInt[1] == 0 || dataInt[1] == 1) {
          desiredDirection = dataInt[1];
        }
      }
      if (dataInt[2] != -1){ // speed
        if (dataInt[2] >= 0 && dataInt[2] <= 1000) {
          desiredSpeed = dataInt[2];
        }
      }
      if (dataInt[3] != -1){ // microstep mod
        if (dataInt[3] >= 0 && dataInt[3] <= 4) {
          mod = dataInt[3];
          updateMod();
        }
      }
      updateVelocity();
    } else if (strcmp(command.c_str(), "multipliers") == 0) {
      for (int i = 0; i<5; i++) {
        float number = dataFloat[i];
        if (number <= 0.0) {
          number = 1.0;
        }
        multipliers[i] = number;
      }
      updateVelocity();
      Serial.println("Multipliers:");
      Serial.println(multipliers[0]);
      Serial.println(multipliers[1]);
      Serial.println(multipliers[2]);
      Serial.println(multipliers[3]);
      Serial.println(multipliers[4]);
    }
  }
}

void refresh() {
  // int velocity = desiredVelocity;
  // runUntilStop();
  // desiredVelocity = velocity;
  actualVelocity = 0;
}

void updateMod() {
  // mod      : (MS1, MS2, MS3)
  // 0 (1/1)  :   L    L    L
  // 1 (1/2)  :   H    L    L
  // 2 (1/4)  :   L    H    L
  // 3 (1/8)  :   H    H    L
  // 4 (1/16) :   H    H    H
  // digitalWrite(MS1, mod == 1 || mod == 3 || mod == 4);
  // digitalWrite(MS2, mod == 2 || mod == 3 || mod == 4);
  // digitalWrite(MS3, mod == 4);
  switch (mod){
    case 0:
      digitalWrite(MS1, LOW);
      digitalWrite(MS2, LOW);
      digitalWrite(MS3, LOW);
      Serial.println("Case: 0");
      break;
    case 1:
      digitalWrite(MS1, HIGH);
      digitalWrite(MS2, LOW);
      digitalWrite(MS3, LOW);
      Serial.println("Case: 1");
      break;
    case 2:
      digitalWrite(MS1, LOW);
      digitalWrite(MS2, HIGH);
      digitalWrite(MS3, LOW);
      Serial.println("Case: 2");
      break;
    case 3:
      digitalWrite(MS1, HIGH);
      digitalWrite(MS2, HIGH);
      digitalWrite(MS3, LOW);
      Serial.println("Case: 3");
      break;
    case 4:
      digitalWrite(MS1, HIGH);
      digitalWrite(MS2, HIGH);
      digitalWrite(MS3, HIGH);
      Serial.println("Case: 4");
      break;
  }
  Serial.print("Mod: ");
  Serial.println(mod);
}

void updateVelocity() {
  desiredVelocity = (desiredDirection-1) * 2 * desiredSpeed + desiredSpeed;
  desiredVelocity = desiredVelocity * multipliers[mod];
  desiredVelocity = desiredVelocity * power;
  Serial.print("Desired velocity: ");
  Serial.println(desiredVelocity);
}
