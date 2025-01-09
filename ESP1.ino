//192.168.0.103
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <RTClib.h>
#include <ESP8266WebServer.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

RTC_DS1307 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600); // UTC+7 for Vietnam
ESP8266WebServer webServer(80);
LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x27 is the I2C address, 16x2 LCD	
Servo servo;
const char* remoteIP = "172.20.10.4"; // Địa chỉ IP của ESP thứ hai
unsigned int remoteUdpPort = 1234;      // Cổng UDP của ESP thứ hai

const int SENSOR_PIN = D3; //cảm biến cho D3
//const int BUZZER_PIN = D0; //còi cho d0

const byte ROWS    = 4; // Số hàng
const byte COLS = 3; // Số cột
// Layout của keypad
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

int START_HOUR = 6; //chỉnh sửa thời gian cho phép vào ra
int START_MINUTE = 0;
int END_HOUR = 8;
int END_MINUTE = 35;

int START_HOUR2 = 6; //chỉnh sửa thời gian cho phép vào ra
int START_MINUTE2 = 0;
int END_HOUR2 =8;
int END_MINUTE2 = 35;

const char* ssid = "Thieu"; // Thay bằng tên mạng WiFi 
const char* password = "05102003"; // Thay bằng mật khẩu WiFi 
const int UDP_PORT = 12345; // Cổng UDP để nhận thông báo
WiFiUDP udp;
bool motionDetected = false; //trạng thái
bool sensorActive = true;
String udpMessage = "";

void setup() {
  Serial.begin(115200);
  setupPins();
  setupRTC();
  connectWiFi(); // Kết nối WiFi
  setupWebServer(); // Khởi động máy chủ web
  timeClient.begin(); // Khởi động NTPClient
  servo.attach(16);//D0 of NodeMcu
  servo.write(35);
  delay(2000);
  udp.begin(UDP_PORT); // Bắt đầu lắng nghe UDP
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
}

void setupPins() {
  //pinMode(BUZZER_PIN, OUTPUT); 
  pinMode(SENSOR_PIN, INPUT);
  //digitalWrite(BUZZER_PIN, LOW); 
}

void setupRTC() {
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("Không thể tìm thấy RTC");
    while (1) delay(10);
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC không hoạt động, hãy đặt thời gian!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void connectWiFi() { //kiểm tra wifi có kết nối không
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
  Serial.println("WiFi đã kết nối thành công");
  Serial.println("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
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
        lcd.print("Cua 1 mo"); // Display "Cửa mở"
        delay(3000); // Keep the message for 3 seconds
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter Password:");
        servo.write(150);
        delay(10000);          
        servo.write(35);
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

void checkMotionSensor() {
  // check Trạng thái cảm biến chuyển động
  motionDetected = isMotionDetected();
}

bool isMotionDetected() {
  // Trả về trạng thái của cảm biến chuyển động
  return digitalRead(SENSOR_PIN) == HIGH; //đọc trạng thái cảm biến
}

void receiveUDP() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255];
    udp.read(packetBuffer, packetSize);
    packetBuffer[packetSize] = '\0';

    // Lưu thông điệp UDP nhận được vào udpMessage
    udpMessage = String(packetBuffer);

    // Xử lý thông báo nhận được ở đây
    Serial.println("Thông điệp UDP nhận được: " + udpMessage); //INRA UDP
  }
}

void sendUDP(){
  const char* command = "START_SERVO";
  udp.beginPacket(remoteIP, remoteUdpPort);
  udp.write(command);
  udp.endPacket();
  Serial.println("Đã gửi lệnh: START_SERVO");
  delay(200); // Gửi lệnh sau mỗi 2 giây (tùy chỉnh thời gian theo ý muốn)
}

void sendUDP1(){
  const char* command = "START_SERVO1";
  udp.beginPacket(remoteIP, remoteUdpPort);
  udp.write(command);
  udp.endPacket();
  Serial.println("Đã gửi lệnh: START_SERVO1");
  delay(200); // Gửi lệnh sau mỗi 2 giây (tùy chỉnh thời gian theo ý muốn)
}
//còi
// void activateAlarm() {
//   digitalWrite(BUZZER_PIN, HIGH); 
// }
// void deactivateAlarm() {
//   digitalWrite(BUZZER_PIN, LOW);
// }

