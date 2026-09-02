#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// ===============================
// LCD
// ===============================

#define SDA_PIN 21
#define SCL_PIN 22

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===============================
// MICRO:BIT
// ===============================

#define MICROBIT_ADDRESS "fb:fe:84:0d:e5:45"

// ===============================
// UART BLE
// ===============================

#define UART_SERVICE_UUID \
"6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#define UART_TX_UUID \
"6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ===============================

BLEClient *client = nullptr;

BLERemoteCharacteristic *txCharacteristic = nullptr;

bool connected = false;

String buffer = "";

// ===============================
// RECEBER TEMPERATURA
// ===============================

void notifyCallback(
    BLERemoteCharacteristic *characteristic,
    uint8_t *data,
    size_t length,
    bool isNotify)
{
    for (size_t i = 0; i < length; i++)
    {
        char c = (char)data[i];

        if (c == '\n')
        {
            Serial.print("Recebido: ");
            Serial.println(buffer);

            if (buffer.startsWith("TEMP:"))
            {
                String valor = buffer.substring(5);

                int temperatura = valor.toInt();

                Serial.print("Temperatura = ");
                Serial.print(temperatura);
                Serial.println(" C");

                lcd.clear();

                lcd.setCursor(0, 0);
                lcd.print("Temperatura:");

                lcd.setCursor(0, 1);
                lcd.print(temperatura);

                lcd.write(223);

                lcd.print("C");
            }

            buffer = "";
        }
        else
        {
            buffer += c;
        }
    }
}

// ===============================
// CONECTAR
// ===============================

bool connectMicrobit()
{
    Serial.println();
    Serial.println("Conectando ao micro:bit...");

    BLEAddress address(MICROBIT_ADDRESS);

    client = BLEDevice::createClient();

    if (!client->connect(address))
    {
        Serial.println("ERRO: nao conectou!");

        return false;
    }

    Serial.println("================================");
    Serial.println("MICRO:BIT CONECTADO!");
    Serial.println("================================");

    // ============================
    // LISTAR SERVICOS
    // ============================

    Serial.println("Procurando servico UART...");

    BLERemoteService *service =
        client->getService(
            BLEUUID(UART_SERVICE_UUID)
        );

    if (service == nullptr)
    {
        Serial.println();
        Serial.println("UART SERVICE NAO ENCONTRADO!");

        Serial.println();
        Serial.println("Servicos encontrados:");

        auto *services = client->getServices();

        for (auto const &item : *services)
        {
            Serial.print("UUID: ");
            Serial.println(item.first.c_str());
        }

        client->disconnect();

        return false;
    }

    Serial.println("UART SERVICE ENCONTRADO!");

    // ============================
    // TX
    // ============================

    txCharacteristic =
        service->getCharacteristic(
            BLEUUID(UART_TX_UUID)
        );

    if (txCharacteristic == nullptr)
    {
        Serial.println("TX NAO ENCONTRADO!");

        client->disconnect();

        return false;
    }

    Serial.println("TX ENCONTRADO!");

    // ============================
    // NOTIFICACOES
    // ============================

    if (txCharacteristic->canNotify())
    {
        txCharacteristic->registerForNotify(
            notifyCallback
        );

        Serial.println(
            "NOTIFICACOES ATIVADAS!"
        );
    }
    else
    {
        Serial.println(
            "TX NAO SUPORTA NOTIFICACOES!"
        );

        client->disconnect();

        return false;
    }

    connected = true;

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("micro:bit");

    lcd.setCursor(0, 1);
    lcd.print("Conectado!");

    return true;
}

// ===============================
// SETUP
// ===============================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32 + micro:bit");
    Serial.println("Temperatura BLE");
    Serial.println("==============================");

    // ============================
    // I2C
    // ============================

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );

    // ============================
    // LCD
    // ============================

    lcd.init();

    lcd.backlight();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("ESP32");

    lcd.setCursor(0, 1);
    lcd.print("Iniciando...");

    delay(2000);

    // ============================
    // BLE
    // ============================

    Serial.println("Inicializando BLE...");

    BLEDevice::init("ESP32");

    delay(1000);

    // ============================
    // CONECTAR
    // ============================

    connectMicrobit();
}

// ===============================
// LOOP
// ===============================

void loop()
{
    if (!connected)
    {
        delay(3000);

        connectMicrobit();
    }

    delay(100);
}