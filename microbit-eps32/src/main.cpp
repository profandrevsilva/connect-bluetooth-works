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

const char* WIFI_SSID = "andre";
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
// MICRO:BIT -> ESP32
//
// NUS TX characteristic from micro:bit perspective
// UUID:
// 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
//
// ESP32 receives notifications here.
// =====================================================

static BLEUUID NUS_RX(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
);

// =====================================================
// ESP32 -> MICRO:BIT
//
// NUS RX characteristic from micro:bit perspective
// =====================================================

static BLEUUID NUS_TX(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
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

BLERemoteCharacteristic* rxCharacteristic = nullptr;
BLERemoteCharacteristic* txCharacteristic = nullptr;

bool bleConnected = false;

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

void notificationCallback(
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

        rxCharacteristic = nullptr;
        txCharacteristic = nullptr;

        lastReconnect = millis();
    }
};

// =====================================================
// BLE NOTIFICATION CALLBACK
// =====================================================
//
// This is the most important diagnostic function.
//
// Expected:
//
// TEMP:24
//
// or:
//
// TEMP:24\n
//
// =====================================================

void notificationCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify
) {

    Serial.println();
    Serial.println(
        "********************************"
    );

    Serial.println(
        "BLE NOTIFICATION RECEIVED!"
    );

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
        else {

            if (c == '\n') {
                Serial.print("\\n");
            }
            else if (c == '\r') {
                Serial.print("\\r");
            }
            else {
                Serial.print(".");
            }
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

            Serial.print("0");
        }

        Serial.print(
            data[i],
            HEX
        );

        Serial.print(" ");
    }

    Serial.println();

    // =================================================
    // NOTIFICATION STATUS
    // =================================================

    Serial.print(
        "isNotify: "
    );

    Serial.println(
        isNotify
        ? "YES"
        : "NO"
    );

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

    Serial.print(
        "RX BUFFER: "
    );

    Serial.println(
        rxBuffer
    );

    // =================================================
    // PROCESS COMPLETE LINES
    // =================================================

    while (true) {

        int newline =
            rxBuffer.indexOf('\n');

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

        line.trim();

        if (
            line.length() == 0
        ) {

            continue;
        }

        Serial.println();
        Serial.println(
            "================================"
        );

        Serial.print(
            "MICRO:BIT MESSAGE: "
        );

        Serial.println(
            line
        );

        Serial.println(
            "================================"
        );

        // =================================================
        // TEMPERATURE
        // =================================================

        int tempPos =
            line.indexOf("TEMP:");

        if (
            tempPos >= 0
        ) {

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
        }

        // =================================================
        // HUMIDITY
        // =================================================

        int humPos =
            line.indexOf("HUM:");

        if (
            humPos >= 0
        ) {

            int start =
                humPos + 4;

            int end =
                line.length();

            int semicolon =
                line.indexOf(
                    ';',
                    start
                );

            if (
                semicolon >= 0
            ) {

                end =
                    semicolon;
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
        }

        // =================================================
        // DATA RECEIVED
        // =================================================

        if (
            line.indexOf("TEMP:") >= 0 ||
            line.indexOf("HUM:") >= 0
        ) {

            dataReceived =
                true;

            lastDataTime =
                millis();

            Serial.println();
            Serial.println(
                ">>> SENSOR DATA RECEIVED <<<"
            );

            Serial.print(
                "Temperature: "
            );

            Serial.print(
                temperature,
                1
            );

            Serial.println(
                " C"
            );

            Serial.print(
                "Humidity: "
            );

            Serial.print(
                humidity,
                1
            );

            Serial.println(
                " %"
            );

            Serial.println(
                "================================"
            );
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

        rxBuffer = "";
    }

    Serial.println(
        "********************************"
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
        "================================"
    );

    Serial.println(
        "BLE: CONNECTING TO MICRO:BIT"
    );

    Serial.println(
        "================================"
    );

    // =================================================
    // DEVICE INFORMATION
    // =================================================

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

    delay(300);

    // =================================================
    // NUS SERVICE
    // =================================================

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

    // =================================================
    // RX CHARACTERISTIC
    // =================================================

    Serial.println(
        "BLE: Searching RX characteristic..."
    );

    rxCharacteristic =
        service->getCharacteristic(
            NUS_RX
        );
    
    Serial.println();
    Serial.println("========== RX CAPABILITIES ==========");

    Serial.print("Can Read: ");
    Serial.println(
    rxCharacteristic->canRead()
    ? "YES"
    : "NO"
    );

    Serial.print("Can Write: ");
    Serial.println(
    rxCharacteristic->canWrite()
    ? "YES"
    : "NO"
    );

    Serial.print("Can Notify: ");
    Serial.println(
    rxCharacteristic->canNotify()
    ? "YES"
    : "NO"
    );

    Serial.print("Can Indicate: ");
    Serial.println(
    rxCharacteristic->canIndicate()
    ? "YES"
    : "NO"
    );

    Serial.println(
    "====================================="
    );

    if (
        rxCharacteristic == nullptr
    ) {

        Serial.println(
            "BLE ERROR: RX CHARACTERISTIC NOT FOUND"
        );

        cleanupConnection();

        return false;
    }

    Serial.println(
        "BLE: *** RX CHARACTERISTIC FOUND ***"
    );

    Serial.print(
        "BLE RX UUID: "
    );

    Serial.println(
        rxCharacteristic->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // CHECK PROPERTIES
    // =================================================

    Serial.println();
    Serial.println(
        "BLE RX CHARACTERISTIC PROPERTIES:"
    );

    if (
        rxCharacteristic->canNotify()
    ) {

        Serial.println(
            "  NOTIFY: YES"
        );

    }
    else {

        Serial.println(
            "  NOTIFY: NO"
        );
    }

    if (
        rxCharacteristic->canIndicate()
    ) {

        Serial.println(
            "  INDICATE: YES"
        );

    }
    else {

        Serial.println(
            "  INDICATE: NO"
        );
    }

    if (
        rxCharacteristic->canRead()
    ) {

        Serial.println(
            "  READ: YES"
        );

    }
    else {

        Serial.println(
            "  READ: NO"
        );
    }

    if (
        rxCharacteristic->canWrite()
    ) {

        Serial.println(
            "  WRITE: YES"
        );

    }
    else {

        Serial.println(
            "  WRITE: NO"
        );
    }

    // =================================================
    // REGISTER NOTIFICATION
    // =================================================

    Serial.println();
    Serial.println(
        "BLE: Registering notification callback..."
    );

    if (
        rxCharacteristic->canNotify()
    ) {

        rxCharacteristic->registerForNotify(
            notificationCallback
        );

        Serial.println(
            "BLE: registerForNotify() called"
        );

    }
    else {

        Serial.println(
            "BLE ERROR: RX does NOT support NOTIFY"
        );
    }

    delay(1000);

    // =================================================
    // TX CHARACTERISTIC
    // =================================================

    Serial.println();
    Serial.println(
        "BLE: Searching TX characteristic..."
    );

    txCharacteristic =
        service->getCharacteristic(
            NUS_TX
        );

    if (
        txCharacteristic != nullptr
    ) {

        Serial.println(
            "BLE: *** TX CHARACTERISTIC FOUND ***"
        );

        Serial.print(
            "BLE TX UUID: "
        );

        Serial.println(
            txCharacteristic->getUUID()
                .toString()
                .c_str()
        );

    }
    else {

        Serial.println(
            "BLE: TX characteristic not found"
        );

        Serial.println(
            "BLE: This is OK for reception."
        );
    }

    // =================================================
    // SAVE DEVICE INFORMATION
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

    rxCharacteristic =
        nullptr;

    txCharacteristic =
        nullptr;

    bleConnected =
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
        "================================"
    );

    Serial.println(
        "BLE: SCANNING FOR MICRO:BIT"
    );

    Serial.println(
        "================================"
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

        // =================================================
        // CHECK MICRO:BIT
        // =================================================

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
        "<meta http-equiv='refresh' content='3'>"
        "<title>ESP32 Micro:bit Monitor</title>"

        "<style>"

        "body{"
        "font-family:Arial;"
        "background:#111827;"
        "color:white;"
        "text-align:center;"
        "padding:30px;"
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

        "</style>"
        "</head>"

        "<body>"

        "<div class='container'>"

        "<h1>ESP32 + micro:bit</h1>";

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
    // BLE STATUS
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

    }
    else {

        html +=
            "<p class='bad'>DISCONNECTED</p>";
    }

    html +=
        "</div>";

    // =================================================
    // DATA STATUS
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
            "<p class='ok'>DATA RECEIVING</p>";

    }
    else {

        html +=
            "<p class='bad'>NO RECENT DATA</p>";
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
        ",\"name\":\"";

    json +=
        connectedName;

    json +=
        "\",\"address\":\"";

    json +=
        connectedAddress;

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
        "WiFi: ";

    text +=
        WiFi.status() == WL_CONNECTED
        ? "CONNECTED\n"
        : "DISCONNECTED\n";

    text +=
        "IP: ";

    text +=
        WiFi.localIP().toString();

    text +=
        "\n\n";

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
        "========================================"
    );

    Serial.println(
        "ESP32 MICRO:BIT TEMPERATURE RECEIVER"
    );

    Serial.println(
        "NORDIC UART SERVICE"
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

        Serial.print(".");
    }

    Serial.println();

    if (
        WiFi.status() == WL_CONNECTED
    ) {

        Serial.println(
            "WiFi: CONNECTED"
        );

        Serial.print(
            "WiFi IP: "
        );

        Serial.println(
            WiFi.localIP()
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
    // CHECK BLE CONNECTION
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

            rxCharacteristic =
                nullptr;

            txCharacteristic =
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