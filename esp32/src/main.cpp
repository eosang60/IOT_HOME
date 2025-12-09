#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>  // 서보 모터 라이브러리 추가

// ------------------ WiFi 설정 ------------------
const char* ssid = "iPhone (76)";   // 접속할 WiFi SSID
const char* password = "djgudwls";  // WiFi 비밀번호

// ------------------ MQTT 브로커 설정 ------------------
const char* mqtt_server = "172.20.10.2";  // MQTT 브로커 IP
const int mqtt_port = 1883;               // MQTT 브로커 포트

// MQTT 구독(수신) 토픽
const char* mqtt_topic_subscribe_lighting = "home/lighting/command";     // LED 제어 수신 토픽
const char* mqtt_topic_subscribe_humidifier = "home/humidifier/command"; // 가습기 제어 수신 토픽
const char* mqtt_topic_subscribe_servo = "home/servo/command";           // 서보 모터 제어 명령 수신 토픽
const char* mqtt_topic_subscribe_security_warn = "home/security/command"; // 초음파 센서 경고 메시지 수신 토픽

// MQTT 발행(송신) 토픽
const char* mqtt_topic_publish_lighting = "home/lighting/status";     // LED 상태 발행 토픽
const char* mqtt_topic_publish_humidifier = "home/humidifier/status"; // 가습기 상태 발행 토픽
const char* mqtt_topic_publish_sensor = "home/sensor/data";           // 온습도 데이터 발행 토픽
const char* mqtt_topic_publish_servo_status = "home/servo/status";    // 서보 상태 발행 토픽

// ------------------ 핀 설정 ------------------
const int ledPins[] = {4, 5, 18, 19, 21}; // 제어할 LED의 핀 번호 배열
#define DHTPIN 22            // DHT22 센서 핀 번호
#define DHTTYPE DHT22        // DHT 타입(DHT22)
#define HUMIDIFIER_PIN 23    // 가습기 제어 핀
#define SERVO_PIN 15         // 서보 모터 핀

DHT dht(DHTPIN, DHTTYPE);   
Servo myServo;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastDHTReadMillis = 0; // 마지막으로 DHT22를 읽은 시간
int interval = 2000;                 // DHT22를 2초마다 읽기
float humidity = 0;                  // 읽은 습도 값 저장
float temperature = 0;               // 읽은 온도 값 저장

// ------------------ WiFi 연결 함수 ------------------
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    // WiFi에 연결 시도
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // mDNS 서비스 시작
    if (!MDNS.begin("thermoSensor")) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        Serial.println("mDNS responder started");
    }
}

// ------------------ 서보 상태 발행 함수 ------------------
void publishServoStatus(const char* status) {
    // 서보 모터 상태("on"/"off")를 MQTT로 발행
    client.publish(mqtt_topic_publish_servo_status, status);
}

// ------------------ DHT22 센서 읽기 함수 ------------------
void readDHT22() {
    unsigned long currentMillis = millis();
    // interval(2초)마다 한 번씩 DHT22 센서를 읽는다.
    if (currentMillis - lastDHTReadMillis >= interval) {
        lastDHTReadMillis = currentMillis;
        humidity = dht.readHumidity();       // 습도 읽기
        temperature = dht.readTemperature(); // 온도 읽기

        if (!isnan(humidity) && !isnan(temperature)) {
            // NaN이 아닐 경우만 유효한 데이터
            char temperatureStr[10];
            char humidityStr[10];
            dtostrf(temperature, 4, 2, temperatureStr); // 온도값 문자열 변환
            dtostrf(humidity, 4, 2, humidityStr);       // 습도값 문자열 변환

            // JSON 형태로 데이터 구성 후 MQTT 발행
            StaticJsonDocument<200> jsonDoc;
            jsonDoc["temperature"] = temperatureStr;
            jsonDoc["humidity"] = humidityStr;
            char buffer[256];
            serializeJson(jsonDoc, buffer);
            client.publish(mqtt_topic_publish_sensor, buffer);
        }
    }
}

