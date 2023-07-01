// libraries
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Replace with your network credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Get UTC Time
// NTP (Network Time Protocol) settings
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = -4 * 60 * 60;  // Offset for Eastern Standard Time (EST)

// Create an NTPClient instance to get the time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output26State = "off";  // Forward
String output27State = "off";  // Backward
String feedState = "off";      // Feed Function

// Motor driver pins
const int output26 = 26;
const int output27 = 27;
const int enable1Pin = 14;


// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;


// Function to move the motor forward for 15 seconds
// 15 seconds averages out to one cup per auger at 7.5v
void feed() {
  digitalWrite(output27, LOW);
  digitalWrite(output26, HIGH);
  digitalWrite(enable1Pin, 255);
  delay(15000);
  digitalWrite(output27, LOW);
  digitalWrite(output26, LOW);
  digitalWrite(enable1Pin, 0);
}

// String getCurrentTime(int hour, int minute, int second) {
//   String now;
//   String stringHour;
//   String stringMinute;
//   String stringSecond;
//   stringHour = String(hour);
//   stringMinute = String(minute);
//   stringSecond = String(second);
//   now = stringHour + ":" + stringMinute + ":" + stringSecond;
//   return now;
// }

void setup() {
  Serial.begin(115200);
  // Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  // pinMode(enable1Pin, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize the NTP client
  timeClient.begin();

  // Start the HTTP server
  server.begin();
}

void loop() {


  // Update the NTP client
  timeClient.update();

  // Check if it's time to feed
  int currentHourUtc = timeClient.getHours();
  int currentMinuteUtc = timeClient.getMinutes();
  int currentSecondUtc = timeClient.getSeconds();

  // Feed if 5am or 5pm
  if (currentHourUtc == 05 && currentMinuteUtc == 00 && currentSecondUtc == 00 || currentHourUtc == 17 && currentMinuteUtc == 00 && currentSecondUtc == 00) {

    feedState = "on";
    Serial.println("Feeding on");
    feed();
    Serial.println("Feeding off");
    feedState = "off";
  }

  // FOR TESTING ONLY
  // int currentTime = timeClient.getEpochTime();
  // // Serial.println(currentTime);
  // if (currentHourUtc == 00 && currentMinuteUtc == 04 && currentSecondUtc == 00) {
  //   feedState = "on";
  //   Serial.println("Feeding on");
  //   feed();
  //   Serial.println("Feeding off");
  //   feedState = "off";
  // }

  WiFiClient client = server.available();  // Listen for incoming clients

  if (client) {  // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");                                             // print a message out in the serial port
    String currentLine = "";                                                   // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /feed") >= 0) {
              Serial.println("Feeding on");
              feedState = "on";
              feed();
              feedState = "off";
              Serial.println("Feeding off");
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<title>FeedMeow V2</title>");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>FeedMeow V2</h1>");

            // Display current time

            // client.println("<p>Current Time EST - " + getCurrentTime(timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds()) + "</p>");
            if (feedState == "off") {
              client.println("<p><a href=\"/feed\"><button class=\"button\">Feed</button></a></p>");
            } else {
              client.println("<p><a href=\"/feeding\"><button class=\"button button2\">Feeding</button></a></p>");
            }

            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else {  // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
