#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID     = "andre";
const char* WIFI_PASSWORD = "@Nd16727316";

WebServer server(80);

// =====================================================
// MICRO:BIT IDENTIFICATION
// =====================================================

const char* MICROBIT_NAME_PREFIX = "BBC micro:bit";

const char* MICROBIT_ADDRESSES[] = {
    "fb:fe:84:0d:e5:45"
};

const int MICROBIT_ADDRESS_COUNT =
    sizeof(MICROBIT_ADDRESSES) /
    sizeof(MICROBIT_ADDRESSES[0]);

// =====================================================
// NORDIC UART SERVICE
// =====================================================

static BLEUUID NUS_SERVICE(
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
);

// =====================================================
// IMPORTANT
//
// Based on the successful diagnostic test:
//
// 6E400003:
//     no CCCD
//     no notify
//     no indicate
//
// 6E400002:
//     CCCD 0x2902 FOUND
//     INDICATE = YES
//
// The micro:bit is actually sending:
//
//     TEMP:29\r\n
//
// through 6E400002 as an INDICATION.
//
// Therefore we intentionally use 6E400002
// as the RECEIVE characteristic.
// =====================================================

static BLEUUID RECEIVE_UUID(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
);

// Keep 6E400003 for diagnostics/reference.
static BLEUUID UNUSED_003_UUID(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
);

// =====================================================
// SENSOR DATA
// =====================================================

float temperature = 0.0;

float humidity = 0.0;

bool dataReceived = false;

unsigned long lastDataTime = 0;

const unsigned long DATA_TIMEOUT = 10000;

// =====================================================
// BLE
// =====================================================

BLEClient* client = nullptr;

BLERemoteCharacteristic* receiveCharacteristic = nullptr;

bool bleConnected = false;

bool indicationEnabled = false;

String connectedName = "";

String connectedAddress = "";

String rxBuffer = "";

unsigned long lastReconnect = 0;

const unsigned long RECONNECT_INTERVAL = 5000;

// =====================================================
// FORWARD DECLARATIONS
// =====================================================

void scanAndConnect();

bool connectToDevice(
    BLEAdvertisedDevice* device
);

void indicationCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify
);

bool isAllowedMicrobit(
    BLEAdvertisedDevice& device
);

bool isAllowedAddress(
    String address
);

void cleanupConnection();

void handleRoot();

void handleAPI();

void handleStatus();

void processMessage(
    String line
);

// =====================================================
// BLE CLIENT CALLBACKS
// =====================================================

class ClientCallbacks : public BLEClientCallbacks {

    void onConnect(
        BLEClient* pClient
    ) override {

        Serial.println();
        Serial.println(
            "BLE: Client connected"
        );
    }

    void onDisconnect(
        BLEClient* pClient
    ) override {

        Serial.println();
        Serial.println(
            "BLE: !!! MICRO:BIT DISCONNECTED !!!"
        );

        bleConnected = false;

        indicationEnabled = false;

        receiveCharacteristic = nullptr;

        lastReconnect = millis();
    }
};

// =====================================================
// PROCESS SENSOR MESSAGE
// =====================================================

