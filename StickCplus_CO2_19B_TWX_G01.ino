#include <DHT.h>
#include <WiFi.h>
#include <M5StickCPlus.h>
#include "AXP192.h"
#include <HTTPClient.h>

#define DHTPIN 33
#define DHTTYPE DHT11
  
  DHT dht(DHTPIN, DHTTYPE);

  int PERIOD = 60;
  WiFiClient client;

  const float     bat_percent_max_vol = 4.1f;     // バッテリー残量の最大電圧
  const float     bat_percent_min_vol = 3.3f;     // バッテリー残量の最小電圧
  float           bat_per             = 0;        // バッテリー残量のパーセンテージ
  float           bat_per_inclination = 0;        // バッテリー残量のパーセンテージ式の傾き
  float           bat_per_intercept   = 0;        // バッテリー残量のパーセンテージ式の切片
  float           bat_vol             = 0;        // バッテリー電圧

  const char* ssid = "your AP's ssid";
  const char* password = "your AP's password";

  byte cmd[9] = { 0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };//CO2センサに投げるコマンド
  byte response[9];

  const String host = "Your host name/Thingworx/Things/your thing name/Properties/";
  const String appkey = "your application key";

  int valuePost(String property, float value);
  
void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Axp.begin();
//  M5.Axp.EnableCoulombcounter();
  
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(3);
//  M5.Lcd.setTextFont(4);

  Serial.begin(115200);
  Serial.println("CO2 sensor test!!");

  Serial1.begin(9600, SERIAL_8N1, 36, 26);
  gpio_pulldown_dis(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_25);
   
  int cnt = 0;

  // バッテリー残量のパーセンテージ式の算出
  // 線分の傾きを計算
  bat_per_inclination = 100.0F/(bat_percent_max_vol-bat_percent_min_vol);
  // 線分の切片を計算
  bat_per_intercept = -1 * bat_percent_min_vol * bat_per_inclination;

  dht.begin();

  M5.Lcd.printf("Connect to %s\n", ssid);
  WiFi.begin(ssid, password);  //  Wi-Fi APに接続 ----A
  while (WiFi.status() != WL_CONNECTED) {  //  Wi-Fi AP接続待ち
    cnt++;
    delay(500);
    M5.Lcd.print(".");
    Serial.print(".");
    if(cnt%10==0){
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      M5.Lcd.println("");
    }
    if(cnt>=30){
      ESP.restart();      
    }
  }
  Serial.print("WiFi connected\r\nIP address: ");
  Serial.println(WiFi.localIP());
  M5.Lcd.printf("\nWiFi connected\r\nIP address: ");
  M5.Lcd.println(WiFi.localIP());

}

void loop() {
  // put your main code here, to run repeatedly:
  float temp_hum_val[2] = {0};
  int status_code = 0;
  
  Serial1.write(cmd, 9);
  Serial1.flush();
  memset(response, 0, 9);
  Serial1.readBytes(response, 9);
  int stByte = (int)response[0];
  int cmdByte = (int)response[1];
//  Serial.printf("%03x",stByte);
//  Serial.printf("  %03x\r\n", cmdByte);
  int resHigh = (int)response[2];
  int resLow = (int)response[3];
  int ppm = (256 * resHigh)+resLow;

  if (ppm > 0){
    Serial.printf("%4d\r\n", ppm);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.printf(" CO2 :   \n  %4d", ppm);
//    M5.Lcd.setTextSize(2);    
    M5.Lcd.printf("ppm  ");

  }
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0,70);

//  M5.Lcd.printf("Temp : %.1f C\r\n", M5.Axp.GetTempInAXP192());
  if (!dht.readTempAndHumidity(temp_hum_val)) {
    M5.Lcd.printf(" Temp  : %.1fC\r\n", temp_hum_val[1]);
    M5.Lcd.printf(" Humid : %.1f%%\r\n", temp_hum_val[0]);
  } else {
    M5.Lcd.printf(" Temp  : n/a \r\n");
    M5.Lcd.printf(" Humid : n/a \r\n");    
  }

  bat_vol = M5.Axp.GetBatVoltage();   // V
  M5.Lcd.printf(" Bat_V : %.2fV ", bat_vol);
  // バッテリーの残量を簡易的なパーセンテージで表示
  bat_per = bat_per_inclination * bat_vol + bat_per_intercept;    // %
  if(bat_per > 100.0f){
      bat_per = 100;
  }
  M5.Lcd.printf("%.0f%% \r\n", bat_per);

  status_code = valuePost("CO2", ppm);  
  Serial.printf("Status code CO2: %d\r\n", status_code);
  status_code = valuePost("Humidity", temp_hum_val[0]);  
  Serial.printf("Status code Humidity: %d\r\n", status_code);
  status_code = valuePost("Temperature", temp_hum_val[1]);  
  Serial.printf("Status code Temperature: %d\r\n", status_code);
  
  M5.update();    
  delay(PERIOD * 1000);
}

int valuePost(String property, float value){
  HTTPClient http;
  http.begin(host+ property);
  http.addHeader("Content-type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("appKey", appkey);
  String body = "{\"" + property +"\": "+ String(value)+"}";
  int status_code = http.PUT(body);
  return status_code;
}
