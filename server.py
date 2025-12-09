from http.server import BaseHTTPRequestHandler, HTTPServer
import paho.mqtt.client as mqtt
import requests
import json
from urllib.parse import urlparse, parse_qs

# ------------------ 서버 및 포트 설정 ------------------
hostName = '0.0.0.0'
serverPort = 8000

# ------------------ MQTT 설정 ------------------
# MQTT 브로커 주소 및 포트
mqtt_server = "172.20.10.2"
mqtt_port = 1883

# MQTT 명령 발행 토픽 설정
mqtt_topic_publish_lighting = "home/lighting/command" # LED 제어 명령 발행 토픽
mqtt_topic_publish_humidifier = "home/humidifier/command" # 가습기 ON/OFF 명령 발행 토픽
mqtt_topic_publish_servo = "home/servo/command" # 서보 모터(문 열림/닫힘) 명령 발행 토픽
mqtt_topic_publish_security_count = "home/security/count" # 최대 인원 수 설정 명령 발행 토픽

# MQTT 구독 토픽 설정
mqtt_topic_subscribe_security_warn = "home/security/command"  # 경고 메시지 수신 토픽
mqtt_topic_subscribe_security_status = "home/security/status" # 현재 인원수 상태 수신 토픽
mqtt_topic_subscribe_sensor = "home/sensor/data" # 온/습도 센서 데이터 수신 토픽
mqtt_topic_subscribe_servo = "home/servo/status" # 서보 상태 수신 토픽 (0도/90도 상태)
mqtt_topic_subscribe_otp = "home/otp" # OTP 수신 토픽

# ------------------ InfluxDB 설정 ------------------
influxdb_url = "http://172.20.10.2:8086"
influxdb_token = "7u65tuouGE6xBkac0dt9rLjdMCqVJU6-P3wvHiiUzIkvWB4e_kfvDqyC2H-28k3YLJoVxfM4P4Tqh2E29VYNPw=="
influxdb_bucket = "bucket01"
influxdb_org = "iotlab"

# ------------------ 전역 변수 (상태 저장) ------------------
# 센서 데이터 (온도, 습도)
sensor_data = {
    "temperature": "--",
    "humidity": "--"
}

# 보안 데이터 (현재 인원수, 최대 인원수)
security_data = {
    "people_count": "--",
    "max_people_allowed": "--"    
}

# 조명 상태 (Room1 ~ Room5)
lighting_status = {f"room{room}": "off" for room in range(1, 6)}

# 가습기 상태
humidifier_status = "off"

# OTP 데이터
otp_data = None  # 수신한 OTP를 저장

# ------------------ MQTT 콜백 함수 정의 ------------------
def on_connect(client, userdata, flags, rc):
    # MQTT 브로커 접속 성공 시, 필요 토픽 구독
    print("Connected with RC: " + str(rc))
    client.subscribe([
        (mqtt_topic_subscribe_sensor, 0),
        (mqtt_topic_subscribe_servo, 0),
        (mqtt_topic_subscribe_otp, 0),
        (mqtt_topic_subscribe_security_warn, 0),
        (mqtt_topic_subscribe_security_status, 0)
    ])
