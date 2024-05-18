#include <driver/i2s.h>
#include <Arduino.h>
#include <WiFi.h>

// Microphone and Speaker settings
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define BUFFER_LEN 64
#define MAX_RECORDING_TIME 1
#define MAX_BUFFER (SAMPLE_RATE * MAX_RECORDING_TIME)

// Microphone pins
#define MIC_I2S_WS 26
#define MIC_I2S_SD 25
#define MIC_I2S_SCK 18

// Speaker pins
#define SPK_I2S_WS 22
#define SPK_I2S_SD 23
#define SPK_I2S_SCK 19

// LED and Button Pins
const int redLedPin = 2;
const int greenLedPin = 5;
const int buttonPin = 13;

// WiFi and API configuration
const char* ssid = "Frono95";
const char* password = "9167228544";
const char* localApiUrl = "192.168.86.33";

// Audio buffers
int16_t* playbackBuffer;
size_t recordIndex = 0;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize LED and Button Pins
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Initialize I2S for microphone and speaker
  i2s_install();
  i2s_setpin_mic();
  i2s_setpin_speaker();

  // Turn on Red LED
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, LOW);

  // Allocate memory for the playback buffer
  playbackBuffer = (int16_t*)malloc(MAX_BUFFER * sizeof(int16_t));
}
void loop() {
  int buttonState = digitalRead(buttonPin);

  // Recording
  if (buttonState == LOW) {
    Serial.println("Recording...");
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);

    int16_t microphoneBuffer[BUFFER_LEN]; // Moved inside loop to save memory
    size_t bytesRead;
    i2s_read(I2S_PORT, microphoneBuffer, sizeof(microphoneBuffer), &bytesRead, portMAX_DELAY);

    // Store the recorded data for later playback
    for (size_t i = 0; i < bytesRead / sizeof(int16_t); i++) {
      if (recordIndex >= MAX_BUFFER) {
        Serial.println("Reached maximum recording time.");
        break;
      }
      playbackBuffer[recordIndex++] = microphoneBuffer[i];
    }

    // Upload audio to your locally hosted API
    String transcribedText = sendAudioToAPI();
    Serial.println("Transcribed Text: " + transcribedText);
  } 
  // Playback
  else if (recordIndex > 0) {
    Serial.println("Playback...");
    digitalWrite(redLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);

    size_t bytesWritten;
    i2s_write(I2S_PORT, playbackBuffer, recordIndex * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    
    // Clear the recording for next time
    memset(playbackBuffer, 0, sizeof(playbackBuffer));
    recordIndex = 0;
  }
}


void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t) BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_LEN
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin_mic() {
  const i2s_pin_config_t pin_config_mic = {
    .bck_io_num = MIC_I2S_SCK,
    .ws_io_num = MIC_I2S_WS,
    .data_out_num = -1,
    .data_in_num = MIC_I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config_mic);
}

void i2s_setpin_speaker() {
  const i2s_pin_config_t pin_config_spk = {
    .bck_io_num = SPK_I2S_SCK,
    .ws_io_num = SPK_I2S_WS,
    .data_out_num = SPK_I2S_SD,
    .data_in_num = -1
  };
  i2s_set_pin(I2S_PORT, &pin_config_spk);
}

String sendAudioToAPI() {
  // Create a WiFi client to send data
  WiFiClient client;

  // Convert localApiUrl to a String object
  String apiUrl = String(localApiUrl);

  // Parse the apiUrl to extract host and path
  String host = apiUrl;
  String path = "/transcribe";

  // Connect to the server
  if (client.connect(host.c_str(), 5000)) {
    Serial.println("Connected to server");

    // Prepare the HTTP POST request
    client.print("POST " + path + " HTTP/1.1\r\n");
    client.print("Host: " + host + "\r\n");
    client.print("Content-Type: audio/wav\r\n");
    client.print("Content-Length: " + String(recordIndex * sizeof(int16_t)) + "\r\n");
    client.print("\r\n");

    // Send the audio data
    client.write((const uint8_t *)playbackBuffer, recordIndex * sizeof(int16_t));

    // Wait for the response
    while (!client.available()) {
      delay(10);
    }

    // Read the response
    String response = client.readString();
    Serial.println("Server response: " + response);

    // Extract and return the transcribed text
    int textStart = response.indexOf("\r\n\r\n") + 4;
    return response.substring(textStart);
  } else {
    Serial.println("Failed to connect to server");
    return "Error connecting to server";
  }
}




void cleanup() {
  // Free dynamically allocated memory when done
  free(playbackBuffer);
}