// ------------------ MQTT 메시지 콜백 함수 ------------------
void callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0'; // 수신 데이터 끝에 널 문자 추가해 문자열로 변환
    String message = String((char*)payload);

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);

    StaticJsonDocument<256> doc;
    // 수신한 메시지를 JSON으로 파싱
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
    }

    // ------------------ 조명 제어 처리 ------------------
    if (strcmp(topic, mqtt_topic_subscribe_lighting) == 0) {
        if (doc.containsKey("led") && doc.containsKey("status")) {
            // 특정 LED 제어
            int ledIndex = doc["led"].as<int>() - 1; // led=1이면 index=0
            String status = doc["status"].as<String>();
            if (ledIndex >= 0 && ledIndex < (int)(sizeof(ledPins) / sizeof(ledPins[0]))) {
                // "on"/"off"에 따라 LED 핀 상태 설정
                if (status == "on") {
                    digitalWrite(ledPins[ledIndex], HIGH);
                } else if (status == "off") {
                    digitalWrite(ledPins[ledIndex], LOW);
                }
                // 현재 LED 상태 MQTT 발행
                StaticJsonDocument<200> statusDoc;
                statusDoc["led"] = ledIndex + 1;
                statusDoc["status"] = (digitalRead(ledPins[ledIndex]) == HIGH) ? "on" : "off";
                char statusBuffer[256];
                serializeJson(statusDoc, statusBuffer);
                client.publish(mqtt_topic_publish_lighting, statusBuffer);
            }
        } else if (doc.containsKey("status")) {
            // 모든 LED에 대한 일괄 제어
            String status = doc["status"].as<String>();
            int newStatus = (status == "on") ? HIGH : LOW;
            for (int i = 0; i < (int)(sizeof(ledPins) / sizeof(ledPins[0])); i++) {
                digitalWrite(ledPins[i], newStatus);
            }
            // 전체 LED 상태 MQTT 발행
            StaticJsonDocument<200> statusDoc;
            statusDoc["status"] = status;
            char statusBuffer[256];
            serializeJson(statusDoc, statusBuffer);
            client.publish(mqtt_topic_publish_lighting, statusBuffer);
        }

    // ------------------ 가습기 제어 처리 ------------------
    } else if (strcmp(topic, mqtt_topic_subscribe_humidifier) == 0) {
        if (doc.containsKey("status")) {
            String status = doc["status"].as<String>();
            // "on"/"off"에 따라 가습기 핀 상태 설정
            if (status == "on") {
                digitalWrite(HUMIDIFIER_PIN, HIGH);
            } else if (status == "off") {
                digitalWrite(HUMIDIFIER_PIN, LOW);
            }
            // 현재 가습기 상태 MQTT 발행
            StaticJsonDocument<200> statusDoc;
            statusDoc["status"] = status;
            char statusBuffer[256];
            serializeJson(statusDoc, statusBuffer);
            client.publish(mqtt_topic_publish_humidifier, statusBuffer);
        }

    // ------------------ 서보 모터 제어 처리 ------------------
    } else if (strcmp(topic, mqtt_topic_subscribe_servo) == 0) {
        if (doc.containsKey("command")) {
            String command = doc["command"].as<String>();
            // "on"이면 문 열기(서보 90도), "off"이면 문 닫기(서보 0도)
            if (command == "on") {
                myServo.write(90);
                Serial.println("Servo: Door Open");
                publishServoStatus("on");
            } else if (command == "off") {
                myServo.write(0);
                Serial.println("Servo: Door Closed");
                publishServoStatus("off");
            }
        }

    // ------------------ 경고 메시지 수신 처리 ------------------
    } else if (strcmp(topic, mqtt_topic_subscribe_security_warn) == 0) {
        // 경고 수신 시 모든 LED 깜빡이는 동작
        if (doc.containsKey("command")){
            String command = doc["command"].as<String>();
            if (command == "blink") {
                for (int i = 0; i < 5; i++) { // 5회 깜빡임
                    for (int j = 0; j < (int)(sizeof(ledPins) / sizeof(ledPins[0])); j++) {
                        digitalWrite(ledPins[j], HIGH);
                    }
                    delay(500); // 500ms 켜기
                    for (int j = 0; j < (int)(sizeof(ledPins) / sizeof(ledPins[0])); j++) {
                        digitalWrite(ledPins[j], LOW);
                    }
                    delay(500); // 500ms 끄기
                }
            }
        }
    }
}

// ------------------ MQTT 재연결 함수 ------------------
void reconnect() {
    // MQTT 브로커에 연결될 때까지 시도
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // "ESP32Client" 이름으로 접속 시도
        if (client.connect("ESP32Client")) {
            Serial.println("connected");
            // 접속 성공 시 필요한 토픽들 구독
            client.subscribe(mqtt_topic_subscribe_lighting);
            client.subscribe(mqtt_topic_subscribe_humidifier);
            client.subscribe(mqtt_topic_subscribe_servo);
            client.subscribe(mqtt_topic_subscribe_security_warn);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // 실패 시 5초 후 재시도
            delay(5000);
        }
    }
}

// ------------------ 초기 설정 함수(Setup) ------------------
void setup() {
    Serial.begin(115200); // 시리얼 통신 시작
    setup_wifi();          // WiFi 연결

    dht.begin();           // DHT22 센서 초기화

    // LED 핀 출력 모드 및 초기 OFF 설정
    for (int i = 0; i < (int)(sizeof(ledPins) / sizeof(ledPins[0])); i++) {
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW);
    }

    // 가습기 핀 출력 모드 및 초기 OFF 설정
    pinMode(HUMIDIFIER_PIN, OUTPUT);
    digitalWrite(HUMIDIFIER_PIN, LOW);

    // 서보 모터 초기화 (0도로 문 닫기 상태)
    myServo.attach(SERVO_PIN);
    myServo.write(0);

    // MQTT 설정
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

// ------------------ 메인 루프 함수(Loop) ------------------
void loop() {
    // MQTT 연결 확인 및 재연결 처리
    if (!client.connected()) {
        reconnect();
    }
    client.loop();   // MQTT 클라이언트 루프
    readDHT22();     // 주기적으로 DHT22 센서 데이터 읽기 및 발행
}
