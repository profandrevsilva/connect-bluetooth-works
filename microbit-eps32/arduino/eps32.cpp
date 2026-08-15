#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLERemoteDescriptor.h>

// =====================================================
// MICRO:BIT
// =====================================================

const char* MICROBIT_ADDRESS =
    "fb:fe:84:0d:e5:45";

// =====================================================
// NORDIC UART SERVICE
// =====================================================

static BLEUUID NUS_SERVICE(
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
);

// =====================================================
// NUS CHARACTERISTICS
// =====================================================

// 6E400002
//
// Normally:
// ESP32 -> micro:bit
//
// In YOUR current GATT database this characteristic
// reports:
//     INDICATE = YES
//     CCCD     = FOUND
//
// We are going to TEST whether it can actually
// deliver data from the micro:bit.

static BLEUUID NUS_002(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
);

// 6E400003
//
// Normally:
// micro:bit -> ESP32
//
// In YOUR current GATT database:
//
//     WRITE = YES
//     NOTIFY = NO
//     CCCD  = NOT FOUND
//
// We will only inspect it.

static BLEUUID NUS_003(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
);

// =====================================================
// CCCD
// =====================================================

static BLEUUID CCCD_UUID(
    "00002902-0000-1000-8000-00805F9B34FB"
);

// =====================================================
// BLE
// =====================================================

BLEClient* client = nullptr;

BLERemoteService* nusService = nullptr;

BLERemoteCharacteristic* characteristic002 = nullptr;
BLERemoteCharacteristic* characteristic003 = nullptr;

BLERemoteDescriptor* cccd002 = nullptr;

bool bleConnected = false;

bool indicationEnabled = false;

unsigned long lastStatus = 0;

// =====================================================
// CALLBACK
// =====================================================
//
// This callback is deliberately attached to 6E400002.
//
// We want to know if the micro:bit sends anything there.
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
    // CHARACTERISTIC UUID
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
    // TYPE
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

        uint8_t b =
            data[i];

        if (
            b >= 32 &&
            b <= 126
        ) {

            Serial.print(
                (char)b
            );

        }
        else if (
            b == '\n'
        ) {

            Serial.print(
                "\\n"
            );

        }
        else if (
            b == '\r'
        ) {

            Serial.print(
                "\\r"
            );

        }
        else if (
            b == '\t'
        ) {

            Serial.print(
                "\\t"
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
            data[i] < 0x10
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
    // TRY TO INTERPRET AS TEXT
    // =================================================

    String message = "";

    for (
        size_t i = 0;
        i < length;
        i++
    ) {

        message +=
            (char)data[i];
    }

    message.trim();

    Serial.print(
        "MESSAGE: "
    );

    Serial.println(
        message
    );

    // =================================================
    // SEARCH FOR SENSOR DATA
    // =================================================

    if (
        message.indexOf("TEMP:") >= 0
    ) {

        Serial.println(
            ">>> TEMP FIELD DETECTED <<<"
        );
    }

    if (
        message.indexOf("HUM:") >= 0
    ) {

        Serial.println(
            ">>> HUM FIELD DETECTED <<<"
        );
    }

    Serial.println(
        "################################################"
    );
}

// =====================================================
// CLIENT CALLBACKS
// =====================================================

class MyClientCallbacks
    : public BLEClientCallbacks {

    void onConnect(
        BLEClient* pClient
    ) override {

        Serial.println();
        Serial.println(
            "BLE: Client connected"
        );

        bleConnected =
            true;
    }

    void onDisconnect(
        BLEClient* pClient
    ) override {

        Serial.println();
        Serial.println(
            "BLE: MICRO:BIT DISCONNECTED"
        );

        bleConnected =
            false;

        indicationEnabled =
            false;
    }
};

// =====================================================
// PRINT PROPERTIES
// =====================================================

void printProperties(
    BLERemoteCharacteristic* c
) {

    if (
        c == nullptr
    ) {

        Serial.println(
            "Characteristic = NULL"
        );

        return;
    }

    Serial.print(
        "Can Read: "
    );

    Serial.println(
        c->canRead()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Write: "
    );

    Serial.println(
        c->canWrite()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Write No Response: "
    );

    Serial.println(
        c->canWriteNoResponse()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Notify: "
    );

    Serial.println(
        c->canNotify()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Indicate: "
    );

    Serial.println(
        c->canIndicate()
        ? "YES"
        : "NO"
    );

    Serial.print(
        "Can Broadcast: "
    );

    Serial.println(
        c->canBroadcast()
        ? "YES"
        : "NO"
    );
}

// =====================================================
// FIND CCCD
// =====================================================

BLERemoteDescriptor* findCCCD(
    BLERemoteCharacteristic* characteristic
) {

    if (
        characteristic == nullptr
    ) {

        return nullptr;
    }

    Serial.println();
    Serial.println(
        "Searching for CCCD 0x2902..."
    );

    BLERemoteDescriptor* descriptor =
        characteristic->getDescriptor(
            CCCD_UUID
        );

    if (
        descriptor != nullptr
    ) {

        Serial.println(
            ">>> CCCD FOUND <<<"
        );

        Serial.print(
            "CCCD UUID: "
        );

        Serial.println(
            descriptor->getUUID()
                .toString()
                .c_str()
        );

        Serial.print(
            "CCCD Handle: "
        );

        Serial.println(
            descriptor->getHandle()
        );

    }
    else {

        Serial.println(
            ">>> CCCD NOT FOUND <<<"
        );
    }

    return descriptor;
}

// =====================================================
// ENABLE INDICATION
// =====================================================
//
// CCCD values:
//
// 0x0000 = disabled
// 0x0001 = notifications
// 0x0002 = indications
//
// For 6E400002 we want:
//
// 02 00
//
// =====================================================

bool enableIndicationManually(
    BLERemoteDescriptor* descriptor
) {

    if (
        descriptor == nullptr
    ) {

        Serial.println(
            "ERROR: CCCD is NULL"
        );

        return false;
    }

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "ENABLING INDICATIONS MANUALLY"
    );

    Serial.println(
        "========================================"
    );

    uint8_t indicationEnable[2] =
    {
        0x02,
        0x00
    };

    Serial.println(
        "Writing CCCD: 02 00"
    );

    try {

        descriptor->writeValue(
            indicationEnable,
            2,
            true
        );

    }
    catch (...) {

        Serial.println(
            "ERROR: Exception while writing CCCD"
        );

        return false;
    }

    Serial.println(
        "CCCD write completed."
    );

    indicationEnabled =
        true;

    return true;
}

