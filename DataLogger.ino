/**
 * @file DataLogger.ino
 * @brief Implement the INA219-based datalogger, with an SSD1306 OLED display
 * and MicroSD card logging
 * @author Gilles Henrard
 * @date 29/07/2024
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
Adafruit_INA219 ina219;        ///< INA219 control instance
float current_mA = 0.0F;       ///< Latest Current measurement (mA)
float loadvoltage_V = 0.0F;    ///< Latest Load voltage measurement (V)
float power_mW = 0.0F;         ///< Latest Power measurement (mW)
float energy_mWh = 0.0F;       ///< Latest Energy measurement (mWh)
unsigned long msSinceBoot = 0; ///< Time elapsed since boot (ms)

// declare microSD variables
#define CHIPSELECT 10
#define ENABLE_DEDICATED_SPI 1
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#define SPI_DRIVER_SELECT 0
SdFat32 sd;        ///< MicroSD card control instance
File32 outputFile; ///< File instance to which data will be flushed

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
    outputFile.open("MEAS.csv", O_WRITE | O_CREAT | O_TRUNC);
    outputFile.print("Time,Voltage,Current\n");
    outputFile.sync();

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
    static float previousCurrent = 0.0F; ///< previous current measurement buffer (retains value at each function pass)
    static float previousVoltage = 0.0F; ///< previous voltage measurement buffer (retains value at each function pass)
    static float previousPower = 0.0F;   ///< previous power measurement buffer (retains value at each function pass)
    static float previousEnergy = 0.0F;  ///< previous energy measurement buffer (retains value at each function pass)

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
        if (loadvoltage_V != previousVoltage)
        {
            displayline(loadvoltage_V, 0, " V");
            previousVoltage = loadvoltage_V;
        }

        // update the current line on the SSD1306 display
        if (current_mA != previousCurrent)
        {
            displayline(current_mA, 2, " mA");
            previousCurrent = current_mA;
        }

        // update the power line on the SSD1306 display
        if (power_mW != previousPower)
        {
            displayline(power_mW, 4, " mW");
            previousPower = power_mW;
        }

        // update the energy line on the SSD1306 display
        if (energy_mWh != previousEnergy)
        {
            displayline(energy_mWh, 6, " mWh");
            previousEnergy = energy_mWh;
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
    const float MS_PER_HOUR PROGMEM = 3600000.0F; ///< Number of milliseconds in an hour
    float shuntvoltage_mV = 0.0F;
    float busvoltage_V = 0.0F;
    static unsigned long previousTime_ms = 0; // retains value at each pass

    // turn the INA219 on
    ina219.powerSave(false);

    // get the shunt voltage, bus voltage, current and power consumed from the
    // INA219
    shuntvoltage_mV = ina219.getShuntVoltage_mV();
    busvoltage_V = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    msSinceBoot = millis();

    // turn the INA219 off
    ina219.powerSave(true);

    // compute the load voltage
    loadvoltage_V = busvoltage_V + (shuntvoltage_mV / 1000.0F);

    // compute the power consumed
    power_mW = loadvoltage_V * current_mA;

    // compute the energy consumed since last measurement  (E[mWh] = P[mW] * dt[h])
    // + add it to previously consumed
    energy_mWh += ((power_mW * (float)(msSinceBoot - previousTime_ms)) / MS_PER_HOUR);

    // update time at latest measurements
    previousTime_ms = msSinceBoot;
}

/**
 * @brief Append the measurments in a CSV file
 */
void writeFile()
{
    static uint8_t nbLinesWritten = 0; ///< Number of cycles since last MicroSD flush (retains value at each pass)
    char finalLine[32], voltageString[16] = {0}, currentString[16] = {0};

    // prepare buffers with the voltage and current values in strings
    dtostrf(loadvoltage_V, 10, 3, voltageString);
    dtostrf(current_mA, 10, 3, currentString);

    // format a csv line : time,voltage,current\n
    sprintf_P(finalLine, PSTR("%ld,%s,%s\n"), msSinceBoot, voltageString, currentString);

    // write the line in the file
    outputFile.write(finalLine);

    // after 9 cycles (1 sec.), apply SD buffer changes to file in SD
    if (nbLinesWritten >= 9)
    {
        outputFile.sync();
    }

    // increment cycles count + reset to 0 after 10 cycles
    nbLinesWritten++;
    nbLinesWritten %= 10;
}
