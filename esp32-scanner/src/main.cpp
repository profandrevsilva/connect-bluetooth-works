#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NimBLEDevice.h>

// ========================================
// LCD I2C
// ========================================

#define SDA_PIN 21
#define SCL_PIN 22
#define LCD_ADDRESS 0x27

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// ========================================
// MICRO:BIT UART SERVICE
// ========================================

static NimBLEUUID UART_SERVICE_UUID(
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
);

// MICRO:BIT -> ESP32
// Esta é a característica que devemos receber
static NimBLEUUID UART_TX_UUID(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
);

// ESP32 -> MICRO:BIT
static NimBLEUUID UART_RX_UUID(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
);

// ========================================
// BLE
// ========================================

NimBLEClient* client = nullptr;

NimBLERemoteCharacteristic* txCharacteristic = nullptr;

String receivedData = "";

// ========================================
// DISPLAY TEMPERATURE
// ========================================

void displayTemperature(int temperature)
{
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Temperature:");

    lcd.setCursor(0, 1);

    lcd.print(temperature);

    lcd.print((char)223);

    lcd.print("C");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");
}

// ========================================
// BLE NOTIFICATION CALLBACK
// ========================================

void notifyCallback(
    NimBLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify)
{
    Serial.print("BLE DATA: ");

    for (size_t i = 0; i < length; i++)
    {
        Serial.print((char)data[i]);
    }

    Serial.println();

    // Processar os dados
    for (size_t i = 0; i < length; i++)
    {
        char c = (char)data[i];

        if (c == '\n')
        {
            Serial.print("Received: ");
            Serial.println(receivedData);

            // Verificar TEMP:
            if (receivedData.startsWith("TEMP:"))
            {
                String value = receivedData.substring(5);

                int temperature = value.toInt();

                displayTemperature(temperature);
            }

            receivedData = "";
        }
        else if (c != '\r')
        {
            receivedData += c;
        }
    }
}

// ========================================
// CONNECT TO MICRO:BIT
// ========================================

