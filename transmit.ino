// The code does three things:
// 1. Gets data from adafruit.io; if the switch if turned on (1) then the LED will light up and vice versa.
// 2. Gets a city's humudity and temperature data and sends it to adafruit.io.
// 3. Gets and sends humidity data from the Si7021 sensor

#include "config.h" // edit the config.h tab and enter your credentials
#include <ESP8266WiFi.h>// provides the ability to connect the arduino with the WiFi
#include <ESP8266HTTPClient.h> //provides the ability to interact with websites
#include <ArduinoJson.h> //provides the ability to parse and construct JSON objects
#include "Adafruit_Si7021.h" // access to the sensor's library
// Required libraries for code to work
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include <SPI.h>
#include <PubSubClient.h>  
#include <Adafruit_MPL115A2.h>

Adafruit_Si7021 sensor = Adafruit_Si7021(); 
Adafruit_MPL115A2 mpl115a2; 
WiFiClient espClient;            
PubSubClient mqtt(espClient);  

const char* city = "Bellevue"; // Change the name of the city to whatever you want
char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!
char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array


void setup() {
 
  Serial.begin(115200); // start the serial connection

  // Prints the results to the serial monitor
  Serial.print("This board is running: ");  //Prints that the board is running
  Serial.println(F(__FILE__));
  Serial.print("Compiled: "); //Prints that the program was compiled on this date and time
  Serial.println(F(__DATE__ " " __TIME__));
 
  while(! Serial); // wait for serial monitor to open

  setup_wifi();
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function

  mpl115a2.begin(); // initialize barometric sensor
  
  // Si7021 test!
  if (!sensor.begin()) {
    Serial.println("Did not find Si7021 sensor!");
    while (true)
      ;
  }
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
      delay(5000); // Wait 5 seconds before retrying
    }
  }
}

void loop() {

  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'

  float fahrenheit = (sensor.readTemperature() * 1.8) + 32; // temperature data conversion to fahrenheit
  float pressure = mpl115a2.getPressure(); // pressure data from mpl115a2 and stored as numbers with decimal points
  float humidity = sensor.readHumidity(); // hum data from dht22 and stored as numbers with decimal points

  char fah[6]; //a temporary array of size 6 to hold "XX.XX" + the terminating character
  char pres[7]; //a temp array of size 7 to hold "XX.XX" + the terminating character
  char hum[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character

  //take pres, format it into 5 or 6 char array with a decimal precision of 2
  dtostrf(pressure, 6, 2, pres);
  dtostrf(humidity, 5, 2, hum);
  dtostrf(fahrenheit, 5, 2, fah);

  // send message to topics containing temperature/humidity/pressure data
  sprintf(message, "{\"Temperature (F) Room\":\"%s\", \"Pressure Room\":\"%s\", \"Humidity Room\":\"%s\"}", fah, pres, hum);
  mqtt.publish("Treasure/room", message);

  getMet(); //calles the getMet function to get weather data

  delay(10000); // wait for a second
}

void getMet() { 
  HTTPClient theClient;
  theClient.begin(String("http://api.openweathermap.org/data/2.5/weather?q=") + city + "&units=imperial&appid=" + weatherKey);//return weather as .json object
  int httpCode = theClient.GET();

  //checks wether got an error while trying to access the website/API url
  if (httpCode > 0) {
    if (httpCode == 200) {
      String payload = theClient.getString();
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(payload);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed in getMet().");
        return;
      }
      //collects values from JSON keys and stores them as strings because the slots in MetData are strings
      double hd = root["main"]["humidity"];
      double tp = root["main"]["temp"];
      double ps = root["main"]["pressure"];

      // Prints statement to Serial
      Serial.print("Temperature from API is ");
      Serial.print(tp);
      Serial.println(" F");

      Serial.print("Pressure from API is ");
      Serial.print(ps / 10); // convert from hPA to kPA
      Serial.println(" kPA");

      Serial.print("Humidity from API is ");
      Serial.print(hd);
      Serial.println(" %");
    }
  }
  else {
    Serial.printf("Something went wrong with connecting to the endpoint in getMet().");//prints the statement in case of failure connecting to the end point
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; 
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }

  double temp = root["Temperature (F) Room"]; // reads the temperature JSON key and holds the value
  double pres = root["Pressure Room"]; // reads the pressure JSON key and holds the value
  double hum = root["Humidity Room"]; // reads the humidity JSON key and holds the value

  // Prints statement to Serial
  Serial.print("Temperature in room is ");
  Serial.print(temp);
  Serial.println(" F");

  Serial.print("Pressure in room is ");
  Serial.print(pres);
  Serial.println(" kPA");

  Serial.print("Humidity in room is ");
  Serial.print(hum);
  Serial.println(" %");
  Serial.println();
}