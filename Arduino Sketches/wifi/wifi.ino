#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "Frono95";
const char *password = "9167228544";
const char *serverIp = "192.168.86.33";  // Replace with your computer's local IP
const int serverPort = 5000;

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Define the URL
        String url = "http://" + String(serverIp) + ":" + String(serverPort) + "/test";
        http.begin(url);

        // Specify content-type header
        http.addHeader("Content-Type", "application/json");

        // Prepare the JSON payload
        String payload = "{\"key\":\"value\"}";

        // Make a POST request
        int httpResponseCode = http.POST(payload);

        // Check the returning code
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println(httpResponseCode);
            Serial.println(response);
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        // Free resources
        http.end();
    }
    delay(5000); // Send a request every 5 seconds
}