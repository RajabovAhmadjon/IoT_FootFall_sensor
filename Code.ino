#include <ESP8266WiFi.h>    
#include <rgb_lcd.h>        // include the grove RGB LCD library
#include <ESP8266mDNS.h>    // mDNS library for creating local server with domain name
#include <ESP8266WebServerSecure.h> // library which make secure or connection
#include "Adafruit_MQTT.h"      
#include "Adafruit_MQTT_Client.h" 

/*********************************** Hardware defines ***********************************************

 ***************************************************************************************************/
#define LEDBUTPIN 15

#define TASK1L 5
#define TASK2L 1000
#define TASK3L 60000

/******************************* Network defines ************************************************

 ***************************************************************************************************/
#define ssid          "Akhmadjon"    // your hotspot SSID or name
#define password      "12345678"    // your hotspot password
#define domainName    "ahmadjon"

/*********************************** Adafruit IO defines*********************************************

 ***************************************************************************************************/
#define NOPUBLISH      // comment this out once publishing at less than 10 second intervals

#define ADASERVER     "io.adafruit.com"     // do not change this
#define ADAPORT       8883                  // port 1883 non-secure, and port 8883 is secure
#define ADAUSERNAME   "Akhmadjon"               // ADD YOUR username here between the qoutation marks
#define ADAKEY        "aio_Svqc26DWaEYZrVBaH5haztUPMtH8" // ADD YOUR Adafruit key here betwwen marks

/******************************** Global instances / variables***************************************

 ***************************************************************************************************/
WiFiClientSecure client;    // create a class instance for the MQTT server
rgb_lcd LCD;          // create a class instance of the rgb_lcd class

Adafruit_MQTT_Client MQTT(&client, ADASERVER, ADAPORT, ADAUSERNAME, ADAKEY);
ESP8266WebServer SERVER (80); //create a global instance of the ESP8266WebServerSecure class
//443 is a parameter for the class constructor, configures the server to operate on port 443,port typically used by HTTPS (port 80 used for HTTP which offers no encryption or autentication)
Adafruit_MQTT_Subscribe *subscription;                    // create a subscriber object instance

/******************************** Feeds *************************************************************

 ***************************************************************************************************/
Adafruit_MQTT_Subscribe RESET_TOTAL = Adafruit_MQTT_Subscribe(&MQTT, ADAUSERNAME "/feeds/reset-total");

Adafruit_MQTT_Publish PUBLISH_TOTAL = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/total");
Adafruit_MQTT_Publish PUBLISH_AVERAGE = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/average");

/******************************* Function prototypes ************************************************

 ***************************************************************************************************/
void MQTTS_Connect ( void );
void Wifi_Connection( void );
void HTTP_Server ( void );
void Serve_Page_Footfall ( void );
void Respond( void ); ///function will contain our response to a HTTPS request
int Check_Button ( void );
void Task1 ( void );
void Task2 ( void );
void Task3 ( void );

/******************************** Variables *************************************************************

 ***************************************************************************************************/
static const char *fingerprint PROGMEM = "77 00 54 2D DA E7 D8 03 27 31 23 99 EB 27 DB CB A5 4C 57 18";

int current = 0, last = 0, timesPressed = 0, times = 0;
float avgPress;
unsigned long current_millis = 0, task1Time = 0, task2Time = 0, task3Time = 0;

/********************************** Main*************************************************************
 *  
 ***************************************************************************************************/
int Check_Button() {
  pinMode(LEDBUTPIN, INPUT);
  current = digitalRead(LEDBUTPIN);

  if (current != last)
  {
    delay(200);
    if (current == HIGH) {
      return 1;
      last = current;
      pinMode(LEDBUTPIN, OUTPUT);
    } else {
      return 0;
      last = current;
      pinMode(LEDBUTPIN, OUTPUT);
    }
  }
  pinMode(LEDBUTPIN, OUTPUT);
  return 0;
}

void Task1 ( void ) {
  if (Check_Button()) {
    timesPressed++;
  }
}

void Task2 ( void ) {
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.print("IP: ");
  LCD.print(WiFi.localIP());
  
  LCD.setCursor(0, 1);
  LCD.print("Average: ");
  LCD.print(avgPress);

  while ( subscription = MQTT.readSubscription(5) )      // Read a subscription and wait for max of 5 ms.
  { // will return 1 on a subscription being read.
    if (subscription == &RESET_TOTAL)                               // if the subscription we have receieved matches the one we are after
    {
      Serial.println((char *)RESET_TOTAL.lastread);                 // we have to cast the array of 8 bit values back to chars to print
      if (strcmp((char *)RESET_TOTAL.lastread, "1") == 0) {
        timesPressed = 0;
        times = 0;
        avgPress = 0;
      }
    }
  }
}

void Task3 ( void ) {
  times++;
  avgPress = (float) timesPressed / times;

  MQTTS_Connect();
  if ( PUBLISH_TOTAL.publish( timesPressed ) && PUBLISH_AVERAGE.publish(  avgPress ) ) {
    Serial.println("Publishing was successiful");
  } else {
    Serial.println("Publishing wasn't successiful");
  }
}