// =====================================================
// CONNECT TO MICRO:BIT
// =====================================================

bool connectToMicrobit(
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
        "CONNECTING TO MICRO:BIT"
    );

    Serial.println(
        "========================================"
    );

    Serial.print(
        "Name: "
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
        "Address: "
    );

    Serial.println(
        device->getAddress()
            .toString()
            .c_str()
    );

    // =================================================
    // CREATE CLIENT
    // =================================================

    client =
        BLEDevice::createClient();

    if (
        client == nullptr
    ) {

        Serial.println(
            "ERROR: createClient() failed"
        );

        return false;
    }

    client->setClientCallbacks(
        new MyClientCallbacks()
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
            "ERROR: Connection failed"
        );

        delete client;

        client =
            nullptr;

        return false;
    }

    Serial.println(
        "BLE: Physical connection OK"
    );

    delay(500);

    // =================================================
    // NUS
    // =================================================

    Serial.println();
    Serial.println(
        "BLE: Searching NUS..."
    );

    nusService =
        client->getService(
            NUS_SERVICE
        );

    if (
        nusService == nullptr
    ) {

        Serial.println(
            "ERROR: NUS SERVICE NOT FOUND"
        );

        client->disconnect();

        return false;
    }

    Serial.println(
        "BLE: *** NUS SERVICE FOUND ***"
    );

    Serial.print(
        "NUS UUID: "
    );

    Serial.println(
        nusService->getUUID()
            .toString()
            .c_str()
    );

    // =================================================
    // 6E400002
    // =================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "TEST TARGET: 6E400002"
    );

    Serial.println(
        "========================================"
    );

    characteristic002 =
        nusService->getCharacteristic(
            NUS_002
        );

    if (
        characteristic002 == nullptr
    ) {

        Serial.println(
            "ERROR: 6E400002 NOT FOUND"
        );

        return false;
    }

    Serial.println(
        "6E400002 FOUND"
    );

    Serial.print(
        "Handle: "
    );

    Serial.println(
        characteristic002->getHandle()
    );

    Serial.println();
    Serial.println(
        "PROPERTIES:"
    );

    printProperties(
        characteristic002
    );

    // =================================================
    // CCCD
    // =================================================

    cccd002 =
        findCCCD(
            characteristic002
        );

    // =================================================
    // REGISTER CALLBACK
    // =================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "REGISTERING CALLBACK ON 6E400002"
    );

    Serial.println(
        "========================================"
    );

    if (
        characteristic002->canIndicate()
    ) {

        Serial.println(
            "6E400002 supports INDICATE."
        );

        Serial.println(
            "Calling registerForNotify()..."
        );

        characteristic002->registerForNotify(
            indicationCallback
        );

        Serial.println(
            "Callback registered."
        );

    }
    else {

        Serial.println(
            "WARNING:"
        );

        Serial.println(
            "6E400002 does NOT report INDICATE."
        );
    }

    // =================================================
    // MANUAL CCCD
    // =================================================

    if (
        cccd002 != nullptr
    ) {

        enableIndicationManually(
            cccd002
        );

    }
    else {

        Serial.println();
        Serial.println(
            "Cannot enable indications:"
        );

        Serial.println(
            "CCCD was not found."
        );
    }

    // =================================================
    // 6E400003
    // =================================================

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "REFERENCE: 6E400003"
    );

    Serial.println(
        "========================================"
    );

    characteristic003 =
        nusService->getCharacteristic(
            NUS_003
        );

    if (
        characteristic003 == nullptr
    ) {

        Serial.println(
            "6E400003 NOT FOUND"
        );

    }
    else {

        Serial.println(
            "6E400003 FOUND"
        );

        Serial.print(
            "Handle: "
        );

        Serial.println(
            characteristic003->getHandle()
        );

        Serial.println();

        printProperties(
            characteristic003
        );

        Serial.println();

        Serial.println(
            "CCCD test on 6E400003:"
        );

        BLERemoteDescriptor*
            cccd003 =
                findCCCD(
                    characteristic003
                );

        if (
            cccd003 == nullptr
        ) {

            Serial.println(
                "As expected from the previous test:"
            );

            Serial.println(
                "6E400003 has NO CCCD."
            );
        }
    }

    // =================================================
    // FINAL STATUS
    // =================================================

    bleConnected =
        true;

    Serial.println();
    Serial.println(
        "################################################"
    );

    Serial.println(
        "TEST IS NOW RUNNING"
    );

    Serial.println(
        "################################################"
    );

    Serial.println();
    Serial.println(
        "The ESP32 is now listening on:"
    );

    Serial.println(
        "6E400002"
    );

    Serial.println(
        "using INDICATIONS."
    );

    Serial.println();
    Serial.println(
        "Send temperature/humidity from the micro:bit."
    );

    Serial.println(
        "Every received packet will be printed."
    );

    Serial.println();
    Serial.println(
        "If NOTHING arrives, that is also useful evidence."
    );

    Serial.println(
        "################################################"
    );

    return true;
}

