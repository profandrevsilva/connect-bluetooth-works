#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLERemoteDescriptor.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "andre";
const char* password = "@Nd16727316";

// =====================================================
// WEB SERVER
// =====================================================

WebServer server(80);

// =====================================================
// MICRO:BIT
// =====================================================

const char* MICROBIT_NAME_PREFIX = "BBC micro:bit";

// =====================================================
// NORDIC UART SERVICE
// =====================================================

// NUS Service
static BLEUUID UART_SERVICE_UUID(
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
);

// micro:bit -> ESP32
// NUS TX characteristic
static BLEUUID UART_RX_UUID(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
);

// ESP32 -> micro:bit
// NUS RX characteristic
static BLEUUID UART_TX_UUID(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
);

// Client Characteristic Configuration Descriptor
static BLEUUID CCCD_UUID(
    (uint16_t)0x2902
);

// =====================================================
// SENSOR
// =====================================================

float temperature = 0.0;
float humidity = 0.0;

bool dataReceived = false;

unsigned long lastTemperatureTime = 0;

const unsigned long DATA_TIMEOUT = 10000;

// =====================================================
// BLE
// =====================================================

BLEClient* bleClient = nullptr;

BLERemoteCharacteristic* rxCharacteristic = nullptr;
BLERemoteCharacteristic* txCharacteristic = nullptr;

bool bleConnected = false;

String microbitAddress = "<unknown>";
String microbitName = "<unknown>";

String receivedData = "";

unsigned long lastBLERetry = 0;

const unsigned long BLE_RETRY_INTERVAL = 5000;

// =====================================================
// FORWARD DECLARATIONS
// =====================================================

void cleanupBLEConnection();

bool connectToMicrobit();

bool connectAndVerifyNUS(
    BLEAdvertisedDevice* device
);

bool enableNotifications();

bool sendToMicrobit(
    const String& message
);

bool sendConnectionBeep();

void notifyCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify
);

void handleRoot();
void handleAPIData();
void handleStatus();
void handleBeep();
void handleFavicon();

String makeHTML();

// =====================================================
// BLE CLIENT CALLBACK
// =====================================================

class MyClientCallback : public BLEClientCallbacks {

    void onConnect(
        BLEClient* client
    ) override {

        Serial.println();
        Serial.println(
            "BLE: Client connected"
        );
    }

    void onDisconnect(
        BLEClient* client
    ) override {

        Serial.println();
        Serial.println(
            "BLE: !!! CLIENT DISCONNECTED !!!"
        );

        bleConnected = false;

        rxCharacteristic = nullptr;
        txCharacteristic = nullptr;

        lastBLERetry = millis();
    }
};

// =====================================================
// NOTIFICATION CALLBACK
// =====================================================

void notifyCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify
) {

    String message = "";

    for (
        size_t i = 0;
        i < length;
        i++
    ) {
        message += (char)data[i];
    }

    Serial.print(
        "BLE RX: "
    );

    Serial.println(message);

    receivedData += message;

    // Prevent unlimited buffer growth
    if (receivedData.length() > 2048) {

        receivedData = "";

        Serial.println(
            "BLE RX: buffer cleared"
        );
    }

    // =================================================
    // PROCESS COMPLETE LINES
    // =================================================

    int newlineIndex;

    while (
        (newlineIndex =
            receivedData.indexOf('\n')) >= 0
    ) {

        String line =
            receivedData.substring(
                0,
                newlineIndex
            );

        receivedData.remove(
            0,
            newlineIndex + 1
        );

        line.trim();

        if (line.length() == 0) {
            continue;
        }

        Serial.print(
            "BLE MESSAGE: "
        );

        Serial.println(line);

        // =================================================
        // TEMP
        // =================================================

        int tempIndex =
            line.indexOf("TEMP:");

        if (tempIndex >= 0) {

            int tempStart =
                tempIndex + 5;

            int tempEnd =
                line.indexOf(
                    ';',
                    tempStart
                );

            if (tempEnd < 0) {
                tempEnd = line.length();
            }

            String tempString =
                line.substring(
                    tempStart,
                    tempEnd
                );

            temperature =
                tempString.toFloat();

            lastTemperatureTime =
                millis();

            dataReceived = true;

            Serial.print(
                "Temperature = "
            );

            Serial.print(
                temperature,
                1
            );

            Serial.println(
                " C"
            );
        }

        // =================================================
        // HUMIDITY
        // =================================================

        int humIndex =
            line.indexOf("HUM:");

        if (humIndex >= 0) {

            int humStart =
                humIndex + 4;

            int humEnd =
                line.length();

            int semicolon =
                line.indexOf(
                    ';',
                    humStart
                );

            if (semicolon >= 0) {
                humEnd = semicolon;
            }

            String humString =
                line.substring(
                    humStart,
                    humEnd
                );

            humidity =
                humString.toFloat();

            Serial.print(
                "Humidity = "
            );

            Serial.print(
                humidity,
                1
            );

            Serial.println(
                " %"
            );
        }

        // =================================================
        // BEEP
        // =================================================

        if (line == "BEEP_OK") {

            Serial.println(
                "MICRO:BIT: BEEP_OK"
            );
        }
    }
}

