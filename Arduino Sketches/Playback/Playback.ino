#include <driver/i2s.h>
#include <Arduino.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "Frono95";
const char* password = "9167228544";

// Microphone and Speaker settings
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define BUFFER_LEN 64
#define MAX_RECORDING_TIME 5 // in seconds
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

// Audio buffers
int16_t microphoneBuffer[BUFFER_LEN];
int16_t playbackBuffer[MAX_BUFFER];
size_t recordIndex = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  
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
  
  // Initialize the SPIFFS file system
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // Recording
  if (buttonState == LOW) {
    Serial.println("Recording...");
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);

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

    // Open a file to store the recorded audio
    File file = SPIFFS.open("/audio.wav", "w");
    if(!file){
      Serial.println("Failed to create file");
      return;
    }
    
    // Write the WAV header to the file
    writeWavHeader(file, recordIndex * sizeof(int16_t));

    // Write the raw audio data to the file
    file.write((const uint8_t *)playbackBuffer, recordIndex * sizeof(int16_t));

    file.close();

    // Upload the recorded audio to the Flask server
    uploadAudioToServer("/audio.wav");
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

void writeWavHeader(File file, uint32_t dataSize) {
  const int sampleRate = SAMPLE_RATE;
  const int bitsPerSample = BITS_PER_SAMPLE;
  const int numChannels = 1;  // mono

  uint32_t chunkSize = 36 + dataSize;
  uint32_t subChunk1Size = 16;
  uint16_t audioFormat = 1; // PCM
  uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  uint16_t blockAlign = numChannels * bitsPerSample / 8;
  uint32_t subChunk2Size = dataSize;

  // Write the header fields to the file
  file.write("RIFF");
  file.write((const uint8_t *)&chunkSize, 4);
  file.write("WAVE");
  file.write("fmt ");
  file.write((const uint8_t *)&subChunk1Size, 4);
  file.write((const uint8_t *)&audioFormat, 2);
  file.write((const uint8_t *)&numChannels, 2);
  file.write((const uint8_t *)&sampleRate, 4);
  file.write((const uint8_t *)&byteRate, 4);
  file.write((const uint8_t *)&blockAlign, 2);
  file.write((const uint8_t *)&bitsPerSample, 2);
  file.write("data");
  file.write((const uint8_t *)&subChunk2Size, 4);
}

void uploadAudioToServer(const char* filePath) {
  HTTPClient http;

  http.begin("http://your-flask-api-url/upload"); // Replace with your server's URL
  http.addHeader("Content-Type", "multipart/form-data");
  
  File file = SPIFFS.open(filePath, "r");
  if(!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  http.sendRequest("POST", &file, file.size());
  
  String response = http.getString();
  Serial.println(response); // Print the response to the serial monitor

  file.close();
  http.end();
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
  const i2s_pin_config_t pin_config_speaker = {
    .bck_io_num = SPK_I2S_SCK,
    .ws_io_num = SPK_I2S_WS,
    .data_out_num = SPK_I2S_SD,
    .data_in_num = -1
  };

  i2s_set_pin(I2S_PORT, &pin_config_speaker);
}
