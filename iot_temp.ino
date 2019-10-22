#include <Wire.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <AM2320.h>

WiFiClientSecure client;

const char* ssid = "ABB_Indgym_Guest";
const char* password = "Welcome2abb";

String macAddress;

const char* base_uri = "jj3mdr0w1m.execute-api.us-east-1.amazonaws.com";
const char *fingerprint = "72:D4:00:92:77:37:50:C9:9B:A1:38:FA:21:8A:9B:FD:BA:CF:CD:49";

struct Response
{
  int statusCode;
  String body;
};

String owner;
String location;
String updateRate;
String token;

AM2320 sensor;

struct Data
{
  String temp;
  String hum;
  String errMsg = "";
};

Response sendRequest(String method, String uri, String query, String body)
{
  Serial.print("Connecting to ");
  Serial.println(base_uri);

  if (!client.connect(base_uri, 443))
  {
    Serial.println("Connection failed");
    
    Response res;
    
    res.statusCode = -1;
    res.body = "no connection";
    
    return res;
  }

  Serial.println(method + " " + uri + query + " HTTP/1.1");

  client.println(method + " " + uri + query + " HTTP/1.1");
  client.print("Host: ");
  client.println(base_uri);
  client.println("Connection: close");

  // Include body if necessarily
  if (body.length() > 0) {
    client.println("Connection-Type: application/json");
    client.print("Content-Length: ");
    client.println(body.length());
    client.println();
    client.println(body);
  }
  
  client.println(); // must end with blank line to show end of request

  Response res;

  // Go through all lines in response
  while(client.connected())
  {
    String line;

    line = client.readStringUntil('\n');

    // Check if line starts with HTTP to get status code
    if (line.startsWith("HTTP"))
    {
      // Take the appropriate substring and convert to an integer
      res.statusCode = line.substring(9, 12).toInt();
    }

    // Check if line starts with a curly bracket and get response body
    if (line.startsWith("{"))
    {
      res.body = line;
    }
  }
  return res;
}

bool isNumeric(String str)
{
    unsigned int stringLength = str.length();

    if (stringLength == 0)
    {
        return false;
    }
 
    boolean seenDecimal = false;
 
    for(unsigned int i = 0; i < stringLength; ++i)
    {
        if (isDigit(str.charAt(i)))
        {
            continue;
        }
 
        if (str.charAt(i) == '.')
        {
            if (seenDecimal)
            {
                return false;
            }
            seenDecimal = true;
            continue;
        }
        return false;
    }
    return true;
}

int getUpdateRate()
{
  Response r = sendRequest("GET", "/beta/device", "?macAddress=" + macAddress, "");
  
  if (r.statusCode != 200)
  {
    Serial.println();
    Serial.println("Device could not be found");
    return -1;
  }

  int updateRateIndex = r.body.indexOf("\"updateRate\":");
  
  int updateRateStart = updateRateIndex + 15;
  int updateRateEnd = r.body.indexOf("\"", updateRateStart);
  
  return r.body.substring(updateRateStart, updateRateEnd).toInt();
}

void registerDevice()
{
  Serial.println();
  Serial.println("Registering new device...");
  
  Serial.println();
  Serial.println("Please enter the owner of the device:");

  while (owner.length() == 0) {
    if (Serial.available()) {
      owner = Serial.readString();
      owner.trim();
      
      Serial.println(owner);
    }
  }

  Serial.println();
  Serial.println("Please enter the location of the device:");

  while (location.length() == 0) {
    if (Serial.available()) {
      location = Serial.readString();
      location.trim();
      
      Serial.println(location);
    }
  }

  Serial.println();
  Serial.println("Please enter the amount of ms between each update:");

  while (updateRate.length() == 0)
  {
    if (Serial.available())
    {
      updateRate = Serial.readString();
      updateRate.trim();

      // Check if user didn't enter a numeric value
      if (!isNumeric(updateRate))
      {
        Serial.println("ERR: Value must be numeric.");
        updateRate = "";
        
        continue;
      }

      updateRate.toInt();
      Serial.println(updateRate);
    }
  }

  Serial.println();

  // Run until user has entered the correct authorization token
  while (true)
  {
    Serial.println("Please enter the authorization token to access the API:");
  
    while (token.length() == 0) {
      if (Serial.available()) {
        token = Serial.readString();
        token.trim();
        
        Serial.println(token);
      }
    }

    String body = "{\"macAddress\": \"" + macAddress + "\", \"owner\": \"" + owner + "\", \"location\": \"" + location + "\", \"updateRate\": \"" + updateRate + "\", \"password\": \"" + token + "\"}";
    Serial.println(body);
    
    Response r = sendRequest("POST", "/beta/device", "", body);

    if (r.statusCode == 400)
    {
      Serial.println();
      Serial.println("Invalid authorization token");
      token = "";
      continue;
    }
    break;
  }
  Serial.println("Device has successfully been registered");
}

Data getData()
{
  Data data;
  
  if (sensor.measure())
  {
    data.temp = sensor.getTemperature();
    data.hum = sensor.getHumidity();
  }
  else
  {
    int errCode = sensor.getErrorCode();
    
    switch(errCode) {
      case 1: data.errMsg = "ERR: Sensor is offline"; break;
      case 2: data.errMsg = "ERR: CRC validation failed"; break;
    }
  }

  return data;
}

void setup()
{
  // Begin serial communication
  Serial.begin(9600);
  Wire.begin(12, 13);

  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.print(ssid);
  
  WiFi.begin(ssid, password);

  // Wait for WiFi to connect
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  // Display information about device
  Serial.println();
  Serial.println("WiFi connected");
  
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  macAddress = WiFi.macAddress();
  
  Serial.print("MAC Address: ");
  Serial.println(macAddress);
  
  Serial.println();

  // Set fingerprint to use SSL
  client.setFingerprint(fingerprint);

  // Get update rate
  updateRate = getUpdateRate();

  // Register device if it doesn't exists
  if (updateRate == -1)
  {
    registerDevice();
  }
}

void loop()
{
  Serial.println();
  
  // Get temperature and humidity from sensor
  Data data = getData();

  // Check if it wasn't possible to get data
  if (data.errMsg.length() > 0)
  {
    Serial.println(data.errMsg);
    delay(updateRate);
    return;
  }

  Serial.println("Temperature: " + data.temp);
  Serial.println("Humidity: " + data.hum);
  Serial.println();

  // Send data to API
  String body = "{\"macAddress\": \"" + macAddress + "\", \"humidity\": \"" + data.hum + "\", \"temperature\": \"" + data.temp + "\"}";
  Response r = sendRequest("POST", "/beta/data", "", body);
  
  delay(updateRate);
}