// =====================================================
// ENABLE NOTIFICATIONS
// =====================================================
//
// This is the important part.
//
// The old code did:
//
//     if (!rxCharacteristic->canNotify())
//         reject;
//
// That caused:
//
//     RX cannot notify
//
// We do NOT reject the connection anymore.
//
// We first register the callback.
// Then we explicitly try to enable the CCCD.
//
// =====================================================

bool enableNotifications()
{
    if (rxCharacteristic == nullptr) {

        Serial.println(
            "BLE ERROR: RX characteristic is NULL"
        );

        return false;
    }

    Serial.println();
    Serial.println(
        "BLE: Configuring RX notifications..."
    );

    // =================================================
    // REGISTER CALLBACK
    // =================================================

    rxCharacteristic->registerForNotify(
        notifyCallback
    );

    Serial.println(
        "BLE: Notification callback registered"
    );

    delay(200);

    // =================================================
    // FIND CCCD
    // =================================================

    BLERemoteDescriptor* cccd =
        rxCharacteristic->getDescriptor(
            CCCD_UUID
        );

    if (cccd != nullptr) {

        Serial.println(
            "BLE: CCCD descriptor found"
        );

        uint8_t notifyOn[2] =
        {
            0x01,
            0x00
        };

        try {

            cccd->writeValue(
                notifyOn,
                2,
                true
            );

            Serial.println(
                "BLE: CCCD notification ENABLED"
            );

            return true;

        }
        catch (...) {

            Serial.println(
                "BLE: CCCD write failed"
            );
        }
    }
    else {

        Serial.println(
            "BLE: CCCD descriptor not found"
        );

        Serial.println(
            "BLE: Continuing with registered callback..."
        );
    }

    // =================================================
    // IMPORTANT:
    //
    // Do not disconnect simply because the old
    // BLE library cannot retrieve the descriptor.
    // =================================================

    return true;
}

// =====================================================
// SEND TO MICRO:BIT
// =====================================================

bool sendToMicrobit(
    const String& message
)
{
    if (
        bleClient == nullptr
    ) {

        Serial.println(
            "BLE TX: Client NULL"
        );

        return false;
    }

    if (
        !bleClient->isConnected()
    ) {

        Serial.println(
            "BLE TX: Not connected"
        );

        return false;
    }

    if (
        txCharacteristic == nullptr
    ) {

        Serial.println(
            "BLE TX: TX characteristic NULL"
        );

        return false;
    }

    Serial.print(
        "BLE TX -> "
    );

    Serial.print(
        message
    );

    Serial.println();

    if (
        txCharacteristic->canWrite()
    ) {

        txCharacteristic->writeValue(
            (uint8_t*)message.c_str(),
            message.length(),
            false
        );

        Serial.println(
            "BLE TX: sent"
        );

        return true;
    }

    if (
        txCharacteristic->canWriteNoResponse()
    ) {

        txCharacteristic->writeValue(
            (uint8_t*)message.c_str(),
            message.length(),
            true
        );

        Serial.println(
            "BLE TX: sent without response"
        );

        return true;
    }

    Serial.println(
        "BLE TX ERROR: Cannot write"
    );

    return false;
}

