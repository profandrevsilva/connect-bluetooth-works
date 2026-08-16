bluetooth.onBluetoothConnected(function () {
    basic.showIcon(IconNames.Yes)
    bluetooth.uartWriteString("HELLO FROM MICROBIT\n")
})
bluetooth.onBluetoothDisconnected(function () {
    basic.showIcon(IconNames.No)
})
bluetooth.startUartService()