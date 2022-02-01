# DataLogger
An Arduino-based data logger, from the work of GreatScott


## 1. Introduction
As mentioned, this project is based on the work of GreatScott. It merely aims on learning and improvement.

[Project's Instructables page](https://www.instructables.com/id/Make-Your-Own-Power-MeterLogger/)

[Project's Youtube video](https://www.youtube.com/watch?v=lrugreN2K4w&feature=emb_logo)

A data logger is a tool which monitors physical data such as voltage, current, power, ... and stores that information
somewhere.

It can have a display, a storage space, anything useful to treat data.


## 2. Improvements
###	a. Display
* Used SSD1306AsciiAvrI2c.h instead of Adafruit_SSD1306.h (more optimised)
* Got rid of SPI.h (less clogging)
* Used a tinier font + reorganised display (faster display)
* Each display line updates only if value changed since last measurment (faster display)
### b. Text files
* Used a SdFat32 object instead of an SdFat (more optimised but requires FAT32 formatting)
* Merged all 3 text files into one *.CSV (faster and more manageable)
* Used optimised SDfat settings
* Made the file open only at startup instead of each cycle (faster SD timing)
* CSV lines are written for 10 cycles, then the file is synched every 10 cycles
### c. Performance
* Made ina219 go to sleep after measurments (is up only 11 ms / 100 ms, less power consumption)
* Disabled ADC (less power consumption)
* Used an Arduino Pro Mini 3.3V (less power consumption)
* Used a timer interrupt instead of a timed loop (more precise when cycle < 100 ms)
* Fixed a power consumption calculation mistake (0.1s / 3600s/h)
* Added a mosfet driven by the Arduino between VCC and the SD card reader to deactivate it if no SD card (less power consumption)


## 3. To Improve
* Sometimes (34 times over 26500 cycles), SD logging can take up to 100ms alone.
    * Use of a circular buffer to write 64 bytes blocks at a time
	* Use of a binary file instead of a text file
	* Asynchronous use of the files (measurments buffered each 100ms, then written when free time)
* Power consumption is still high
    * Disable Brownout Detection, as it is useless because of the LiPo management chip (Vin always = 4.2V)
	* Use of sleep modes
* SD logging is not necessary when no SD card is inserted. Can be disabled then.
* Screen refresh every 100 ms can be hard to read
    * A potentiometer to vary the screen refresh rate
	* A button to completely hold a value
* Power measurment is done by dividing by 100 ms, which is not accurate
    * Must divide by the time elapsed since last cycle