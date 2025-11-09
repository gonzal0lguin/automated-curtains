#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h> 

const char* ssid = "ssid";
const char* password = "password";
// ---------------------

// Create a web server object on port 80 (standard HTTP port)
WebServer server(80);

/**
 * @brief This function is called when the /open URL is requested
 */
void handleOpenBlinds() {
  Serial.println("TRIGGER RECEIVED: Opening blinds now!");

  // Send a simple response back to the client (your iPhone)
  server.send(200, "text/plain", "OK. Blinds are opening.");
}

void handleCloseBlinds() {
  Serial.println("TRIGGER RECEIVED: Closing blinds now!");

  // Send a simple response back to the client (your iPhone)
  server.send(200, "text/plain", "OK. Blinds are closing.");
}

/**
 * @brief A simple "root" page, just for testing in a browser
 */
void handleRoot() {
  server.send(200, "text/plain", "Hello! This is the Blinds Controller. Visit /open to trigger.");
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect to Wi-Fi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Waiting for intenet connection.");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // --- 2. ADD THIS CODE ---
  // This sets up the name "blinds.local"
  if (!MDNS.begin("blinds")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started at http://blinds.local");
  // -------------------------

  server.on("/open", handleOpenBlinds); 
  server.on("/close", handleCloseBlinds);
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");

  MDNS.addService("http", "tcp", 80); // Announce the web server
  Serial.println("HTTP server started");
}

void loop() {
  // This is required to listen for and handle incoming client requests
  server.handleClient();
}