void processMessage(
    String line
) {

    line.trim();

    if (
        line.length() == 0
    ) {

        return;
    }

    Serial.println();
    Serial.println(
        "################################################"
    );

    Serial.println(
        ">>> DATA RECEIVED FROM MICRO:BIT <<<"
    );

    Serial.println(
        "################################################"
    );

    Serial.print(
        "MESSAGE: "
    );

    Serial.println(
        line
    );

    // =================================================
    // TEMPERATURE
    // =================================================

    int tempPos =
        line.indexOf("TEMP:");

    if (
        tempPos >= 0
    ) {

        Serial.println(
            ">>> TEMP FIELD DETECTED <<<"
        );

        int start =
            tempPos + 5;

        int end =
            line.indexOf(
                ';',
                start
            );

        if (
            end < 0
        ) {

            end =
                line.length();
        }

        String value =
            line.substring(
                start,
                end
            );

        value.trim();

        temperature =
            value.toFloat();

        Serial.print(
            "TEMPERATURE = "
        );

        Serial.print(
            temperature,
            1
        );

        Serial.println(
            " C"
        );

        dataReceived =
            true;

        lastDataTime =
            millis();

        Serial.println();
        Serial.println(
            ">>> TEMPERATURE UPDATED <<<"
        );

        Serial.print(
            "temperature variable = "
        );

        Serial.println(
            temperature,
            1
        );
    }

    // =================================================
    // HUMIDITY
    // =================================================

    int humPos =
        line.indexOf("HUM:");

    if (
        humPos >= 0
    ) {

        Serial.println(
            ">>> HUM FIELD DETECTED <<<"
        );

        int start =
            humPos + 4;

        int end =
            line.indexOf(
                ';',
                start
            );

        if (
            end < 0
        ) {

            end =
                line.length();
        }

        String value =
            line.substring(
                start,
                end
            );

        value.trim();

        humidity =
            value.toFloat();

        Serial.print(
            "HUMIDITY = "
        );

        Serial.print(
            humidity,
            1
        );

        Serial.println(
            " %"
        );

        dataReceived =
            true;

        lastDataTime =
            millis();
    }

    Serial.println(
        "################################################"
    );
}

// =====================================================
// INDICATION CALLBACK
// =====================================================
//
// The successful diagnostic test showed:
//
// Characteristic:
// 6e400002-b5a3-f393-e0a9-e50e24dcca9e
//
// Packet type:
// INDICATION
//
// Data:
// TEMP:29\r\n
//
// =====================================================

void indicationCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify
) {

    Serial.println();
    Serial.println(
        "################################################"
    );

    Serial.println(
        ">>> DATA RECEIVED FROM MICRO:BIT <<<"
    );

    Serial.println(
        "################################################"
    );

    // =================================================
    // CHARACTERISTIC
    // =================================================

    Serial.print(
        "Characteristic UUID: "
    );

    Serial.println(
        characteristic->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // PACKET TYPE
    // =================================================

    Serial.print(
        "Packet type: "
    );

    if (
        isNotify
    ) {

        Serial.println(
            "NOTIFICATION"
        );

    }
    else {

        Serial.println(
            "INDICATION"
        );
    }

    // =================================================
    // LENGTH
    // =================================================

    Serial.print(
        "Length: "
    );

    Serial.println(
        length
    );

    // =================================================
    // ASCII
    // =================================================

    Serial.print(
        "ASCII: "
    );

    for (
        size_t i = 0;
        i < length;
        i++
    ) {

        char c =
            (char)data[i];

        if (
            c >= 32 &&
            c <= 126
        ) {

            Serial.print(c);

        }
        else if (
            c == '\r'
        ) {

            Serial.print(
                "\\r"
            );

        }
        else if (
            c == '\n'
        ) {

            Serial.print(
                "\\n"
            );

        }
        else {

            Serial.print(
                "."
            );
        }
    }

    Serial.println();

    // =================================================
    // HEX
    // =================================================

    Serial.print(
        "HEX: "
    );

    for (
        size_t i = 0;
        i < length;
        i++
    ) {

        if (
            data[i] < 16
        ) {

            Serial.print(
                "0"
            );
        }

        Serial.print(
            data[i],
            HEX
        );

        Serial.print(
            " "
        );
    }

    Serial.println();

    // =================================================
    // ADD TO BUFFER
    // =================================================

    for (
        size_t i = 0;
        i < length;
        i++
    ) {

        rxBuffer +=
            (char)data[i];
    }

    // =================================================
    // PROCESS COMPLETE LINES
    // =================================================

    while (
        true
    ) {

        int newline =
            rxBuffer.indexOf(
                '\n'
            );

        if (
            newline < 0
        ) {

            break;
        }

        String line =
            rxBuffer.substring(
                0,
                newline
            );

        rxBuffer.remove(
            0,
            newline + 1
        );

        processMessage(
            line
        );
    }

    // =================================================
    // HANDLE MESSAGE WITHOUT NEWLINE
    // =================================================

    if (
        rxBuffer.indexOf("TEMP:") >= 0
    ) {

        // If the micro:bit ever sends TEMP:29
        // without \n, we can still process it.

        if (
            rxBuffer.length() > 0 &&
            rxBuffer.indexOf('\n') < 0
        ) {

            String tempMessage =
                rxBuffer;

            tempMessage.trim();

            if (
                tempMessage.endsWith(
                    "\r"
                )
            ) {

                tempMessage.remove(
                    tempMessage.length() - 1
                );
            }

            if (
                tempMessage.indexOf(
                    "TEMP:"
                ) >= 0
            ) {

                processMessage(
                    tempMessage
                );

                rxBuffer =
                    "";
            }
        }
    }

    // =================================================
    // BUFFER PROTECTION
    // =================================================

    if (
        rxBuffer.length() > 2048
    ) {

        Serial.println(
            "BLE RX: buffer overflow - clearing"
        );

        rxBuffer =
            "";
    }

    Serial.println(
        "################################################"
    );
}

// =====================================================
// CHECK ADDRESS
// =====================================================

bool isAllowedAddress(
    String address
) {

    address.toLowerCase();

    for (
        int i = 0;
        i < MICROBIT_ADDRESS_COUNT;
        i++
    ) {

        String allowed =
            String(
                MICROBIT_ADDRESSES[i]
            );

        allowed.toLowerCase();

        if (
            address == allowed
        ) {

            return true;
        }
    }

    return false;
}

// =====================================================
// CHECK MICRO:BIT
// =====================================================

bool isAllowedMicrobit(
    BLEAdvertisedDevice& device
) {

    String address =
        device.getAddress()
            .toString()
            .c_str();

    String name = "";

    if (
        device.haveName()
    ) {

        name =
            device.getName()
                .c_str();
    }

    Serial.print(
        "Checking device: "
    );

    Serial.print(
        name
    );

    Serial.print(
        " / "
    );

    Serial.println(
        address
    );

    // =================================================
    // NAME
    // =================================================

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

    // =================================================
    // ADDRESS
    // =================================================

    if (
        isAllowedAddress(
            address
        )
    ) {

        Serial.println(
            ">>> MICRO:BIT ADDRESS MATCH <<<"
        );

        return true;
    }

    return false;
}

// =====================================================
// CONNECT TO DEVICE
// =====================================================

bool connectToDevice(
    BLEAdvertisedDevice* device
) {

    if (
        device == nullptr
    ) {

        return false;
    }

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "BLE: CONNECTING TO MICRO:BIT"
    );

    Serial.println(
        "========================================"
    );

    Serial.print(
        "NAME: "
    );

    if (
        device->haveName()
    ) {

        Serial.println(
            device->getName()
                .c_str()
        );

    }
    else {

        Serial.println(
            "<none>"
        );
    }

    Serial.print(
        "ADDRESS: "
    );

    Serial.println(
        device->getAddress()
            .toString()
            .c_str()
    );

    // =================================================
    // CLEAN OLD CONNECTION
    // =================================================

    cleanupConnection();

    // =================================================
    // CREATE CLIENT
    // =================================================

    client =
        BLEDevice::createClient();

    if (
        client == nullptr
    ) {

        Serial.println(
            "BLE ERROR: createClient() failed"
        );

        return false;
    }

    client->setClientCallbacks(
        new ClientCallbacks()
    );

    // =================================================
    // CONNECT
    // =================================================

    Serial.println(
        "BLE: Connecting..."
    );

    if (
        !client->connect(
            device
        )
    ) {

        Serial.println(
            "BLE ERROR: connect() failed"
        );

        cleanupConnection();

        return false;
    }

    Serial.println(
        "BLE: Physical connection OK"
    );

    delay(500);

    // =================================================
    // NUS SERVICE
    // =================================================

    Serial.println();
    Serial.println(
        "BLE: Searching NUS..."
    );

    BLERemoteService* service =
        client->getService(
            NUS_SERVICE
        );

    if (
        service == nullptr
    ) {

        Serial.println(
            "BLE ERROR: NUS SERVICE NOT FOUND"
        );

        cleanupConnection();

        return false;
    }

    Serial.println(
        "BLE: *** NUS SERVICE FOUND ***"
    );

    Serial.print(
        "NUS UUID: "
    );

    Serial.println(
        service->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // IMPORTANT:
    //
    // DO NOT USE 6E400003.
    //
    // We discovered that it has no notification
    // infrastructure on this micro:bit.
    //
    // Use 6E400002 because it has:
    //
    //     INDICATE = YES
    //     CCCD = 0x2902
    //
    // and it is where TEMP:29 is actually arriving.
    // =================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "BLE: USING 6E400002 AS RECEIVE CHANNEL"
    );

    Serial.println(
        "BLE: EXPECTING INDICATIONS"
    );

    Serial.println(
        "========================================"
    );

    receiveCharacteristic =
        service->getCharacteristic(
            RECEIVE_UUID
        );

    // =================================================
    // CHECK NULL BEFORE USING CHARACTERISTIC
    // =================================================

    if (
        receiveCharacteristic == nullptr
    ) {

        Serial.println(
            "BLE ERROR: 6E400002 NOT FOUND"
        );

        cleanupConnection();

        return false;
    }

    Serial.println(
        "BLE: *** 6E400002 FOUND ***"
    );

    Serial.print(
        "UUID: "
    );

    Serial.println(
        receiveCharacteristic->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // HANDLE
    // =================================================

    Serial.print(
        "Handle: "
    );

    Serial.println(
        receiveCharacteristic->getHandle()
    );

    // =================================================
    // CAPABILITIES
    // =================================================

    Serial.println();
    Serial.println(
        "========== 6E400002 CAPABILITIES =========="
    );

    Serial.print(
        "Can Read: "
    );

    Serial.println(
        receiveCharacteristic->canRead()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Write: "
    );

    Serial.println(
        receiveCharacteristic->canWrite()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Write No Response: "
    );

    Serial.println(
        receiveCharacteristic->canWriteNoResponse()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Notify: "
    );

    Serial.println(
        receiveCharacteristic->canNotify()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Indicate: "
    );

    Serial.println(
        receiveCharacteristic->canIndicate()
        ? "YES"
        : "NO"
    );

    Serial.println(
        "==========================================="
    );

    // =================================================
    // REGISTER INDICATION
    // =================================================

    if (
        !receiveCharacteristic->canIndicate()
    ) {

        Serial.println();
        Serial.println(
            "BLE ERROR:"
        );

        Serial.println(
            "6E400002 does NOT support indication."
        );

        cleanupConnection();

        return false;
    }

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "BLE: REGISTERING INDICATION CALLBACK"
    );

    Serial.println(
        "========================================"
    );

    // =================================================
    // IMPORTANT
    //
    // notifications = false
    //
    // means:
    //
    //     use INDICATION
    //
    // descriptorRequiresRegistration = true
    //
    // because the characteristic has CCCD 0x2902.
    // =================================================

    receiveCharacteristic->registerForNotify(
        indicationCallback,
        false,
        true
    );

    indicationEnabled =
        true;

    Serial.println(
        "BLE: Indication registration requested"
    );

    delay(1000);

    // =================================================
    // SAVE DEVICE INFO
    // =================================================

    connectedAddress =
        device->getAddress()
            .toString()
            .c_str();

    if (
        device->haveName()
    ) {

        connectedName =
            device->getName()
                .c_str();

    }
    else {

        connectedName =
            "<unknown>";
    }

    // =================================================
    // READY
    // =================================================

    bleConnected =
        true;

    rxBuffer =
        "";

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "BLE: *** MICRO:BIT CONNECTED ***"
    );

    Serial.println(
        "BLE: *** NUS READY ***"
    );

    Serial.print(
        "BLE: RECEIVE CHARACTERISTIC = "
    );

    Serial.println(
        "6E400002"
    );

    Serial.print(
        "BLE: PACKET TYPE = "
    );

    Serial.println(
        "INDICATION"
    );

    Serial.print(
        "BLE: NAME = "
    );

    Serial.println(
        connectedName
    );

    Serial.print(
        "BLE: ADDRESS = "
    );

    Serial.println(
        connectedAddress
    );

    Serial.println(
        "BLE: WAITING FOR SENSOR DATA..."
    );

    Serial.println(
        "========================================"
    );

    return true;
}

