/*******************************************************************************************
  Author: Md. Asifuzzaman Khan (HAALniner21) <https://github.com/AsifKhan991>
  Date:9/3/2022

  This example shows how to save template data to an  byte array buffer for further
  manipulation (i.e: save in SD card or cloud).

  You can load this data on the 'fingerTemplate' buffer of the "write_template_from_SD.ino"
  example to write it back to the sensor.

  Happy coding! :)
 *******************************************************************************************/
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <string>
#include <vector>
#include <sstream>
#include <Base64.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>
#define TRIGGER_PIN D4                              // ESP8266 D3 pin connected to US-015 TRIG
#define ECHO_PIN D8                                 // ESP8266 D4 pin connected to US-015 ECHO
#define MAX_DISTANCE 200                            // Maximum distance we want to ping for (in centimeters)
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup

SoftwareSerial swser(D5, D6); // RX, TX
Servo myservo;                // create servo object to control a servo
// lcd D1,D2
LiquidCrystal_I2C lcd(0x27, 16, 2);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&swser, 0);

const char *ssid = "VIDZ 3461";
const char *password = "8Ky49@16";
const char *scriptURL = "https://script.google.com/macros/s/AKfycbzOJYrqkN4avnf0Mz_Thw8eMGhhHdfnwp__QfzDEvfIp7MikALSebTIabGFfirXTfBURg/exec";
const char *mqtt_server = "10.1.240.150";
const int mqtt_port = 1883;
const char *mqtt_id = "esp8266";
const char *topic_subscribe = "to-esp8266";
const char *topic_publish = "from-esp8266";
WiFiClient client;
PubSubClient mqtt_client(client);
uint8_t f_buf[768]; // here is where the template data is sotred
// create a JSON payload from the template data
void displayMessage(String message)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  Serial.println("Displaying Message");

  int lcdLineLength = 16; // Adjust this value to match your LCD's line length

  if (message.length() > lcdLineLength)
  {
    lcd.print(message.substring(0, lcdLineLength));
    lcd.setCursor(0, 1); // Move cursor to the beginning of the second line
    lcd.print(message.substring(lcdLineLength));
  }
  else
  {
    lcd.print(message);
  }
}
struct uint128_t
{
  uint64_t high;
  uint64_t low;
};
void openDoor()
{
  Serial.println("runngin OPne dooor");
  // waits 2 seconds for the servo to reach the position
  myservo.write(180); // tell servo to go to position 90 degrees
  delay(2000);        // waits 2 seconds for the servo to reach the position
  myservo.write(0);   // tell servo to go to position 0 degrees
}
String bytesToHex(uint8_t *bytes, size_t length)
{
  String hexString;
  for (int i = 0; i < length; i++)
  {
    if (bytes[i] < 16)
    {
      hexString += '0';
    }
    hexString += String(bytes[i], HEX);
  }
  return hexString;
}

String bytesTo64BitHex(uint8_t *byteArray, size_t length)
{
  String hexString;
  for (int i = 0; i < length; i += 8)
  {
    uint64_t value = 0;
    for (int j = 0; j < 8; j++)
    {
      value = (value << 8) | byteArray[i + j];
    }
    char buffer[17];
    sprintf(buffer, "%016llx", value);
    hexString += buffer;
  }
  return hexString;
}

void hexToBytes(const String &hexString, uint8_t *byteArray)
{
  size_t length = hexString.length();
  for (int i = 0; i < length; i += 2)
  {
    String hexPair = hexString.substring(i, i + 2);
    byteArray[i / 2] = (uint8_t)strtol(hexPair.c_str(), NULL, 16);
  }
}

