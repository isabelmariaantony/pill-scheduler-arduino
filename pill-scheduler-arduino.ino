/*
  Web client

  This sketch connects to the PillScheduler service
  using the WiFi module.

 
  This program is written for a network using WPA encryption. For
  WEP or WPA, change the WiFi.begin() call accordingly.

  created April 25, 2024
  by Isabel Maria Antony

  Reference:
  https://docs.arduino.cc/tutorials/uno-r4-wifi/wifi-examples#wi-fi-web-client
 */


#include "WiFiS3.h"
#include <ArduinoJson.h>



class Dispense {
public:
    int boxNumber;
    int pillQuantity;
    Dispense():  boxNumber(0), pillQuantity(0) {}
    Dispense(int boxNumber, int pillQuantity) : boxNumber(boxNumber), pillQuantity(pillQuantity) {}
};

struct DispenseArrayWrapper {
  Dispense* array;
  int size;
  DispenseArrayWrapper(Dispense* array, int size) : array(array), size(size) {}
};


///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = "wifi-name";        // your network SSID (name)
char pass[] = "wifi-password";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key index number (needed only for WEP)

int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(192,168,210,148);   // On Mac, call ifconfig to get the ip address. It will be under en0 , inet
char server[] = "pill-scheduler-backend.onrender.com";   

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is def  ault for HTTP):
WiFiSSLClient client;
DynamicJsonDocument doc(1024);


/* -------------------------------------------------------------------------- */
void setup() {
/* -------------------------------------------------------------------------- */  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
  
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
     
    // wait 10 seconds for connection:
    delay(10000);
  }

  printWifiStatus();
}

/* just wrap the received data up to 80 columns in the serial print
  In most cases, the response will not be avaialble quickly. The function will wait for 10 millis 
  for upto 50 times 
*/
/* -------------------------------------------------------------------------- */
String read_response_with_wait() {
/* -------------------------------------------------------------------------- */  
  //try 50 times
  int tries = 0;
  String message = "Try: ";
  String output = "";
  while ( tries < 50)  {
    uint32_t received_data_num = 0;
    bool dataRead = false;
    Serial.println(message + tries);
    while (client.available()) {
      /* actual data reception */
      char c = client.read();
      output.concat(c);
      /* print data to serial port */
      Serial.print(c);
      /* wrap data to 80 columns*/
      received_data_num++;
      if(received_data_num % 80 == 0) { 
        Serial.println();
      }
      dataRead = true;
    }
    if (dataRead) {
      return output;
    }
    delay(10);
    tries ++;     
  }  
}

/*
Check if the HTTP Server response is valid 
*/
bool isValid(String response) {
  return (response.indexOf("HTTP/1.1 200 OK") != -1) ;
}

/*
 get the actual response from the backend Server by removing the header portion
*/
String getBody(String response) {
  int bodyIndex = response.indexOf("[{");
  return response.substring(bodyIndex);
}

/*
Convert the response from the Server to an easy to to use object which has an array
The array will have a list of objects.
Each object has the BOxNumber from which to dispense and the the number of pills to dispense from the box
*/
DispenseArrayWrapper getDispenses(String body) {
  // Parse the JSON string
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }

  // Extract each device object from the JSON array
  JsonArray arr = doc.as<JsonArray>();
  int length = arr.size();
  Dispense dispenses[length];
  int index = 0;
  for(JsonVariant v : arr) {
    String boxInfo = v["boxNumber"]; // Get the BoxNumber as in "Box 1"
    int boxNumber = boxInfo.toInt();
    int pillQuantity = v["count"];
    dispenses[index++] = Dispense(boxNumber, pillQuantity);
  }
  DispenseArrayWrapper dispenseArrayWrapper = DispenseArrayWrapper(dispenses, length);
  return dispenseArrayWrapper;
}

/*
Print the Dispense information 
*/
void print(DispenseArrayWrapper dispenseArrayWrapper) {
    // output to serial monitor
  Serial.println();
  Dispense* dispenses = dispenseArrayWrapper.array;
  for (int i = 0; i < dispenseArrayWrapper.size; i++) {
    Serial.print("Box Number: ");
    Serial.print(dispenses[i].boxNumber);
    Serial.print(", Pill Quanity: ");
    Serial.println(dispenses[i].pillQuantity);
  }
}

/* -------------------------------------------------------------------------- */
void loop() {
/* -------------------------------------------------------------------------- */  

  // if you get a connection, report back via serial:
  Serial.println("\nStarting connection to server...");
  boolean pillsDispensed = false;
  if (client.connect(server, 443)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    client.println("GET /pillsByTimeRange HTTP/1.1");
    client.println("Host: pill-scheduler-backend.onrender.com");
    client.println("Connection: close");
    client.println();
    String response = read_response_with_wait();

    //check if this was a valid response
    if (isValid(response)) {
      String body = getBody(response);
      DispenseArrayWrapper dispenseArrayWrapper = getDispenses(body);
      print(dispenseArrayWrapper);
      pillsDispensed = true;
    }
    
  }
  if (pillsDispensed && client.connect(server, 443)) {
    client.println("GET /markServed HTTP/1.1");
    client.println("Host: pill-scheduler-backend.onrender.com");
    client.println("Connection: close");
    client.println();

    String response2 = read_response_with_wait();
    Serial.println(response2);
  }
  // wait for 10 seconds before checking again 
  delay(10000);
}

//

/* -------------------------------------------------------------------------- */
void printWifiStatus() {
/* -------------------------------------------------------------------------- */  
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
