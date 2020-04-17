#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include "SdFat.h"

//declare breakout variables
SdFat SD;
#define OLED_RESET 4
const int chipSelect = 10;
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_INA219 ina219;

//declare time variables
unsigned long previousMillis = 0;
unsigned long interval = 100;

//declare INA219 variables
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float energy = 0;

//declare microSD variables
File TimeFile;
File VoltFile;
File CurFile;

/******************************************************************************/
/*  I : /                                                                     */
/*  P : setup procedure                                                       */
/*  O : /                                                                     */
/******************************************************************************/
void setup() {
  SD.begin(chipSelect);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  ina219.begin();
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : main program loop                                                     */
/*  O : /                                                                     */
/******************************************************************************/
void loop() {
  //every 100 ms
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    //update timestamp
    previousMillis = currentMillis;
    
    //get the values from the INA219
    ina219values();

    //write the data at the end of TIME.txt
    TimeFile = SD.open("TIME.txt", FILE_WRITE);
    if (TimeFile) {
      TimeFile.println(currentMillis);
      TimeFile.close();
    }

    //write the data at the end of VOLT.txt
    VoltFile = SD.open("VOLT.txt", FILE_WRITE);
    if (VoltFile) {
      VoltFile.println(loadvoltage);
      VoltFile.close();
    }

    //write the data at the end of CUR.txt
    CurFile = SD.open("CUR.txt", FILE_WRITE);
    if (CurFile) {
      CurFile.println(current_mA);
      CurFile.close();
    }

    //display the data on the SSD1306 display
    displaydata();
  }
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : send the data to be displayed by the SSD1306                          */
/*  O : /                                                                     */
/******************************************************************************/
void displaydata() {
  //set up the display (clear, then white colour and size 1)
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  //write the first line (x.xx V  xxx.xx A)
  display.setCursor(0, 0);
  display.println(loadvoltage);
  display.setCursor(35, 0);
  display.println("V");
  display.setCursor(50, 0);
  display.println(current_mA);
  display.setCursor(95, 0);
  display.println("mA");

  //write the second line (xxx.xx mW)
  display.setCursor(0, 10);
  display.println(loadvoltage * current_mA);
  display.setCursor(65, 10);
  display.println("mW");

  //write the third line (x.xx mWh)
  display.setCursor(0, 20);
  display.println(energy);
  display.setCursor(65, 20);
  display.println("mWh");

  //refresh the screen
  display.display();
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : get the values from the INA219 via I²C                                */
/*  O : /                                                                     */
/******************************************************************************/
void ina219values() {
  //get the shunt voltage, bus voltage and current from the INA219
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();

  //compute the load voltage
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  
  //compute the energy consumed
  energy = energy + loadvoltage * current_mA / 3600;
}
