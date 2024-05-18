#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

// WiFi credentials
const char* ssid = "Frono95";
const char* password = "9167228544";

// I2S configuration for your speaker
i2s_pin_config_t pin_config = {
  .bck_io_num = 32,  // BCK (Bit Clock)
  .ws_io_num = 33,   // LCK (Word Select or Left/Right Clock)
  .data_out_num = 27, // DIN (Data Input)
  .data_in_num = I2S_PIN_NO_CHANGE
};

#define I2S_PORT   I2S_NUM_0
#define SAMPLE_RATE 44100


String inputString = ""; // a string to hold incoming data from serial
bool stringComplete = false; // whether the string is complete

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  
  i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX), // Master and transmitter
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo
    .communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true, // Auto clear DMA buffer
};

  size_t bytesWritten;

  
  Serial.println("Enter a string and press enter:");
}

void loop() {
  if (stringComplete) {
    sendStringToAPI(inputString);
    inputString = "";
    stringComplete = false;
  }
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

void sendStringToAPI(String data) {
  HTTPClient http;
  
  http.begin("http://127.0.0.1:5000/response"); // Replace with your server's URL
  http.addHeader("Content-Type", "text/plain");
  
  int httpResponseCode = http.POST(data);
  
  if(httpResponseCode > 0){
    String response = http.getString();
    playResponseOnSpeaker(response);
  }
  
  http.end();
}

void playResponseOnSpeaker(String response) {
  // Assuming the Flask API sends back raw PCM data
  int len = response.length();
  for (int i = 0; i < len; i+=2) {
    int16_t sample = ((response[i] & 0xFF) | ((response[i+1] & 0xFF) << 8)); // Assuming 16-bit samples
size_t bytesWritten;
i2s_write(I2S_PORT, (const char*)&sample, 2, &bytesWritten, portMAX_DELAY);

  }
}
