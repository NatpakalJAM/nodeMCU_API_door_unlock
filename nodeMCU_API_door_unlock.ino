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
// Load Timer library
#include <Timer.h>

Timer t;

LiquidCrystal_I2C lcd(0x3f, 16, 2);

// network credentials
const char *ssid = "TrueGigatexFiber_2.4G_6A0";
const char *password = "64a7u5hn";

const char *www_username = "admin";
const char *www_password = "esp8266";

String JWT_key = "svnRJ8ZvBxK9SSPq";
ArduinoJWT jwt = ArduinoJWT(JWT_key);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const String months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int timezone = 7 * 3600; // thai timezone
unsigned long dst = 7 * 3600;
unsigned long nowTime, dstTime;

// Set Static IP
IPAddress local_IP(10, 10, 103, 250);
IPAddress gateway(10, 10, 103, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// Set web server port number to 80
ESP8266WebServer server(80);

// Assign output variables to GPIO pins
const int Door_button = D5;
const int buzzer = D6;
const int LED_OK = D8;

boolean doorOpen = false;
int beepCount = 0;
String nameOpenDoor;

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  lcd.init();
  lcd.backlight();

  // Initialize the output variables
  pinMode(Door_button, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(LED_OK, OUTPUT);
  digitalWrite(Door_button, LOW);
  digitalWrite(buzzer, LOW);
  digitalWrite(LED_OK, LOW);

  jwt.setPSK(JWT_key);
  // Serial.print("JWT_key = ");
  // Serial.println(JWT_key);
  // Serial.println();

  WIFI_Connect();
  API_handler();

  t.every(1000, Show_Time);
  t.every(100, beepOpenDoor);
}

void WIFI_Connect()
{
  WiFi.disconnect();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to ");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  // Serial.print("Connecting to ");
  // Serial.println(ssid);

  // Connect to Wi-Fi network with SSID and password
  WiFi.begin(ssid, password);
  WiFi.config(local_IP, primaryDNS, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED)
  {
    // Serial.print(".");
    digitalWrite(LED_OK, HIGH);
    delay(500);
    digitalWrite(LED_OK, LOW);
    delay(500);
  }

  digitalWrite(LED_OK, LOW);

  // set real-time clock
  timeClient.begin();
  timeClient.setTimeOffset(timezone);

  // Print local IP address and start web server
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected.");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // Serial.println("");
  // Serial.println("WiFi connected.");
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());
  // Serial.print("MAC address: ");
  // Serial.println(WiFi.macAddress());

  digitalWrite(buzzer, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
}

void Show_Time()
{
  // https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
  timeClient.update();
  nowTime = timeClient.getEpochTime();
  dstTime = nowTime - dst;
  String formattedTime = timeClient.getFormattedTime();
  //Get a time structure
  struct tm *ptm = gmtime((time_t *)&nowTime);
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  String currentMonthName = months[currentMonth - 1];
  int currentYear = ptm->tm_year + 1900;
  String currentDate = String(currentYear) + "-" + String(currentMonthName) + "-" + String(monthDay);

  if (!doorOpen)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(currentDate);
    lcd.setCursor(0, 1);
    lcd.print(formattedTime);
  }
}

void beepOpenDoor()
{
  if (beepCount != 0)
  {
    if (doorOpen && beepCount == 14)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(nameOpenDoor);
      lcd.setCursor(0, 1);
      lcd.print("Access Allowed!!");
    }
    if (beepCount <= 8)
    {
      digitalWrite(buzzer, LOW);
      digitalWrite(LED_OK, LOW);
      if (doorOpen && beepCount == 1)
      {
        digitalWrite(Door_button, LOW);
        doorOpen = false;
      }
      beepCount--;
    }
    else if (beepCount % 2 == 0)
    {
      digitalWrite(buzzer, HIGH);
      digitalWrite(LED_OK, HIGH);
      if (doorOpen)
      {
        digitalWrite(Door_button, HIGH);
      }
      beepCount--;
    }
    else
    {
      digitalWrite(buzzer, LOW);
      digitalWrite(LED_OK, LOW);
      if (doorOpen)
      {
        digitalWrite(Door_button, HIGH);
      }
      beepCount--;
    }
  }
}

void handleError(int code, String errorMsg)
{
  server.send(code, "text/plain", errorMsg);
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
  // https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html
  if (!server.hasArg("token") || server.arg("token") == NULL)
    return handleError(400, "Do not have token");

  // String token = "eyJhbGciOiJIUzI1NiIsIm5iZiI6IjE1OTI4MTU1ODkiLCJleHAiOiIxNTkyODMwMDk0IiwidHlwIjoiSldUIn0.eyJuYW1lIjoiTmF0cGFrYWwgSy4ifQ.bJZUtN8XWsK5YXvq7svctK0tpvjz5XbkeFoW6hITVhI";
  String token = server.arg("token");
  String jsonStrHeader, jsonStrPaylode;
  boolean decodedToken = jwt.decodeJWT(token, jsonStrHeader, jsonStrPaylode);
  if (!decodedToken)
  {
    return handleError(400, "JWT token invalid");
  }

  StaticJsonDocument<200> jsonHeader;
  DeserializationError errorHeader = deserializeJson(jsonHeader, jsonStrHeader);
  if (errorHeader)
  {
    return handleError(400, "deserialize Json header error");
  }
  StaticJsonDocument<200> jsonPayload;
  DeserializationError errorPayload = deserializeJson(jsonPayload, jsonStrPaylode);
  if (errorPayload)
  {
    return handleError(400, "deserialize Json payload error");
  }

  if (jsonHeader["nbf"] == NULL || jsonHeader["exp"] == NULL || jsonPayload["name"] == NULL)
  {
    return handleError(400, "JWT token invalid");
  }

  unsigned long jwtNbf = jsonHeader["nbf"];
  unsigned long jwtExp = jsonHeader["exp"];
  String getName = jsonPayload["name"];

  if ((dstTime < jwtNbf) || (dstTime > jwtExp))
  {
    return handleError(400, "Invalid time");
  }

  String successMsg = "door open by " + getName;
  server.send(200, "text/plain", successMsg);
  if (!doorOpen)
  {
    beepCount = 14;
    nameOpenDoor = getName;
    doorOpen = true;
  }
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
    // Serial.print("WIFI connection lost ...");
    WIFI_Connect();
  }

  t.update();

  server.handleClient();
}