void loop() {
  webServer.handleClient(); // Xử lý yêu cầu từ máy chủ web
  timeClient.update(); // Cập nhật thời gian từ NTP

  DateTime now = timeClient.getEpochTime(); // Lấy thời gian từ NTP
  DateTime now2 = timeClient.getEpochTime();

  if (isWithinAllowedTime(now) && isWithinAllowedTime2(now2)){
    //handleAllowedTime(now);
    servo.write(150);
    sendUDP();
    delay(200);
  } else if(!isWithinAllowedTime(now) && isWithinAllowedTime2(now2)){
    if (!access_granted) {
    servo.write(35);
    sendUDP(); 
    pass();
    } else {
    //Serial.println("Cửa 1 mở!");
    // servo.write(180);
    // delay(10000);
    // servo.write(0);
    // delay(200);
    // Thêm các lệnh cần thực thi khi mật khẩu đúng ở đây
    // Ví dụ: 
    }
  }else if(isWithinAllowedTime(now) && !isWithinAllowedTime2(now2)){
    if (!access_granted) {
      servo.write(150);   
      delay(200);
      sendUDP1(); 
  } else{
    //Serial.println("Cửa 2 mở!");
    delay(200); 
    }
  }else{
     if (!access_granted) {
    servo.write(35);
    sendUDP1();  
    pass();  // Gọi hàm nhập mật khẩu nếu chưa được cấp quyền truy cập
  } else {
    //Serial.println("OKK!");
    delay(200);  // Đợi 1 giây trước khi thực hiện hành động tiếp theo
  }
  }
  checkMotionSensor(); // Kiểm tra cảm biến chuyển động
  receiveUDP(); // Lắng nghe gói tin UDP
  delay(200);
}


bool isWithinAllowedTime(DateTime currentTime) {  //trong thời gian cho phép
  int currentHour = currentTime.hour(); //giờ hiện tại
  int currentMinute = currentTime.minute(); //phút hiện tại
   //  08:30 -17:45 ;  09:15 true 18:00 false
  return (currentHour > START_HOUR || (currentHour == START_HOUR && currentMinute >= START_MINUTE)) && //
         (currentHour < END_HOUR || (currentHour == END_HOUR && currentMinute <= END_MINUTE));
}

bool isWithinAllowedTime2(DateTime currentTime2) {  //trong thời gian cho phép
  int currentHour2 = currentTime2.hour(); //giờ hiện tại
  int currentMinute2 = currentTime2.minute(); //phút hiện tại
   //  08:30 -17:45 ;  09:15 true 18:00 false
  return (currentHour2 > START_HOUR2 || (currentHour2 == START_HOUR2 && currentMinute2 >= START_MINUTE2)) && //
         (currentHour2 < END_HOUR2 || (currentHour2 == END_HOUR2 && currentMinute2 <= END_MINUTE2));
}
void handleAllowedTime(DateTime currentTime) {
  Serial.println("Đang trong thời gian cho phép vào/ra cả 2 cửa");
}
void handleAllowedTime1(DateTime currentTime) {
  Serial.println("Đang trong thời gian cho phép vào/ra cửa 2");
  
 
  delay(1000);  // Đợi 1 giây trước khi thực hiện hành động tiếp theo
  
}
void handleAllowedTime2(DateTime currentTime) {
  Serial.println("Đang trong thời gian cho phép vào/ra cửa 1");
  Serial.println("Cửa 2 đóng");
  Serial.println("Vui lòng nhập mk: ");
  pass();
  Serial.println("Cửa 2 đã mở");
}
void handleNotAllowedTime(DateTime currentTime) {
  Serial.println("Ngoài thời gian cho phép vào/ra");
  
  if (isMotionDetected()) {
    Serial.println("Cảnh báo, có đột nhập");
    //activateAlarm();
  } else {
    Serial.println("Không có đột nhập");
    //deactivateAlarm();
  }
}

