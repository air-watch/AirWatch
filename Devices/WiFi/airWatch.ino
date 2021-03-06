#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_system.h>
#include <driver/adc.h>
#include <Adafruit_BME280.h>

#define USE_SERIAL Serial
#define uS_TO_S_FACTOR 1000000 
#define ADC_MAX_VALUE 4095U
#define ADC_MIN_VALUE 0U
#define ADC_SAMPLES 3U
#define ADC_SAMPLE_DELAY 100U


boolean enableSleep = false;    // set to true to sleep between measurments. Less power consumption if powering from external battery. 
                                // if sleep is set, then the RGB led will not be powered on when sleeping. 
                                // if sleep it not set, then the RGB led will stay on indicating the air quality (green = very good/ good, blue = fair , red = poor, red flashing = very poor )


Adafruit_BME280 bme280;

char airWatchCode[16] = "xxxxxxxxxxxxxxx";      // AirWatch device key. Get this from your AirWatch account. If sending data to your own server, not required. 


int attemptsCount = 0;                  // we track our attempts at connecting to wifi 
int wake;                               // we store the wakeup reason, which is used to determine program logic
boolean interruptReceived = false;      // set to true if we received an interrupt
bool smartconfigDone = false;           // set to true when the smartconfig process has been completed




int red = 0;
int green = 2;
int blue = 15;
 
 
 
 

 

int pmsSetPin = 4;                     // must set this pin low to put the pms5003 to sleep, driven by pin 4 / GPIO4 on wemos


#define TIME_TO_SLEEP  300             // sleep for 300 seconds (5 minutes)


// PMS5003 sensor variables 

#define LENG 31                       //0x42 + 31 bytes equal to 32 bytes
unsigned char buf[LENG];

int PM01Value=0;          //define PM1.0 value of the air detector module
int PM2_5Value=0;         //define PM2.5 value of the air detector module
int PM10Value=0;          //define PM10 value of the air detector module



// Create a hardware timer. We use this timer when the device goes into provisioning mode due to not being able to connect to the WIFI network. 
// The timer will ensure that after staying some time in provisioning mode (if the device cannot connect to a WIFI network it will always go into provisioing mode) it will restart and try again. 
// This will support a scenario where the WIFI network is down, example router switched off, the device cannot connect so goes
// into provisioning mode, but then the router is switched on and WIFI is restored. So now the device will restart and connect to the WIFI. 


hw_timer_t * timer = NULL;  

void IRAM_ATTR onTimer()
{
  Serial.println("interrupt recevied......");
  interruptReceived = true;
  Serial.println("interruptRecevied variable set to ......");
  Serial.println(interruptReceived);
}






