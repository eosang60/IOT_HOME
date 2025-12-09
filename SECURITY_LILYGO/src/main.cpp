#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

// ------------------ TFT 디스플레이 객체 생성 ------------------
TFT_eSPI display = TFT_eSPI();

// ------------------ 초음파 센서 핀 정의 ------------------
const int trigPin = 43;
const int echoPin = 44; 
const int trigPin2 = 18;
const int echoPin2 = 17;

long duration1 = 0, duration2 = 0;    // 초음파 신호 왕복 시간 변수
float distance1 = 0, distance2 = 0;   // 센서로부터 측정된 거리(cm)
int peopleCount = 0;                  // 현재 인원 수
int previousPeopleCount = -1;         // 이전 인원수 (변화 감지용)
int maxPeopleAllowed = 0;             // 설정된 최대 인원 수

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastSensorCheck = 0;           // 마지막으로 센서 체크한 시간(ms)
const unsigned long sensorInterval = 50;     // 초음파 센서 체크 주기(ms)
const float distanceThreshold = 50.0;        // 센서 거리 판정 임계값(cm)
const unsigned long maxStateDuration = 2000; // 상태 유지 최대 시간(ms) (미사용 변수이지만 원문 유지)
unsigned long lastWarningBlink = 0;          // 경고 깜빡임 시간 저장 변수
const unsigned long warningBlinkInterval = 500; // 경고 깜빡임 간격(ms)
bool warningState = false;                   // 경고 상태 플래그

// 메시지 발행 타이머 변수
unsigned long lastPublishTime = 0;            // 마지막 메시지 발행 시간
const unsigned long publishInterval = 5000;  // 메시지 발행 간격 (15초)

// ------------------ WiFi 및 MQTT 설정 ------------------
const char* wifiSSID = "iPhone (76)";      // 연결할 WiFi SSID
const char* wifiPassword = "djgudwls";     // WiFi 비밀번호
const char* mqttServer = "172.20.10.2";    // MQTT 브로커 IP
const int mqttPort = 1883;                 // MQTT 브로커 포트

// MQTT 토픽 정의
const char* mqtt_topic_subscribe_security = "home/security/count"; // 최대 인원 수 수신 토픽

const char* mqtt_topic_publish_security_status = "home/security/status";   // 현재 인원수 상태 발행 토픽
const char* mqtt_topic_publish_security_warning = "home/security/command"; // 경보(LED 점멸 등) 발행 토픽

// ------------------ 사람 수 디스플레이 업데이트 함수 ------------------
void updatePeopleCountOnDisplay() {
  // 디스플레이 특정 영역을 검은색으로 지우고 새 정보 출력
  display.fillRect(20, 80, 240, 60, TFT_BLACK); 
  display.setCursor(20, 80);
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.print("MAX : ");
  display.println(maxPeopleAllowed);
  display.setCursor(20, 110);
  display.print("CURRENT : ");
  display.println(peopleCount);
}

// ------------------ WiFi 연결 함수 ------------------
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifiSSID);

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
  // MQTT 브로커에 연결될 때까지 재시도
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // "LilyGoClient"라는 클라이언트 ID로 접속 시도
    if (mqttClient.connect("LilyGoClient")) {
      Serial.println("connected");
      // 연결 성공 시 최대 인원 수 설정 토픽 구독
      mqttClient.subscribe(mqtt_topic_subscribe_security);
    } else {
      // 실패 시 오류 코드 출력 후 5초 대기 후 재시도
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

// ------------------ MQTT 메시지 콜백 함수 ------------------
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char*)payload);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // 최대 인원수 설정 토픽 수신 처리
  if (String(topic) == mqtt_topic_subscribe_security) {
    DynamicJsonDocument jsonDoc(200);
    DeserializationError error = deserializeJson(jsonDoc, message);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.f_str());
      return;
    }
    // JSON에서 max_people 값 추출
    maxPeopleAllowed = jsonDoc["max_people"];
    Serial.print("Maximum people allowed set to: ");
    Serial.println(maxPeopleAllowed);

    // 최대 인원수 변경 시 디스플레이 업데이트
    updatePeopleCountOnDisplay();
  }
}

// ------------------ 초기 설정 함수 ------------------
void setup() {
  Serial.begin(115200);
  delay(500); // 시리얼 통신 안정화를 위한 대기

  // 초음파 센서 핀 모드 설정
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);

  // 디스플레이 초기화 및 초기 화면 출력
  display.begin();
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.println("SMART HOME");

  // WiFi 연결
  setup_wifi();

  // MQTT 클라이언트 설정
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  // MQTT 연결 시도
  reconnect();
}

