/**
 * @file DataLogger.ino
 * @brief Implement the INA219-based datalogger, with an SSD1306 OLED display
 * and MicroSD card logging
 * @author Gilles Henrard
 * @date 28/07/2024
 *
 * @note Datasheets :
 *   - SSD1306 : https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
 *   - INA219 https://www.ti.com/lit/gpn/ina219
 */
#include <Adafruit_INA219.h>
#include <SSD1306AsciiAvrI2c.h>
#include <SdFat.h>

// global variables
volatile boolean timerOccurred = false; ///< Flag indicating whether a timer interrupt occurred

// SSD1306 OLED display variables
#define OLED_RESET 4        ///< GPIO pin used to reset the screen
SSD1306AsciiAvrI2c display; ///< Display control instance

// INA219 variables
Adafruit_INA219 ina219;                 ///< INA219 control instance
float current_mA = 0.0, oldcurr = 0.0;  ///< Current (mA) buffers
float loadvoltage = 0.0, oldvolt = 0.0; ///< Load voltage (V) buffers
float power_mW = 0.0, oldpow = 0.0;     ///< Power (mW) buffers
float energy_mWh = 0.0, oldegy = 0.0;   ///< Energy (mWh) buffers
unsigned long elapsed = 0;              ///< Time elapsed since last measurements (ms)

// declare microSD variables
#define CHIPSELECT 10
#define ENABLE_DEDICATED_SPI 1
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#define SPI_DRIVER_SELECT 0
SdFat32 sd;         ///< MicroSD card control instance
File32 measurFile;  ///< File instance to which data will be flushed

/**
 * @brief Setup the hardware
 */
void setup()
{
    // Disable ADC
    ADCSRA = 0;
    ACSR = 0x80;

    // setup the INA219
    ina219.begin();

    // setup the SDcard reader
    sd.begin(CHIPSELECT);
    measurFile.open("MEAS.csv", O_WRITE | O_CREAT | O_TRUNC);
    measurFile.print("Time,Voltage,Current\n");
    measurFile.sync();

    // setup the display
    display.begin(&Adafruit128x64, 0x3C, OLED_RESET);
    display.setFont(System5x7);
    display.clear();

    // stop interrupts
    cli();

    // TIMER 1 for interrupt frequency 1 Hz:

    // initialise the CCR register and the counter
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

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

/**
 * @brief Main program loop
 */
void loop()
{
    // if timer has been reached
    if (timerOccurred)
    {
        // reset the flag
        timerOccurred = false;

        // get the values measured by the INA219
        ina219values();

        // write the data at the end of MEAS.csv
        writeFile();

        //
        //	Display update procedure in main loop to avoid
        //		wasting clock time in function call
        //

        // update the voltage line on the SSD1306 display
        if (loadvoltage != oldvolt)
        {
            displayline(loadvoltage, 0, " V");
            oldvolt = loadvoltage;
        }

        // update the current line on the SSD1306 display
        if (current_mA != oldcurr)
        {
            displayline(current_mA, 2, " mA");
            oldcurr = current_mA;
        }

        // update the power line on the SSD1306 display
        if (power_mW != oldpow)
        {
            displayline(power_mW, 4, " mW");
            oldpow = power_mW;
        }

        // update the energy line on the SSD1306 display
        if (energy_mWh != oldegy)
        {
            displayline(energy_mWh, 6, " mWh");
            oldegy = energy_mWh;
        }
    }
}

/**
 * @brief Process a Timer1 interrupt
 * @details Set the flag indicating timer has been reached
 *
 * @param TIMER1_COMPA_vect timer1 comparator vector
 */
ISR(TIMER1_COMPA_vect)
{
    timerOccurred = true;
}

/**
 * @brief Format and display a measurment at the right line
 *
 * @param measurment Value measured to display
 * @param line_num Line number at which display the value
 * @param line_end End of line (unit) to append to the line
 */
void displayline(const float measurment, const uint8_t line_num, const char line_end[])
{
    char floatbuf[16] = {0};

    // format the line ([-]xxxxx.xxx [unit])
    dtostrf(measurment, 10, 3, floatbuf);
    strcat(floatbuf, line_end);

    // place the cursor and write the line
    display.setCursor(0, line_num);
    display.print(floatbuf);
}

/**
 * @brief Request measurement values to the INA219 via I²C
 * @note This takes 11ms overall
 */
void ina219values()
{
    float shuntvoltage = 0.0;
    float busvoltage = 0.0;

    // turn the INA219 on
    ina219.powerSave(false);

    // get the shunt voltage, bus voltage, current and power consumed from the
    // INA219
    shuntvoltage = ina219.getShuntVoltage_mV();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    elapsed = millis();

    // turn the INA219 off
    ina219.powerSave(true);

    // compute the load voltage
    loadvoltage = busvoltage + (shuntvoltage / 1000.0);

    // compute the power consumed
    power_mW = loadvoltage * current_mA;

    // compute the energy consumed (t = elapsed[ms] / 3600[s/h] * 1000[ms/s])
    energy_mWh += power_mW * (elapsed / 3600000.0);
}

/**
 * @brief Append the measurments in a CSV file
 */
void writeFile()
{
    static uint8_t cycles = 0; ///< Number of cycles since last MicroSD card buffer flush (retains its value at each function pass)
    char buf[32], voltbuf[16] = {0}, curbuf[16] = {0};

    // prepare buffers with the voltage and current values in strings
    dtostrf(loadvoltage, 10, 3, voltbuf);
    dtostrf(current_mA, 10, 3, curbuf);

    // format a csv line : time,voltage,current\n
    sprintf(buf, "%ld,%s,%s\n", elapsed, voltbuf, curbuf);

    // write the line in the file
    measurFile.write(buf);

    // after 9 cycles (1 sec.), apply SD buffer changes to file in SD
    if (cycles >= 9)
        measurFile.sync();

    // increment cycles count + reset to 0 after 10 cycles
    cycles++;
    cycles %= 10;
}
