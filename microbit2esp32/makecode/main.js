let temperatura = 0
bluetooth.startUartService()
bluetooth.setTransmitPower(7)
basic.forever(function () {
    temperatura = input.temperature()
    bluetooth.uartWriteString("TEMP:" + temperatura + "\n")
    basic.showNumber(temperatura)
    basic.pause(2000)
})
