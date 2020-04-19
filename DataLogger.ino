#include <Adafruit_INA219.h>
#include <Adafruit_SSD1306.h>
#include <SdFat.h>

//declare timer trigger flag and counter value
volatile boolean triggered = false;
volatile unsigned long elapsed = 0;

//declare SSD1306 OLED display variables
#define OLED_RESET 4
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

//declare INA219 variables
Adafruit_INA219 ina219;
float shuntvoltage = 0.0;
float busvoltage = 0.0;
float current_mA = 0.0;
float loadvoltage = 0.0;
float power_mW = 0.0;
float energy_mWh = 0.0;

//declare microSD variables
const int chipSelect = 10;
SdFat sd;
SdFile measurFile;

/******************************************************************************/
/*  I : /                                                                     */
/*  P : setup procedure                                                       */
/*  O : /                                                                     */
/******************************************************************************/
void setup() {
  sd.begin(chipSelect);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  ina219.begin();

  // stop interrupts
  cli();

  // TIMER 1 for interrupt frequency 1 Hz:

  //initialise the CCR register and the counter
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  
  // set compare match register for 10 Hz increments
  OCR1A = 12499; // = 8000000 / (64 * 10) - 1 (must be <65536)
  
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  
  // Set CS12, CS11 and CS10 bits for 64 prescaler
  TCCR1B |= (0 << CS12) | (1 << CS11) | (1 << CS10);
  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  // allow interrupts
  sei();
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
    writeFile();
      
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
/*  P : get the values from the INA219 via I²C (takes 11ms)                   */
/*  O : /                                                                     */
/******************************************************************************/
void ina219values() {
  //turn the INA219 on
  ina219.powerSave(false);
  
  //get the shunt voltage, bus voltage, current and power consumed from the INA219
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();

  //turn the INA219 off
  ina219.powerSave(true);

  //compute the load voltage
  loadvoltage = busvoltage + (shuntvoltage / 1000.0);

  //compute the power consumed
  power_mW = loadvoltage*current_mA;
  
  //compute the energy consumed (t = 0.1s / 3600)
  energy_mWh += power_mW / 36000.0;
}

/******************************************************************************/
/*  I : /                                                                     */
/*  P : Append the measurments in a CSV file                                  */
/*  O : /                                                                     */
/******************************************************************************/
void writeFile() {
  if(measurFile.open("MEAS.csv", O_WRITE | O_CREAT | O_APPEND)) {
    char buf[32], voltbuf[8]={0}, curbuf[8]={0};

    //prepare buffers with the voltage and current values in strings
    dtostrf(loadvoltage, 6, 3, voltbuf);
    dtostrf(current_mA, 6, 3, curbuf);
    
    //format a csv line : time,voltage,current
    sprintf(buf, "%ld,%s,%s\n", elapsed, voltbuf, curbuf);

    //write the line in the file
    measurFile.write(buf);
    measurFile.close();
  }
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
