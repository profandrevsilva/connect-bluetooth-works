bluetooth.onBluetoothConnected(function () {
    basic.showIcon(IconNames.Yes)
})
bluetooth.onBluetoothDisconnected(function () {
    basic.showIcon(IconNames.No)
})
bluetooth.onUartDataReceived(serial.delimiters(Delimiters.NewLine), function () {
    robotbit.Servo(robotbit.Servos.S1, 90)
    command = bluetooth.uartReadUntil(serial.delimiters(Delimiters.NewLine))
    if (command == "right") {
        basic.pause(1000)
        robotbit.Servo(robotbit.Servos.S1, 0)
        basic.pause(1000)
        robotbit.Servo(robotbit.Servos.S1, 90)
    } else if (command == "left") {
        basic.pause(1000)
        robotbit.Servo(robotbit.Servos.S1, 180)
        basic.pause(1000)
        robotbit.Servo(robotbit.Servos.S1, 90)
    } else if (command == "horn") {
        music.play(music.stringPlayable("C5 B A G F E D C ", 140), music.PlaybackMode.InBackground)
    }
})
let command = ""
bluetooth.startUartService()
basic.showIcon(IconNames.Heart)