// =====================================================
// BEEP
// =====================================================

bool sendConnectionBeep()
{
    if (!bleConnected) {

        return false;
    }

    return sendToMicrobit(
        "BEEP\n"
    );
}

// =====================================================
// CLEAN CONNECTION
// =====================================================

void cleanupBLEConnection()
{
    rxCharacteristic = nullptr;
    txCharacteristic = nullptr;

    if (
        bleClient != nullptr
    ) {

        if (
            bleClient->isConnected()
        ) {

            Serial.println(
                "BLE: Disconnecting old connection..."
            );

            bleClient->disconnect();

            delay(200);
        }

        delete bleClient;

        bleClient = nullptr;
    }

    bleConnected = false;
}

// =====================================================
// PRINT MANUFACTURER
// =====================================================

void printManufacturerData(
    BLEAdvertisedDevice& device
)
{
    if (
        !device.haveManufacturerData()
    ) {

        Serial.println(
            "Manufacturer: <none>"
        );

        return;
    }

    std::string data =
        device.getManufacturerData();

    Serial.print(
        "Manufacturer: "
    );

    for (
        size_t i = 0;
        i < data.length();
        i++
    ) {

        uint8_t b =
            (uint8_t)data[i];

        if (b < 16) {
            Serial.print("0");
        }

        Serial.print(
            b,
            HEX
        );

        Serial.print(" ");
    }

    Serial.println();
}

// =====================================================
// PRINT DEVICE
// =====================================================

void printBLEDevice(
    BLEAdvertisedDevice& device,
    int number
)
{
    Serial.println();
    Serial.println(
        "--------------------------------"
    );

    Serial.print(
        "DEVICE #"
    );

    Serial.println(
        number
    );

    Serial.print(
        "Address: "
    );

    Serial.println(
        device.getAddress()
            .toString()
            .c_str()
    );

    Serial.print(
        "RSSI: "
    );

    Serial.print(
        device.getRSSI()
    );

    Serial.println(
        " dBm"
    );

    if (
        device.haveName()
    ) {

        Serial.print(
            "Name: "
        );

        Serial.println(
            device.getName()
                .c_str()
        );
    }
    else {

        Serial.println(
            "Name: <none>"
        );
    }

    if (
        device.haveServiceUUID()
    ) {

        Serial.print(
            "Service UUID: "
        );

        Serial.println(
            device.getServiceUUID()
                .toString()
                .c_str()
        );
    }
    else {

        Serial.println(
            "Service UUID: <none>"
        );
    }

    printManufacturerData(
        device
    );
}

// =====================================================
// CHECK MICRO:BIT NAME
// =====================================================

bool isMicrobitDevice(
    BLEAdvertisedDevice& device
)
{
    if (!device.haveName()) {

        return false;
    }

    String name =
        device.getName().c_str();

    Serial.print(
        "Checking device name: "
    );

    Serial.println(name);

    if (
        name.startsWith(
            MICROBIT_NAME_PREFIX
        )
    ) {

        Serial.println(
            ">>> MICRO:BIT NAME MATCH <<<"
        );

        return true;
    }

    return false;
}

// =====================================================
// CONNECT AND VERIFY NUS
// =====================================================

