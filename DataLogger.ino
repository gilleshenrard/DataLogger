#include <Adafruit_INA219.h>
#include <SSD1306AsciiAvrI2c.h>
#include <SdFat.h>

//declare timer trigger flag and counter value
volatile boolean triggered = false;
volatile unsigned long elapsed = 0;

//declare SSD1306 OLED display variables
#define OLED_RESET 4
SSD1306AsciiAvrI2c display;

//declare INA219 variables
Adafruit_INA219 ina219;
float shuntvoltage = 0.0;
float busvoltage = 0.0;
float current_mA = 0.0, oldcurr = 0.0;
float loadvoltage = 0.0, oldvolt = 0.0;
float power_mW = 0.0, oldpow = 0.0;
float energy_mWh = 0.0, oldegy = 0.0;

//declare display formatting variables
char floatbuf[16]={0};

//declare microSD variables
uint8_t cycles = 0;
#define CHIPSELECT 10
SdFat sd;
SdFile measurFile;

/******************************************************************************/
/*  I : /                                                                     */
/*  P : setup procedure                                                       */
/*  O : /                                                                     */
/******************************************************************************/
void setup() {
  //setup serial
  Serial.begin(115200);

  //setup the INA219
  ina219.begin();

  //setup the SDcard reader
  sd.begin(CHIPSELECT);
  measurFile.open("MEAS.csv", O_WRITE | O_CREAT | O_TRUNC);
  measurFile.print("Time,Voltage,Current\n");
  measurFile.sync();

  //setup the display
  display.begin(&Adafruit128x64, 0x3C, OLED_RESET);
  display.setFont(System5x7);
  display.clear();

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
    unsigned long timed = millis();
    writeFile();
    Serial.println(millis() - timed);
	
	//
	//	Display update procedure in main loop to avoid
	//		wasting clock time in function call
	//
    //update the voltage line on the SSD1306 display
	if(loadvoltage != oldvolt){
		displayline(loadvoltage, 0, " V");
		oldvolt = loadvoltage;
	}
	
    //update the current line on the SSD1306 display
	if(current_mA != oldcurr){
		displayline(current_mA, 2, " mA");
		oldcurr = current_mA;
	}
	
    //update the power line on the SSD1306 display
	if(power_mW != oldpow){
		displayline(power_mW, 4, " mW");
		oldpow = power_mW;
	}
	
    //update the energy line on the SSD1306 display
	if(energy_mWh != oldegy){
		displayline(energy_mWh, 6, " mWh");
		oldegy = energy_mWh;
	}

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

/****************************************************************************/
/*  I : Value measured to display                                           */
/*		Buffer holding the last saved measurment							*/
/*		Line number at which display the value								*/
/*		End of line (unit) to append to the line							*/
/*  P : Format and display a measurment at the right line, only if changed	*/
/*  O : /                                                                   */
/****************************************************************************/
void displayline(const float measurment, const uint8_t line_num, const char line_end[]) {
  //format the line (xxxx.xxx [unit])
  dtostrf(measurment, 8, 3, floatbuf);
  strcat(floatbuf, line_end);
  
  //place the cursor and write the line
  display.setCursor(0, line_num);
  display.print(floatbuf);
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
    char buf[32], voltbuf[8]={0}, curbuf[8]={0};

    //prepare buffers with the voltage and current values in strings
    dtostrf(loadvoltage, 6, 3, voltbuf);
    dtostrf(current_mA, 6, 3, curbuf);
    
    //format a csv line : time,voltage,current\n
    sprintf(buf, "%ld,%s,%s\n", elapsed, voltbuf, curbuf);

    //write the line in the file
    measurFile.write(buf);

    //after 9 cycles (1 sec.), apply SD buffer changes to file in SD
    if(cycles >=9)
      measurFile.sync();

    //increment cycles count + reset to 0 after 10 cycles
    cycles++;
    cycles %= 10;
}
