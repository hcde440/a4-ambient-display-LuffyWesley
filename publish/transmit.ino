// The code does 3 things:
// 1. Get current time from an NTP server and publish the time to mqtt
// 2. Sets up motion sensor and sends motion data to mqtt
// 3. Call openweathermap api for current weather conditions and sends data to mqtt

#include <Wire.h>   // library allows you to communicate with I2C / TWI devices
#include <SPI.h>    // Needed to communicate with MQTT
#include <PubSubClient.h>   // Needed to communicate with MQTT  
#include <ESP8266WiFi.h>    // library provides ESP8266 specific WiFi methods we are calling to connect to network
#include <ArduinoJson.h>   // Needed to parse json files
#include <ESP8266HTTPClient.h> // Needed to communicate with websites
#include <NTPClient.h>    // time library which does graceful NTP server synchronization
#include <WiFiUdp.h>    // library handles UDP protocol like opening a UDP port, sending and receiving UDP packets etc

#define WIFI_SSID "University of Washington"    // wifi network name
#define WIFI_PASS ""    // wifi password

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

const char* weatherKey = ""; // API key for openweathermap (https://openweathermap.org)
const char* city = "Seattle"; // Change the name of the city to whatever you want, used to get weather conditions data

// adjust the UTC offset for your timezone in milliseconds (https://en.wikipedia.org/wiki/List_of_UTC_time_offsets)
// UTC -5.00 : -5 * 60 * 60 : -18000
// UTC +1.00 : 1 * 60 * 60 : 3600
// UTC -7.00 : -8 * 60 * 60 : -28800 (Washington)
const long utcOffsetInSeconds = -28800;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds); // specify the address of the NTP Server we wish to use

WiFiClient espClient;            
PubSubClient mqtt(espClient);  

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!
char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array

int pirSensor = 2;               // choose the input pin (for PIR sensor)
int pirState = LOW;             // we start, assuming no motion detected
int val = 0;                    // variable for reading the pin status

void setup() {
 
  Serial.begin(115200); // start the serial connection

  // Prints the results to the serial monitor
  Serial.print("This board is running: ");  //Prints that the board is running
  Serial.println(F(__FILE__));
  Serial.print("Compiled: "); //Prints that the program was compiled on this date and time
  Serial.println(F(__DATE__ " " __TIME__));
 
  while(! Serial); // wait for serial monitor to open

  pinMode(pirSensor, INPUT);     // declare sensor as input

  setup_wifi(); // calls setup_wifi funciton to connect to wifi
  mqtt.setServer(mqtt_server, 1883);    // connect to mqtt server
  timeClient.begin(); // initialize the NTP client
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // wait 5 ms
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
}                                     

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("Treasure/+"); //we are subscribing to 'Treasure' and all subtopics below that topic
    } else {                       
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(2000); // Wait 2 secs
    }
  }
}

void loop() {

  // runs in case we are not connected to mqtt
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'
  
  timeClient.update();    //call the update() function whenever we want current day & time
  int hrs = timeClient.getHours();
  int mins = timeClient.getMinutes();

  // send message containing time data
  sprintf(message, "{\"Time\":\"%d:%d\"}", hrs, mins);
  mqtt.publish("Treasure/time", message);

  val = digitalRead(pirSensor);  // read input value
  if (val == HIGH) {            // check if the input is HIGH
    if (pirState == LOW) {
      pirState = HIGH;  //change state to high once motion has been detected
    }
  } else {
    if (pirState == HIGH){
      pirState = LOW;   // change state to low once motion has stopped
    }
  }

  // send message to topic containing motion data
  sprintf(message, "{\"Motion Room\":\"%d\"}", pirState);
  mqtt.publish("Treasure/motion", message);

  weatherCondition(); // run weatherCondition function to get weather condition data

  delay(5000); // wait for 5 seconds
}

void weatherCondition() { 
  HTTPClient theClient;
  theClient.begin(String("http://api.openweathermap.org/data/2.5/weather?q=") + city + "&units=imperial&appid=" + weatherKey);  //return weather as .json object
  int httpCode = theClient.GET();

  //checks wether got an error while trying to access the website/API url
  if (httpCode > 0) {
    if (httpCode == 200) {
      String payload = theClient.getString();

      DynamicJsonDocument doc(1024); 
      DeserializationError error = deserializeJson(doc, payload); //parse it!

      // Test if parsing succeeds.
      if (error) { 
        Serial.print("deserializeJson() in weatherCondition() failed with code ");
        Serial.println(error.c_str());
        return;
      }

      // store parsed data into a string
      String tp = doc["weather"][0]["main"];
      
      // send message containing weather condition data
      sprintf(message, "{\"Weather outside\":\"%s\"}", tp.c_str());
      mqtt.publish("Treasure/weather", message);
    }
  }
  else {
    Serial.printf("Something went wrong with connecting to the endpoint in weatherCondition().");//prints the statement in case of failure connecting to the end point
  }
}