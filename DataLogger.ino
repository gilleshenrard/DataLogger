#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_SSD1306.h>
#include <SdFat.h>

//declare breakout variables
#define OLED_RESET 4
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
Adafruit_INA219 ina219;

//declare timer trigger flag and counter value
volatile boolean triggered = false;
volatile unsigned long elapsed = 0;

//declare INA219 variables
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power_mW = 0;
float energy_mWh = 0;

//declare microSD variables
SdFat SD;
const int chipSelect = 10;
File measurFile;

/******************************************************************************/
/*  I : /                                                                     */
/*  P : setup procedure                                                       */
/*  O : /                                                                     */
/******************************************************************************/
void setup() {
  SD.begin(chipSelect);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  ina219.begin();

  // TIMER 1 for interrupt frequency 1 Hz:
  cli(); // stop interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0
  // set compare match register for 10 Hz increments
  OCR1A = 12499; // = 8000000 / (256 * 1) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12, CS11 and CS10 bits for 256 prescaler
  TCCR1B |= (1 << CS12) | (0 << CS11) | (0 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei(); // allow interrupts
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : main program loop                                                     */
/*  O : /                                                                     */
/******************************************************************************/
void loop() {
  //if timer has been reached
  if (triggered)
  {
    //get the values measured by the INA219
    ina219values();

    //write the data at the end of MEAS.csv
    measurFile = SD.open("MEAS.csv", FILE_WRITE);
    if (measurFile) {
      char buf[32], voltbuf[8]={0}, curbuf[8]={0};
      dtostrf(loadvoltage, 6, 3, voltbuf);
      dtostrf(current_mA, 6, 3, curbuf);
      
      //format a csv line : time,voltage,current
      sprintf(buf, "%ld,%s,%s", elapsed, voltbuf, curbuf);

      //write the line in the file
      measurFile.println(buf);
      measurFile.close();
    }
      
    //display the data on the SSD1306 display
    displaydata();

    //reset the flag
    triggered = false;
  }
}

/****************************************************************************/
/*  I : timer1 comparator vector                                            */
/*  P : set the flag indicating timer has been reached + increment time var.*/
/*  O : /                                                                   */
/****************************************************************************/
ISR(TIMER1_COMPA_vect){
  triggered = true;
  elapsed += 100;
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : send the data to be displayed by the SSD1306 (takes 75ms)             */
/*  O : /                                                                     */
/******************************************************************************/
void displaydata() {
  char buffer[32]={0};
  char floatbuf[8]={0};

  //set up the display (clear, then white colour and size 1)
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  //write the first line (xxx.xxx V  xxx.xxx A)
  dtostrf(busvoltage, 6, 3, floatbuf);
  sprintf(buffer, "%s V  ", floatbuf);
  dtostrf(current_mA, 6, 3, floatbuf);
  strcat(buffer, floatbuf);
  strcat(buffer, " mA");
  display.setCursor(0, 0);
  display.println(buffer);

  //write the second line (xxx.xxx mW)
  dtostrf(power_mW, 6, 3, floatbuf);
  sprintf(buffer, "%s mW", floatbuf);
  display.setCursor(0, 10);
  display.println(buffer);

  //write the third line (xxx.xxx mWh)
  dtostrf(energy_mWh, 6, 3, floatbuf);
  sprintf(buffer, "%s mWh", floatbuf);
  display.setCursor(0, 20);
  display.println(buffer);
  
  //refresh the screen
  display.display();
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : get the values from the INA219 via I²C (takes 7ms)                    */
/*  O : /                                                                     */
/******************************************************************************/
void ina219values() {
  //get the shunt voltage, bus voltage, current and power consumed from the INA219
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();

  //compute the load voltage
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  
  //compute the energy consumed (divider : 0.1s / 3600)
  energy_mWh += power_mW / 36000;
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : print all values through the serial monitor                           */
/*  O : /                                                                     */
/******************************************************************************/
void serialData() {
  char buffer[16]={0};
  char floatbuf[8]={0};

  dtostrf(busvoltage, 6, 3, floatbuf);
  sprintf(buffer, "Vbus: %s V", floatbuf);
  Serial.println(buffer);

  dtostrf(shuntvoltage, 6, 3, floatbuf);
  sprintf(buffer, "Vsh : %s mV", floatbuf);
  Serial.println(buffer);

  dtostrf(loadvoltage, 6, 3, floatbuf);
  sprintf(buffer, "Vlo : %s V", floatbuf);
  Serial.println(buffer);

  dtostrf(current_mA, 6, 3, floatbuf);
  sprintf(buffer, "I   : %s mA", floatbuf);
  Serial.println(buffer);

  dtostrf(power_mW, 6, 3, floatbuf);
  sprintf(buffer, "P   : %s mW", floatbuf);
  Serial.println(buffer);

  dtostrf(energy_mWh, 6, 3, floatbuf);
  sprintf(buffer, "E   : %s mWh", floatbuf);
  Serial.println(buffer);

  Serial.println("");
}
