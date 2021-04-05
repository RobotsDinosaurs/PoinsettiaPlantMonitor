//**********************************
// Moisture Monitor
//
// Measuring moisure and displaying the value on 
// an LCD display. 
// Using DEEP SLEEP mode on ESP32 to save power.
// Send an email alert if the soil is dry.
//
//**********************************
//
// Connections for I2C LCD16x2:
// SDA - SDA
// SCL - SCL
// VCC - 3.3V/5V
// GND - GND
//
//
// Connections for Moisture Sensor:
// GND - GND
// VCC - GPIO12
// AOUT - GPIO34
//
//**********************************

#include <Arduino.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,20,4);  

// Sensor pins
#define sensorPower 12
#define sensorPin 34

int value = 0;
char printBuffer[128];
char emailText[128];

// moisture sensor calibration values
const int airValue = 3207;   // value in the air 
const int waterValue = 1475;  // value in a cup full of water with fertilizer
int soilMoisturePercentage = 0;

// Deep Sleep parameters
#define AWAKE_TIME_MINS 1
#define NUMBER_OF_MEASUREMENTS 5
// 12 hours = 720 mins
// 720 ULL - force unsigned long long, otherwise overflow
#define SLEEP_TIME_MINS 720ULL

// Wi-fi/E-mail parameters
#define WIFI_SSID "TODO"
#define WIFI_PASSWORD "TODO"

// To send Email using Gmail use port 465 (SSL) and SMTP Server smtp.gmail.com
// YOU MUST ENABLE less secure app option https://myaccount.google.com/lesssecureapps?pli=1
#define AUTHOR_EMAIL "TODO"
#define AUTHOR_PASSWORD "TODO"
#define EMAIL_RECIPIENT "TODO"
#define EMAIL_RECIPIENT_NAME "TODO"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define EMAIL_SUBJECT "Please water me!"
#define EMAIL_SENDER "Poinsettia"

// The SMTP Session object used for Email sending 
SMTPSession smtp;

// Callback function to get the Email sending status 
void smtpCallback(SMTP_Status status);


// Method to print the reason by which ESP32
// has been awaken from sleep
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

// Connect to Wi-fi
void connectWifi()
{
  Serial.print("Connecting to AP");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(200);
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status)
{
  // Print the current status 
  Serial.println(status.info());
}

// Set the timezone, callback, send the message
void sendEmail()
{
  Serial.println("Connect to NTP server and set the device time\r\nPlease wait...\r\n");
  float timeZone = -4;// GMT-4 (Toronto, New York)
  float daylightOffset = 0;
  
  // Set the device time 
  MailClient.Time.setClock(timeZone, daylightOffset);

  // Enable the debug via Serial port
  // none debug or 0
  // basic debug or 1
  smtp.debug(0);

  // Set the callback function to get the sending results 
  smtp.callback(smtpCallback);

  Serial.println("Preparing to send email");
  Serial.println();

  // Declare the session config data
  ESP_Mail_Session session;

  // Set the session config 
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;

  // Declare the message class 
  SMTP_Message message;

  // Set the message headers 
  message.sender.name = EMAIL_SENDER;
  message.sender.email = AUTHOR_EMAIL;
  message.subject = EMAIL_SUBJECT;
  message.addRecipient(EMAIL_RECIPIENT_NAME, EMAIL_RECIPIENT);

  message.text.content = emailText;

  // Connect to server with the session config 
  if (!smtp.connect(&session))
    return;

  // Start sending Email and close the session 
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}

void setup()
{  
  Serial.begin(115200);
  Serial.println();
  
  //Print the wakeup reason for ESP32
  print_wakeup_reason();
  
  // First we configure the wake up source as timer
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_MINS * 60 * 1000000); // convert to uS
  
  // initialize LCD
  lcd.init();

  // turn on LCD backlight                      
  lcd.on();
  lcd.backlight();

  pinMode(sensorPower, OUTPUT);

  // set cursor to first column, first row
  lcd.setCursor(0, 0);

  // print message
  lcd.print("Soil Moisture:");

  for (int i = 0; i < NUMBER_OF_MEASUREMENTS; i++)
  {
    digitalWrite(sensorPower, HIGH);  // Turn the sensor ON
    delay(1000); // wait 1000 milliseconds

    value = analogRead(sensorPin); // get adc value
    sprintf(printBuffer, "Value: %d", value);      
    Serial.println(printBuffer);

    // Do not power the sensor constantly, but power it only when you take the readings.    
    digitalWrite(sensorPower, LOW);   // Turn the sensor OFF

    soilMoisturePercentage = map(value, airValue, waterValue, 0, 100);
    sprintf(printBuffer, "%d%%", soilMoisturePercentage);      
      
    int interval = (airValue - waterValue)/3; // interval for 3 soil moisture values : Dry/Moist/Wet

    if (value<=waterValue + interval){ 
      sprintf(printBuffer + strlen(printBuffer), " Wet!  "); 
    }
    else if (value>waterValue+interval && value<=airValue - interval){ 
      sprintf(printBuffer + strlen(printBuffer), " Moist  "); 
    }
    else if (value>airValue - interval){ 
      sprintf(printBuffer + strlen(printBuffer), " Dry!  "); 

      // for the last measurement, send email if dry
      if (i == NUMBER_OF_MEASUREMENTS - 1)
      {
        sprintf(emailText, "Soil moisture is: %s", printBuffer);
        
        // send an email
        connectWifi();
        sendEmail();
  
        // turn off Wi-Fi
        WiFi.disconnect();
        Serial.println("");
        Serial.println("WiFi disconnected.");
      }
    }

    Serial.println(printBuffer);
    // set the cursor to column 0, line 1
    // (note: line 1 is the second row, since counting begins with 0):
    lcd.setCursor(0, 1);
    // print the soil moisture percentage and condition
    lcd.print(printBuffer);

    // delay before next measurement, convert to ms
    delay((AWAKE_TIME_MINS * 60 * 1000)/NUMBER_OF_MEASUREMENTS);
  }

  // turn off LCD
  lcd.clear();
  lcd.noBacklight();
  lcd.off();

  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop()
{
  //This is not going to be called
}