
// libraries
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h> // Include the EEPROM library

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

// Feed duration in seconds
unsigned long feedDuration = 15;

// Feed hours
int feedHour1 = 5;
int feedHour2 = 17;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Function to move the motor forward for the specified feed duration
void feed() {
  digitalWrite(output27, LOW);
  digitalWrite(output26, HIGH);
  digitalWrite(enable1Pin, 255);
  delay(feedDuration * 1000); // Multiply feed duration by 1000 to convert it to milliseconds
  digitalWrite(output27, LOW);
  digitalWrite(output26, LOW);
  digitalWrite(enable1Pin, 0);
}

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

  // Read feed duration from EEPROM
  EEPROM.begin(sizeof(feedDuration));
  EEPROM.get(0, feedDuration);
  EEPROM.end();

  // Read feed hours from EEPROM
  EEPROM.begin(sizeof(feedHour1) + sizeof(feedHour2));
  EEPROM.get(sizeof(feedDuration), feedHour1);
  EEPROM.get(sizeof(feedDuration) + sizeof(feedHour1), feedHour2);
  EEPROM.end();
}

void loop() {
  // Update the NTP client
  timeClient.update();

  // Check if it's time to feed
  int currentHourUtc = timeClient.getHours();
  int currentMinuteUtc = timeClient.getMinutes();
  int currentSecondUtc = timeClient.getSeconds();

  // Feed if it's feedHour1 or feedHour2
  if ((currentHourUtc == feedHour1 && currentMinuteUtc == 0 && currentSecondUtc == 0) ||
      (currentHourUtc == feedHour2 && currentMinuteUtc == 0 && currentSecondUtc == 0)) {

    feedState = "on";
    Serial.println("Feeding on");
    feed();
    Serial.println("Feeding off");
    feedState = "off";
  }

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

            // Display feed duration form
            client.println("<form action=\"/saveduration\" method=\"POST\">");
            client.println("<label for=\"duration\">Feed Duration (seconds):</label><br>");
            client.println("<input type=\"text\" id=\"duration\" name=\"duration\" value=\"" + String(feedDuration) + "\"><br><br>");
            client.println("<input type=\"submit\" value=\"Save\">");
            client.println("</form>");

            // Display feed hours form
            client.println("<form action=\"/savefeedhours\" method=\"POST\">");
            client.println("<label for=\"feedHour1\">Feed Hour 1:</label><br>");
            client.println("<input type=\"number\" id=\"feedHour1\" name=\"feedHour1\" value=\"" + String(feedHour1) + "\"><br><br>");
            client.println("<label for=\"feedHour2\">Feed Hour 2:</label><br>");
            client.println("<input type=\"number\" id=\"feedHour2\" name=\"feedHour2\" value=\"" + String(feedHour2) + "\"><br><br>");
            client.println("<input type=\"submit\" value=\"Save\">");
            client.println("</form>");

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

void saveFeedDuration(String data) {
  // Convert the data to unsigned long
  unsigned long newDuration = data.toInt();

  // Only update the feed duration if the new value is valid
  if (newDuration > 0) {
    // Write the new feed duration to EEPROM
    EEPROM.begin(sizeof(feedDuration));
    EEPROM.put(0, newDuration);
    EEPROM.commit();
    EEPROM.end();

    // Update the feedDuration variable
    feedDuration = newDuration;
  }
}

void saveFeedHours(String hour1Data, String hour2Data) {
  // Convert the data to integers
  int newHour1 = hour1Data.toInt();
  int newHour2 = hour2Data.toInt();

  // Only update the feed hours if the new values are valid
  if (newHour1 >= 0 && newHour1 < 24 && newHour2 >= 0 && newHour2 < 24) {
    // Write the new feed hours to EEPROM
    EEPROM.begin(sizeof(feedHour1) + sizeof(feedHour2));
    EEPROM.put(sizeof(feedDuration), newHour1);
    EEPROM.put(sizeof(feedDuration) + sizeof(feedHour1), newHour2);
    EEPROM.commit();
    EEPROM.end();

    // Update the feedHour1 and feedHour2 variables
    feedHour1 = newHour1;
    feedHour2 = newHour2;
  }
}

void handleHttpPost() {
  if (header.indexOf("POST /saveduration") >= 0) {
    // Find the start and end of the data in the header
    int start = header.indexOf("duration=");
    int end = header.indexOf(" ", start);
    if (start >= 0 && end >= 0) {
      // Extract the data
      String data = header.substring(start + 9, end);
      // Save the feed duration
      saveFeedDuration(data);
    }
  } else if (header.indexOf("POST /savefeedhours") >= 0) {
    // Find the start and end of the data for feedHour1
    int startHour1 = header.indexOf("feedHour1=");
    int endHour1 = header.indexOf("&", startHour1);
    if (startHour1 >= 0 && endHour1 >= 0) {
      // Extract the data for feedHour1
      String hour1Data = header.substring(startHour1 + 10, endHour1);

      // Find the start and end of the data for feedHour2
      int startHour2 = header.indexOf("feedHour2=");
      int endHour2 = header.indexOf(" ", startHour2);
      if (startHour2 >= 0 && endHour2 >= 0) {
        // Extract the data for feedHour2
        String hour2Data = header.substring(startHour2 + 10, endHour2);

        // Save the feed hours
        saveFeedHours(hour1Data, hour2Data);
      }
    }
  }
}
