// Load Wi-Fi library
#include <ESP8266WiFi.h>
// Load WebServer library
#include <ESP8266WebServer.h>
// Load real-time clock library
#include <NTPClient.h>
#include <WiFiUdp.h>
// Load JSON library
#include <ArduinoJson.h>
// Load JWT library
#include <ArduinoJWT.h>
// Load LiquidCrystal_I2C library
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x3f, 16, 4);

// network credentials
const char *ssid = "username";
const char *password = "password";

const char *www_username = "admin";
const char *www_password = "esp8266";

String JWT_key = "ff9eebddfdad95947fd83f52f53f7077"; // QR_open_door
ArduinoJWT jwt = ArduinoJWT(JWT_key);

unsigned long timeDiff = 600; // 10 min

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const String months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int timezone = 7 * 3600; //ตั้งค่า TimeZone ตามเวลาประเทศไทย
int dst = 0;             //กำหนดค่า Date Swing Time
unsigned long Unixtime;

// Set Static IP
IPAddress local_IP(192, 168, 1, 99);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);

// Set web server port number to 80
ESP8266WebServer server(80);

// Assign output variables to GPIO pins
const int Door_button = D5;
const int buzzer = D6;
const int LED_Denied = D7;
const int LED_OK = D8;

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  lcd.begin();
  lcd.backlight();

  // Initialize the output variables
  pinMode(Door_button, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_Denied, OUTPUT);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_Denied, LOW);

  jwt.setPSK(JWT_key);
  Serial.print("JWT_key = ");
  Serial.println(JWT_key);
  Serial.println();

  WIFI_Connect();

  API_handler();
}

void WIFI_Connect()
{
  WiFi.disconnect();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to ");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Connect to Wi-Fi network with SSID and password
  WiFi.begin(ssid, password);
  WiFi.config(local_IP, primaryDNS, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    digitalWrite(LED_OK, HIGH);
    digitalWrite(LED_Denied, LOW);
    delay(500);
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_Denied, HIGH);
    delay(500);
  }

  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_Denied, LOW);

  // set real-time clock
  timeClient.begin();
  timeClient.setTimeOffset(timezone);

  // Print local IP address and start web server
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected.");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  digitalWrite(buzzer, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
  delay(3000);
}

void Show_Time()
{
  // https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
  timeClient.update();
  Unixtime = timeClient.getEpochTime();
  String formattedTime = timeClient.getFormattedTime();
  //Get a time structure
  struct tm *ptm = gmtime((time_t *)&Unixtime);
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  String currentMonthName = months[currentMonth - 1];
  int currentYear = ptm->tm_year + 1900;
  String currentDate = String(currentYear) + "-" + String(currentMonthName) + "-" + String(monthDay);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(currentDate);
  lcd.setCursor(0, 1);
  lcd.print(formattedTime);

  delay(1000);
}

void Access_OK(String name)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(name);
  lcd.setCursor(0, 1);
  lcd.print("Access Allowed!!");
  digitalWrite(LED_Denied, LOW);
  digitalWrite(buzzer, HIGH);
  digitalWrite(LED_OK, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_OK, LOW);
  delay(100);
  digitalWrite(buzzer, HIGH);
  digitalWrite(LED_OK, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_OK, LOW);
  delay(100);
  digitalWrite(buzzer, HIGH);
  digitalWrite(LED_OK, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_OK, LOW);
  digitalWrite(Door_button, HIGH);
  delay(2000);
  digitalWrite(Door_button, LOW);
}

void Access_Denied(String error)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access Denied!!");
  lcd.setCursor(0, 1);
  lcd.print(error);
  digitalWrite(LED_OK, LOW);

  digitalWrite(buzzer, HIGH);
  digitalWrite(LED_Denied, HIGH);
  delay(300);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_Denied, LOW);
  delay(50);
  digitalWrite(buzzer, HIGH);
  digitalWrite(LED_Denied, HIGH);
  delay(700);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_Denied, LOW);

  delay(2000);
}

void handleError(int code, String errorMsg)
{
  server.send(code, "text/plain", errorMsg);
  Access_Denied(errorMsg);
}

void handleNotFound()
{
  String message = "Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handlerOpenDoor()
{
  if (!server.authenticate(www_username, www_password))
    return server.requestAuthentication();
  // JWT - Decode a JWT and retreive the payload
  //bool jwt.decodeJWT(String& jwt, String& payload);
  // https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html
  if (!server.hasArg("token") || server.arg("token") == NULL)
    return handleError(400, "Do not have token");

  // String token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJuYW1lIjoidGVzdCIsInVuaXh0aW1lIjoxNTkyNDgyMjU2fQ.vpwnhHVSiicYBzdt9GyPQ0V32cq0EH3VcZ4czRv4RX8";
  String token = server.arg("token");
  String decodedData;
  boolean decodedToken = jwt.decodeJWT(token, decodedData);
  if (!decodedToken)
    return handleError(400, "JWT token invalid");

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, decodedData);
  if (error)
    return handleError(400, "deserialize Json error");

  String getName = doc["name"];
  unsigned long getTime = doc["unixtime"];
  if (doc["name"] == NULL || doc["unixtime"] == NULL)
    return handleError(400, "Object name or unixtime is NULL");

  timeClient.update();
  Unixtime = timeClient.getEpochTime();
  // String test = String(Unixtime) + " | " + String(getTime) + " | " + String((Unixtime - timeDiff)) + " | " + String((Unixtime + timeDiff));
  // Serial.println(test);
  if ((getTime < (Unixtime - timeDiff)) || (getTime > (Unixtime + timeDiff)))
    return handleError(400, "invalid time");

  String sucess_msg = "door open by " + getName;
  server.send(200, "text/plain", sucess_msg);
  Access_OK(getName);
  return;
}

void API_handler()
{
  // https://www.mischianti.org/2020/05/16/how-to-create-a-rest-server-on-esp8266-and-esp32-startup-part-1/
  // https://ioxhop.github.io/ESPIOX2-Document/esp8266-basic-webserver.html
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/json", "I'm Still Alive!!");
  });
  server.on("/open", HTTP_POST, handlerOpenDoor);
  server.onNotFound(handleNotFound);

  server.begin();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("WIFI connection lost ...");
    WIFI_Connect();
  }

  server.handleClient();

  Show_Time();
}