// ------------------ 메인 루프 함수 ------------------
void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  unsigned long currentMillis = millis();

  // ------------------ 초음파 센서로 사람 수 검출 ------------------
  if (currentMillis - lastSensorCheck >= sensorInterval) {
    lastSensorCheck = currentMillis;

    // 첫 번째 센서 측정(입구)
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration1 = pulseIn(echoPin, HIGH, 30000); // 타임아웃 30ms
    distance1 = duration1 * 0.017; // 거리 계산

    // 두 번째 센서 측정(출구)
    digitalWrite(trigPin2, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin2, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin2, LOW);
    duration2 = pulseIn(echoPin2, HIGH, 30000);
    distance2 = duration2 * 0.017;

    // 사람이 1번 센서 -> 2번 센서 방향으로 이동한 경우(입장)
    if (distance1 > 0 && distance1 < 50) {
      delay(200); // 방향 확인 위해 대기
      digitalWrite(trigPin2, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin2, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin2, LOW);
      
      duration2 = pulseIn(echoPin2, HIGH, 30000);
      distance2 = duration2 * 0.017;

      if (distance2 > 0 && distance2 < 50) {
        Serial.println("1번 -> 2번으로 이동: 사람 들어옴");
        peopleCount++;
        delay(500); // 중복 카운트 방지
        updatePeopleCountOnDisplay();
      }
    }

    // 사람이 2번 센서 -> 1번 센서 방향으로 이동한 경우(퇴장)
    if (distance2 > 0 && distance2 < 50) {
      delay(200); // 방향 확인 위해 대기
      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);

      duration1 = pulseIn(echoPin, HIGH, 30000);
      distance1 = duration1 * 0.017;

      if (distance1 > 0 && distance1 < 50) {
        Serial.println("2번 -> 1번으로 이동: 사람 나감");
        peopleCount--;
        if (peopleCount < 0) peopleCount = 0; // 음수 방지
        delay(500); // 중복 카운트 방지
        updatePeopleCountOnDisplay();
      }
    }

    // ------------------ 인원 수 변화 시 즉시 MQTT로 상태 전송 ------------------
    if (peopleCount != previousPeopleCount) {
      previousPeopleCount = peopleCount;

      DynamicJsonDocument jsonDoc(200);
      jsonDoc["people_count"] = peopleCount;
      jsonDoc["max_people_allowed"] = maxPeopleAllowed;
      char buffer[256];
      serializeJson(jsonDoc, buffer);
      mqttClient.publish(mqtt_topic_publish_security_status, buffer);

      Serial.printf("Published updated people count: %d\n", peopleCount);
      lastPublishTime = currentMillis; // 마지막 발행 시간 갱신
    }
    // 인원 수 변경이 없으면 일정 주기마다 상태 전송
    else if (currentMillis - lastPublishTime >= publishInterval) {
      lastPublishTime = currentMillis;
      DynamicJsonDocument jsonDoc(200);
      jsonDoc["people_count"] = peopleCount;
      jsonDoc["max_people_allowed"] = maxPeopleAllowed;
      char buffer[256];
      serializeJson(jsonDoc, buffer);
      mqttClient.publish(mqtt_topic_publish_security_status, buffer);

      Serial.printf("Published periodic people count: %d\n", peopleCount);
    }
  }

  // ------------------ 최대 인원 초과 시 경고 처리 ------------------
  if (peopleCount > maxPeopleAllowed && maxPeopleAllowed > 0) {
    // 경고 상태: 일정 간격으로 디스플레이에 경고 표시 깜빡임
    if (currentMillis - lastWarningBlink >= warningBlinkInterval) {
      lastWarningBlink = currentMillis;
      warningState = !warningState;
      if (warningState) {
        // 경고 시 빨간 화면에 WARNING!
        display.fillScreen(TFT_RED);
        display.setTextColor(TFT_WHITE, TFT_RED);
        display.setTextSize(3);
        display.setCursor(20, 60);
        display.println("WARNING!");
      } else {
        // 깜빡임 상태가 꺼질 때 다시 인원 표시 화면으로 복귀
        updatePeopleCountOnDisplay();
      }
    }
    // MQTT로 경고 명령 발행 (LED 점멸 등)
    mqttClient.publish(mqtt_topic_publish_security_warning, "{\"command\": \"blink\"}");
  } else {
    // 최대 인원 초과가 아닌 경우
    if (warningState) {
      // 이전에 경고 상태였다면 원래 화면 복원
      warningState = false;
      updatePeopleCountOnDisplay();
    }
  }
}
