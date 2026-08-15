bluetooth.onBluetoothConnected(function () {
    connected = true
    basic.showIcon(IconNames.SmallHeart)
})
bluetooth.onBluetoothDisconnected(function () {
    connected = false
    basic.showIcon(IconNames.No)
})
let temperature = 0
let connected = false
bluetooth.startUartService()
basic.showIcon(IconNames.Yes)
basic.pause(1000)
basic.forever(function () {
    if (connected) {
        // ==========================================
        // MICRO:BIT INTERNAL TEMPERATURE SENSOR
        // ==========================================
        temperature = input.temperature()
        // ==========================================
        // SEND TO ESP32
        // ==========================================
        bluetooth.uartWriteLine("TEMP:" + temperature)
        // Display temperature
        basic.showNumber(temperature)
        // Send every 2 seconds
        basic.pause(2000)
    } else {
        basic.pause(500)
    }
})