bool connectAndVerifyNUS(
    BLEAdvertisedDevice* device
)
{
    if (device == nullptr) {
        return false;
    }

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "BLE: CONNECTING TO MICRO:BIT"
    );

    Serial.println(
        "================================"
    );

    Serial.print(
        "Address: "
    );

    Serial.println(
        device->getAddress()
            .toString()
            .c_str()
    );

    if (
        device->haveName()
    ) {

        Serial.print(
            "Name: "
        );

        Serial.println(
            device->getName()
                .c_str()
        );
    }

    // =================================================
    // CLEAN PREVIOUS CLIENT
    // =================================================

    cleanupBLEConnection();

    // =================================================
    // CREATE CLIENT
    // =================================================

    bleClient =
        BLEDevice::createClient();

    if (
        bleClient == nullptr
    ) {

        Serial.println(
            "BLE ERROR: createClient failed"
        );

        return false;
    }

    bleClient->setClientCallbacks(
        new MyClientCallback()
    );

    // =================================================
    // CONNECT
    // =================================================

    Serial.println(
        "BLE: Connecting..."
    );

    if (
        !bleClient->connect(device)
    ) {

        Serial.println(
            "BLE ERROR: Connection failed"
        );

        cleanupBLEConnection();

        return false;
    }

    Serial.println(
        "BLE: Physical connection OK"
    );

    // =================================================
    // SAVE DEVICE
    // =================================================

    microbitAddress =
        device->getAddress()
            .toString()
            .c_str();

    if (
        device->haveName()
    ) {

        microbitName =
            device->getName()
                .c_str();
    }

    // =================================================
    // GET NUS
    // =================================================

    Serial.println(
        "BLE: Looking for Nordic UART Service..."
    );

    BLERemoteService* service =
        bleClient->getService(
            UART_SERVICE_UUID
        );

    if (
        service == nullptr
    ) {

        Serial.println(
            "BLE ERROR: NUS NOT FOUND"
        );

        cleanupBLEConnection();

        return false;
    }

    Serial.println(
        "BLE: *** NUS SERVICE FOUND ***"
    );

    // =================================================
    // RX
    // =================================================

    Serial.println(
        "BLE: Looking for RX..."
    );

    rxCharacteristic =
        service->getCharacteristic(
            UART_RX_UUID
        );

    if (
        rxCharacteristic == nullptr
    ) {

        Serial.println(
            "BLE ERROR: RX NOT FOUND"
        );

        cleanupBLEConnection();

        return false;
    }

    Serial.println(
        "BLE: RX characteristic FOUND"
    );

    Serial.print(
        "BLE: RX UUID: "
    );

    Serial.println(
        rxCharacteristic->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // NOTIFICATIONS
    // =================================================

    if (
        !enableNotifications()
    ) {

        Serial.println(
            "BLE ERROR: Notification setup failed"
        );

        cleanupBLEConnection();

        return false;
    }

    // =================================================
    // TX
    // =================================================

    Serial.println(
        "BLE: Looking for TX..."
    );

    txCharacteristic =
        service->getCharacteristic(
            UART_TX_UUID
        );

    if (
        txCharacteristic == nullptr
    ) {

        Serial.println(
            "BLE ERROR: TX NOT FOUND"
        );

        cleanupBLEConnection();

        return false;
    }

    Serial.println(
        "BLE: TX characteristic FOUND"
    );

    Serial.print(
        "BLE: TX UUID: "
    );

    Serial.println(
        txCharacteristic->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // DO NOT REQUIRE canNotify()
    // =================================================

    if (
        txCharacteristic->canWrite()
    ) {

        Serial.println(
            "BLE: TX supports WRITE"
        );
    }

    if (
        txCharacteristic->canWriteNoResponse()
    ) {

        Serial.println(
            "BLE: TX supports WRITE NO RESPONSE"
        );
    }

    // =================================================
    // CONNECTION SUCCESS
    // =================================================

    bleConnected = true;

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "BLE: *** MICRO:BIT CONNECTED ***"
    );

    Serial.println(
        "BLE: *** NUS READY ***"
    );

    Serial.println(
        "BLE: *** CONNECTION WILL STAY OPEN ***"
    );

    Serial.println(
        "================================"
    );

    delay(500);

    // =================================================
    // TEST BEEP
    // =================================================

    sendConnectionBeep();

    return true;
}

// =====================================================
// FIND MICRO:BIT
// =====================================================