void setup() 
{
//  put your setup code here, to run once
  
    Serial.begin(9600);   //use serial 0 for debug print 
    Serial.setTimeout(1500); 
    Serial.println("Starting now");
    
//  the wake reason allows us to know what woke the esp32 up, power on, ext int or other reason
    wake  = wakeup_reason1();
    Serial.println("Wakeup reason is: ");                      
    Serial.println(wake); 
    
//  initialize the pins used for the RGB led
    pinMode(red, OUTPUT);
    pinMode(green, OUTPUT);
    pinMode(blue, OUTPUT);

//  initialize the pin used to control PMS5003 
    pinMode(pmsSetPin, OUTPUT);
    delay(2000); // should here fan for 2 seconds before we turn it off
    Serial.println("Set PMS SET Pin LOW to turn off as its using serial output......");
    digitalWrite(pmsSetPin, LOW);

//  Turn off RGB led
    digitalWrite(red, HIGH);
    digitalWrite(green, HIGH);
    digitalWrite(blue, HIGH);
    delay(2000);


    flash_led(blue,3);


  
    int counter = 0;
    int counter2 = 0;
    
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    while (WiFi.status() != WL_CONNECTED) 
    
    {
        delay(2000);
        Serial.print(".");
        counter += 1;
        
              if(counter == 10)
              {
              
                Serial.println("Counter is 10, failed to connect to a WIFI network..."); 
                Serial.println("exit while loop");
                break;
              }

    }





    if((WiFi.status() == WL_CONNECTED) )
    {
        Serial.println("WiFi Connected.");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        flash_led(green,3);     // Indicates successfully connected to WIFI
        delay(2000);

      
        


        readSensorsAndSendEvent();    // Read the values and send over HTTP to AirWatch or any other server

      
        

        if(enableSleep == true)
        {
        Serial.println("Now go to sleep for 5 mins");
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
        }
        else
        {
          // keep RGB indicator led on, so dont sleep, just wait 5 mins then restart
        delay(300000); 
        esp_restart();  
        }



    }                 
    else            // The device has not been able to connect to a WIFI network and get an IP address. It could be the first time on this WIFI. Go into provisioning mode. 
    {
              
              flash_led(red,3);     // flash 3 reds to indicate unable to connect to WIFI. This is normal if its the first time trying to connect since it doesnt know WIFI login credentials.
              delay(2000);  
              Serial.println("Unable to connect to any WIFI..");

             

              // Set up a 5 minute timer. Once 5 minutes in Provisioning mode has expired, reset the device.  
              // This is to support the scenario where the router for example is switched off or there is a problem with ISP / Internet at home. 
              // Then when te device resets after 5 minutes sleep, it cannot connect to the WIFI. 
              // So it enters provisioning mode. But then the router is switched on and WIFI becomes available. Without this timer we would just stay in Provisioing mode forever. 
              // With the timee the device will exit provisoining mode and re attempt to connect every 5 minutes. So when router turned back on, AirWatch device will connect. 
              
              /* Set up 5 minute timer. Use 1st timer of 4 */
              /* 1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up */
              timer = timerBegin(0, 80, true);
              /* Attach onTimer function to our timer */
              timerAttachInterrupt(timer, &onTimer, true);
              /* Set alarm to call onTimer function every second 1 tick is 1us
              => 1 second is 1000000us */
              /* Repeat the alarm (third parameter) */
              timerAlarmWrite(timer, 300000000, true);
              /* Start an alarm */
              timerAlarmEnable(timer);
              Serial.println("Started timer");
              Serial.println("We have setup an interrupt to happen in 5 minutes. In the meantime go into smartconfig mode..");
              Serial.println("Start SmartConfig.");
              
              

            

              
              WiFi.beginSmartConfig();
              /* Wait for SmartConfig packet from mobile */
              Serial.println("Waiting for SmartConfig.");
              while (!WiFi.smartConfigDone())                               // wait in this loop until smartconfig is complete (i.e user provides wifi details via app, OR the interrupt previosuly set for 5 minutes occured
              {                                                             
                  delay(500);
                  Serial.print(".");
                  if(interruptReceived == true)                             // we have been 5 minutes in Prov mode, reset and attempt to connect to WIFI
                  {
                    Serial.print("Back to while loop after interrupt occured, exit loop");
                    break;
                  }
                  else
                  {
                    Serial.print("*");      // waiting 

                  // Toggle LED between blue and red to indicate provosioning mode
                  digitalWrite(blue, LOW); 
                  delay(500);
                  digitalWrite(blue, HIGH); 
                  digitalWrite(red, LOW); 
                  delay(500);
                  digitalWrite(red, HIGH); 


                  
              
                  }
                  
              }

              // we have exited the above while loop by a smart config done, I.E user supplied wifi details via the mobile app
                  
                  if(WiFi.smartConfigDone())
                  {
                    Serial.println("");
                    Serial.println("SmartConfig done.");

                    // RED/Blue LED off to indicate provosioning mode complete
                    digitalWrite(blue, HIGH); 
                    digitalWrite(red, HIGH);
                    
                    delay(2000);

                   
                    /* Wait for WiFi to connect to AP */
                    Serial.println("Waiting for WiFi");
                    while (WiFi.status() != WL_CONNECTED) 
                    {
                    
                    delay(2000);
                    Serial.print("!");
                    counter2 += 1;
                    
                        if(counter2 == 10)
                        {
                          Serial.println("Counter2 is 10, failed to connect to a WIFI network after smart config done.. restart "); 
                          flash_led(red,10);
                          esp_restart(); 
                        }
                    }

                    
                    Serial.println("WiFi Connected.");
                    Serial.print("IP Address: ");
                    Serial.println(WiFi.localIP());
                    flash_led(green,10);
                    Serial.print("RESTART ...................should then find wifi and go through normal program execution");
                    esp_restart(); 
                    
                  }
                  else
                  {
                    Serial.println("SmartConfig NOT done, here due to interrupt. Restart. WIFI may have been down and is back now.");
                    
                  }
              
              
              
    }


  
}





void loop()
{

  if(interruptReceived == true)
  {
   // we are here after a 5 minute timeout in provisioning mode occured. Retry, the WIFI may have been down, and now back!
   
    Serial.println("interruptRecevied variable set to ......");
    Serial.println(interruptReceived);
    Serial.println("Disable Timer......");
    timerAlarmDisable(timer);
    Serial.println("Issue a restart in 1 seconds......");
    delay(1000);
    esp_restart(); 
  }
  else
  {
    Serial.println("~~");
    delay(1000);
  }
  
}















