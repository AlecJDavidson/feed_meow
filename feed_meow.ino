
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* serverName = "Feed Meow V2";
const char* filename = "/values.txt";

AsyncWebServer server(80);

struct FeedSettings {
  String feedAmount;
  String feedTimeOne;
  String feedTimeTwo;
};





// Handle Time

// Get UTC Time
// NTP (Network Time Protocol) settings
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = -4 * 60 * 60;  // Offset for Eastern Standard Time (EST)

// Create an NTPClient instance to get the time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)


// End Handle Time

// Setup Motors
// Auxiliar variables to store the current output state
String output26State = "off";  // Forward
String output27State = "off";  // Backward
String feedState = "off";      // Feed Function

// Motor driver pins
const int output26 = 26;
const int output27 = 27;
const int enable1Pin = 14;

// End Setup Motors

// Feed function to move motors
void feed(int valueSeconds) {
  digitalWrite(output27, LOW);
  digitalWrite(output26, HIGH);
  digitalWrite(enable1Pin, 255);
  delay(valueSeconds * 1000); // converts seconds to milliseconds
  digitalWrite(output27, LOW);
  digitalWrite(output26, LOW);
  digitalWrite(enable1Pin, 0);
}
// End feed function


void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);

  // More motor setup
   // Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  // pinMode(enable1Pin, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);
  // End motor setup

  // Connect to wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..."); 
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // End connect to wifi


  // Initialize the NTP client
  timeClient.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){
    FeedSettings settings = readSettingsFromFile();
    String html = "<html><head><style>";
    html += "body { text-align: center; }";
    html += "form { display: inline-block; text-align: left; }";
    html += "input[type='submit'] { padding: 10px 20px; font-size: 16px; }";
    html += ".feed-button { padding: 20px 40px; font-size: 24px; background-color: red; color: white; border-radius: 10px; border: none; }";
    html += ".header { border-radius: 10px; background-color: lightgray; padding: 10px; }";
    html += ".horizontal-rule { border: none; border-top: 2px solid gray; margin: 30px 0; }";
    html += ".save-button { margin: 0 auto; display: block; padding: 10px 20px; font-size: 16px; background-color: green; color: white; border: none; }";
    html += "</style></head><body>";
    html += "<div class='header'>";
    html += "<h1>";
    html += serverName;
    html += "</h1>";
    html += "</div>";
    html += "<hr class='horizontal-rule'>";
    html += "<h2>Settings</h2>";
    html += "<form method='POST' action='/save'>";
    html += "Feed Amount: <input type='text' name='feedAmount' value='" + String(settings.feedAmount) + "'><br><br>";
    html += "Feed Time One: <input type='text' name='feedTimeOne' value='" + String(settings.feedTimeOne) + "'><br><br>";
    html += "Feed Time Two: <input type='text' name='feedTimeTwo' value='" + String(settings.feedTimeTwo) + "'><br><br>";
    html += "<input type='submit' class='save-button' value='Save'>";
    html += "</form>";
    html += "<br>";
    html += "<button class='feed-button' onclick='feedNow()'>Feed</button>";
    html += "<script>function feedNow() { fetch('/feed').then(response => { console.log('Feeding now'); }).catch(error => { console.error(error); }); }</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest* request){
    FeedSettings settings;
    if (request->hasParam("feedAmount", true)) {
      AsyncWebParameter* feedAmountParam = request->getParam("feedAmount", true);
      settings.feedAmount = feedAmountParam->value().toInt();
    }
    if (request->hasParam("feedTimeOne", true)) {
      AsyncWebParameter* feedTimeOneParam = request->getParam("feedTimeOne", true);
      settings.feedTimeOne = feedTimeOneParam->value().toInt();
    }
    if (request->hasParam("feedTimeTwo", true)) {
      AsyncWebParameter* feedTimeTwoParam = request->getParam("feedTimeTwo", true);
      settings.feedTimeTwo = feedTimeTwoParam->value().toInt();
    }
    if (writeSettingsToFile(settings)) {
      request->send(200, "text/plain", "Values saved successfully.");
    } else {
      request->send(500, "text/plain", "Failed to save values.");
    }
    request->redirect("/");
  });

  server.on("/feed", HTTP_GET, [](AsyncWebServerRequest* request){
    Serial.println("Feeding now");
    String feedAmount = readSettingsFromFile().feedAmount;

    feed(feedAmount.toInt());
    request->send(200, "text/plain", "Feeding now");
  });

  server.begin();
}

void loop() {

  String feedTimeOne = readSettingsFromFile().feedTimeOne;
String feedTimeTwo = readSettingsFromFile().feedTimeTwo;
String feedAmount = readSettingsFromFile().feedAmount;

  // Update the NTP client
  timeClient.update();

  // Check if it's time to feed
  int currentHourUtc = timeClient.getHours();
  int currentMinuteUtc = timeClient.getMinutes();
  int currentSecondUtc = timeClient.getSeconds();

  // Feed if 5am or 5pm
  if (currentHourUtc == feedTimeOne.toInt() && currentMinuteUtc == 00 && currentSecondUtc == 00 || currentHourUtc == feedTimeTwo.toInt() && currentMinuteUtc == 00 && currentSecondUtc == 00) {

    feedState = "on";
    Serial.println("Feeding on");
    feed(feedAmount.toInt());
    Serial.println("Feeding off");
    feedState = "off";
  }

}

FeedSettings readSettingsFromFile() {
  FeedSettings settings;
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return settings;
  }
  String line;
  while (file.available()) {
    line = file.readStringUntil('\n');
    int separatorIndex = line.indexOf(':');
    if (separatorIndex != -1) {
      String key = line.substring(0, separatorIndex);
      String value = line.substring(separatorIndex + 1);
      if (key == "Feed Amount") {
        settings.feedAmount = value;
      } else if (key == "Feed Time One") {
        settings.feedTimeOne = value;
      } else if (key == "Feed Time Two") {
        settings.feedTimeTwo = value;
      }
    }
  }
  file.close();
  return settings;
}

bool writeSettingsToFile(const FeedSettings& settings) {
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  if (file.print("Feed Amount:" + settings.feedAmount + "\n")) {
    if (file.print("Feed Time One:" + settings.feedTimeOne + "\n")) {
      if (file.print("Feed Time Two:" + settings.feedTimeTwo + "\n")) {
        file.close();
        return true;
      }
    }
  }
  Serial.println("Failed to write values to file");
  file.close();
  return false;
}
