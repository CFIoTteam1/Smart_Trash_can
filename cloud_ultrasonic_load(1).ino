#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <HX711.h>
#include "HCSR04.h"
 
const int id = 1;

const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 4;
HX711 scale;

const int trigPin = 16;
const int echoPin = 17;
 
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"


float getWeight();
float getDistance();
 
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);
 
void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}
 
void publishMessage(int id, int state)
{
  StaticJsonDocument<200> doc;
  doc["id"] = id;
  doc["state"] = state;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
 
void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}
 
void setup()
{
  Serial.begin(9600);
  connectAWS();
  //dht.begin();

   // Ultrasonic sensor setup
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Load cell sensor setup
  Serial.println("Initializing the scale");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.tare(); // Zero out the scale
  Serial.println("Tare done. Ready for measurements.");
}
 
void loop()
{
 // Measure weight using the load cell sensor
  float weight_kg = getWeight();
  Serial.print("Weight (kg): ");
  Serial.println(weight_kg);

  // Measure distance using the ultrasonic sensor
  float distance_cm = getDistance();
  Serial.print("Distance (cm): ");
  Serial.println(distance_cm);

  // Check trash can status based on conditions
  int state;
  if (weight_kg == 0.0 && distance_cm >= 40.0) {
    state = 0;
    Serial.println("Trash can is empty!");
  } else if (weight_kg >= 3.0 || distance_cm <= 10.0) {
    state = 100;
    Serial.println("Trash can is full (100%)!");
  } else if ((weight_kg >= 2.0 && weight_kg < 3.0) || (distance_cm > 10.0 && distance_cm <= 20.0)) {
    state = 80;
    Serial.println("Trash can is 80% full!");
  } else if ((weight_kg >= 1.0 && weight_kg < 2.0) || (distance_cm > 20.0 && distance_cm <= 30.0)) {
    state = 50;
    Serial.println("Trash can is 50% full!");
  } else if ((weight_kg >= 0.5 && weight_kg < 1.0) || (distance_cm > 30.0 && distance_cm < 40.0)) {
    state = 20;
    Serial.println("Trash can is 20% full!");
  } else {
    state = 10;
    Serial.println("Trash can is 10% full.");
  }

  
  // Publish the message to the cloud
  publishMessage(id, state);

  delay(10000); // Delay before taking the next measurement
}

float getWeight() {
  if (scale.wait_ready_timeout(200)) {
    //float emptyWeight = 586146.19; // Adjust this value based on calibration
    float emptyWeight = 2264320;
    float currentReading = scale.get_units(10); // Average reading over 10 samples
    //float weight_kg = (-emptyWeight + currentReading) / 464000; // Convert to kg using calibration factor
    float weight_kg = ( -currentReading + emptyWeight ) / 464000;
    Serial.println("currentReading");
    Serial.println(currentReading);
    return weight_kg;
  } else {
    Serial.println("HX711 not found. Check wiring.");
    return 0.0; // Return 0.0 if measurement fails
  }
}

// Function to measure distance using the HC-SR04 ultrasonic sensor
float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH); // Measure the duration of the echo pulse
  float distance_cm = duration * 0.0343 / 2; // Convert duration to distance in centimeters
  return distance_cm;
}
