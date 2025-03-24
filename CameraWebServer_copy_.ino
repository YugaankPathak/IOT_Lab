#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>  // Added for HTTP client functionality

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#define CAPTURE_BUTTON_PIN 12
#include "camera_pins.h"
#include "esp_http_server.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Skynet";
const char *password = "Mojushibu1";

// Define the laptop server URL that will receive the image
const char* laptopURL = "http://192.168.29.61:8080/upload";

void startCameraServer();
void setupLedFlash(int pin);

//-------------------------------------------------
// Function: sendImageToLaptop()
// - Captures an image from the camera,
// - Converts it to JPEG if needed (since the current pixel_format is RGB565),
// - And sends it via HTTP POST to your laptop.
//-------------------------------------------------
void sendImageToLaptop() {
  Serial.println("Capturing image...");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  uint8_t *jpg_buf = NULL;
  size_t jpg_buf_len = 0;
  // Our camera is set to RGB565 for face detection, so convert to JPEG.
  // Quality value (e.g., 80) can be adjusted as needed.
  bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
  esp_camera_fb_return(fb);

  if (!jpeg_converted) {
    Serial.println("JPEG conversion failed");
    return;
  }

  // Send the JPEG image via HTTP POST
  HTTPClient http;
  Serial.println("Connecting to laptop server...");

  http.begin(laptopURL);  // Specify your laptop server URL
  http.addHeader("Content-Type", "image/jpeg");
  
  int httpResponseCode = http.POST(jpg_buf, jpg_buf_len);
  if(httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    Serial.println("Response: " + response);
  } else {
    Serial.printf("Error on sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();

  free(jpg_buf);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(CAPTURE_BUTTON_PIN, INPUT_PULLUP);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  // For streaming you might use JPEG, but here we are using RGB565 for face detection.
  config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // Check if the capture button is pressed (active LOW)
  if (digitalRead(CAPTURE_BUTTON_PIN) == LOW) {
    // Debounce delay
    delay(50);
    // Wait for button release to avoid multiple triggers
    while (digitalRead(CAPTURE_BUTTON_PIN) == LOW) {
      delay(10);
    }
    Serial.println("Capture button pressed: Capturing and sending image...");
    sendImageToLaptop();
    // Small delay to avoid bouncing
    delay(500);
  }
  
  // You can perform other tasks here if needed
  delay(100);
}
