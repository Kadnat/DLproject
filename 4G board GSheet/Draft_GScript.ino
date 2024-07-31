// Author: Nathanael Blavo Ballarin
// Date: July 24, 2024

#include <Wire.h>
#include <SD.h>
#include <SPI.h>

#define CS_PIN 4  // SD card selection pin
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7

String receivedData = "";
bool dataReceived = false;
const int maxRetries = 15;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 86400000; // 24 hours in milliseconds
String currentDate = "";
String lastSentCSV = "";

File dataFile;
File lastSentFile;

/**
 * @brief Initializes the necessary components and performs setup operations.
 * 
 * This function is called once when the microcontroller starts up. It initializes the serial communication,
 * sets the pin modes, initializes the SD card, and reads the name of the last sent CSV file. It also calls
 * the `sendCSVToGoogleSheets` function to send a CSV file to Google Sheets.
 */
void setup() {
    SerialUSB.begin(115200);
    Serial1.begin(115200);
    
    pinMode(LTE_RESET_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);
    
    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);
    delay(100);
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(2000);
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    
    pinMode(LTE_FLIGHT_PIN, OUTPUT);
    digitalWrite(LTE_FLIGHT_PIN, LOW); // Normal Mode
    
    delay(5000);

    SerialUSB.println("4G HTTP Test Begin!");

    delay(1000);
    
    Wire.begin(0x08); // Initialize as slave with address 0x08
    Wire.onReceive(receiveEvent); // Attach the receiveEvent function

    // Initialize the SD card
    if (!SD.begin(CS_PIN)) {
        SerialUSB.println("SD card initialization failed!");
        while (1);
    }
    SerialUSB.println("SD card initialized.");

    // Read the name of the last sent CSV file
    lastSentFile = SD.open("lastsent.txt");
    if (lastSentFile) {
        lastSentCSV = lastSentFile.readStringUntil('\n');
        lastSentFile.close();
    } else {
        SerialUSB.println("Error opening lastsent.txt for reading");
    }

    sendCSVToGoogleSheets("2024-6-5");
}

/**
 * The main loop of the program.
 * This function is called repeatedly and handles the processing of received data.
 */
void loop() {
    if (dataReceived) {
        dataReceived = false;

        SerialUSB.println(receivedData);

        // Create a mutable copy of the received data
        char receivedDataCopy[receivedData.length() + 1];
        strcpy(receivedDataCopy, receivedData.c_str());

        // Parse the received data for date, time, node number, temperature, humidity, pressure, dendrometer, and soil moisture
        int node = 0;
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        float t = 0.0;
        float h = 0.0;
        float p = 0.0;
        float d = 0.0;
        float m = 0.0;

        char *token = strtok(receivedDataCopy, ",");
        node = atoi(token);

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

        // Build the date and time as a string
        String datetime = String(year) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(minute) + ":" + String(second);
        String date = String(year) + String(month) + String(day);

        token = strtok(NULL, ",");
        t = atof(token);

        token = strtok(NULL, ",");
        h = atof(token);

        token = strtok(NULL, ",");
        p = atof(token);

        token = strtok(NULL, ",");
        d = atof(token);

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
        SerialUSB.print(", Dendro=");
        SerialUSB.print(d);
        SerialUSB.print(", Moisture=");
        SerialUSB.println(m);

        // If the date has changed, send the data from the previous day
        if (currentDate != "" && currentDate != date) {
            sendCSVToGoogleSheets(currentDate);
            currentDate = date;
        } else if (currentDate == "") {
            currentDate = date;
        }
        
        // Save the data to the SD card in a CSV file
        String fileName = currentDate + ".csv";
        dataFile = SD.open(fileName, FILE_WRITE);
        if (dataFile) {
            dataFile.print(datetime);
            dataFile.print(", Node=");
            dataFile.print(node);
            dataFile.print(", Temp=");
            dataFile.print(t, 4);
            dataFile.print(", Hum=");
            dataFile.print(h, 4);
            dataFile.print(", Press=");
            dataFile.print(p, 4);
            dataFile.print(", Dendro=");
            dataFile.print(d, 4);
            dataFile.print(", Moisture=");
            dataFile.println(m, 4);
            dataFile.close();
            SerialUSB.println("Data saved to " + fileName);
        } else {
            SerialUSB.println("Error opening file " + fileName);
        }
    }
}

/**
 * @brief Receives data from the I2C bus.
 * 
 * This function is called when data is received from the I2C bus. It reads the data
 * byte by byte and stores it in the `receivedData` variable. Once all the data is
 * received, it sets the `dataReceived` flag to true.
 * 
 * @param howMany The number of bytes received.
 */
