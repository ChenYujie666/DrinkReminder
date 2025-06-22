#include "HX711.h"

#include <WiFi.h>
#include <time.h>
#include <ESP32Time.h>
#include <iostream>

#include <unordered_map>
#include <LittleFS.h>

#include <WebServer.h>

const char *ssid = "CYJ";          // 替换为你的WiFi名称
const char *password = "88888888"; // 替换为你的WiFi密码
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; // 东八区
const int daylightOffset_sec = 0;

int lastWeight = 0; // 上次读取的重量
int curWeight = 0;  // 当前读取的重量
int weight0Count = 0;
int weightThreshold = 50; // 重量阈值，单位为mg 杯子的重量
int previousWeight = 0;   // 上次有效重量

#define LED_PIN 16 // LED连接的引脚
std::unordered_map<int, int> weightCountMap;

long timestamp;
ESP32Time rtc;

WebServer server(80);

// 连接WiFi
bool connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    Serial.print("连接WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("\nWiFi连接失败");
        return false;
    }

    Serial.println("\nWiFi已连接!");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    return true;
}

void read_data()
{
    Serial.println("start to read data");
    // 读文件
    File file = LittleFS.open("/data.txt", "r");
    if (file)
    {
        while (file.available())
        {
            String line = file.readStringUntil('\n');
            weightCountMap[line.substring(0, 10).toInt()] = line.substring(11).toInt();
        }
        file.close();
    }
    else
    {
        Serial.println("failed to read data");
    }
    Serial.println("end to read data");
}
void save_data(int ts, int water)
{
    // 写文件
    File file = LittleFS.open("/data.txt", "a");
    if (file)
    {
        file.print(ts);
        file.print(",");
        file.println(water);
        file.close();
    }
    else
    {
        Serial.println("failed to append data !");
    }
}
void delete_data()
{

    if (LittleFS.remove("/data.txt"))
    {
        Serial.println("文件已删除");
    }
    else
    {
        Serial.println("删除文件失败");
    }
}

// 列出所有文件
void listFiles()
{
    String html = "<html><body><h1>ESP32 file system</h1>";
    html += "<form method='post' action='/upload' enctype='multipart/form-data'>";
    html += "<input type='file' name='upload'>";
    html += "<input type='submit' value='upload'>";
    html += "</form><hr><table border='1'>";

    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        html += "<tr>";
        html += "<td>" + String(file.name()) + "</td>";
        html += "<td>" + String(file.size()) + " bytes</td>";
        html += "<td><a href='/view?file=" + String(file.name()) + "'>view</a></td>";
        html += "<td><a href='/download?file=" + String(file.name()) + "'>download</a></td>";
        html += "<td><a href='/delete?file=" + String(file.name()) + "'>delete</a></td>";
        html += "</tr>";
        file = root.openNextFile();
    }

    html += "</table></body></html>";
    server.send(200, "text/html", html);
}

// 查看文件内容
void viewFile()
{
    String path = server.arg("file");

    if (!path.startsWith("/"))
    {

        path = "/" + path;
    }
    if (LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "r");
        String content = file.readString();
        server.send(200, "text/plain", content);
        file.close();
    }
    else
    {
        server.send(404, "text/plain", "file not exist! ");
    }
}

// 下载文件
void downloadFile()
{
    String path = server.arg("file");

    if (!path.startsWith("/"))
    {
        path = "/" + path;
    }

    if (LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "r");
        server.sendHeader("Content-Type", "application/octet-stream");
        server.sendHeader("Content-Disposition", "attachment; filename=" + path);
        server.streamFile(file, "application/octet-stream");
        file.close();
    }
    else
    {
        server.send(404, "text/plain", "file not exist! ");
    }
}

// 处理文件上传
void handleFileUpload()
{
    HTTPUpload &upload = server.upload();
    File uploadFile;
    if (upload.status == UPLOAD_FILE_START)
    {
        // 删除已存在的同名文件
        if (LittleFS.exists(upload.filename))
        {
            LittleFS.remove(upload.filename);
        }
        uploadFile = LittleFS.open(upload.filename, "w");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile)
        {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
        {
            uploadFile.close();
        }
    }
}

// 删除文件
void deleteFile()
{
    String path = server.arg("file");

    if (!path.startsWith("/"))
    {
        path = "/" + path;
    }
    if (LittleFS.exists(path))
    {
        LittleFS.remove(path);
        server.sendHeader("Location", "/");
        server.send(303);
    }
    else
    {
        server.send(404, "text/plain", "文件不存在");
    }
}

void startWebServer()
{

    // 设置Web服务器路由
    server.on("/", HTTP_GET, []()
              { listFiles(); });

    server.on("/view", HTTP_GET, []()
              { viewFile(); });

    server.on("/download", HTTP_GET, []()
              { downloadFile(); });

    server.on("/upload", HTTP_POST, []()
              { server.send(200); }, handleFileUpload);

    server.on("/delete", HTTP_GET, []()
              { deleteFile(); });
    server.enableCORS();
    server.begin();
}
void setup()
{

    Serial.begin(115200);
    Serial.print("Initializing... \n");

    Init_Hx711(); // 初始化HX711模块连接的IO设置
    pinMode(LED_PIN, OUTPUT);

    delay(100);
    Get_Maopi(); // 获取毛皮

    // ESP32 WIFI initialization
    // sync Time From NTP
    if (connectWiFi())
    {
        Serial.println("Wi-Fi connected");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            rtc.setTimeStruct(timeinfo);
        }
    }
    else
    {
        Serial.println("failed to connected Wi-Fi");
    }

    // 初始化 LittleFS
    if (!LittleFS.begin(true))
    { // true 表示如果挂载失败则格式化
        Serial.println("LittleFS 挂载失败!");
        // return;
    }
    else
    {
        Serial.println("LittleFS 挂载成功");
    }
    read_data();

    startWebServer();

    Serial.print("Initialized Done! \n");
}

void loop()
{
    timestamp = rtc.getLocalEpoch();
    if (timestamp % 3600 >= 1800 && weightCountMap[timestamp / 3600] <= 300)
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
    }

    curWeight = Get_Weight(); // 计算放在传感器上的重物重量(mg)
    while (curWeight != Get_Weight())
    {
        curWeight = Get_Weight();
    }

    if (curWeight <= 50)
    {
        curWeight = 0;
        weight0Count++;
        // return;
    }
    else
    {
        if (weight0Count >= 3)
        {
            Serial.print("lastWeight = ");
            Serial.print(lastWeight);
            Serial.print("\t");

            Serial.print("curWeight = ");
            Serial.print(curWeight);
            Serial.print("\n");

            while (curWeight != Get_Weight())
            {
                curWeight = Get_Weight();
            }
            int diff = lastWeight - curWeight;
            if (diff > 0)
            {
                Serial.print("Weight decreased by: ");
                Serial.print(diff);
                Serial.print("\n");
                weightCountMap[timestamp / 3600] += diff;
                save_data(timestamp, diff);
            }
            else
            {
                Serial.print("add water: ");
            }

            weight0Count = 0;
        }
        lastWeight = curWeight; // 更新上次重量
    }

    server.handleClient();

    delay(1000);
}