def on_message(client, userdata, msg):
    # MQTT 메시지 수신 시 처리
    global sensor_data, otp_data, lighting_status, humidifier_status, security_data
    payload = msg.payload.decode('utf-8')
    topic = msg.topic
    print(f"Received message: {payload} from topic: {topic}")

    if topic == mqtt_topic_subscribe_sensor:
        # 센서 데이터 수신 -> sensor_data 갱신 및 InfluxDB 저장
        try:
            data = json.loads(payload)
            sensor_data["temperature"] = data.get("temperature", "--")
            sensor_data["humidity"] = data.get("humidity", "--")
            send_to_influxdb("ambient", sensor_data)
        except json.JSONDecodeError:
            print("Failed to parse sensor data JSON")

    elif topic == mqtt_topic_subscribe_otp:
        # OTP 데이터 수신 -> otp_data 갱신
        try:
            data = json.loads(payload)
            otp_data = str(data.get("otp", "--"))
            print(f"OTP received: {otp_data}")
        except json.JSONDecodeError:
            print("Failed to parse OTP JSON")

    elif topic == mqtt_topic_subscribe_security_warn:
        # 경고 메시지 수신 시 LED 점멸 명령 발행
        mqtt_client.publish(mqtt_topic_publish_lighting, json.dumps({"command": "blink"}))
        print("Warning received! Sending LED alert command.")

    elif topic == mqtt_topic_subscribe_security_status:
        # 현재 인원수 상태 수신 -> security_data 갱신 및 InfluxDB 저장
        try:
            data1 = json.loads(payload)
            security_data["people_count"] = data1.get("people_count", "--")
            security_data["max_people_allowed"] = data1.get("max_people_allowed", "--")
            send_to_influxdb("security", security_data)
        except json.JSONDecodeError:
            print("Failed to parse security status JSON")

def send_to_influxdb(measurement, fields):
    # InfluxDB로 데이터 전송
    url = f"{influxdb_url}/api/v2/write?bucket={influxdb_bucket}&org={influxdb_org}&precision=s"
    headers = {
        "Authorization": f"Token {influxdb_token}",
        "Content-Type": "text/plain"
    }
    field_set = ",".join([f"{key}={value}" for key, value in fields.items() if value != "--"])
    data = f"{measurement} {field_set}"
    try:
        response = requests.post(url, headers=headers, data=data)
        if response.status_code == 204:
            print("Data written to InfluxDB successfully.")
        else:
            print(f"Failed to write to InfluxDB: {response.status_code} {response.text}")
    except Exception as e:
        print(f"Error sending data to InfluxDB: {str(e)}")

# MQTT 클라이언트 생성 및 이벤트 함수 할당
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(mqtt_server, mqtt_port, 60)
mqtt_client.loop_start()