void setup() {
  Wifi_Connection();

  LCD.begin(16, 2);                             // initialise the LCD

  HTTP_Server();
  
  MQTT.subscribe(&RESET_TOTAL);                         // subscribe to the RESET feed
  MQTTS_Connect();
}

void loop() {
  //TIMER DRIVEN SCHEDULER
  current_millis = millis();
  
  if((current_millis - task1Time) >= TASK1L) 
  {
    Task1(); //execute the task
    task1Time = current_millis;
  }
  if((current_millis - task2Time) >= TASK2L) 
  {
    Task2(); //execute the task
    task2Time = current_millis;
  }
  if((current_millis - task3Time) >= TASK3L) 
  {
    Task3(); //execute the task
    task3Time = current_millis;
  }

  SERVER.handleClient();
  MDNS.update();
}

void Wifi_Connection() {
  Serial.begin(115200);                         // open a serial port at 115,200 baud
  while (!Serial)                               // wait for serial peripheral to initialise
  {
    ;
  }
  delay(10);                                    // additional delay before attempting to use the serial peripheral
  Serial.print("Attempting to connect to ");    // Inform of us connecting
  Serial.print(ssid);                           // print the ssid over serial

  WiFi.begin(ssid, password);                   // attemp to connect to the access point SSID with the password

  while (WiFi.status() != WL_CONNECTED)         // whilst we are not connected
  {
    delay(500);                                 // wait for 0.5 seconds (500ms)
    Serial.print(".");                          // print a .
    
    pinMode(LEDBUTPIN, OUTPUT);
    digitalWrite(LEDBUTPIN, HIGH);
  }
  Serial.print("\n");                           // print a new line

  Serial.println("Succesfully connected");      // let us now that we have now connected to the access point
  
  digitalWrite(LEDBUTPIN, LOW);
  pinMode(LEDBUTPIN, INPUT);
  
  Serial.print("Mac address: ");                // print the MAC address
  Serial.println(WiFi.macAddress());            // note that the arduino println function will print all six mac bytes for us
  Serial.print("IP:  ");                        // print the IP address
  Serial.println(WiFi.localIP());               // In the same way, the println function prints all four IP address bytes
  Serial.print("Subnet masks: ");               // Print the subnet mask
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");                    // print the gateway IP address
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");                        // print the DNS IP address
  Serial.println(WiFi.dnsIP());
}

void HTTP_Server(){
  if (MDNS.begin(domainName, WiFi.localIP())) {
    /* we start the mDNS processes with the MDNS.begin method and pass it the value of our desired domain
      name. Upon successfully starting the mDNS processes (returning 1), we can print a message letting us know the full address to
      access the board's HTTPS server   */
    Serial.print("Access your server at http://");
    Serial.print(domainName);
    Serial.println(".local");
  }
  client.setFingerprint(fingerprint);

  SERVER.on("/", Respond);
  SERVER.begin(); //starts the actual server.

  Serial.println("Server is now running");
  Serve_Page_Footfall();
}

void Serve_Page_Footfall ( void )
{
  String reply;

  reply += "<!DOCTYPE HTML>";
  reply += "<html>";
  reply += "<head>";
  reply += "<meta http-equiv=\"refresh\" content=\"2\">";
  reply += "<title>Akhamdjon's Sensor</title>";
  reply += "</head>";
  reply += "<body>";
  reply += "<h1>This is Footfall counter sensor node</h1>";

  reply += "<br> Domain Name:" + String(domainName);
  reply += "<br> Total Number of Footfall:" + String(timesPressed);
  reply += "<br> Minutes Passed:" + String(times);
  reply += "<br> Avarage Number of Footfall:" + String(avgPress);


  reply += "<br><a href = \"?RESET\">RESET</a>";

  reply += "</body>";
  reply += "</html>";

  SERVER.send(200, "text/html", reply);
}

void Respond(void) {
  if (SERVER.hasArg("RESET"))
  {
    timesPressed = 0;
    times = 0;
    avgPress = 0;
    SERVER.sendHeader("Location", String("/"), true);
    SERVER.send ( 302, "text/plain", "");
  } else {
      Serve_Page_Footfall();
    }
}

void MQTTS_Connect ( void )
{
  unsigned char tries = 0;

  // Stop if already connected.
  if ( MQTT.connected() )
  {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ( MQTT.connect() != 0 )                                        // while we are
  {
    Serial.println("Will try to connect again in five seconds");   // inform user
    MQTT.disconnect();                                             // disconnect
    delay(5000);                                                   // wait 5 seconds
    tries++;
    if (tries == 3)
    {
      Serial.println("problem with communication, forcing WDT for reset");
      while (1)
      {
        ;   // forever do nothing
      }
    }
  }

  Serial.println("MQTT succesfully connected!");
}
