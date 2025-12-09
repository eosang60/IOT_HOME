// LilyGo main.cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <qrcode_espi.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

// ------------------ TFT 디스플레이 및 QR 코드 객체 생성 ------------------
TFT_eSPI display = TFT_eSPI();
QRcode_eSPI qrcode(&display);

// ------------------ 핀 정의 ------------------
// 여기서는 GPIO 1, 2번 핀을 버튼으로 사용하고 있음
int button1Pin = 1;  // 버튼 1 핀
int button2Pin = 2;  // 버튼 2 핀

// ------------------ WiFi 및 MQTT 설정 ------------------
const char* wifiSSID = "iPhone (76)";   // WiFi SSID
const char* wifiPassword = "djgudwls";  // WiFi 비밀번호
const char* mqttServer = "172.20.10.2"; // MQTT 브로커 IP
const int mqttPort = 1883;              // MQTT 브로커 포트

// MQTT로 OTP 발행용 토픽
const char* mqtt_topic_publish_otp = "home/otp";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool initialScreenShown = false; // 초기 화면 출력 여부
int generatedOTP = 0;            // 생성된 OTP 번호

// ------------------ WiFi 연결 함수 ------------------
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifiSSID);

  // WiFi에 연결 시도
  WiFi.begin(wifiSSID, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// ------------------ MQTT 재연결 함수 ------------------
void reconnect() {
  // MQTT 서버에 연결될 때까지 반복
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // "LilyGoClient"라는 이름으로 MQTT 브로커에 접속 시도
    if (mqttClient.connect("LilyGoClient")) {
      Serial.println("connected");
      // 현재는 OTP 발행 토픽만 사용하므로, 필요 시 다른 토픽 구독 가능
      mqttClient.subscribe(mqtt_topic_publish_otp);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" trying again in 5 seconds");
      // 연결 실패 시 5초 대기 후 재시도
      delay(5000);
    }
  }
}

// ------------------ MQTT 메시지 콜백 함수 ------------------
// 현재는 별도로 처리할 메시지가 없으나, 추후 확장 가능
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// ------------------ 초기 설정 함수 ------------------
void setup() {
  Serial.begin(115200);
  delay(500);  // 시리얼 안정화

  // 버튼 핀 모드 설정 (내부 풀업)
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);

  // 디스플레이 초기화
  display.begin();
  qrcode.init();  // QR 코드 표시용 객체 초기화
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.println("SMART HOME");
  initialScreenShown = true;

  // WiFi 연결
  setup_wifi();

  // MQTT 설정 및 연결
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);
  reconnect();
}

// ------------------ QR 코드 표시 함수 ------------------
void showQRCode() {
  display.fillScreen(TFT_BLACK);
  // QR 코드에 표시할 URL
  char msg[] = "http://172.20.10.2:8000/";
  qrcode.create(msg);
  Serial.println("QR Code displayed!");
}

// ------------------ OTP 생성 및 MQTT 발행 함수 ------------------
void showOTP() {
  display.fillScreen(TFT_BLACK);
  // 6자리 OTP 생성 (000000 ~ 999999)
  generatedOTP = random(000000, 999999);
  display.setTextSize(2);
  display.setCursor(20, 50);
  display.print("OTP: ");
  display.println(generatedOTP);
  Serial.print("Generated OTP: ");
  Serial.println(generatedOTP);

  // MQTT로 OTP 발행
  if (!mqttClient.connected()) {
    reconnect();
  }
  StaticJsonDocument<200> otpDoc;
  otpDoc["otp"] = generatedOTP;
  char jsonBuffer[512];
  serializeJson(otpDoc, jsonBuffer);
  mqttClient.publish(mqtt_topic_publish_otp, jsonBuffer);
  Serial.println("OTP published to MQTT");
}

// ------------------ 메인 루프 함수 ------------------
void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  if (!initialScreenShown) {
    // 초기 화면이 표시되지 않았다면 표시
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.println("SMART HOME");
    initialScreenShown = true;
  }

  // 버튼 1 누를 시 QR 코드 표시
  if (digitalRead(button1Pin) == LOW) {
    showQRCode();
    // 버튼 해제 기다림(디바운싱)
    while (digitalRead(button1Pin) == LOW) {
      delay(10);
    }
    delay(500);
  }

  // 버튼 2 누를 시 OTP 생성 및 표시
  if (digitalRead(button2Pin) == LOW) {
    showOTP();
    // 버튼 해제 기다림(디바운싱)
    while (digitalRead(button2Pin) == LOW) {
      delay(10);
    }
    delay(500);
  }
}