void receiveEvent(int howMany) {
    receivedData = "";
    while (Wire.available()) {
        char c = Wire.read();
        receivedData += c;
    }
    dataReceived = true;
}

/**
 * Sends the contents of a CSV file to Google Sheets.
 * 
 * @param date The date used to construct the filename of the CSV file.
 */
void sendCSVToGoogleSheets(String date) {
    String fileName = date + ".csv";

    // Activate the 4G module
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(2000);
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    delay(5000); // Wait for the module to be ready

    // Read data from the CSV file
    dataFile = SD.open(fileName);
    if (dataFile) {
        while (dataFile.available()) {
            String line = dataFile.readStringUntil('\n');

            // Create a copy of the line for strtok
            char lineCopy[line.length() + 1];
            strcpy(lineCopy, line.c_str());

            // Parse the values from the line
            char *token;
            char datetime[20];
            int node;
            float temp, hum, press, dendro, moisture;

            token = strtok(lineCopy, ",");
            strcpy(datetime, token);

            token = strtok(NULL, ",");
            node = atoi(strchr(token, '=') + 1);

            token = strtok(NULL, ",");
            temp = atof(strchr(token, '=') + 1);

            token = strtok(NULL, ",");
            hum = atof(strchr(token, '=') + 1);

            token = strtok(NULL, ",");
            press = atof(strchr(token, '=') + 1);

            token = strtok(NULL, ",");
            dendro = atof(strchr(token, '=') + 1);

            token = strtok(NULL, ",");
            moisture = atof(strchr(token, '=') + 1);

            // Send the data without retrying
            sendDataToGoogleSheets(String(datetime), node, temp, hum, press, dendro, moisture);
        }
        dataFile.close();

        // Save the name of the last sent CSV file
        lastSentFile = SD.open("lastsent.txt", FILE_WRITE);
        if (lastSentFile) {
            lastSentFile.println(fileName);
            lastSentFile.close();
        } else {
            SerialUSB.println("Error opening lastsent.txt for writing");
        }
    } else {
        SerialUSB.println("Error opening file " + fileName + " for reading");
    }

    // Turn off the 4G module
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(2000);
    digitalWrite(LTE_PWRKEY_PIN, LOW);
}


/**
 * Sends data to Google Sheets using a Web App URL.
 * 
 * @param datetime The date and time of the data.
 * @param node The node identifier.
 * @param temp The temperature value.
 * @param hum The humidity value.
 * @param press The pressure value.
 * @param dendro The dendrometer value.
 * @param moisture The moisture value.
 * @return True if the data is sent successfully, false otherwise.
 */
bool sendDataToGoogleSheets(String datetime, int node, float temp, float hum, float press, float dendro, float moisture) {
    // URL of your Web App
    String webAppUrl = "https://script.google.com/macros/s/testestexec";

    // Build the named parameters
    String params = "datetime=" + urlEncode(datetime) +
                    "&node=" + String(node) +
                    "&temp=" + String(temp, 4) +
                    "&hum=" + String(hum, 4) +
                    "&press=" + String(press, 4) +
                    "&dendro=" + String(dendro, 4) +
                    "&moisture=" + String(moisture, 4);

    // Build the URL with the encoded data
    String http_str = "AT+HTTPPARA=\"URL\",\"" + webAppUrl + "?" + params + "\"\r\n";
    SerialUSB.println(http_str);

    bool success = false;

    sendData("AT+HTTPINIT\r\n", 2000, DEBUG);
    delay(2000); // Delay after HTTP initialization

    String response = sendData(http_str, 2000, DEBUG);
    delay(2000); // Delay after sending HTTP parameters

    String actionResponse = sendData("AT+HTTPACTION=0\r\n", 3000, DEBUG);
    delay(3000); // Delay after HTTP action

    sendData("AT+HTTPTERM\r\n", 3000, DEBUG);
    delay(3000); // Delay after HTTP termination

    if (actionResponse.indexOf("200") >= 0) {
        success = true;
        SerialUSB.println("Data sent successfully!");
    } else {
        SerialUSB.println("Failed to send data.");
    }

    return success;
}

/**
 * Sends a command to the Serial1 port and waits for a response.
 * 
 * @param command The command to send.
 * @param timeout The maximum time to wait for a response, in milliseconds.
 * @param debug   Whether to print the response to the SerialUSB port for debugging purposes.
 * @return        The response received from the Serial1 port.
 */
String sendData(String command, const int timeout, boolean debug) {
    String response = "";
    Serial1.println(command);
    
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

/**
 * Encodes a given string using URL encoding.
 * 
 * @param str The string to be encoded.
 * @return The URL-encoded string.
 */
String urlEncode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            code2 = '\0';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
        yield();
    }
    return encodedString;
}

