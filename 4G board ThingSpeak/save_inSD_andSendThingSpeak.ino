// Author: Nathanael Blavo Ballarin
// Date: July 24, 2024

#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoLowPower.h>

// Define API keys for each channel
String ApikeyNode2 = "PVQKI3W5Y1ENSEFD";
String ApikeyNode3 = "YOUR_API_KEY_NODE_3";
String ApikeyNode4 = "YOUR_API_KEY_NODE_4";
String ApikeyNode5 = "YOUR_API_KEY_NODE_5";

#define DEBUG true

#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7
#define WAKE_PIN 3
#define CS_PIN 4  // SD card selection pin

String receivedData = "";
bool dataReceived = false;
const int maxRetries = 15;

File dataFile;

void setup() {
    SerialUSB.begin(115200);
    Serial1.begin(115200);
    
    pinMode(LTE_RESET_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);
    
    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);
    delay(100);
    //digitalWrite(LTE_PWRKEY_PIN, HIGH);
    //delay(2000);
    //digitalWrite(LTE_PWRKEY_PIN, LOW);
    //powerOnSIM7600();
    pinMode(LTE_FLIGHT_PIN, OUTPUT);
    digitalWrite(LTE_FLIGHT_PIN, LOW); // Normal Mode
    
    pinMode(WAKE_PIN, INPUT_PULLUP); // Configure wake pin

    delay(5000);

    SerialUSB.println("4G HTTP Test Begin!");

    delay(1000);
    
    Wire.begin(0x08); // Initialize as slave with address 0x08
    Wire.onReceive(receiveEvent); // Attach receiveEvent function
    
    // Attach wake interrupt
    //attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUp, LOW);

    // Initialize SD card
    if (!SD.begin(CS_PIN)) {
        SerialUSB.println("SD card initialization failed!");
        while (1);
    }
    SerialUSB.println("SD card initialized.");
}

