#include "HX711.h"

#include <WiFi.h>
#include <time.h>

const char* ssid = "CYJ"; // 替换为你的WiFi名称
const char* password = "88888888"; // 替换为你的WiFi密码
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; // 东八区
const int daylightOffset_sec = 0;

// 封装获取时间戳的函数
long getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return -1;
    }
    return mktime(&timeinfo);
}


float drinkingWater(float weight) {
    // 假设每升水的重量为1000克
    return weight / 1000.0; // 返回升数
}

#define CACHE_SIZE 100
float dataCache[CACHE_SIZE];
int cacheIndex = 0;
float Weight = 0;

void setup()
{
	Init_Hx711();				//初始化HX711模块连接的IO设置

	Serial.begin(115200);
	Serial.print("Welcome to use!\n");

	delay(3000);
	Get_Maopi();		//获取毛皮



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
	  Weight = Get_Weight();	//计算放在传感器上的重物重量(mg)
    // 将数据存入缓存
    dataCache[cacheIndex] = Weight;
    cacheIndex = (cacheIndex + 1) % CACHE_SIZE;

    Serial.print(float(Weight),3);	//串口显示重量
	  Serial.print(" ml\n");	//显示单位
	  Serial.print("\n");		//显示单位

    long timestamp = getTimestamp();
    if (timestamp != -1) {
      Serial.print("Timestamp: ");
      Serial.println(timestamp);
    }

	
	delay(500);				//延时10ms

}