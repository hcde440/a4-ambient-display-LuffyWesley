// The code receives data from mqtt. If motion is detected between 9:30 pm and 8:30 pm (essentially around the time I am in my room), 
// the buzzer will play buzz. If motion is detected outside those hrs, the buzzer will not buzz but instead send data to io.adafruit as a 
// changable light or iftt as an email (this part needs more work in integration). The code also receives data about weather conditions. 
// If its between 8 and 8:30 am, the led's will light up (gotta conserve energy). If the weather condition is 'Rainy' the red led will light up.
// If the weather condition is 'Clear' the green led will light up. The leds will continue to light unless there's a weather condition change or
// its outside the time range.  

#define WIFI_SSID "University of Washington" // wifi network name
#define WIFI_PASS "" // wifi password

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

// Required libraries for code to work
#include <Wire.h> // library allows you to communicate with I2C / TWI devices
#include <SPI.h> // Needed to communicate with MQTT
#include <PubSubClient.h>   // Needed to communicate with MQTT
#include <ArduinoJson.h>    // Needed to parse json files
#include <ESP8266WiFi.h>  // library provides ESP8266 specific WiFi methods we are calling to connect to network
#include <ESP8266HTTPClient.h>  // Needed to communicate with websites

WiFiClient espClient;            
PubSubClient mqtt(espClient);  

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!
char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array

int redLed = 13;  // red led pin
int greenLed = 12;  // green led pin
int BUZZER_PIN = 15; // buzzer pin

void setup() {
 
  Serial.begin(115200); // start the serial connection

  // Prints the results to the serial monitor
  Serial.print("This board is running: ");  //Prints that the board is running
  Serial.println(F(__FILE__));
  Serial.print("Compiled: "); //Prints that the program was compiled on this date and time
  Serial.println(F(__DATE__ " " __TIME__));
 
  while(! Serial); // wait for serial monitor to open

  pinMode(redLed, OUTPUT);      // declare red as output
  pinMode(greenLed, OUTPUT);    // declare green led as output
  pinMode(BUZZER_PIN, OUTPUT);  // declare buzzer as output
  digitalWrite(BUZZER_PIN, LOW);  // buzzer is off

  setup_wifi();
  mqtt.setServer(mqtt_server, 1883); // connect to mqtt server
  mqtt.setCallback(callback); //register the callback function
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

  delay(1000); // wait for a second
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  StaticJsonDocument<200> doc; 
  DeserializationError error = deserializeJson(doc, payload); //parse it!

  // Test if parsing succeeds.
  if (error) { 
    Serial.print("deserializeJson() in callback failed with code ");
    Serial.println(error.c_str());
    return;
  }

  String tim = doc["Time"]; // reads time json and stores data
  double mot = doc["Motion Room"]; // reads motion json and stores data
  String weather = doc["Weather Outside"]; // reads weather json and stores data

  // Sound the buzzer when anytime between 9 pm and 8:30 am when motion is detected
  if (tim > "20:59" && tim < "8:31") {
    digitalWrite(BUZZER_PIN, LOW);    // buzzer is off
    if (mot == 1) {
      digitalWrite(BUZZER_PIN, HIGH);   // buzzer is on
    } 
  } else {
    digitalWrite(BUZZER_PIN, LOW);    // buzzer is off
  }

  // led light up only when its between 8 am and 8:30 am. Green is for a clear day (no jacket)
  // Red is for a rainy day (carry jacket)
  if (tim > "7:59" && tim < "8:31") {
    digitalWrite(redLed, LOW); // green led off
    digitalWrite(greenLed, LOW);  // red led off
    if (weather == "Clear") {
      digitalWrite(greenLed, HIGH); // green led on
    } else if (weather == "Rainy") {
      digitalWrite(redLed, HIGH); // red led on
    } 
  } else {
    digitalWrite(redLed, LOW); // red led off
    digitalWrite(greenLed, LOW);  // green led off
  }
}