void loop() {
    if (dataReceived) {
        dataReceived = false;

        // Create a mutable copy of the received data
        char receivedDataCopy[receivedData.length() + 1];
        strcpy(receivedDataCopy, receivedData.c_str());

        // Parse the received data for node number, date, time, temperature, humidity, pressure, and dendrometer
        int node = 0;
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        char *token;
        float t = 0.0;
        float h = 0.0;
        float p = 0.0;
        float d0 = 0.0;
        float d1 = 0.0;
        float d2 = 0.0;
        float d3 = 0.0;
        float m = 0.0;

        token = strtok(receivedDataCopy, ",");
        node = atoi(token); // The first field is the node number

        token = strtok(NULL, ",");
        year = atoi(token);

        token = strtok(NULL, ",");
        month = atoi(token);

        token = strtok(NULL, ",");
        day = atoi(token);

        token = strtok(NULL, ",");
        hour = atoi(token);

        token = strtok(NULL, ",");
        minute = atoi(token);

        token = strtok(NULL, ",");
        second = atoi(token);

        String datetime = String(year) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(minute) + ":" + String(second);

        token = strtok(NULL, ",");
        t = atof(token);

        token = strtok(NULL, ",");
        h = atof(token);

        token = strtok(NULL, ",");
        p = atof(token);

        token = strtok(NULL, ",");
        d0 = atof(token);

        token = strtok(NULL, ",");
        d1 = atof(token);

        token = strtok(NULL, ",");
        d2 = atof(token);

        token = strtok(NULL, ",");
        d3 = atof(token);

        token = strtok(NULL, ",");
        m = atof(token);

        // Debugging: print out the parsed values
        SerialUSB.print("Parsed values: DateTime=");
        SerialUSB.print(datetime);
        SerialUSB.print(", Node=");
        SerialUSB.print(node);
        SerialUSB.print(", Temp=");
        SerialUSB.print(t);
        SerialUSB.print(", Hum=");
        SerialUSB.print(h);
        SerialUSB.print(", Press=");
        SerialUSB.print(p);
        SerialUSB.print(", Dendro0=");
        SerialUSB.print(d0);
        SerialUSB.print(", Dendro1=");
        SerialUSB.print(d1);
        SerialUSB.print(", Dendro2=");
        SerialUSB.print(d2);
        SerialUSB.print(", Dendro3=");
        SerialUSB.print(d3);
        SerialUSB.print(", Moisture=");
        SerialUSB.println(m);

        // Save data to SD card in a CSV file per node
        String fileName = "Node" + String(node) + ".csv";
        dataFile = SD.open(fileName, FILE_WRITE);
        if (dataFile) {
            if (dataFile.size() == 0) {
                // Write header if file is empty
                dataFile.println("Datetime,Node,Temp,Hum,Press,Dendro,Moisture");
            }
            dataFile.print(datetime);
            dataFile.print(",");
            dataFile.print(node);
            dataFile.print(",");
            dataFile.print(t, 4);
            dataFile.print(",");
            dataFile.print(h, 4);
            dataFile.print(",");
            dataFile.print(p, 4);
            dataFile.print(",");
            dataFile.print(d0, 4);
            dataFile.print(",");
            dataFile.print(d1, 4);
            dataFile.print(",");
            dataFile.print(d2, 4);
            dataFile.print(",");
            dataFile.print(d3, 4);
            dataFile.print(",");
            dataFile.println(m, 4);
            dataFile.close();
            SerialUSB.println("Data saved to " + fileName);
        } else {
            SerialUSB.println("Error opening file " + fileName);
        }

        // Convert floats to strings
        String t_str = String(t, 4);
        String h_str = String(h, 4);
        String p_str = String(p, 4);
        String d0_str = String(d0, 4);
        String d1_str = String(d1, 4);
        String d2_str = String(d2, 4);
        String d3_str = String(d3, 4);
        String m_str = String(m, 4);

        // Set appropriate API key based on node
        String Apikey;
        switch (node) {
            case 2:
                Apikey = ApikeyNode2;
                break;
            case 3:
                Apikey = ApikeyNode3;
                break;
            case 4:
                Apikey = ApikeyNode4;
                break;
            case 5:
                Apikey = ApikeyNode5;
                break;
            default:
                SerialUSB.println("Unknown node, no API key available.");
                return; // Exit function if node is unknown
        }

        // Build URL with all fields except date and time
        String http_str = "AT+HTTPPARA=\"URL\",\"https://api.thingspeak.com/update?api_key=" + Apikey + "&field1=" + t_str + "&field2=" + h_str + "&field3=" + p_str + "&field4=" + d0_str + "&field5=" + m_str + "&field6=" + d1_str + "&field7=" + d2_str + "&field8=" + d3_str + "\"\r\n";
        SerialUSB.println(http_str);

        int retryCount = 0;
        bool success = false;

        powerOnSIM7600();
        delay(20000);

        while (retryCount < maxRetries && !success) {
            sendData("AT+HTTPINIT\r\n", 2000, DEBUG);
            String response = sendData(http_str, 2000, DEBUG);
            String actionResponse = sendData("AT+HTTPACTION=0\r\n", 3000, DEBUG);
            sendData("AT+HTTPTERM\r\n", 3000, DEBUG);

            if (actionResponse.indexOf("200") >= 0) {
                success = true;
                SerialUSB.println("Data sent successfully!");
                powerOffSIM7600();

            } else {
                retryCount++;
                SerialUSB.print("Retrying... (");
                SerialUSB.print(retryCount);
                SerialUSB.println(")");
                delay(1000); // Wait 1 second before retrying
            }
        }

        if (!success) {
            SerialUSB.println("Failed to send data after maximum retries.");
        }
    } else {
        //enterSleepMode();
    }
}

void receiveEvent(int howMany) {
    receivedData = "";
    while (Wire.available()) {
        char c = Wire.read();
        receivedData += c;
    }
    dataReceived = true;
    detachInterrupt(digitalPinToInterrupt(WAKE_PIN));
}

void enterSleepMode() {
    SerialUSB.println("Sleep mode!");
    attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUp, LOW);


    LowPower.sleep();
}

void wakeUp() {
    // Nothing to do here, just to exit sleep mode
    SerialUSB.println("wake up");
}

bool moduleStateCheck() {
    int i = 0;
    bool moduleState = false;
    for (i = 0; i < 5; i++) {
        String msg = String("");
        msg = sendData("AT", 1000, DEBUG);
        if (msg.indexOf("OK") >= 0) {
            SerialUSB.println("SIM7600 Module had turned on.");
            moduleState = true;
            return moduleState;
        }
        delay(1000);
    }
    return moduleState;
}

String sendData(String command, const int timeout, boolean debug) {
    String response = "";
    Serial1.print(command);
    long int time = millis();
    while ((time + timeout) > millis()) {
        while (Serial1.available()) {
            char c = Serial1.read();
            response += c;
        }
    }
    if (debug) {
        SerialUSB.print(response);
    }
    return response;
}

void powerOnSIM7600() {
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    delay(2000); // Wait for module to be ready
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(1000);
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    SerialUSB.println("SIM7600 turned on.");
}

void powerOffSIM7600() {
    //digitalWrite(LTE_PWRKEY_PIN, HIGH);
    //delay(3000);
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(4000); // Ensure module is properly turned off
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    //delay(3000);
    //sendData("AT+CPOF", 3000, DEBUG);
    SerialUSB.println("SIM7600 turned off.");
}