// =====================================================
// CLEAN CONNECTION
// =====================================================

void cleanupConnection()
{

    receiveCharacteristic =
        nullptr;

    bleConnected =
        false;

    indicationEnabled =
        false;

    rxBuffer =
        "";

    if (
        client != nullptr
    ) {

        if (
            client->isConnected()
        ) {

            Serial.println(
                "BLE: Disconnecting old client..."
            );

            client->disconnect();

            delay(200);
        }

        delete client;

        client =
            nullptr;
    }
}

// =====================================================
// SCAN AND CONNECT
// =====================================================

void scanAndConnect()
{

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "BLE: SCANNING FOR MICRO:BIT"
    );

    Serial.println(
        "========================================"
    );

    BLEScan* scanner =
        BLEDevice::getScan();

    if (
        scanner == nullptr
    ) {

        Serial.println(
            "BLE ERROR: Scanner unavailable"
        );

        return;
    }

    scanner->setActiveScan(
        true
    );

    scanner->setInterval(
        100
    );

    scanner->setWindow(
        80
    );

    BLEScanResults results =
        scanner->start(
            8,
            false
        );

    int count =
        results.getCount();

    Serial.print(
        "BLE: Devices found: "
    );

    Serial.println(
        count
    );

    for (
        int i = 0;
        i < count;
        i++
    ) {

        BLEAdvertisedDevice device =
            results.getDevice(i);

        Serial.println();
        Serial.println(
            "--------------------------------"
        );

        Serial.print(
            "DEVICE #"
        );

        Serial.println(
            i + 1
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
            isAllowedMicrobit(
                device
            )
        ) {

            Serial.println();
            Serial.println(
                "BLE: *** MICRO:BIT CANDIDATE FOUND ***"
            );

            BLEAdvertisedDevice* candidate =
                new BLEAdvertisedDevice(
                    device
                );

            scanner->clearResults();

            bool success =
                connectToDevice(
                    candidate
                );

            delete candidate;

            if (
                success
            ) {

                return;
            }

            Serial.println(
                "BLE: Candidate connection failed"
            );

            return;
        }
    }

    scanner->clearResults();

    Serial.println();
    Serial.println(
        "BLE: No allowed micro:bit found"
    );
}