bool connectToMicrobit()
{
    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "BLE: AUTOMATIC MICRO:BIT SEARCH"
    );

    Serial.println(
        "================================"
    );

    Serial.println(
        "Identification:"
    );

    Serial.println(
        "  1. BLE name"
    );

    Serial.println(
        "  2. Nordic UART Service"
    );

    Serial.println(
        "  3. NUS RX/TX"
    );

    Serial.println(
        "MAC address is NOT fixed"
    );

    // =================================================
    // CLEAN OLD CLIENT
    // =================================================

    cleanupBLEConnection();

    // =================================================
    // SCANNER
    // =================================================

    BLEScan* scan =
        BLEDevice::getScan();

    if (
        scan == nullptr
    ) {

        Serial.println(
            "BLE ERROR: Scanner unavailable"
        );

        return false;
    }

    scan->setActiveScan(true);

    scan->setInterval(100);

    scan->setWindow(80);

    Serial.println();
    Serial.println(
        "BLE: Scanning..."
    );

    BLEScanResults results =
        scan->start(
            8,
            false
        );

    int count =
        results.getCount();

    Serial.print(
        "BLE: Devices found: "
    );

    Serial.println(count);

    if (
        count <= 0
    ) {

        scan->clearResults();

        return false;
    }

    // =================================================
    // FIND MICRO:BIT
    // =================================================

    for (
        int i = 0;
        i < count;
        i++
    ) {

        BLEAdvertisedDevice device =
            results.getDevice(i);

        printBLEDevice(
            device,
            i + 1
        );

        if (
            isMicrobitDevice(device)
        ) {

            Serial.println();
            Serial.println(
                "BLE: *** MICRO:BIT CANDIDATE FOUND ***"
            );

            Serial.print(
                "BLE: Name: "
            );

            Serial.println(
                device.getName().c_str()
            );

            Serial.print(
                "BLE: Address: "
            );

            Serial.println(
                device.getAddress()
                    .toString()
                    .c_str()
            );

            // Make a copy before clearing scan
            BLEAdvertisedDevice* candidate =
                new BLEAdvertisedDevice(
                    device
                );

            scan->clearResults();

            bool success =
                connectAndVerifyNUS(
                    candidate
                );

            delete candidate;

            if (success) {

                Serial.println();
                Serial.println(
                    "BLE: *** MICRO:BIT READY ***"
                );

                return true;
            }

            Serial.println(
                "BLE: Candidate failed"
            );

            return false;
        }
    }

    scan->clearResults();

    Serial.println();
    Serial.println(
        "BLE: micro:bit not found"
    );

    return false;
}

// =====================================================
// HTML
// =====================================================

String makeHTML()
{
    String html = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<meta http-equiv="refresh" content="5">

<title>ESP32 Environmental Monitor</title>

<style>

body {
    font-family: Arial;
    background: #111827;
    color: white;
    text-align: center;
    margin: 0;
    padding: 30px;
}

.container {
    max-width: 700px;
    margin: auto;
}

.card {
    background: #1f2937;
    border-radius: 15px;
    padding: 30px;
    margin: 20px;
}

.value {
    font-size: 60px;
    font-weight: bold;
}

.unit {
    font-size: 25px;
}

.connected {
    color: #22c55e;
    font-weight: bold;
}

.disconnected {
    color: #ef4444;
    font-weight: bold;
}

.device {
    word-break: break-all;
}

button {
    padding: 15px 30px;
    font-size: 18px;
    border-radius: 10px;
    border: none;
    cursor: pointer;
}

</style>

</head>

<body>

<div class="container">

<h1>🌡️ ESP32 Environmental Monitor</h1>

<div class="card">

<h2>Temperature</h2>

<div class="value">
%TEMPERATURE%
</div>

<div class="unit">
°C
</div>

</div>

<div class="card">

<h2>Humidity</h2>

<div class="value">
%HUMIDITY%
</div>

<div class="unit">
%
</div>

</div>

<div class="card">

<h2>Status</h2>

<p>
Wi-Fi:
%WIFI_STATUS%
</p>

<p>
Bluetooth:
%BLE_STATUS%
</p>

<p>
Sensor:
%DATA_STATUS%
</p>

</div>

<div class="card">

<h2>Micro:bit</h2>

<p>
%BLE_NAME%
</p>

<p class="device">
%BLE_ADDRESS%
</p>

<br>

<button onclick="location.href='/beep'">
🔊 BEEP
</button>

</div>

</div>

</body>

</html>

)rawliteral";

    html.replace(
        "%TEMPERATURE%",
        String(
            temperature,
            1
        )
    );

    html.replace(
        "%HUMIDITY%",
        String(
            humidity,
            1
        )
    );

    html.replace(
        "%BLE_ADDRESS%",
        microbitAddress
    );

    html.replace(
        "%BLE_NAME%",
        microbitName
    );

    String wifiStatus =
        WiFi.status() == WL_CONNECTED
        ? "<span class='connected'>CONNECTED</span>"
        : "<span class='disconnected'>DISCONNECTED</span>";

    html.replace(
        "%WIFI_STATUS%",
        wifiStatus
    );

    String bleStatus =
        bleConnected
        ? "<span class='connected'>CONNECTED</span>"
        : "<span class='disconnected'>DISCONNECTED</span>";

    html.replace(
        "%BLE_STATUS%",
        bleStatus
    );

    bool recent =
        dataReceived &&
        millis() -
        lastTemperatureTime <
        DATA_TIMEOUT;

    String dataStatus =
        recent
        ? "<span class='connected'>RECEIVING</span>"
        : "<span class='disconnected'>NO RECENT DATA</span>";

    html.replace(
        "%DATA_STATUS%",
        dataStatus
    );

    return html;
}