String createTemplateJsonPayload(uint8_t *f_buf)
{
  // Create a JSON document
  StaticJsonDocument<1024> doc;

  // Add the fingerprint template data to the JSON document
  JsonArray data = doc.createNestedArray("template");
  for (int k = 0; k < (768 / finger.packet_len); k++)
  {
    for (int l = 0; l < finger.packet_len; l++)
    {
      data.add(f_buf[(k * finger.packet_len) + l]);
    }
  }

  // Convert the JSON document to a string
  String payload;
  serializeJson(doc, payload);

  return payload;
}

std::vector<uint8_t> jsonArrayToByteArray(std::string jsonStr)
{
  // Find the start and end of the "data" array in the JSON string
  size_t start = jsonStr.find('[') + 1;
  size_t end = jsonStr.find(']');

  // Extract the array data as a string
  std::string arrayData = jsonStr.substr(start, end - start);

  // Split the array data string into elements
  std::stringstream ss(arrayData);
  std::string item;
  std::vector<uint8_t> f_buf;
  while (std::getline(ss, item, ','))
  {
    // Convert each element to a uint8_t and add it to the vector
    f_buf.push_back(static_cast<uint8_t>(std::stoi(item)));
  }

  return f_buf;
}

std::vector<uint8_t> hexToBytes(const char *hex)
{
  std::vector<uint8_t> bytes;
  size_t len = strlen(hex);
  for (size_t i = 0; i < len; i += 2)
  {
    std::string byteString = std::string(hex + i, 2);
    uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
    bytes.push_back(byte);
  }
  return bytes;
}

std::vector<uint8_t> hexToBytes64Bit(const std::string &hexString)
{
  std::vector<uint8_t> byteArray;
  size_t length = hexString.length();
  for (size_t i = 0; i < length; i += 16)
  {
    std::string valueString = hexString.substr(i, 16);
    uint64_t value = std::stoull(valueString, nullptr, 16);
    for (int j = 7; j >= 0; j--)
    {
      byteArray.push_back((value >> (j * 8)) & 0xFF);
    }
  }
  return byteArray;
}

void insertRegisterToDB(int id, uint8_t *myTemplate, size_t size)
{
  Serial.println("Inserting to DB");
  if (WiFi.status() == WL_CONNECTED)
  {                  // Check WiFi connection status
    HTTPClient http; // Declare an object of class HTTPClient

    // Create JSON document
    JsonDocument doc;
    doc["id"] = id;
    doc["action"] = "registerFinger";
    doc["template"] = bytesTo64BitHex(myTemplate, 768);
    String json;
    serializeJson(doc, json);

    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, scriptURL);                         // Specify request destination
    http.addHeader("Content-Type", "application/json");    // Specify content-type header
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // Enable following of redirects

    int httpCode = http.POST(json); // Send the request
    if (httpCode > 0)
    {                                    // Check the returning code
      String payload = http.getString(); // Get the response payload
      Serial.println(payload);
    }
    else
    {
      Serial.println("Error on sending POST: " + String(httpCode));
    }
    http.end(); // Close connection
  }
  else
  {
    Serial.println("Error in WiFi connection");
  }
}

String verifyUser(int id)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected");
    return "error";
  }
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  http.begin(client, scriptURL);                         // Specify request destination
  http.addHeader("Content-Type", "application/json");    // Specify content-type header
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // Enable following of redirects
  JsonDocument doc;
  doc["action"] = "verifyUser";
  doc["id"] = id;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody); // Send the actual POST request

  if (httpResponseCode > 0)
  {
    String response = http.getString(); // Get the response to the request

    JsonDocument responseDoc;
    deserializeJson(responseDoc, response);
    String status = responseDoc["status"];
    String message = responseDoc["message"];
    displayMessage(message);
    delay(2000);
    lcd.clear();
    return status;
  }
  else
  {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }

  http.end(); // Free resources
  return "error";
}

// Wait for data to be available on the stream
bool wait_for_data(WiFiClient &stream, unsigned long timeout)
{
  unsigned long start = millis();
  while (!stream.available())
  {
    if (millis() - start >= timeout)
    {
      return false; // Timeout
    }
    delay(10); // Wait for 10ms
  }
  return true; // Data is available
}