void flash_led(int color, int numFlashes)
{
// ensure all are off

digitalWrite(red, HIGH);
digitalWrite(green, HIGH);
digitalWrite(blue, HIGH);

  for (int i = 0; i < numFlashes; i++) 
  {

    digitalWrite(color, LOW);     // turn on
    delay(300); 
    digitalWrite(color, HIGH);     // turn off
    delay(300); 
  }



delay(2000);  
}



// PMS5003 functions

char checkValue(unsigned char *thebuf, char leng)
{  
  char receiveflag=0;
  int receiveSum=0;

  for(int i=0; i<(leng-2); i++){
  receiveSum=receiveSum+thebuf[i];
  }
  receiveSum=receiveSum + 0x42;
 
  if(receiveSum == ((thebuf[leng-2]<<8)+thebuf[leng-1]))  //check the serial data 
  {
    receiveSum = 0;
    receiveflag = 1;
  }
  return receiveflag;
}




int transmitPM01(unsigned char *thebuf)
{
  int PM01Val;
  PM01Val=((thebuf[3]<<8) + thebuf[4]); //count PM1.0 value of the air detector module
  return PM01Val;
}






//transmit PM Value to PC
int transmitPM2_5(unsigned char *thebuf)
{
  int PM2_5Val;
  PM2_5Val=((thebuf[5]<<8) + thebuf[6]);//count PM2.5 value of the air detector module
  return PM2_5Val;
}




//transmit PM Value to PC
int transmitPM10(unsigned char *thebuf)
{
  int PM10Val;
  PM10Val=((thebuf[7]<<8) + thebuf[8]); //count PM10 value of the air detector module  
  return PM10Val;
}




// Returns reason why chip woke up

int wakeup_reason1() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  return wakeup_reason;
}




// Returns Mac Id of chip

String macToStr(const uint8_t* mac)
{
String result;
for (int i = 0; i < 6; ++i) {
result += String(mac[i], 16);
if (i < 5)
result += ':';
}
return result;
}





// MICS 6814 functions




bool mics6814_init(void) {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
  return true;
}





 
bool mics6814_read(uint16_t* no2, uint16_t* nh3, uint16_t* co) {
  bool result = true;
 
  if ((NULL == no2) || (NULL == nh3) || (NULL == co)) {
    result = false;
  }
 
  uint16_t tempNo2 = 0U;
  uint16_t tempNh3 = 0U;
  uint16_t tempCo = 0U;
  uint8_t count = 0U;
  while ((true == result) && (count < ADC_SAMPLES)) {
    if (count > 0U) {
      delay(ADC_SAMPLE_DELAY);
    }
 
    int temp = adc1_get_raw(ADC1_CHANNEL_6);
    if ((temp >= ADC_MIN_VALUE) && (temp <= ADC_MAX_VALUE)) {
      tempNo2 += (uint16_t)temp;
    } else {
      result = false;
    }
 
    if (true == result) {
      temp = adc1_get_raw(ADC1_CHANNEL_7);
      if ((temp >= ADC_MIN_VALUE) && (temp <= ADC_MAX_VALUE)) {
        tempNh3 += (uint16_t)temp;
      } else {
        result = false;
      }
    }
 
    if (true == result) {
      temp = adc1_get_raw(ADC1_CHANNEL_4);
      if ((temp >= ADC_MIN_VALUE) && (temp <= ADC_MAX_VALUE)) {
        tempCo += (uint16_t)temp;
        count++;
      } else {
        result = false;
      }
    }
  }
 
  if (true == result) {
    *no2 = ADC_MAX_VALUE - (tempNo2 / ADC_SAMPLES);
    *nh3 = ADC_MAX_VALUE - (tempNh3 / ADC_SAMPLES);
    *co = ADC_MAX_VALUE - (tempCo / ADC_SAMPLES);
  }
 
  return result;
}








