# DataLogger
An Arduino-based data logger, from the work of GreatScott

---
## 1. Introduction
As mentioned, this project is based on the work of GreatScott. It merely aims on learning and improvement.

[Project's Instructables page](https://www.instructables.com/id/Make-Your-Own-Power-MeterLogger/)

[Project's Youtube video](https://www.youtube.com/watch?v=lrugreN2K4w&feature=emb_logo)

A data logger is a tool which monitors physical data such as voltage, current, power, ... and stores that information
somewhere.

It can have a display, a storage space, anything useful to treat data.

---
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