JsonDocument retrieveFingerPrintData()
{
  JsonDocument doc; // Adjust the size to fit your JSON document
  int retryCount = 0;

  while (retryCount < 3)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;
      WiFiClientSecure client;
      client.setInsecure();
      http.begin(client, scriptURL);
      http.addHeader("Content-Type", "application/json");
      http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

      // Create JSON document
      JsonDocument requestDoc;
      requestDoc["action"] = "getTemplates";
      String json;
      serializeJson(requestDoc, json);

      int httpCode = http.POST(json); // Send the request with JSON body

      if (httpCode > 0)
      {
        String payload = http.getString();
        Serial.println("retrieved data from server:");
        Serial.println(httpCode);

        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          retryCount++;
          continue; // Skip the rest of the loop and try again
        }
        return doc;
      }
      else
      {
        Serial.println("Error on HTTP request");
        retryCount++;
        continue; // Skip the rest of the loop and try again
      }

      http.end();
    }
    else
    {
      Serial.println("Not connected to WiFi");
      retryCount++;
      continue; // Skip the rest of the loop and try again
    }
  }

  return doc; // Return an empty document if all retries failed
}
void write_template_data_to_sensor(uint16_t id)
{
  // load f-buf to buffer
  // Serial.println("Template data (comma sperated HEX):");
  // for (int k = 0; k < (512 / finger.packet_len); k++)
  // { // printing out the template data in seperate rows, where row-length = packet_length
  //   for (int l = 0; l < finger.packet_len; l++)
  //   {
  //     Serial.print("0x");
  //     Serial.print(f_buf[(k * finger.packet_len) + l], HEX);
  //     Serial.print(",");
  //   }
  //   Serial.println("");
  // }
  int template_buf_size = 768; // usually hobby grade sensors have  byte template data, watch datasheet to know the info
  uint8_t fingerTemplate[768]; // buffer to store the template data
  std::copy(std::begin(f_buf), std::end(f_buf), std::begin(fingerTemplate));
  Serial.println("Ready to write template to sensor...");
  Serial.println("Enter the id to enroll against, i.e id (1 to 127)");
  if (id == 0)
  { // ID #0 not allowed, try again!
    return;
  }
  Serial.print("Writing template against ID #");
  Serial.println(id);

  if (finger.write_template_to_sensor(template_buf_size, fingerTemplate))
  { // telling the sensor to download the template data to it's char buffer from upper computer (this microcontroller's "fingerTemplate" buffer)
    Serial.println("now writing to sensor...");
  }
  else
  {
    Serial.println("writing to sensor failed");
    displayMessage("Failed to write to sensor");
    return;
  }

  Serial.print("ID ");
  Serial.println(id);

  int attempts = 0;
  while (attempts < 3)
  { // Try up to 3 times
    if (finger.storeModel(id) == FINGERPRINT_OK)
    {
      // saving the template against the ID you entered or manually set
      Serial.print("Successfully stored against ID#");
      Serial.println(id);
      break; // Exit the loop
    }
    else
    {
      Serial.println("Storing error, trying again...");
      attempts++; // Increase the attempt count
    }
  }

  if (attempts == 3)
  {
    Serial.println("Failed to store after 3 attempts");
    return;
  }
}

