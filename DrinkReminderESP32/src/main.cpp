#include "HX711.h"

#include <WiFi.h>
#include <time.h>
#include <unordered_map>

const char* ssid = "CYJ"; // 替换为你的WiFi名称
const char* password = "88888888"; // 替换为你的WiFi密码
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; // 东八区
const int daylightOffset_sec = 0;

int lastWeight = 0; // 上次读取的重量
int curWeight = 0; // 当前读取的重量
int weight0Count = 0;
int weightThreshold = 50; // 重量阈值，单位为mg 杯子的重量
int previousWeight = 0; // 上次有效重量
#define LED_PIN 16 // LED连接的引脚
std::unordered_map<int, int> weightCountMap;

// 封装获取时间戳的函数
long getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return -1;
    }
    return mktime(&timeinfo);
}




#define CACHE_SIZE 100
float dataCache[CACHE_SIZE];
int cacheIndex = 0;

void setup()
{
	Init_Hx711();				//初始化HX711模块连接的IO设置
  pinMode(LED_PIN, OUTPUT);
	Serial.begin(115200);
	Serial.print("Welcome to use!\n");

	delay(100);
	// Get_Maopi();		//获取毛皮

  // lastWeight = HX711_Read();

  // ESP32 WIFI initialization
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);



}

void loop()
{


    curWeight = Get_Weight();	//计算放在传感器上的重物重量(mg)
    while (curWeight != Get_Weight())
    {
        curWeight = Get_Weight();
    }

    long timestamp = getTimestamp();
    if (timestamp == -1) {
        Serial.println("Failed to get timestamp");
        // return;
    }
    
    if (curWeight <= 50){
        curWeight = 0;
        weight0Count++;
        return;
    }else{
        if (weight0Count >= 3){
          Serial.print("lastWeight = ");
          Serial.print(lastWeight);
          Serial.print("\t");

          Serial.print("curWeight = ");
          Serial.print(curWeight);
          Serial.print("\n");

          while(curWeight != Get_Weight())
          {
              curWeight = Get_Weight();
          }
          int  diff = lastWeight - curWeight;
          if (diff > 0) {
              Serial.print("Weight decreased by: ");
              Serial.print(diff);
              Serial.print("\n");
              weightCountMap[timestamp/3600]+=diff; 
          }else{
              Serial.print("add water: ");
          }

          weight0Count = 0;
        }
        lastWeight = curWeight; // 更新上次重量

    }
    

    Serial.print(timestamp);
    Serial.print(", ");
    Serial.print(float(curWeight),3);	//串口显示重量
	  Serial.print(" ml\n");	//显示单位
	

    if (timestamp%3600 >= 1800 && weightCountMap[timestamp/3600] <= 300){
        digitalWrite(LED_PIN, HIGH); 
    }else{
        digitalWrite(LED_PIN, LOW); 
    }
    delay(1000);				//延时10ms

}