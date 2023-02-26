#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_AHTX0.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <ESP_Mail_Client.h>

const long utcOffsetInSeconds = 3600 * 2; // Timezone offset
const unsigned long sleepTime = (60 * 60000000); // Minutes

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSW"
#define SMTP_HOST "smtp..."
#define SMTP_PORT 465
#define AUTHOR_EMAIL "email@gmail.com"
#define AUTHOR_PASSWORD "password"			\\smtp credentials
#define RECIPIENT_EMAIL "email@gmail.com"

SMTPSession smtp; // The SMTP Session object used for Email sending

void smtpCallback(SMTP_Status status); // Callback function to get the Email sending status

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds, 60000);
RTC_DATA_ATTR float mData[24][3];
Adafruit_AHTX0 aht; // Temperature & humidity sensor
const int VPin = 34; // analog input for VoltMeter
float vout = 0.0;
float vin = 0.0;
float R1 = 100000;                // Resistor R1 100k 
float R2 = 10000;                 // Resistor R2 10k
int value = 0;
String $vin = "0";

void setup(){
  pinMode(VPin, INPUT);
  Serial.begin(115200);
  esp_sleep_enable_timer_wakeup(sleepTime);
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT20 found");
  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(200);
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  timeClient.begin(); // NTP start
  delay(500);
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int id = ptm->tm_hour;
  mData[id][0] = ptm->tm_min;

  String sOut = String(ptm->tm_mday) + "/";
  if ((ptm->tm_mon + 1) < 10) sOut += ("0" + String(ptm->tm_mon + 1));
  else sOut += (ptm->tm_mon + 1);
  sOut += " " + String(id) + ":";
  if(mData[id][0] < 10) sOut += "0" + String(int(mData[id][0]));
  else sOut += String(int(mData[id][0]));

  value = analogRead(VPin);
  vout = (value * 4.175) / 4095.0;   // mybe u must change the 3.3 to 5 and 
                                  //for the ESP32 u have to change the 1023 to 4095
  vin = vout / (R2/(R1+R2));
  $vin = String(vin);

  sensors_event_t humidity, temp; // HUM & TEMP module
  aht.getEvent(&humidity, &temp);
  String sTemp = String(temp.temperature);
  String sHum = String(humidity.relative_humidity);
  mData[id][1] = temp.temperature;
  mData[id][2] = humidity.relative_humidity;
  if (id < 7) esp_deep_sleep_start();
  String strHTML = "<div style=\"color:#2f4468;\"><h2> Temp.: " + String(mData[id][1]) + "° C<br>Humidity: ";
  strHTML += String(mData[id][2]) + "% rH </h2>Battery: " + vin + " Volts</div>";
  strHTML += "<table width = 100%><tr><td><b> Time </td><td> Temp. </td><td> Humid. </b></td></tr>";
  int idt = id;
  String colorT = "color = black>";
  String colorH = "color = black>";
  for (int i = 0; i < 24; i++) {
    int idc;
    if (idt == 0) idc = 23;
    else idc = idt - 1;
    if (mData[idt][1] > mData[idc][1]) colorT = "color = red>";
    else colorT = "color = blue>";
    if (mData[idt][2] > mData[idc][2]) colorH = "color = green>";
    else colorH = "color = orange>";
    String sHour = "0";
    String sMin = "0";
    if (idt < 10) sHour += String(idt);
    else sHour = String(idt);
    if (int(mData[idt][0]) < 10) sMin += String(int(mData[idt][0]));
    else sMin = String(int(mData[idt][0]));
    strHTML += "<tr><td>" + sHour + ":" + sMin + "</td><td> <font " + colorT + String(mData[idt][1]) + "° C </font></td>";
    strHTML += "<td> <font " + colorH + String(mData[idt][2]) + "% rH </font></td></tr>";
    idt--;
    if (idt < 0) idt = 23;
  }
  strHTML += "</table>";

  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1 */
  smtp.debug(0);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  ESP_Mail_Session session;

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "ESP Weather Station";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Weather at: " + sOut;
  message.addRecipient("Eugene", RECIPIENT_EMAIL);
  //message.addCc("marinatka@ya.ru");

  /*Send HTML message*/
  message.html.content = strHTML.c_str();
  message.html.content = strHTML.c_str();
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
   Serial.println("Error sending Email, " + smtp.errorReason());
  esp_deep_sleep_start();
}

void loop(){
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");
  }
}