void setupWebServer() {
  webServer.on("/", HTTP_GET, []() {
    DateTime now = timeClient.getEpochTime();
    DateTime now2 = timeClient.getEpochTime();
    if (isWithinAllowedTime(now)  && isWithinAllowedTime2(now2)) {
      sendAllowedTimeAlertToWebClient("Hệ thống chống trộm 1", "Đang trong thời gian cho phép vào/ra",now);
    }else if(!isWithinAllowedTime(now) && isWithinAllowedTime2(now2)) {
      sendAllowedTimeAlertToWebClient("Hệ thống chống trộm 1", motionDetected ? "Cảnh báo: Có đột nhập" : "Không có đột nhập",now);
    }else if(isWithinAllowedTime(now) && !isWithinAllowedTime2(now2)){
      sendAlertToWebClient("Hệ thống chống trộm 1", "Đang trong thời gian cho phép vào/ra",now, udpMessage);
    }else {
      sendAlertToWebClient("Hệ thống chống trộm 1", motionDetected ? "Cảnh báo: Có đột nhập" : "Không có đột nhập", now, udpMessage);
    }
  });

  webServer.on("/edit-time", HTTP_GET, []() {
    // Trang chỉnh sửa thời gian
    String editPage = 
"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>Chỉnh sửa thời gian</title>"
    "<style>/* CSS styling */</style>"
"</head>"
"<body>"
    "<h2>Chỉnh sửa thời gian</h2>"

    "<!-- Form gửi 4 biến thời gian -->"
    "<form action='/save-time' method='post'>"
        "<!-- Thời gian 1 -->"
        "<label for='start-time'>Thời gian bắt đầu 1:</label><br>"
        "<input type='time' id='start-time' name='start-time'><br>"
        "<label for='end-time'>Thời gian kết thúc 1:</label><br>"
        "<input type='time' id='end-time' name='end-time'><br><br>"

        "<!-- Thời gian 2 -->"
        "<label for='start-time2'>Thời gian bắt đầu 2:</label><br>"
        "<input type='time' id='start-time2' name='start-time2'><br>"
        "<label for='end-time2'>Thời gian kết thúc 2:</label><br>"
        "<input type='time' id='end-time2' name='end-time2'><br><br>"

        "<!-- Nút Lưu -->"
        "<input type='submit' value='Lưu'>"
 		    "</form></body></html>";
    
    webServer.send(200, "text/html", editPage);
  });

  webServer.on("/save-time", HTTP_POST, []() {
    // Lưu thời gian mới từ dữ liệu POST
    String startTime = webServer.arg("start-time");
    String endTime = webServer.arg("end-time");
    String startTime2 = webServer.arg("start-time2");
    String endTime2 = webServer.arg("end-time2");

    // Chuyển đổi thời gian từ chuỗi sang số nguyên
    int startHour = startTime.substring(0, 2).toInt();
    int startMinute = startTime.substring(3).toInt();
    int endHour = endTime.substring(0, 2).toInt();
    int endMinute = endTime.substring(3).toInt();

    int startHour2 = startTime2.substring(0, 2).toInt();
    int startMinute2 = startTime2.substring(3).toInt();
    int endHour2 = endTime2.substring(0, 2).toInt();
    int endMinute2 = endTime2.substring(3).toInt();

    // Cập nhật thời gian bắt đầu và kết thúc
    START_HOUR = startHour;
    START_MINUTE = startMinute;
    END_HOUR = endHour;
    END_MINUTE = endMinute;

    START_HOUR2 = startHour2;
    START_MINUTE2 = startMinute2;
    END_HOUR2 = endHour2;
    END_MINUTE2 = endMinute2;
    // Chuyển hướng người dùng sau khi lưu
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
  });

  webServer.begin();
}