// =====================================================
// WEB ROOT
// =====================================================

void handleRoot()
{

    String html;

    html +=
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>"

        "<meta http-equiv='refresh' content='2'>"

        "<title>ESP32 Micro:bit Monitor</title>"

        "<style>"

        "body{"
        "font-family:Arial;"
        "background:#111827;"
        "color:white;"
        "text-align:center;"
        "padding:20px;"
        "}"

        ".container{"
        "max-width:700px;"
        "margin:auto;"
        "}"

        ".card{"
        "background:#1f2937;"
        "padding:25px;"
        "margin:15px;"
        "border-radius:15px;"
        "}"

        ".ipcard{"
        "background:#243047;"
        "padding:25px;"
        "margin:15px;"
        "border-radius:15px;"
        "}"

        ".ip{"
        "font-size:30px;"
        "font-weight:bold;"
        "}"

        ".value{"
        "font-size:55px;"
        "font-weight:bold;"
        "}"

        ".ok{"
        "color:#22c55e;"
        "font-weight:bold;"
        "}"

        ".bad{"
        "color:#ef4444;"
        "font-weight:bold;"
        "}"

        ".device{"
        "font-size:18px;"
        "word-break:break-all;"
        "}"

        ".small{"
        "font-size:14px;"
        "color:#9ca3af;"
        "}"

        "</style>"
        "</head>"

        "<body>"

        "<div class='container'>"

        "<h1>ESP32 + micro:bit</h1>";

    // =================================================
    // SERVER IP
    // =================================================

    html +=
        "<div class='ipcard'>"
        "<h2>ESP32 Server IP</h2>"
        "<div class='ip'>";

    html +=
        WiFi.localIP().toString();

    html +=
        "</div>"
        "<p class='small'>"
        "Open this IP in your browser"
        "</p>"
        "</div>";

    // =================================================
    // TEMPERATURE
    // =================================================

    html +=
        "<div class='card'>"
        "<h2>Temperature</h2>"
        "<div class='value'>";

    html +=
        String(
            temperature,
            1
        );

    html +=
        " &deg;C"
        "</div>"
        "</div>";

    // =================================================
    // HUMIDITY
    // =================================================

    html +=
        "<div class='card'>"
        "<h2>Humidity</h2>"
        "<div class='value'>";

    html +=
        String(
            humidity,
            1
        );

    html +=
        " %"
        "</div>"
        "</div>";

    // =================================================
    // BLUETOOTH
    // =================================================

    html +=
        "<div class='card'>"
        "<h2>Bluetooth</h2>";

    if (
        bleConnected
    ) {

        html +=
            "<p class='ok'>CONNECTED</p>";

        html +=
            "<p class='device'>";

        html +=
            connectedName;

        html +=
            "</p>";

        html +=
            "<p class='device'>";

        html +=
            connectedAddress;

        html +=
            "</p>";

        html +=
            "<p class='ok'>"
            "Receiving on 6E400002"
            "</p>";

        html +=
            "<p class='small'>"
            "BLE INDICATION"
            "</p>";

    }
    else {

        html +=
            "<p class='bad'>DISCONNECTED</p>";
    }

    html +=
        "</div>";

    // =================================================
    // SENSOR STATUS
    // =================================================

    html +=
        "<div class='card'>"
        "<h2>Sensor</h2>";

    bool recentData =
        dataReceived &&
        (
            millis() -
            lastDataTime <
            DATA_TIMEOUT
        );

    if (
        recentData
    ) {

        html +=
            "<p class='ok'>"
            "DATA RECEIVING"
            "</p>";

    }
    else {

        html +=
            "<p class='bad'>"
            "NO RECENT DATA"
            "</p>";
    }

    html +=
        "</div>";

    // =================================================
    // LAST DATA
    // =================================================

    html +=
        "<div class='card'>"
        "<h2>Last Data</h2>";

    if (
        dataReceived
    ) {

        html +=
            "<p>"
            "Temperature: ";

        html +=
            String(
                temperature,
                1
            );

        html +=
            " &deg;C"
            "</p>";

    }
    else {

        html +=
            "<p class='bad'>"
            "No sensor data yet"
            "</p>";
    }

    html +=
        "</div>";

    // =================================================
    // END
    // =================================================

    html +=
        "</div>"
        "</body>"
        "</html>";

    server.send(
        200,
        "text/html",
        html
    );
}

