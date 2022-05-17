# Team_D_ESP32C3_G-NIMO-005

## Issues
Current Status: One node has been deployed, outside the SEC. It's temperature reading is high, but is updated to the dashboard (http://en1-pi.eecs.tufts.edu/) at NODE1 for "dapper-dingos" (Direct Link: http://en1-pi.eecs.tufts.edu/node/dapper-dingos/NODE1). Pictures of it installed on 5/13/22 are in the deployedNodeImages folder. 

## Issues
- Uploading issue with bootloader [timed-out-waiting-for-packet-header]
	- https://github.com/espressif/esptool/issues/136
	- https://arduino.stackexchange.com/questions/38012/major-difference-between-dtr-signal-and-rts-signal
	- [Possible Solution] https://randomnerdtutorials.com/solved-failed-to-connect-to-esp32-timed-out-waiting-for-packet-header/
	- **[Pin Configuration]**
		- https://docs.espressif.com/projects/esptool/en/latest/esp32c3/advanced-topics/boot-mode-selection.html
		- https://www.espressif.com/sites/default/files/documentation/esp32-c3-wroom-02_datasheet_en.pdf
			- section 3.3 strapping pin

## Resources and Documentation
- For an introduction and overview of the project, see the Final Project Report.
- For in-depth documentation on code, continue reading this readme.
- Documentation on sensor location and status, can be found by visiting the EE-193 website. (and mabye the other documentation?)
- Datasheets are collected in the ... folder. 
- For the PCB design, see the Zip file in this repository with KiCAD files
- For the enclosure design, see the Zip file in this repository with Fusion360 files
- A archive of the teams Google Drive folder is also included in a zip folder in this repository


## Contact Info:
Contact Bjorn Isaacson at bjornaisaacson@gmail.com for information about the enclosure, or batteries.  
