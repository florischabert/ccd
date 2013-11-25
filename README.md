CC-Debugger Driver
==================

Driver for Texas Instruments CC-Debugger.

Support
-------
* CC2541 (SensorTag)

Features
--------
* Erase target flash
* Write HEX file to flash
* Verify memory

Usage
-----
    Usage: ccd [options]
    Options:
      -h, --help           	Print this help
      -v, --verbose        	Verbose mode
      -i, --info           	Print target info
      -e, --erase          	Erase flash
      -x, --hex <filename> 	Erase, Write HEX file to flash, Verify
      -s, --slow           	Slow mode