// =====================================================
// API
// =====================================================

void handleAPI()
{

    String json = "{";

    json +=
        "\"temperature\":";

    json +=
        String(
            temperature,
            1
        );

    json +=
        ",\"humidity\":";

    json +=
        String(
            humidity,
            1
        );

    json +=
        ",\"dataReceived\":";

    json +=
        dataReceived
        ? "true"
        : "false";

    json +=
        ",\"ble\":";

    json +=
        bleConnected
        ? "true"
        : "false";

    json +=
        ",\"indication\":";

    json +=
        indicationEnabled
        ? "true"
        : "false";

    json +=
        ",\"receiveCharacteristic\":\"6E400002\"";

    json +=
        ",\"name\":\"";

    json +=
        connectedName;

    json +=
        "\",\"address\":\"";

    json +=
        connectedAddress;

    json +=
        "\",\"ip\":\"";

    json +=
        WiFi.localIP().toString();

    json +=
        "\"}";

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

    String text;

    text +=
        "ESP32 + MICRO:BIT NUS RECEIVER\n";

    text +=
        "================================\n";

    text +=
        "SERVER IP: ";

    text +=
        WiFi.localIP().toString();

    text +=
        "\n\n";

    text +=
        "WiFi: ";

    text +=
        WiFi.status() == WL_CONNECTED
        ? "CONNECTED\n"
        : "DISCONNECTED\n";

    text +=
        "\n";

    text +=
        "BLE: ";

    text +=
        bleConnected
        ? "CONNECTED\n"
        : "DISCONNECTED\n";

    text +=
        "Name: ";

    text +=
        connectedName;

    text +=
        "\n";

    text +=
        "Address: ";

    text +=
        connectedAddress;

    text +=
        "\n\n";

    text +=
        "Receive characteristic: ";

    text +=
        "6E400002\n";

    text +=
        "Packet type: INDICATION\n";

    text +=
        "Indication enabled: ";

    text +=
        indicationEnabled
        ? "YES\n"
        : "NO\n";

    text +=
        "\n";

    text +=
        "Temperature: ";

    text +=
        String(
            temperature,
            1
        );

    text +=
        " C\n";

    text +=
        "Humidity: ";

    text +=
        String(
            humidity,
            1
        );

    text +=
        " %\n";

    text +=
        "Data received: ";

    text +=
        dataReceived
        ? "YES\n"
        : "NO\n";

    server.send(
        200,
        "text/plain",
        text
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
        "################################################"
    );

    Serial.println(
        "ESP32 MICRO:BIT TEMPERATURE RECEIVER"
    );

    Serial.println(
        "NORDIC UART SERVICE"
    );

    Serial.println(
        "################################################"
    );

    // =================================================
    // WIFI
    // =================================================

    WiFi.mode(
        WIFI_STA
    );

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    Serial.print(
        "WiFi connecting"
    );

    unsigned long wifiStart =
        millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - wifiStart < 20000
    ) {

        delay(500);

        Serial.print(
            "."
        );
    }

    Serial.println();

    if (
        WiFi.status() == WL_CONNECTED
    ) {

        Serial.println(
            "WiFi: CONNECTED"
        );

        Serial.println();
        Serial.println(
            "########################################"
        );

        Serial.print(
            "SERVER: http://"
        );

        Serial.println(
            WiFi.localIP()
        );

        Serial.println(
            "########################################"
        );

    }
    else {

        Serial.println(
            "WiFi: CONNECTION FAILED"
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
        "/api",
        handleAPI
    );

    server.on(
        "/status",
        handleStatus
    );

    server.begin();

    Serial.println(
        "Web server: STARTED"
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
    // INITIAL CONNECTION
    // =================================================

    scanAndConnect();

    Serial.println();
    Serial.println(
        "################################################"
    );

    Serial.println(
        "SYSTEM READY"
    );

    Serial.print(
        "SERVER: http://"
    );

    Serial.println(
        WiFi.localIP()
    );

    Serial.println(
        "################################################"
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
    // CHECK BLE
    // =================================================

    if (
        bleConnected &&
        client != nullptr
    ) {

        if (
            !client->isConnected()
        ) {

            Serial.println();
            Serial.println(
                "BLE: !!! CONNECTION LOST !!!"
            );

            bleConnected =
                false;

            indicationEnabled =
                false;

            receiveCharacteristic =
                nullptr;

            rxBuffer =
                "";

            lastReconnect =
                millis();
        }
    }

    // =================================================
    // AUTOMATIC RECONNECT
    // =================================================

    if (
        !bleConnected
    ) {

        if (
            millis() -
            lastReconnect >=
            RECONNECT_INTERVAL
        ) {

            lastReconnect =
                millis();

            Serial.println();
            Serial.println(
                "BLE: Automatic reconnect..."
            );

            scanAndConnect();

            if (
                bleConnected
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
    // SENSOR DATA TIMEOUT
    // =================================================

    static bool timeoutMessage =
        false;

    if (
        dataReceived &&
        millis() -
        lastDataTime >
        DATA_TIMEOUT
    ) {

        if (
            !timeoutMessage
        ) {

            Serial.println();
            Serial.println(
                "WARNING: No sensor data received for 10 seconds"
            );

            timeoutMessage =
                true;
        }

    }
    else {

        timeoutMessage =
            false;
    }

    delay(5);
}