// =====================================================
// WEB ROOT
// =====================================================

void handleRoot()
{
    server.send(
        200,
        "text/html",
        makeHTML()
    );
}

// =====================================================
// API
// =====================================================

void handleAPIData()
{
    String json = "{";

    json += "\"temperature\":";
    json += String(
        temperature,
        1
    );

    json += ",";

    json += "\"humidity\":";
    json += String(
        humidity,
        1
    );

    json += ",";

    json += "\"wifi\":";
    json +=
        WiFi.status() == WL_CONNECTED
        ? "true"
        : "false";

    json += ",";

    json += "\"ble\":";
    json +=
        bleConnected
        ? "true"
        : "false";

    json += ",";

    json += "\"address\":\"";
    json += microbitAddress;
    json += "\"";

    json += "}";

    server.send(
        200,
        "application/json",
        json
    );
}

// =====================================================
// STATUS
// =====================================================

void handleStatus()
{
    String status;

    status +=
        "ESP32 ENVIRONMENTAL MONITOR\n";

    status +=
        "============================\n";

    status +=
        "WiFi: ";

    status +=
        WiFi.status() == WL_CONNECTED
        ? "CONNECTED\n"
        : "DISCONNECTED\n";

    status +=
        "IP: ";

    status +=
        WiFi.localIP().toString();

    status +=
        "\n";

    status +=
        "BLE: ";

    status +=
        bleConnected
        ? "CONNECTED\n"
        : "DISCONNECTED\n";

    status +=
        "Micro:bit: ";

    status +=
        microbitName;

    status +=
        "\nAddress: ";

    status +=
        microbitAddress;

    status +=
        "\nTemperature: ";

    status +=
        String(
            temperature,
            1
        );

    status +=
        " C\nHumidity: ";

    status +=
        String(
            humidity,
            1
        );

    status +=
        " %\n";

    server.send(
        200,
        "text/plain",
        status
    );
}

// =====================================================
// BEEP
// =====================================================

void handleBeep()
{
    if (!bleConnected) {

        server.send(
            503,
            "text/plain",
            "Micro:bit is disconnected"
        );

        return;
    }

    if (
        sendConnectionBeep()
    ) {

        server.send(
            200,
            "text/plain",
            "BEEP sent"
        );

    }
    else {

        server.send(
            500,
            "text/plain",
            "BEEP failed"
        );
    }
}

// =====================================================
// FAVICON
// =====================================================

