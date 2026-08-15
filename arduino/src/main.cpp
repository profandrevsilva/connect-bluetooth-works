#include <Arduino.h>
#include <NimBLEDevice.h>

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

void notifyCallback(
    NimBLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify
) {
    Serial.print("RECEIVED: ");

    for (size_t i = 0; i < length; i++) {
        Serial.print((char)data[i]);
    }

    Serial.println();
}

void setup() {

    Serial.begin(115200);
    delay(1000);

    NimBLEDevice::init("ESP32");

    Serial.println("Scanning...");

    NimBLEScan* scan = NimBLEDevice::getScan();

    scan->setActiveScan(true);

    NimBLEScanResults results = scan->start(5);

    Serial.print("Devices found: ");
    Serial.println(results.getCount());

    for (int i = 0; i < results.getCount(); i++) {

        NimBLEAdvertisedDevice device = results.getDevice(i);

        Serial.println();
        Serial.print("DEVICE ");
        Serial.println(i);

        Serial.print("Address: ");
        Serial.println(device.getAddress().toString().c_str());

        Serial.println("Connecting...");

        NimBLEClient* client = NimBLEDevice::createClient();

        if (!client->connect(&device)) {
            Serial.println("Connection failed");
            NimBLEDevice::deleteClient(client);
            continue;
        }

        Serial.println("CONNECTED");

        NimBLERemoteService* service =
            client->getService(NUS_SERVICE_UUID);

        if (service != nullptr) {

            Serial.println();
            Serial.println("************************");
            Serial.println("NORDIC UART FOUND!");
            Serial.println("THIS IS THE MICRO:BIT");
            Serial.println("************************");

            NimBLERemoteCharacteristic* rx =
                service->getCharacteristic(NUS_RX_UUID);

            if (rx != nullptr) {

                Serial.println("RX characteristic found");

                if (rx->canNotify()) {

                    if (rx->subscribe(true, notifyCallback)) {
                        Serial.println("Notifications enabled");
                    } else {
                        Serial.println("Notification failed");
                    }

                } else {
                    Serial.println("RX cannot notify");
                }

                Serial.println();
                Serial.println("MICRO:BIT CONNECTED!");
                return;
            }
        }

        Serial.println("NUS not found");

        client->disconnect();
        NimBLEDevice::deleteClient(client);
    }

    Serial.println();
    Serial.println("MICRO:BIT NOT FOUND");
}

void loop() {
    delay(1000);
}