// Author: Nathanael Blavo Ballarin
// Date: July 24, 2024

#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGsmClient.h>
#include <ESP_Google_Sheet_Client.h>

// Pin definitions
#define CS_PIN 4  // SD card select pin
#define LTE_RESET_PIN 6
#define LTE_PWRKEY_PIN 5
#define LTE_FLIGHT_PIN 7

// Debug flag
#define DEBUG true

// GSM configuration
#define MODEM_RST LTE_RESET_PIN
#define MODEM_PWRKEY LTE_PWRKEY_PIN
#define MODEM_POWER_ON 7
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_BAUD 115200

// Google Sheets configuration
#define PROJECT_ID "project"
#define CLIENT_EMAIL "client@project.iam.gserviceaccount.com"
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\n\n-----END PRIVATE KEY-----\n";

// GSM and Google Sheets objects
TinyGsm modem(SerialAT);
TinyGsmClient gsm_client(modem);
ESP_Google_Sheet_Client GSheet;

// Variables
bool gsheetSetupReady = false;
bool taskComplete = false;
String receivedData = "";
bool dataReceived = false;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 86400000; // 24 hours in milliseconds
String currentDate = "";
String lastSentCSV = "";

File dataFile;
File lastSentFile;

// Function prototypes
void setupGsheet();
void tokenStatusCallback(TokenInfo info);
void receiveEvent(int howMany);
void sendCSVToGoogleSheets(String date);

/**
 * @brief Initializes the necessary components and configurations for the program.
 * 
 * This function is called once when the program starts. It performs the following tasks:
 * - Initializes serial communication with the USB port and the modem.
 * - Sets the pin modes for various pins used in the program.
 * - Initializes I2C communication as a slave with address 0x08.
 * - Attaches the receiveEvent function to handle I2C data reception.
 * - Initializes the SD card and checks if it is successfully initialized.
 * - Reads the name of the last sent CSV file from the SD card.
 * - Restarts the modem and establishes a GPRS connection.
 * - Sets the GSM client and modem for Google Sheets communication.
 * 
 * @return void
 */
void setup() {
    // Initialize serial communication
    SerialUSB.begin(115200);
    SerialAT.begin(MODEM_BAUD);

    // Set pin modes
    pinMode(LTE_RESET_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);

    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(2000);
    digitalWrite(LTE_PWRKEY_PIN, LOW);

    pinMode(LTE_FLIGHT_PIN, OUTPUT);
    digitalWrite(LTE_FLIGHT_PIN, LOW); // Normal Mode

    delay(5000);

    SerialUSB.println("4G HTTP Test Begin!");

    delay(1000);

    // Initialize I2C communication
    Wire.begin(0x08); // Initialize as slave with address 0x08
    Wire.onReceive(receiveEvent); // Attach receiveEvent function

    // Initialize SD card
    if (!SD.begin(CS_PIN)) {
        SerialUSB.println("Failed to initialize SD card!");
        while (1);
    }
    SerialUSB.println("SD card initialized.");

    // Read the name of the last sent CSV file
    lastSentFile = SD.open("lastsent.txt");
    if (lastSentFile) {
        lastSentCSV = lastSentFile.readStringUntil('\n');
        lastSentFile.close();
    } else {
        SerialUSB.println("Failed to open lastsent.txt for reading");
    }

    // Initialize the modem
    modem.restart();
    modem.gprsConnect(apn, gprsUser, gprsPass);

    GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);

    // Set GSM client and modem for Google Sheets
    GSheet.setGSMClient(&gsm_client, &modem, GSM_PIN, apn, gprsUser, gprsPass);
}

/**
 * The main loop function that runs repeatedly.
 * It checks if data has been received, parses the received data,
 * saves the data to a CSV file, and sends the data to Google Sheets.
 */
void loop() {
    if (dataReceived) {
        dataReceived = false;

        SerialUSB.println(receivedData);

        // Create a mutable copy of the received data
        char receivedDataCopy[receivedData.length() + 1];
        strcpy(receivedDataCopy, receivedData.c_str());

        // Parse the received data for node, date, time, temperature, humidity, pressure, dendrometer, and soil moisture
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
        String date = String(year) + "-" + String(month) + "-" + String(day);

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
            SerialUSB.println("Failed to open file " + fileName);
        }
    }
}

/**
 * @brief This function is called when data is received via I2C communication.
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
 * Sends a CSV file to Google Sheets.
 * 
 * @param date The date used to generate the file name.
 */
void sendCSVToGoogleSheets(String date) {
    String fileName = date + ".csv";

    // Read data from the CSV file
    dataFile = SD.open(fileName);
    if (dataFile) {
        FirebaseJson doc;
        int row = 0;
        while (dataFile.available()) {
            String line = dataFile.readStringUntil('\n');

            // Parse the line into a FirebaseJson object
            char* token = strtok(const_cast<char*>(line.c_str()), ",");
            int col = 0;
            while (token != NULL) {
                doc.set(String("values/[") + row + "]/[" + col + "]", token);
                token = strtok(NULL, ",");
                col++;
            }
            row++;
        }
        dataFile.close();

        // Send the FirebaseJson object to Google Sheets using the append technique
        FirebaseJson response;
        bool success = GSheet.values.append(&response, "sheetID", "Sheet2!A1", &doc);
        if (success) {
            SerialUSB.println("Data sent to Google Sheets successfully.");
        } else {
            SerialUSB.println("Failed to send data to Google Sheets.");
            SerialUSB.println(GSheet.errorReason());
        }

        // Save the name of the last sent CSV file
        lastSentFile = SD.open("lastsent.txt", FILE_WRITE);
        if (lastSentFile) {
            lastSentFile.println(fileName);
            lastSentFile.close();
        } else {
            SerialUSB.println("Failed to open lastsent.txt for writing");
        }
    } else {
        SerialUSB.println("Failed to open file " + fileName + " for reading");
    }
}


/**
 * @brief Initializes the setup for Google Sheets integration.
 * 
 * This function sets the callback for Google API access token generation status (for debug purposes only).
 * It also sets the seconds to refresh the authentication token before it expires.
 * Finally, it begins the access token generation for Google API authentication using the provided client email, project ID, and private key.
 * 
 * @param None
 * @return None
 */
void setupGsheet()
{
    // Set the callback for Google API access token generation status (for debug only)
    GSheet.setTokenCallback(tokenStatusCallback);

    // Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
    GSheet.setPrerefreshSeconds(10 * 60);

    // Begin the access token generation for Google API authentication
    GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

    gsheetSetupReady = true;
}

/**
 * Callback function to handle token status information.
 * 
 * @param info The TokenInfo object containing the token status information.
 */
void tokenStatusCallback(TokenInfo info)
{
    if (info.status == token_status_error)
    {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else
    {
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}
