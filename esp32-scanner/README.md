## Script to scan the devices availables to connect

The esp32 can scan for device and get the Address and name, like show below:

```
================================
ESP32 BLE DIAGNOSTIC SCANNER
================================

Scanning for 15 seconds...


================================
Devices found: 3
================================

--------------------------------
DEVICE #1
--------------------------------
Address: 0b:d6:48:24:ee:6a
RSSI: -93 dBm
Name: <none>
Service UUID: <none>
Manufacturer data: <none>

Advertisement:
Name: , Address: 0b:d6:48:24:ee:6a, rssi: -93, serviceData: K␗#UKXB␑2������j|�_�␗��r�2�

--------------------------------
DEVICE #2
--------------------------------
Address: 72:6e:bf:e9:29:34
RSSI: -66 dBm
Name: <none>
Service UUID: 0000d9f9-0000-1000-8000-00805f9b34fb
Manufacturer data length: 22
Manufacturer data HEX: DA F0 2E AE 9D 8A 93 40 04 92 D0 78 7B 1F 05 FE CE 5B 28 13 54 28 

Advertisement:
Name: , Address: 72:6e:bf:e9:29:34, manufacturer data: daf02eae9d8a93400492d0787b1f05fece5b28135428, serviceUUID: 0000d9f9-0000-1000-8000-00805f9b34fb, rssi: -66

--------------------------------
DEVICE #3
--------------------------------
Address: fb:fe:84:0d:e5:45
RSSI: -57 dBm
Name: BBC micro:bit [pavag]
Service UUID: <none>
Manufacturer data: <none>

Advertisement:
Name: BBC micro:bit [pavag], Address: fb:fe:84:0d:e5:45, rssi: -57

================================
SCAN COMPLETE
================================

```