// =====================================================
// SCAN
// =====================================================

void scanAndConnect()
{

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "SCANNING FOR MICRO:BIT"
    );

    Serial.println(
        "========================================"
    );

    BLEScan* scanner =
        BLEDevice::getScan();

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
        "Devices found: "
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

        String address =
            device.getAddress()
                .toString()
                .c_str();

        Serial.println();
        Serial.println(
            "----------------------------------------"
        );

        Serial.print(
            "Device #"
        );

        Serial.println(
            i + 1
        );

        Serial.print(
            "Address: "
        );

        Serial.println(
            address
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

        Serial.print(
            "Name: "
        );

        if (
            device.haveName()
        ) {

            Serial.println(
                device.getName()
                    .c_str()
            );

        }
        else {

            Serial.println(
                "<none>"
            );
        }

        if (
            address.equalsIgnoreCase(
                MICROBIT_ADDRESS
            )
        ) {

            Serial.println();
            Serial.println(
                ">>> TARGET MICRO:BIT FOUND <<<"
            );

            BLEAdvertisedDevice*
                candidate =
                    new BLEAdvertisedDevice(
                        device
                    );

            scanner->clearResults();

            connectToMicrobit(
                candidate
            );

            delete candidate;

            return;
        }
    }

    scanner->clearResults();

    Serial.println();
    Serial.println(
        "TARGET MICRO:BIT NOT FOUND"
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
    Serial.println();
    Serial.println(
        "================================================"
    );

    Serial.println(
        "ESP32 -> MICRO:BIT NUS INDICATION TEST"
    );

    Serial.println(
        "================================================"
    );

    Serial.println();
    Serial.println(
        "This experiment tests 6E400002."
    );

    Serial.println(
        "It does NOT depend on 6E400003 notifications."
    );

    Serial.println();
    Serial.println(
        "Target:"
    );

    Serial.println(
        MICROBIT_ADDRESS
    );

    Serial.println();
    Serial.println(
        "Initializing BLE..."
    );

    BLEDevice::init(
        "ESP32-INDICATION-TEST"
    );

    Serial.println(
        "BLE initialized."
    );

    scanAndConnect();
}

// =====================================================
// LOOP
// =====================================================

void loop()
{

    // =================================================
    // CONNECTION STATUS
    // =================================================

    if (
        client != nullptr
    ) {

        if (
            !client->isConnected()
        ) {

            if (
                bleConnected
            ) {

                bleConnected =
                    false;

                indicationEnabled =
                    false;

                Serial.println();
                Serial.println(
                    "!!! BLE CONNECTION LOST !!!"
                );
            }

        }
        else {

            bleConnected =
                true;
        }
    }

    // =================================================
    // PERIODIC STATUS
    // =================================================

    if (
        millis() -
        lastStatus >
        10000
    ) {

        lastStatus =
            millis();

        Serial.println();
        Serial.println(
            "----------------------------------------"
        );

        Serial.print(
            "BLE connected: "
        );

        Serial.println(
            bleConnected
            ? "YES"
            : "NO"
        );

        Serial.print(
            "Indication enabled on 6E400002: "
        );

        Serial.println(
            indicationEnabled
            ? "YES"
            : "NO"
        );

        Serial.println(
            "Waiting for micro:bit data..."
        );

        Serial.println(
            "----------------------------------------"
        );
    }

    delay(20);
}