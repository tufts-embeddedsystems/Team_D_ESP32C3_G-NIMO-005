# Team_D_ESP32C3_G-NIMO-005

## Issues
- Uploading issue with bootloader [timed-out-waiting-for-packet-header]
	- https://github.com/espressif/esptool/issues/136
	- https://arduino.stackexchange.com/questions/38012/major-difference-between-dtr-signal-and-rts-signal
	- [Possible Solution] https://randomnerdtutorials.com/solved-failed-to-connect-to-esp32-timed-out-waiting-for-packet-header/
	- **[Pin Configuration]**
		- https://docs.espressif.com/projects/esptool/en/latest/esp32c3/advanced-topics/boot-mode-selection.html
		- https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
			- section 2.4 strapping pin