void handleFavicon()
{
    server.send(
        204,
        "text/plain",
        ""
    );
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(
        115200
    );

    delay(1000);

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "ESP32 + MICRO:BIT ENVIRONMENT MONITOR"
    );

    Serial.println(
        "LONG-LIVED BLE NUS CONNECTION"
    );

    Serial.println(
        "========================================"
    );

    // =================================================
    // WIFI
    // =================================================

    WiFi.mode(
        WIFI_STA
    );

    WiFi.begin(
        ssid,
        password
    );

    Serial.print(
        "WIFI: Connecting"
    );

    unsigned long start =
        millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - start < 20000
    ) {

        delay(500);

        Serial.print(".");
    }

    Serial.println();

    if (
        WiFi.status() == WL_CONNECTED
    ) {

        Serial.println(
            "WIFI: CONNECTED"
        );

        Serial.print(
            "WIFI IP: "
        );

        Serial.println(
            WiFi.localIP()
        );

    }
    else {

        Serial.println(
            "WIFI: FAILED"
        );
    }

    // =================================================
    // WEB SERVER
    // =================================================

    server.on(
        "/",
        handleRoot
    );

    server.on(
        "/api/data",
        handleAPIData
    );

    server.on(
        "/status",
        handleStatus
    );

    server.on(
        "/beep",
        handleBeep
    );

    server.on(
        "/favicon.ico",
        handleFavicon
    );

    server.begin();

    Serial.println(
        "WEB: Server started"
    );

    // =================================================
    // BLE
    // =================================================

    Serial.println(
        "BLE: Initializing..."
    );

    BLEDevice::init(
        "ESP32"
    );

    Serial.println(
        "BLE: Initialized"
    );

    // =================================================
    // FIRST CONNECTION
    // =================================================

    if (
        connectToMicrobit()
    ) {

        Serial.println();
        Serial.println(
            "SYSTEM: *** BLE CONNECTED ***"
        );

    }
    else {

        Serial.println();
        Serial.println(
            "SYSTEM: BLE connection failed"
        );

        lastBLERetry =
            millis();
    }

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "SYSTEM READY"
    );

    Serial.println(
        "========================================"
    );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    // =================================================
    // WEB SERVER
    // =================================================

    server.handleClient();

    // =================================================
    // WIFI
    // =================================================

    static bool lastWifiState =
        false;

    bool currentWifiState =
        WiFi.status() == WL_CONNECTED;

    if (
        currentWifiState !=
        lastWifiState
    ) {

        if (currentWifiState) {

            Serial.println(
                "WIFI: CONNECTED"
            );

            Serial.print(
                "IP: "
            );

            Serial.println(
                WiFi.localIP()
            );

        }
        else {

            Serial.println(
                "WIFI: DISCONNECTED"
            );
        }

        lastWifiState =
            currentWifiState;
    }

    // =================================================
    // BLE CONNECTION
    // =================================================

    if (
        bleClient != nullptr &&
        bleConnected
    ) {

        if (
            !bleClient->isConnected()
        ) {

            Serial.println();
            Serial.println(
                "BLE: !!! CONNECTION LOST !!!"
            );

            bleConnected =
                false;

            rxCharacteristic =
                nullptr;

            txCharacteristic =
                nullptr;

            lastBLERetry =
                millis();
        }
    }

    // =================================================
    // IMPORTANT:
    //
    // DO NOT SCAN WHILE CONNECTED.
    //
    // This is what helps keep the connection alive.
    // =================================================

    if (
        !bleConnected
    ) {

        if (
            millis() -
            lastBLERetry >=
            BLE_RETRY_INTERVAL
        ) {

            lastBLERetry =
                millis();

            Serial.println();
            Serial.println(
                "BLE: Automatic reconnect..."
            );

            if (
                connectToMicrobit()
            ) {

                Serial.println();
                Serial.println(
                    "BLE: *** RECONNECTED ***"
                );

            }
            else {

                Serial.println(
                    "BLE: Reconnect failed"
                );
            }
        }
    }

    // =================================================
    // SENSOR TIMEOUT
    // =================================================

    static bool timeoutPrinted =
        false;

    if (
        dataReceived &&
        millis() -
        lastTemperatureTime >
        DATA_TIMEOUT
    ) {

        if (
            !timeoutPrinted
        ) {

            Serial.println();
            Serial.println(
                "SENSOR: No data for 10 seconds"
            );

            timeoutPrinted =
                true;
        }

    }
    else {

        timeoutPrinted =
            false;
    }

    // =================================================
    // DO NOT USE A LARGE DELAY
    // =================================================

    delay(5);
}