# ------------------ HTTP 서버 핸들러 정의 ------------------
class MyServer(BaseHTTPRequestHandler):
    def do_GET(self):
        # 기본 페이지 ("/"): OTP 입력 페이지
        if self.path == "/":
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            otp_message = ""
            # OTP 입력용 HTML 폼 제공
            html = f"""
            <html>
            <head>
                <title>Home Automation</title>
                <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/milligram/1.4.1/milligram.min.css">
                <style>
                    body {{
                        max-width: 800px;
                        margin: 0 auto;
                        padding: 20px;
                    }}
                    .input-group {{
                        margin-bottom: 20px;
                    }}
                </style>
            </head>
            <body>
                <h1>Home Automation</h1>
                <div class="input-group">
                    <h2>OTP Verification</h2>
                    <form action="/verify_otp" method="get">
                        <label for="otp">Enter OTP:</label>
                        <input type="text" id="otp" name="otp" required>
                        <input type="submit" value="Verify OTP" class="button-primary">
                    </form>
                    {otp_message}
                </div>
            </body>
            </html>
            """
            self.wfile.write(bytes(html, "utf-8"))

        # "/verify_otp": OTP 검증 처리
        elif self.path.startswith("/verify_otp"):
            qs = urlparse(self.path).query
            params = parse_qs(qs)
            otp_message = ""
            if "otp" in params:
                entered_otp = params["otp"][0]
                if str(entered_otp) == str(otp_data):
                    # OTP 성공 시, 서보 모터를 통해 문 열기 명령
                    print("OTP verified successfully!")
                    mqtt_client.publish(mqtt_topic_publish_servo, json.dumps({"command": "on"}))
                    # 홈 관리 페이지로 리다이렉트
                    self.send_response(302)
                    self.send_header('Location', '/home')
                    self.end_headers()
                    return
                else:
                    # OTP 불일치 시 에러 메시지
                    otp_message = "<p style='color: red;'>Invalid OTP. Please try again.</p>"
                    print("Invalid OTP.")
            # OTP 불일치 시 다시 OTP 입력 페이지 제공
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            html = f"""
            <html>
            <head>
                <title>Home Automation</title>
                <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/milligram/1.4.1/milligram.min.css">
                <style>
                    body {{
                        max-width: 800px;
                        margin: 0 auto;
                        padding: 20px;
                    }}
                    .input-group {{
                        margin-bottom: 20px;
                    }}
                </style>
            </head>
            <body>
                <h1>Home Automation</h1>
                <div class="input-group">
                    <h2>OTP Verification</h2>
                    <form action="/verify_otp" method="get">
                        <label for="otp">Enter OTP:</label>
                        <input type="text" id="otp" name="otp" required>
                        <input type="submit" value="Verify OTP" class="button-primary">
                    </form>
                    {otp_message}
                </div>
            </body>
            </html>
            """
            self.wfile.write(bytes(html, "utf-8"))

        # "/home": 홈 관리 페이지
        elif self.path == "/home":
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            html = f"""
            <html>
            <head>
                <title>Home Automation - Control Panel</title>
                <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/milligram/1.4.1/milligram.min.css">
                <style>
                    .container {{
                        display: flex;
                        flex-direction: row;
                    }}
                    .left-panel {{
                        flex: 1;
                    }}
                    .right-panel {{
                        width: 450px;
                        margin-left: 20px;
                    }}
                </style>
            </head>
            <body>
                <h1>Home Control Panel</h1>
                <div class="container">
                    <div class="left-panel">
                        <p>Current Temperature: <span id="temperature">{sensor_data['temperature']}</span> &deg;C</p>
                        <p>Current Humidity: <span id="humidity">{sensor_data['humidity']}</span> %</p>
                        <h2>Set Maximum Occupancy</h2>
                        <form action="/set_max_people" method="get">
                            <label for="max_people">Max People:</label>
                            <input type="number" id="max_people" name="max_people" min="1" required>
                            <input type="submit" value="Set" class="button-primary">
                        </form>
                        <div id="maxPeopleMessage"></div>
                        <h2>Lighting Control</h2>
            """
            for room in range(1, 6):
                html += f"""
                        <button onclick="sendCommand('/lighting?led={room}&status=on')">Turn Room {room} ON</button>
                        <button onclick="sendCommand('/lighting?led={room}&status=off')">Turn Room {room} OFF</button><br>
                """
            html += """
                        <button onclick="sendCommand('/lighting?status=on')">Turn All Rooms ON</button>
                        <button onclick="sendCommand('/lighting?status=off')">Turn All Rooms OFF</button>
                        <h2>Humidifier Control</h2>
                        <button onclick="sendCommand('/humidifier?status=on')">Turn Humidifier ON</button>
                        <button onclick="sendCommand('/humidifier?status=off')">Turn Humidifier OFF</button>
                        <h2>Door Control</h2>
                        <button onclick="sendCommand('/servo?command=on')">Open Door</button>
                        <button onclick="sendCommand('/servo?command=off')">Close Door</button>
                    </div>
                    <div class="right-panel">
                        <!-- 온/습도 패널 iframe -->
                        <iframe src="http://172.20.10.2:3000/d-solo/ee3q14ntn2qyof/new-dashboard?from=1733771615782&to=1733858015782&timezone=browser&orgId=1&panelId=1&__feature.dashboardSceneSolo" width="650" height="400" frameborder="0"></iframe>
                        <!-- 사람 수 패널 iframe -->
                        <iframe src="http://172.20.10.2:3000/public-dashboards/bfff026d927f447f9b15c12ddb6b9727" width="650" height="400" frameborder="0"></iframe>
                    </div>
                </div>
                <script>
                    function sendCommand(url) {
                        fetch(url)
                        .then(function(response) { return response.json(); })
                        .then(function(data) { console.log(data); })
                        .catch(function(error) { console.error('Error:', error); });
                    }
                    // 최대 인원수 설정 폼 제출 이벤트 처리
                    document.querySelector('form[action="/set_max_people"]').addEventListener('submit', function(e) {
                        e.preventDefault();  // 폼 기본 동작 막기
                        const maxPeople = document.getElementById('max_people').value;
                        if (maxPeople < 1) {
                            document.getElementById('maxPeopleMessage').innerHTML = '<p style="color: red;">Please enter a valid number greater than 0.</p>';
                            return;
                        }
                        fetch('/set_max_people?max_people=' + maxPeople)
                        .then(function(response) { return response.json(); })
                        .then(function(data) {
                            console.log(data);
                            document.getElementById('maxPeopleMessage').innerHTML = '<p style="color: green;">Maximum occupancy set to: ' + maxPeople + '</p>';
                        })
                        .catch(function(error) { console.error('Error:', error); });
                    });
                    // 주기적으로 센서 데이터 갱신 (5초 마다)
                    setInterval(async () => {
                        try {
                            const res = await fetch('/sensor');
                            if (res.ok) {
                                const data = await res.json();
                                document.getElementById('temperature').innerText = data.temperature;
                                document.getElementById('humidity').innerText = data.humidity;
                            } else {
                                console.error('Failed to fetch sensor data');
                            }
                        } catch (error) {
                            console.error('Error:', error);
                        }
                    }, 5000);
                </script>
            </body>
            </html>
            """
            self.wfile.write(bytes(html, "utf-8"))

        # "/set_max_people": 최대 인원수 설정 처리
        elif self.path.startswith("/set_max_people"):
            qs = urlparse(self.path).query
            params = parse_qs(qs)
            if "max_people" in params:
                max_people = int(params["max_people"][0])
                # MQTT로 최대 인원 수 설정 명령 발행
                mqtt_client.publish(mqtt_topic_publish_security_count, json.dumps({"max_people": max_people}))
                print(f"Maximum occupancy set to: {max_people}")
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(bytes(json.dumps({"status": "ok", "max_people": max_people}), 'utf-8'))

        # "/lighting": 조명 제어 처리
        elif self.path.startswith("/lighting"):
            qs = urlparse(self.path).query
            params = parse_qs(qs)
            if "led" in params and "status" in params:
                # 특정 LED 제어
                led = int(params["led"][0])
                status = params["status"][0]
                message = {"led": led, "status": status}
                mqtt_client.publish(mqtt_topic_publish_lighting, json.dumps(message))
                print(f"LED {led} status: {status}")
            elif "status" in params:
                # 모든 LED 제어
                status = params["status"][0]
                message = {"status": status}
                mqtt_client.publish(mqtt_topic_publish_lighting, json.dumps(message))
                print(f"All LEDs status: {status}")
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes('{"status":"ok"}', 'utf-8'))

        # "/humidifier": 가습기 제어 처리
        elif self.path.startswith("/humidifier"):
            qs = urlparse(self.path).query
            params = parse_qs(qs)
            if "status" in params:
                status = params["status"][0]
                message = {"status": status}
                mqtt_client.publish(mqtt_topic_publish_humidifier, json.dumps(message))
                print(f"Humidifier status: {status}")
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes('{"status":"ok"}', 'utf-8'))

        # "/servo": 서보 모터(문) 제어 처리
        elif self.path.startswith("/servo"):
            qs = urlparse(self.path).query
            params = parse_qs(qs)
            if "command" in params:
                command = params["command"][0]
                message = {"command": command}
                mqtt_client.publish(mqtt_topic_publish_servo, json.dumps(message))
                print(f"Servo command: {command}")
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes('{"status":"ok"}', 'utf-8'))

        # "/sensor": 현재 센서 데이터 반환
        elif self.path.startswith("/sensor"):
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes(json.dumps(sensor_data), 'utf-8'))

        else:
            # 정의되지 않은 경로 요청 시 404 반환
            self.send_response(404)
            self.end_headers()
            self.wfile.write(bytes("404 Not Found", "utf-8"))

# ------------------ HTTP 서버 시작 ------------------
webServer = HTTPServer((hostName, serverPort), MyServer)
print('Server started http://%s:%s' % (hostName, serverPort))

try:
    webServer.serve_forever()
except KeyboardInterrupt:
    pass

webServer.server_close()
print('Server stopped')
