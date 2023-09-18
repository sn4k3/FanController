# V2.0 (TODO)
- New MCU
- in/out Temperature sensor
- DC and PWM mode

# V1.2 (18/07/2019)
- Added USB-C port for alternative 5V PWR IN and discard VREG
- Added 5V PWR IN header for alternative 5V PWR IN and discard VREG
- Added 5V expansion header
- Added resistor to LED_PWM which fix the LED from damage or overcurrent
- Added 100nF to reset pin (Optional)
- Added passive values to the PCB, only for assembly guideness
- Moved some components, this make V1.1 expansion boards not compatible with 1.2
- Moved R4 near the T1 gate
- Converted all 0805 packages to 0603 packages
- Increased some fonts that were un-readable

# V1.1 (26/06/2019)
- Added 5V and fan pwm LEDs on board for troubleshoot
- Added alternative VREG (AP7381-50SA-7 or similar) and C2 cap
- Added alternative XH headers for fan and power, eg. regular fan header
- Added expansion headers to stack boards for more fans
- Added solder jumper for test or ignore headers
- Added solder jumper to select 2/3/4 fan wire operation
- Added more information about pins on board
- Added inboard smd pot and switch
- Fixed wrong mosfet footprint
- Changed C1 from 0805 to 1206 to handle 60V
- Changed VH orientation, now tab faces out the board
- Better label positions and reorder
- Compact board and components
- Better thermals

# V1.0
- First Release