void readSensorsAndSendEvent()
{




      Serial.println(F("Start the BME280 ...."));
      bme280.begin();
      Serial.println("Read BME280 values...");
      float temperature = bme280.readTemperature();
      float pressure = bme280.readPressure() / 100.0F;
      float humidity = bme280.readHumidity();
      Serial.println("Temp is : ");
      Serial.println(temperature);
      Serial.println("Pressure is : ");
      Serial.println(pressure);        
      Serial.println("Humidity is : ");
      Serial.println(humidity);
    


      Serial.println("Set PMS SET Pin HIGH and wait 20 seconds ......");
      digitalWrite(pmsSetPin, HIGH);
      delay(20000);



      //start to read the PMS5003 when detect 0x42 on serial 
      
      if(Serial.find(0x42))
      {    
        Serial.readBytes(buf,LENG);

        if(buf[0] == 0x4d){
          if(checkValue(buf,LENG)){
            PM01Value=transmitPM01(buf); //count PM1.0 value of the air detector module
            PM2_5Value=transmitPM2_5(buf);//count PM2.5 value of the air detector module
            PM10Value=transmitPM10(buf); //count PM10 value of the air detector module 
          }           
        } 
      }


      Serial.print("PM1.0: ");  
      Serial.print(PM01Value);
      Serial.println("  ug/m3");            
      Serial.print("PM2.5: ");  
      Serial.print(PM2_5Value);
      Serial.println("  ug/m3");     
      Serial.print("PM1 0: ");  
      Serial.print(PM10Value);
      Serial.println("  ug/m3");   
      Serial.println();
        
      
     // now read the gas sensor
     
     Serial.println("Setup gas sensor MICS6814");
     if ( !mics6814_init() ) 
     {
          Serial.println("Waiting to Setup gas sensor MICS6814");
          while(1);
     }

     uint16_t no2 = 0U;
     uint16_t nh3 = 0U;
     uint16_t co = 0U;
     mics6814_read(&no2, &nh3, &co);


     String clientMac = "";
     unsigned char mac[6];
     WiFi.macAddress(mac);
     clientMac += macToStr(mac);
     Serial.println("Mac ID is ..");
     Serial.println(clientMac);


     Serial.println("Send event to platform.. ");
     HTTPClient http;
     USE_SERIAL.print("[HTTP] begin...\n");

     // url format http://aServer.com/aScript.php?pms01=23&pms25=32&pm10=12&thingName=wemos04&serial=a238b327f8f334ed&temeperature=23.4&pressure=1036&humidity=45.2&no2=234&nh3=286&co=23
     
     String url = "http://airwatch.io/update/updateWIFI.php?pms01=";         // point to airwatch. Or update to point to your own server 

     url += PM01Value;
     url += "&pms25=";
     url += PM2_5Value;
     url += "&pms10=";
     url += PM10Value;
     url += "&thingName=xxxxxxxxxxxxx";
     url += "&serial=";
     url += clientMac;
     url += "&temperature=";
     url += temperature;
     url += "&pressure=";
     url += pressure;
     url += "&humidity=";
     url += humidity;
     url += "&no2=";
     url += no2;
     url += "&nh3=";
     url += nh3;
     url += "&co=";
     url += co;
     url += "&key=";
     url +=  airWatchCode;

 
     USE_SERIAL.print("URL is ...\n");
     USE_SERIAL.print(url);
     http.begin(url); //HTTP


     USE_SERIAL.print("[HTTP] GET...\n");
     
     // start connection and send HTTP header
     int httpCode = http.GET();


      if(httpCode > 0) 
      {
                  // HTTP header has been send and Server response header has been handled
                  USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

                  // file found at server
                  if(httpCode == HTTP_CODE_OK) 
                  {
                      String payload = http.getString();
                      USE_SERIAL.println("HTTO OK: http return code is");
                      USE_SERIAL.println(payload);
                      http.end();
                      flash_led(green,5);  // indicates the send was successfull
                  }
      } 
      else // connected to wifi but http get failed
      {
                USE_SERIAL.printf("[HTTP] GET... failed, error code: %s\n", http.errorToString(httpCode).c_str());
                USE_SERIAL.print("WIFI  connected, but update  failed.");
                http.end();
                flash_led(red,5);  // indicates the send failed
                
       }

  // set the led based on last reading of air quality. 

    if(PM2_5Value >= 0 &&  PM2_5Value <= 26.3)
    {
        // set RGB green, very good or good quality
        digitalWrite(green, LOW); 
    }
    else if(PM2_5Value >= 26.4 &&  PM2_5Value <= 39.9)
    {
        // set RGB blue , fair quality
        digitalWrite(blue, LOW); 
    }
    else if(PM2_5Value >= 40.0 )
    { 
       
        // set RGB red, poor quality or very poor quality
        digitalWrite(red, LOW); 
    }
    

        Serial.println("Set PMS SET Pin LOW to turn off fan, it only has 8000 hours of operation life span ......");
        digitalWrite(pmsSetPin, LOW);
        delay(2000);


    
  
}
