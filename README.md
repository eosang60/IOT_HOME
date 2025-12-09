# IoT Smart Home Security System (1인 가구 보안 솔루션)

ESP32와 LilyGo T-Display를 활용하여 구축한 MQTT 기반의 스마트 홈 및 보안 시스템입니다.
본 프로젝트는 1인 가구의 주거 안전을 위해 OTP(One-Time Password)와 QR 코드를 이용한 이중 인증(2FA) 체계를 도입하였으며, 실시간 환경 모니터링 및 가전 제어 기능을 통합적으로 제공합니다.

---

## 1. 프로젝트 개요

- 프로젝트명: IoT Smart Home Security System
- 개발 목적: 주거 침입 범죄 예방을 위한 보안 강화 및 IoT 기술을 활용한 주거 편의성 증대
- 핵심 가치:
  1. 강력한 보안: 하드웨어 기반의 물리적 인증(OTP)을 통과해야만 제어 권한 부여
  2. 실시간성: MQTT 프로토콜을 이용한 저지연 장치 제어 및 센서 데이터 수집
  3. 데이터 시각화: InfluxDB와 Grafana를 활용한 시계열 데이터 저장 및 모니터링

---

## 2. 주요 기능

### 2-1. 보안 및 인증 시스템
- 2단계 인증 (2FA)
  - LilyGo 디바이스에서 생성된 6자리 OTP를 서버와 대조하여 인증된 사용자만 접속 허용
  - 디스플레이에 생성된 QR 코드를 통해 로컬 제어 웹페이지로 즉시 연결
- 침입 감지 및 경보
  - 초음파 센서를 활용한 실시간 출입 인원 카운팅
  - 사용자가 설정한 최대 허용 인원 초과 시 즉시 경보 발령

### 2-2. 스마트 홈 자동화
- 조명 제어: 5개의 개별 방 LED 제어 및 전체 소등/점등 기능
- 환경 제어: DHT22 센서 데이터 기반 가습기 원격 제어
- 출입문 제어: 웹 인터페이스를 통한 서보 모터 원격 개폐

### 2-3. 모니터링 대시보드
- 데이터 수집: 온도, 습도, 재실 인원 데이터를 실시간으로 수집하여 InfluxDB에 저장
- 시각화: Grafana 연동 웹 대시보드를 통해 환경 변화 추이 및 보안 상태 모니터링

---

## 3. 시스템 아키텍처

본 시스템은 MQTT (Publish/Subscribe) 모델을 기반으로 설계되어 장치 간 결합도를 낮추고 확장성을 확보했습니다.

| 장치 구분 | 역할 | 하드웨어 | 주요 부품 |
| :--- | :--- | :--- | :--- |
| 인증 노드 | 사용자 인증, QR 생성 | LilyGo T-Display S3 | TFT LCD, Buttons |
| 제어 노드 | 가전 제어, 환경 감지 | ESP32 Dev Module | DHT22, LEDs, Servo, Relay |
| 센서 노드 | 출입 인원 감지 | ESP32 / LilyGo | Ultrasonic (HC-SR04) x2 |
| 서버 | 데이터 처리, 웹 호스팅 | Raspberry Pi / PC | Python, MQTT Broker |

---

## 4. 통신 프로토콜 (MQTT Topic Map)

모든 데이터는 JSON 포맷을 사용하여 데이터 무결성과 확장성을 보장합니다.

| 토픽 경로 | 타입 | Payload 예시 | 설명 |
| :--- | :--- | :--- | :--- |
| home/otp | Pub | {"otp": 829102} | 생성된 OTP를 서버로 전송 |
| home/sensor/data | Pub | {"temp": "24.5", "humi": "60"} | 온습도 센서 데이터 발행 |
| home/lighting/command | Sub | {"led": 1, "status": "on"} | 조명 제어 (개별/전체) |
| home/humidifier/command | Sub | {"status": "on"} | 가습기 전원 제어 |
| home/servo/command | Sub | {"command": "on"} | 도어락 제어 (on=Open) |
| home/security/status | Pub | {"people_count": 2} | 현재 재실 인원 보고 |
| home/security/command | Sub | {"command": "blink"} | 보안 경고 발생 알림 |

---

## 5. 기술 스택

### Firmware
- Language: C++ (Arduino Framework / PlatformIO)
- Libraries: TFT_eSPI, PubSubClient, ArduinoJson, DHT sensor library, ESP32Servo

### Backend & Server
- Language: Python 3.x
- Modules: http.server (Web), paho-mqtt (MQTT Client), requests (DB Interface)

### Data & Frontend
- Database: InfluxDB (Time-series Database)
- Visualization: Grafana
- Web: HTML5, JavaScript (Fetch API), Milligram CSS

---

## 6. 설치 및 실행 방법

### 6-1. 인프라 설정
- MQTT Broker (Mosquitto) 및 InfluxDB 서비스가 실행 중이어야 합니다.
- server.py 및 펌웨어 코드 내의 MQTT_IP를 본인의 네트워크 환경에 맞게 수정합니다.

### 6-2. 서버 실행
# Python 라이브러리 설치
pip install paho-mqtt requests

# 서버 시작
python server.py

### 6-3. 펌웨어 업로드
- Entry Node (LilyGo): Firmware_Entry 폴더의 코드를 업로드
- Main Controller (ESP32): Firmware_Main 폴더의 코드를 업로드
- Sensor Node: Firmware_Sensor 폴더의 코드를 업로드

### 6-4. 시스템 사용 순서
1. 인증: LilyGo의 버튼 1을 눌러 QR 코드를 생성하고 접속합니다.
2. OTP 발급: 버튼 2를 눌러 OTP를 생성합니다.
3. 접속: 웹 페이지에 OTP를 입력하여 인증을 완료합니다.
4. 제어: 대시보드에서 조명, 문, 가습기를 제어하고 실시간 상태를 확인합니다.

---

## 7. 폴더 구조
-IOT_SMART_HOME/
-├── IOT_HOME.mp4          # 프로젝트 시연 영상
-├── README.md             # 프로젝트 설명서
-├── server.py             # 메인 서버 소스코드 (Python)
-├── otp_liliygo/          # [인증 노드] OTP 및 QR 생성 (LilyGo)
-├── esp32/                # [제어 노드] 가전 제어 및 환경 감지 (ESP32)
-└── SECURITY_LILYGO/      # [센서 노드] 인원 감지 및 보안 시스템 (LilyGo)
