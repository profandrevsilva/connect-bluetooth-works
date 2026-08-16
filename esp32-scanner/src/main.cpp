#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

BLEScan* scan;

void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 BLE DIAGNOSTIC SCANNER");
    Serial.println("================================");

    BLEDevice::init("ESP32");

    scan = BLEDevice::getScan();

    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);

    Serial.println();
    Serial.println("Scanning for 15 seconds...");
    Serial.println();

    BLEScanResults results =
        scan->start(15, false);

    int count =
        results.getCount();

    Serial.println();
    Serial.println("================================");
    Serial.print("Devices found: ");
    Serial.println(count);
    Serial.println("================================");

    for (int i = 0; i < count; i++) {

        BLEAdvertisedDevice device =
            results.getDevice(i);

        Serial.println();
        Serial.println("--------------------------------");
        Serial.print("DEVICE #");
        Serial.println(i + 1);
        Serial.println("--------------------------------");

        // =================================================
        // BLE ADDRESS
        // =================================================

        Serial.print("Address: ");

        Serial.println(
            device.getAddress()
                .toString()
                .c_str()
        );

        // =================================================
        // RSSI
        // =================================================

        Serial.print("RSSI: ");

        Serial.print(
            device.getRSSI()
        );

        Serial.println(" dBm");

        // =================================================
        // NAME
        // =================================================

        if (device.haveName()) {

            Serial.print("Name: ");

            Serial.println(
                device.getName()
                    .c_str()
            );

        } else {

            Serial.println(
                "Name: <none>"
            );
        }

        // =================================================
        // SERVICE UUID
        // =================================================

        if (device.haveServiceUUID()) {

            Serial.print(
                "Service UUID: "
            );

            Serial.println(
                device.getServiceUUID()
                    .toString()
                    .c_str()
            );

        } else {

            Serial.println(
                "Service UUID: <none>"
            );
        }

        // =================================================
        // MANUFACTURER DATA
        // =================================================

        if (device.haveManufacturerData()) {

            String manufacturer =
                device.getManufacturerData()
                    .c_str();

            Serial.print(
                "Manufacturer data length: "
            );

            Serial.println(
                manufacturer.length()
            );

            Serial.print(
                "Manufacturer data HEX: "
            );

            for (
                unsigned int j = 0;
                j < manufacturer.length();
                j++
            ) {

                uint8_t value =
                    (uint8_t)manufacturer[j];

                if (value < 16) {
                    Serial.print("0");
                }

                Serial.print(
                    value,
                    HEX
                );

                Serial.print(" ");
            }

            Serial.println();

        } else {

            Serial.println(
                "Manufacturer data: <none>"
            );
        }

        // =================================================
        // COMPLETE ADVERTISEMENT
        // =================================================

        Serial.println();
        Serial.println(
            "Advertisement:"
        );

        Serial.println(
            device.toString().c_str()
        );
    }

    scan->clearResults();

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "SCAN COMPLETE"
    );

    Serial.println(
        "================================"
    );
}

void loop() {
}