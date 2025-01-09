//192.168.0.101
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <Keypad.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;
unsigned int localUdpPort = 1234; // Cổng UDP của ESP này
char incomingPacket[255];         // Bộ nhớ nhận gói tin

// Định nghĩa các chân
const int SENSOR_PIN = D3;
const byte ROWS    = 4; // Số hàng
const byte COLS = 3; // Số cột

char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {9, 10, 1, 2};  
byte colPins[COLS] = {14, 12 ,13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Mảng chứa các mật khẩu hợp lệ
String passwords[] = {"0510", "1234", "222222","22"}; // Thêm mật khẩu khác vào đây nếu muốn
int num_passwords = sizeof(passwords) / sizeof(passwords[0]);

String input_password = "";
bool access_granted = false;  // Cờ xác nhận quyền truy cập

// Thông tin đăng nhập WiFi
const char* ssid = "Thieu"; // Thay bằng tên WiFi của bạn
const char* password = "05102003"; // Thay bằng mật khẩu WiFi của bạn

// Cài đặt UDP
const char* receiverIP = "172.20.10.5"; // Địa chỉ IP của ESP8266 nhận thông báo
const int UDP_PORT = 12345; // Cổng UDP để gửi thông báo
WiFiUDP udp;

bool motionDetected = false; // Biến để lưu trạng thái chuyển động

void setup() {
  Serial.begin(115200);
  setupPins();
  connectWiFi(); // Kết nối WiFi

  udp.begin(localUdpPort);
  Serial.printf("UDP khởi động tại cổng %d\n", localUdpPort);
  
  myServo.attach(16);//D0 of NodeMcu
  myServo.write(0);
  delay(2000);
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
}

void pass(){
  char key = keypad.getKey(); // Get key press

  if (key) {
    if (key == '#') { // '#' to submit the password
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Checking...");

      if (checkPassword(input_password)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Access Granted!");
        lcd.setCursor(0, 1);
        lcd.print("Cua 2 mo"); // Display "Cửa mở"
        delay(3000); // Keep the message for 3 seconds
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter Password:");
        myServo.write(70);
        delay(10000);          
        myServo.write(0);
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Access Denied!");
        delay(2000); // Show error message for 2 seconds
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter Password:");
      }
      input_password = ""; // Clear input for next attempt
    } else if (key == '*') { // '*' to clear the input
      input_password = "";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Password:");
    } else {
      // Append key to the input and update LCD
      if (input_password.length() < 16) { // Ensure input fits on the LCD
        input_password += key;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Password:");
        lcd.setCursor(0, 1);
        lcd.print(input_password);
      }
    }
  }
}

// Function to check if the input password matches any valid password
bool checkPassword(String input) {
  for (int i = 0; i < num_passwords; i++) {
    if (input == passwords[i]) {
      return true;
    }
  }
  return false;
}

void loop() {
  checkMotion(); // Kiểm tra chuyển động
  sendSensorAlert(); // Gửi thông báo từ cảm biến qua UDP
  receiveUDP();
  delay(200); // Đợi 1 giây
}

void setupPins() {
  pinMode(SENSOR_PIN, INPUT);
 
}

void connectWiFi() {
  Serial.println();
  Serial.println();
  Serial.print("Đang kết nối WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Đã kết nối WiFi thành công");
  Serial.println("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
}

void checkMotion() {
  bool currentMotion = isMotionDetected();

  if (currentMotion && !motionDetected) {
    // Đã phát hiện chuyển động lần đầu
    motionDetected = true;
  } else if (!currentMotion && motionDetected) {
    // Không còn chuyển động
    motionDetected = false;
  }
}

bool isMotionDetected() {
  return digitalRead(SENSOR_PIN) == HIGH;
}


void sendSensorAlert() {
  String sensorData = motionDetected ? "Cảnh báo: Có đột nhập" : "Không có đột nhập";
  udp.beginPacket(receiverIP, UDP_PORT);
  udp.print(sensorData);
  udp.endPacket();
  Serial.println(sensorData);
}

void receiveUDP(){
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0'; // Kết thúc chuỗi
    }
    Serial.printf("Nhận lệnh: %s\n", incomingPacket);

    // Xử lý lệnh
    if (strcmp(incomingPacket, "START_SERVO") == 0) {
      Serial.println("Khởi động servo...");
      myServo.write(70); // Di chuyển servo tới góc 90 độ
      delay(200);       // Giữ ở vị trí này 1 giây
    }
    if (strcmp(incomingPacket, "START_SERVO1") == 0) {
      Serial.println("Khởi động servo...");
      myServo.write(0); // Di chuyển servo tới góc 90 độ
      delay(200);
      pass();       // Giữ ở vị trí này 1 giây
    }
  }
}