void store_template_to_buf(uint16_t id)
{
  bool success = false;
  while (!success)
  {
    displayMessage("Waiting for valid finger....");
    delay(2000);
    while (finger.getImage() != FINGERPRINT_OK)
    { // press down a finger take 1st image
    }
    Serial.println("Image taken");

    if (finger.image2Tz(1) != FINGERPRINT_OK)
    { // creating the character file for 1st image
      Serial.println("Conversion error");
      continue;
    }
    Serial.println("Image converted");

    Serial.println("Remove finger");
    displayMessage("Remove finger");
    delay(2000);
    uint8_t p = 0;
    while (p != FINGERPRINT_NOFINGER)
    {
      p = finger.getImage();
    }

    Serial.println("Place same finger again, waiting....");
    displayMessage("Place same finger again");
    while (finger.getImage() != FINGERPRINT_OK)
    { // press the same finger again to take 2nd image
    }
    Serial.println("Image taken");

    if (finger.image2Tz(2) != FINGERPRINT_OK)
    { // creating the character file for 2nd image
      Serial.println("Conversion error");
      continue;
    }
    Serial.println("Image converted");

    Serial.println("Creating model...");

    if (finger.createModel() != FINGERPRINT_OK)
    { // creating the template from the 2 character files and saving it to char buffer 1
      Serial.println("Template not build");
      continue;
    }
    Serial.println("Prints matched!");
    Serial.println("Template created");
    displayMessage("Template created");

    Serial.println("Attempting to get template...");
    if (finger.getModel() != FINGERPRINT_OK)
    { // requesting sensor to transfer the template data to upper computer (this microcontroller)
      Serial.println("Failed to transfer template");
      continue;
    }
    Serial.println("Transferring Template....");

    if (finger.get_template_buffer(768, f_buf) != FINGERPRINT_OK)
    { // read the template data from sensor and save it to buffer f_buf
      Serial.println("Failed to get the template into buffer..!!");
      displayMessage("failed :( try again!");
      continue;
    }
    write_template_data_to_sensor(id);
    displayMessage("registering...");
    insertRegisterToDB(id, f_buf, 768);
    success = true;
  }
}

uint8_t getFingerprintID()
{
  uint8_t p = finger.getImage();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image taken");
    break;
  case FINGERPRINT_NOFINGER:
    Serial.println("No finger detected");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_IMAGEFAIL:
    Serial.println("Imaging error");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    return p;
  default:
    Serial.println("Unknown error");
    return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK)
  {
    // flash red LED
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
    displayMessage("Verifying...");
    if (verifyUser(finger.fingerID) == "success")
    {
      openDoor();
    }
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_NOTFOUND)
  {
    displayMessage("No User detected! try again");
    delay(2000);
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  return finger.fingerID;
}

