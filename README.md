# dfu-programmer
Here you can find my changes for dfu-programmer.
Original source comes from: [dfu-programmer](https://github.com/dfu-programmer/dfu-programmer)

My changes:
* Device initialization changes: it doesn't matter where DFU iterface is placed, on initialization dfu-programmer will lookup valid configuration interface, it will detach kernel drivers and will active DFU configuration
* If device is in runtime mode (DFU protocol no: 1) and you try do something incorrect for that iface, then you will be notified
* You can set **vid:pid** if you are using other ids, for example: **dfu-programmer atmega16u2:03eb:2fef dfumode**
* You can also use old bus address (NOTICE comma after device/chip name), by: **dfu-programmer atmega16u2,3,21 dfumode**
* You can use both: **dfu-programmer atmega16u2:03eb:2fef,3,21 dfumode**
* dfu-programmer will not jump to bootloader, you have to do it manually by: **dfu-programmer atmega16u2 dfumode**

My workflow:
```sh
$ dfu-programmer atmega32u4:12ab:56cd dfumode
$ dfu-programmer atmega32u4:12ab:56cd erase
$ dfu-programmer atmega32u4:12ab:56cd flash firmware.hex
$ dfu-programmer atmega32u4:12ab:56cd launch
```
