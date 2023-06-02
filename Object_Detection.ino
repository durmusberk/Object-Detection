#include <WiFi.h>                //Load Wifi Library
#include <WiFiClientSecure.h>    //Creates a client that can connect to to a specified internet IP address
#include "esp_camera.h"
#include "soc/soc.h"             //Disble Brownout Problems
#include "soc/rtc_cntl_reg.h"    //Disble Brownout Problems

const char* ssid     = "Zyphere";   //Network SSID
const char* password = "123456780";   //Network Password

const char* apssid = "ESP32-CAM";     //AP SSID
const char* appassword = "12345678";  //AP Password

String Feedback="";
String Command="";
String cmd="";
String P1="";
String P2="";

byte ReceiveState=0;
byte cmdState=1;
byte strState=1;
byte questionstate=0;
byte equalstate=0;
byte semicolonstate=0;

//ESP32-CAM Model AI Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WiFiServer server(80); //Set Web Server port number


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 (lower number = higher quality)
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 (lower number = higher quality)
    config.fb_count = 1;
  }
  
  //Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(500);
    ESP.restart();
  }

  //Set the frame size
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);  

  ledcAttachPin(4, 4);  
  ledcSetup(4, 5000, 8);    
  
  WiFi.mode(WIFI_AP_STA);
  delay(1000);

  //Connect to Wifi Network with SSID and Password
  Serial.println("");
  Serial.print("Connecting to ");
  WiFi.begin(ssid, password);
  Serial.println(ssid);
  long int StartTime=millis();
  while (WiFi.status() != WL_CONNECTED) 
  {
      delay(500);
      if ((StartTime+10000) < millis()) break;
  } 

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.softAP((WiFi.localIP().toString()+"_"+(String)apssid).c_str(), appassword);
    Serial.println("");
    Serial.println("STAIP address: ");
    Serial.println(WiFi.localIP());  

    for (int i=0;i<5;i++) {
      ledcWrite(4,10);
      delay(200);
      ledcWrite(4,0);
      delay(200);    
    }         
  }
  else {
    WiFi.softAP((WiFi.softAPIP().toString()+"_"+(String)apssid).c_str(), appassword);         

    for (int i=0;i<2;i++) {
      ledcWrite(4,10);
      delay(200);
      ledcWrite(4,0);
      delay(200);    
    }
  }     
  
  Serial.println("");
  Serial.println("APIP address: ");
  Serial.println(WiFi.softAPIP());    

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);  

  server.begin();          
}

// Display the HTML web page
static const char PROGMEM INDEX_HTML[] = R"rawliteral( 
  <!DOCTYPE html>
  <head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <script src="https:\/\/ajax.googleapis.com/ajax/libs/jquery/1.8.0/jquery.min.js"></script>
  <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow/tfjs@1.3.1/dist/tf.min.js"> </script>
  <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow-models/coco-ssd@2.1.0"> </script>   
  </head>
  <body>
  </html> 
  
  <script>
    var myTimer;
    var restartCount=0;
    var Model;
    const img = new Image();
    var tensor;
    img.onload = () => {
      tensor= tf.browser.fromPixels(img);
      clearInterval(myTimer);
      restartCount=0;
      if (Model)
      {
        DetectImage();
      }
    };

    function getStill() {
      clearInterval(myTimer);  
      myTimer = setInterval(function(){error_handle();},5000);
      img.src=location.origin+'/?getstill='+Math.random();
    }
    

    function error_handle() {
      restartCount++;
      clearInterval(myTimer);
      if (restartCount<=2) {
        console.log("Get still error. Restart ESP32-CAM "+restartCount+" times.");
        myTimer = setInterval(function(){getStill();},10000);
      }
      else{
        console.log("Get still error.Please close the page and check ESP32-CAM.");
      }
    }    
    
    function ObjectDetect() {
      console.log("Wait for loading model.");
      cocoSsd.load().then(cocoSsd_Model => {
        Model = cocoSsd_Model;
      }); 
      console.log("done");
      getStill();
    }
    
    function DetectImage() {
      Model.detect(img).then(Predictions => {    
        var objectCount=0;
        
        if (Predictions.length>0) {
          for (var i=0;i<Predictions.length;i++) {
            if (Predictions[i].class=="person") {
              objectCount++;
              fetch(document.location.origin+'/?flash='+255+';stop');
              setTimeout(function(){fetch(document.location.origin+'/?flash=0;stop');},100);
            }  
          }
        }
          if (objectCount>0) {
            setTimeout(function(){fetch(document.location.origin+'/?print='+"person"+';'+String(objectCount)+';stop');},100);
          } 
        try { 
          setTimeout(function(){getStill();},250);
        }
        catch(e) { 
          setTimeout(function(){getStill();},150);
        } 
      });
    }
        
    window.onload = function () {
      ObjectDetect();
    }    
  </script>      
)rawliteral";