std::vector<int> getTotalUserRegistered()
{
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // Enable following of redirects
  http.begin(client, scriptURL);                         // Specify the URL

  // Set the Content-Type of the request
  http.addHeader("Content-Type", "application/json");

  // Prepare the body of the request
  String requestBody = "{\"action\":\"getTotalFinger\"}";

  int httpCode = http.POST(requestBody);

  std::vector<int> userIds;

  if (httpCode > 0)
  {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    JsonArray totalUsers = doc["totalUsers"]; // Assuming the JSON response has a "totalUsers" field

    for (JsonVariant v : totalUsers)
    {
      userIds.push_back(v.as<int>());
    }
  }
  else
  {
    Serial.println("Error on HTTP request");
  }

  return userIds;
}
void loadFingerDataToSensor()
{
  // Adjust the size according to your needs

  DeserializationError error;
  JsonDocument doc = retrieveFingerPrintData();

  JsonArray array = doc.as<JsonArray>();
  delay(1000);
  Serial.println("Retrieved data successfully!");

  for (JsonVariant v : array)
  {
    JsonObject obj = v.as<JsonObject>();
    uint16_t id = obj["id"].as<uint16_t>();
    Serial.println(id);
    const char *templateStrConst = obj["template"];
    char *templateStr = strdup(templateStrConst);
    std::vector<uint8_t> byteArray = hexToBytes64Bit(templateStr);
    std::move(byteArray.begin(), byteArray.end(), f_buf);
    // free(byteArray.data();
    Serial.println("Writing template to sensor");
    write_template_data_to_sensor(id);
  }
}
void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';                      // Null-terminate the char array
  String payloadStr = String((char *)payload); // Convert to String

  JsonDocument doc; // Create a DynamicJsonDocument with enough capacity

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, payloadStr);

  // Test if parsing succeeds.
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  // Access values in the JSON
  const char *status = doc["status"];
  int id = doc["id"];
  if (doc["action"] == "register")
  {
    store_template_to_buf(doc["id"].as<uint16_t>());
    Serial.println("Sending data to server");
    Serial.println(id);
  }
  if (doc["action"] == "delete")
  {
    finger.emptyDatabase();
    // flash red LED
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10);
    delay(2000);
  }
  // Print values
  Serial.println(status);
  Serial.println(id);
}
void setup()
{

  Serial.begin(115200);
  finger.begin(57600); // set your sensor's baudrate
  if (finger.verifyPassword())
  {
    Serial.println("Found fingerprint sensor!");
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    while (1)
      ;
  }
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);
  finger.emptyDatabase();

  delay(1000);
  finger.getTemplateCount();
  Serial.print(F("stored template: "));
  Serial.println(finger.templateCount);
  Serial.println("Database emptied");

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(callback);
  Serial.println("Connecting to mqtt... ");
  while (!mqtt_client.connect(mqtt_id))
  {
    delay(500);
  }
  Serial.println("Connected to mqtt ");
  mqtt_client.subscribe("to-esp8266");
  mqtt_client.publish("from-esp8266", "Hello Server");
  // temporarily commented out
  if (finger.templateCount == 0)
  {
    loadFingerDataToSensor();
    displayMessage("Fingerprint data loaded");
  }
  else
  {
    Serial.println("Fingerprint data already exists");
  }
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  myservo.attach(D7); // attaches the servo on D3 pin to the servo object
  myservo.write(0);   // tell servo to go to position 0 degrees
  displayMessage("Ready to use");
  delay(3000);
}
bool firstRun = true;

void loop()
{
  delay(50);                                    // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
  unsigned int uS = sonar.ping();               // Send ping, get ping time in microseconds (uS).
  unsigned int distance = uS / US_ROUNDTRIP_CM; // Convert ping time to distance in cm

  if (distance <= 100)
  {
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
    delay(250);
    lcd.backlight(); // Turn on the backlight
    mqtt_client.loop();
    if (!mqtt_client.connected())
    {
      mqtt_client.connect(mqtt_id);
      mqtt_client.subscribe("to-esp8266");
    }
    // Display a welcome message when the system starts
    if (firstRun)
    {
      displayMessage("Welcome to the");
      delay(1000); // Wait for 1 second so the user can read the message
      displayMessage("Fingerprint Scanner!");
      delay(1000); // Wait for 1 second so the user can read the message
      // Blink the backlight 5 times
      firstRun = false;
    }

    // Display the current system status
    displayMessage("Status: Reading...");
    delay(1000); // Wait for 1 second so the user can read the message

    int fingerprintID = getFingerprintID();

    if (fingerprintID == -1)
    {
      // Display error messages in a clear and user-friendly way
      displayMessage("Error: Could not");
      delay(1000); // Wait for 1 second so the user can read the message
      displayMessage("read fingerprint.");
    }
    else if (fingerprintID == 0)
    {
      displayMessage("No fingerprint");
      delay(1000); // Wait for 1 second so the user can read the message
      displayMessage("detected. Place finger.");
    }
  }
  else
  {
    lcd.noBacklight(); // Turn off the backlight
    firstRun = true;
  }

  delay(200);
}
// TODO: add a function utilizing ultrasonic to detect whther the use is in range!
// TODO: redesign the schema for fetching finger print to fetching 1 by 1
// TODO: redesign the UI for the LCD
// TODO: smoothening the registering processs(must)
// TODO: add the function to check the status of the door(must)
// TODO: add the admin dashboard (will do if have time!)
// TODO: add the delete function for testing(maybe not needed in the final version!)