void sendAlertToWebClient(String title, String message, DateTime currentTime, String udpMessage) {
  String timeStr = 
    String(currentTime.hour()) + ":" + String(currentTime.minute()); // Chuỗi thời gian hiện tại
  
  String alertPage = 
  "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>" + title + "</title>"
    "<style>"
    "body {font-family: Arial, sans-serif;text-align: center;}"
    ".container {margin-top: 50px;}"
    ".alert {padding: 20px;background-color: #f44336;color: white;border-radius: 10px;}"
    "</style>"
    "<script>"
    "function updateContent() {"
    "  var xhttp = new XMLHttpRequest();"
    "  xhttp.onreadystatechange = function() {"
    "    if (this.readyState == 4 && this.status == 200) {"       
    "      document.getElementById('content').innerHTML = this.responseText;"
    "    }"
    "  };"
    "  xhttp.open('GET', '/', true);"
    "  xhttp.send();"
    "}"
    "setInterval(updateContent, 5000); // Cập nhật mỗi 5 giây"
    "</script>"
    "</head>"
    "<body onload='updateContent()'>"
    "<div id='content'>"
    "<div class='alert'>"
    "<h2>" + title + "</h2>"
    "<p>" + message + "</p>"
    "<p>Thời gian: " + timeStr + "</p>"
   
    
    "<title>" + title + "</title>"
    "<style>"
    "body {font-family: Arial, sans-serif;text-align: center;}"
    ".container {margin-top: 50px;}"
    ".alert {padding: 20px;background-color: #f44336;color: white;border-radius: 10px;}"
    "</style>"
    "<script>"
    "function updateContent() {"
    "  var xhttp = new XMLHttpRequest();"
    "  xhttp.onreadystatechange = function() {"
    "    if (this.readyState == 4 && this.status == 200) {"
    "      document.getElementById('content').innerHTML = this.responseText;"
    "    }"
    "  };"
    "  xhttp.open('GET', '/', true);"
    "  xhttp.send();"
    "}"
    "setInterval(updateContent, 5000); // Cập nhật mỗi 5 giây"
    "</script>"
    "</head>"
    "<body onload='updateContent()'>"
    "<div id='content'>"
    "<div class='alert'>"
    "<h2>" + "Hệ thống chống trộm 2" + "</h2>"
    "<p>" + udpMessage + "</p>"
    "<p>Thời gian: " + timeStr + "</p>"
    "</div></div></body></html>";

  webServer.send(200, "text/html", alertPage);
}


void sendAllowedTimeAlertToWebClient(String title, String message, DateTime currentTime) {
  String timeStr = 
    String(currentTime.hour()) + ":" + String(currentTime.minute()); // Chuỗi thời gian hiện tại
  // Tạo chuỗi HTML để hiển thị thông báo trên trang web
  String alertPage = 
"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>" + title + "</title>"
    "<style>"
    "body {font-family: Arial, sans-serif;text-align: center;}"
    ".container {margin-top: 50px;}"
    ".alert {padding: 20px;background-color: #f44336;color: white;border-radius: 10px;}"
    "</style>"
    "<script>"
    "function updateContent() {"
    "  var xhttp = new XMLHttpRequest();"
    "  xhttp.onreadystatechange = function() {"
    "    if (this.readyState == 4 && this.status == 200) {"       
    "      document.getElementById('content').innerHTML = this.responseText;"
    "    }"
    "  };"
    "  xhttp.open('GET', '/', true);"
    "  xhttp.send();"
    "}"
    "setInterval(updateContent, 5000); // Cập nhật mỗi 5 giây"
    "</script>"
    "</head>"
    "<body onload='updateContent()'>"
    "<div id='content'>"
    "<div class='alert'>"
    "<h2>" + title + "</h2>"
    "<p>" + message + "</p>"
    "<p>Thời gian: " + timeStr + "</p>"
   
    
    "<title>" + title + "</title>"
    "<style>"
    "body {font-family: Arial, sans-serif;text-align: center;}"
    ".container {margin-top: 50px;}"
    ".alert {padding: 20px;background-color: #4CAF50;color: white;border-radius: 10px;}"
    "</style>"
    "<script>"
    "function updateContent() {"
    "  var xhttp = new XMLHttpRequest();"
    "  xhttp.onreadystatechange = function() {"
    "    if (this.readyState == 4 && this.status == 200) {"
    "      document.getElementById('content').innerHTML = this.responseText;"
    "    }"
    "  };"
    "  xhttp.open('GET', '/', true);"
    "  xhttp.send();"
    "}"
    "setInterval(updateContent, 5000); // Cập nhật mỗi 5 giây"
    "</script>"
    "</head>"
    "<body onload='updateContent()'>"
    "<div id='content'>"
    "<div class='alert'>"
    "<h2>" + "Hệ thống chống trộm 2" + "</h2>"
    "<p>" + "Đang trong thời gian cho phép vào/ra" + "</p>"
    "<p>Thời gian: " + timeStr + "</p>"
    "</div></div></body></html>";

  // Gửi trang HTML với thông báo cho trình duyệt web
  webServer.send(200, "text/html", alertPage);
}