bool connectToMicrobit()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("Scanning for micro:bit...");
    Serial.println("==============================");

    NimBLEScan* scan = NimBLEDevice::getScan();

    scan->setActiveScan(true);

    scan->setInterval(100);

    scan->setWindow(80);

    // Scan durante 5 segundos
    NimBLEScanResults results = scan->start(5, false);

    NimBLEAdvertisedDevice* foundDevice = nullptr;

    // ========================================
    // SEARCH MICRO:BIT
    // ========================================

    for (int i = 0; i < results.getCount(); i++)
    {
        NimBLEAdvertisedDevice device = results.getDevice(i);

        Serial.print("Device: ");

        Serial.print(
            device.getAddress().toString().c_str()
        );

        if (device.haveName())
        {
            Serial.print("  Name: ");

            Serial.println(
                device.getName().c_str()
            );
        }
        else
        {
            Serial.println();
        }

        // Procurar pelo nome
        if (device.haveName())
        {
            String name = device.getName().c_str();

            if (name.indexOf("micro:bit") >= 0)
            {
                foundDevice =
                    new NimBLEAdvertisedDevice(device);

                break;
            }
        }
    }

    scan->clearResults();

    // ========================================
    // MICRO:BIT NOT FOUND
    // ========================================

    if (foundDevice == nullptr)
    {
        Serial.println();
        Serial.println("micro:bit NOT FOUND!");

        return false;
    }

    // ========================================
    // MICRO:BIT FOUND
    // ========================================

    Serial.println();
    Serial.println("==============================");
    Serial.println("MICRO:BIT FOUND!");
    Serial.println("==============================");

    Serial.print("Address: ");

    Serial.println(
        foundDevice->getAddress().toString().c_str()
    );

    Serial.print("Name: ");

    Serial.println(
        foundDevice->getName().c_str()
    );

    // ========================================
    // CREATE CLIENT
    // ========================================

    Serial.println();
    Serial.println("Creating BLE client...");

    client = NimBLEDevice::createClient();

    // ========================================
    // CONNECT
    // ========================================

    Serial.println("Connecting...");

    if (!client->connect(foundDevice))
    {
        Serial.println("Connection FAILED!");

        delete foundDevice;

        foundDevice = nullptr;

        NimBLEDevice::deleteClient(client);

        client = nullptr;

        return false;
    }

    delete foundDevice;

    foundDevice = nullptr;

    Serial.println();
    Serial.println("==============================");
    Serial.println("MICRO:BIT CONNECTED!");
    Serial.println("==============================");

    // ========================================
    // GET UART SERVICE
    // ========================================

    NimBLERemoteService* uartService =
        client->getService(
            UART_SERVICE_UUID
        );

    if (uartService == nullptr)
    {
        Serial.println(
            "UART SERVICE NOT FOUND!"
        );

        client->disconnect();

        return false;
    }

    Serial.println(
        "UART SERVICE FOUND!"
    );

    // ========================================
    // GET TX CHARACTERISTIC
    // ========================================

    txCharacteristic =
        uartService->getCharacteristic(
            UART_TX_UUID
        );

    if (txCharacteristic == nullptr)
    {
        Serial.println(
            "TX CHARACTERISTIC NOT FOUND!"
        );

        client->disconnect();

        return false;
    }

    Serial.println(
        "TX CHARACTERISTIC FOUND!"
    );

    // ========================================
    // SHOW CHARACTERISTIC PROPERTIES
    // ========================================

    Serial.println();
    Serial.println("TX properties:");

    Serial.print("Read: ");

    Serial.println(
        txCharacteristic->canRead()
            ? "YES"
            : "NO"
    );

    Serial.print("Write: ");

    Serial.println(
        txCharacteristic->canWrite()
            ? "YES"
            : "NO"
    );

    Serial.print("Notify: ");

    Serial.println(
        txCharacteristic->canNotify()
            ? "YES"
            : "NO"
    );

    Serial.print("Indicate: ");

    Serial.println(
        txCharacteristic->canIndicate()
            ? "YES"
            : "NO"
    );

    // ========================================
    // NOTIFY
    // ========================================

    if (txCharacteristic->canNotify())
    {
        Serial.println();
        Serial.println("TX properties:");

        Serial.print("Read: ");
        Serial.println(txCharacteristic->canRead() ? "YES" : "NO");

        Serial.print("Write: ");
        Serial.println(txCharacteristic->canWrite() ? "YES" : "NO");

        Serial.print("Notify: ");
        Serial.println(txCharacteristic->canNotify() ? "YES" : "NO");

        Serial.print("Indicate: ");
        Serial.println(txCharacteristic->canIndicate() ? "YES" : "NO");

        Serial.println();
        Serial.println("Trying subscription...");

        bool subscribed = txCharacteristic->subscribe(
            true,
            notifyCallback
        );

        if (subscribed)
        {
            Serial.println("SUBSCRIPTION SUCCESS!");
            return true;
        }

        Serial.println("Notify subscription failed.");

        Serial.println("Trying indication...");

        subscribed = txCharacteristic->subscribe(
            false,
            notifyCallback
        );

        if (subscribed)
        {
            Serial.println("INDICATION SUCCESS!");
            return true;
        }

        Serial.println("Indication subscription failed.");

        client->disconnect();

        return false;
    }

    // ========================================
    // INDICATE
    // ========================================

    if (txCharacteristic->canIndicate())
    {
        Serial.println();
        Serial.println(
            "Using INDICATE..."
        );

        bool subscribed =
            txCharacteristic->subscribe(
                false,
                notifyCallback
            );

        if (!subscribed)
        {
            Serial.println(
                "INDICATE subscription FAILED!"
            );

            client->disconnect();

            return false;
        }

        Serial.println(
            "INDICATIONS ENABLED!"
        );

        return true;
    }

    // ========================================
    // NO NOTIFY / INDICATE
    // ========================================

    Serial.println();
    Serial.println(
        "ERROR: TX characteristic has"
    );

    Serial.println(
        "no NOTIFY or INDICATE property."
    );

    client->disconnect();

    return false;
}

// ========================================
// SETUP
// ========================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32 TEMPERATURE RECEIVER");
    Serial.println("==============================");

    // ========================================
    // LCD
    // ========================================

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );

    lcd.init();

    lcd.backlight();

    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("ESP32 BLE");

    lcd.setCursor(0, 1);

    lcd.print("Starting...");

    // ========================================
    // BLE
    // ========================================

    Serial.println(
        "Starting BLE..."
    );

    NimBLEDevice::init("ESP32");

    delay(1000);

    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("Waiting for");

    lcd.setCursor(0, 1);

    lcd.print("micro:bit...");
}

// ========================================
// LOOP
// ========================================

void loop()
{
    // ========================================
    // CHECK CONNECTION
    // ========================================

    if (
        client == nullptr ||
        !client->isConnected()
    )
    {
        bool connected =
            connectToMicrobit();

        if (connected)
        {
            Serial.println();
            Serial.println("==============================");
            Serial.println("BLE UART READY!");
            Serial.println("==============================");

            Serial.println(
                "Waiting for temperature..."
            );

            lcd.clear();

            lcd.setCursor(0, 0);

            lcd.print("micro:bit OK");

            lcd.setCursor(0, 1);

            lcd.print("Waiting data...");
        }
        else
        {
            Serial.println();

            Serial.println(
                "Retrying in 3 seconds..."
            );

            delay(3000);
        }

        return;
    }

    delay(100);
}