void loop() {
  Feedback="";
  Command="";
  cmd="";
  P1="";
  P2="";
  
  ReceiveState=0;
  cmdState=1;
  strState=1;
  questionstate=0;
  equalstate=0;
  semicolonstate=0;
  
   WiFiClient client = server.available(); // Listen for incoming clients

  if (client) { 
    String currentLine = "";  // make a String to hold incoming data from the client

    while (client.connected()) {  // loop while the client's connected
      if (client.available()) {   // if there's bytes to read from the client
        char c = client.read();   // read a byte           
        
        getCommand(c);
                
        if (c == '\n') { // if the byte is a newline character
          if (currentLine.length() == 0) {    
            
            if (cmd=="getstill") {
              camera_fb_t * fb = NULL;
              fb = esp_camera_fb_get();  
              if(!fb) {
                Serial.println("Camera capture failed");
                delay(500);
                ESP.restart();
              }
  
              client.println("HTTP/1.1 200 OK");
              client.println("Access-Control-Allow-Origin: *");              
              client.println("Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept");
              client.println("Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS");
              client.println("Content-Type: image/jpeg");
              client.println("Content-Disposition: form-data; name=\"imageFile\"; filename=\"picture.jpg\""); 
              client.println("Content-Length: " + String(fb->len));             
              client.println("Connection: close");
              client.println();
              
              uint8_t *fbBuf = fb->buf;
              size_t fbLen = fb->len;
              for (size_t n=0;n<fbLen;n=n+1024) {
                if (n+1024<fbLen) {
                  client.write(fbBuf, 1024);
                  fbBuf += 1024;
                }
                else if (fbLen%1024>0) {
                  size_t remainder = fbLen%1024;
                  client.write(fbBuf, remainder);
                }
              }  
              
              esp_camera_fb_return(fb);
            
              pinMode(4, OUTPUT);
              digitalWrite(4, LOW);               
            }
            else {
              client.println("HTTP/1.1 200 OK");
              client.println("Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept");
              client.println("Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS");
              client.println("Content-Type: text/html; charset=utf-8");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close");
              client.println();
              
              String Data="";
              if (cmd!="")
                Data = Feedback;
              else {
                Data = String((const char *)INDEX_HTML);
              }
              int Index;
              for (Index = 0; Index < Data.length(); Index = Index+1000) {
                client.print(Data.substring(Index, Index+1000));
              }           
              
              client.println();
            }
                        
            Feedback="";
            break;
          } else {
            currentLine = "";
          }
        } 
        else if (c != '\r') {
          currentLine += c;
        }

        if ((currentLine.indexOf("/?")!=-1)&&(currentLine.indexOf(" HTTP")!=-1)) {
          if (Command.indexOf("stop")!=-1) {
            client.println();
            client.println();
            client.stop();
          }
          currentLine="";
          Feedback="";
          if (cmd=="print") {
            Serial.println(P1+" = "+P2);
          }
          else if (cmd=="flash") {
            ledcAttachPin(4, 4);  
            ledcSetup(4, 5000, 8);   
            
            int val = P1.toInt();
            ledcWrite(4,val);  
          }  
          else if (cmd!="getstill") {
            Serial.println("");
          }
          else {
            Serial.println("");
          }
        }
      }
    }
    delay(1);
    client.stop();
  }
}

void getCommand(char c)
{
  if (c=='?') ReceiveState=1;
  if ((c==' ')||(c=='\r')||(c=='\n')) ReceiveState=0;
  
  if (ReceiveState==1)
  {
    Command=Command+String(c);
    
    if (c=='=') cmdState=0;
    if (c==';') strState++;
  
    if ((cmdState==1)&&((c!='?')||(questionstate==1))) cmd=cmd+String(c);
    if ((cmdState==0)&&(strState==1)&&((c!='=')||(equalstate==1))) P1=P1+String(c);
    if ((cmdState==0)&&(strState==2)&&(c!=';')) P2=P2+String(c);
    if (c=='?') questionstate=1;
    if (c=='=') equalstate=1;
    if ((strState>=9)&&(c==';')) semicolonstate=1;
  }
}