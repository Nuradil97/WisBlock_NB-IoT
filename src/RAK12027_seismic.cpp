/**
 * @file RAK12027_seismic.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Omron D7S Seismic Sensor
 * @version 0.1
 * @date 2022-08-27
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "app.h"

#include <Wire.h>
#include <RAK12027_D7S.h> // Click here to get the library: http://librarymanager/All#RAK12027_D7S

RAK_D7S D7S;

//******************************************************************//
// RAK12027 INT1_PIN
//******************************************************************//
// Slot A      WB_IO1
// Slot B      WB_IO2 ( not recommended, pin conflict with IO2)
// Slot C      WB_IO3
// Slot D      WB_IO5
// Slot E      WB_IO4
// Slot F      WB_IO6
//******************************************************************//
//******************************************************************//
// RAK12027 INT2_PIN
//******************************************************************//
// Slot A      WB_IO2 ( not recommended, pin conflict with IO2)
// Slot B      WB_IO1
// Slot C      WB_IO4
// Slot D      WB_IO6
// Slot E      WB_IO3
// Slot F      WB_IO5
//******************************************************************//

/** Interrupt pin, depends on slot */
#if RAK12027_SLOT == 0 // Slot A
#pragma message "Slot A"
#define INT1_PIN WB_IO1
#define INT2_PIN WB_IO2
#elif RAK12027_SLOT == 1 // Slot B
#pragma message "Slot B"
#define INT1_PIN WB_IO2
#define INT2_PIN WB_IO1
#elif RAK12027_SLOT == 2 // Slot C
#pragma message "Slot C"
#define INT1_PIN WB_IO3
#define INT2_PIN WB_IO4
#elif RAK12027_SLOT == 3 // Slot D
#pragma message "Slot D"
#define INT1_PIN WB_IO5
#define INT2_PIN WB_IO6
#elif RAK12027_SLOT == 4 // Slot E
#pragma message "Slot E"
#define INT1_PIN WB_IO4
#define INT2_PIN WB_IO3
#elif RAK12027_SLOT == 5 // Slot F
#pragma message "Slot F"
#define INT1_PIN WB_IO6
#define INT2_PIN WB_IO5
else
#pragma message "No slot defined"
#endif
// flag variables to handle collapse/shutoff only one time during an earthquake
bool shutoff_alert = false;
bool collapse_alert = false;
bool earthquake_end = true;
bool earthquake_start = false;

float savedSI = 0.0f;
float savedPGA = 0.0f;

void report_status(void)
{
	uint8_t current_state = D7S.getState();
	char status_txt[128];
	switch (current_state)
	{
	case NORMAL_MODE:
		sprintf(status_txt, "Normal");
		break;
	case NORMAL_MODE_NOT_IN_STANBY:
		sprintf(status_txt, "Not in Standby");
		break;
	case INITIAL_INSTALLATION_MODE:
		sprintf(status_txt, "Initial Installation");
		break;
	case OFFSET_ACQUISITION_MODE:
		sprintf(status_txt, "Offset Acquisition");
		break;
	case SELFTEST_MODE:
		sprintf(status_txt, "Selftest");
		break;
	default:
		sprintf(status_txt, "Undefined");
		break;
	}
	MYLOG("SEIS", "Current mode: %s", status_txt);
}

/**
 * @brief Callback for INT 1
 * Wakes up application with signal SEISMIC_ALERT
 * Activated on Collapse and Shutoff signals
 *
 */
void d7s_int1_handler(void)
{
	api_wake_loop(STATUS | SEISMIC_ALERT);
}

/**
 * @brief Callback for INT 2
 * Wakes up application with signal SEISMIC_EVENT
 * Activated on Earthquake start and end
 *
 */
void d7s_int2_handler(void)
{
	if (digitalRead(INT2_PIN) == LOW)
	{
		digitalWrite(LED_BLUE, HIGH);
	}
	else
	{
		digitalWrite(LED_BLUE, LOW);
	}
	api_wake_loop(STATUS | SEISMIC_EVENT);
}

/**
 * @brief Initialize Omron D7S seismic sensor
 *
 * @return true If sensor was found and is initialized
 * @return false If sensor initialization failed
 */
bool init_rak12027(void)
{
	// Read threshold settings from Flash
	read_threshold_settings();

	// start D7S connection
	D7S.begin();

	// wait until the D7S is ready
	time_t start_wait_time = millis();
	while (!D7S.isReady())
	{
		if ((millis() - start_wait_time) > 10000)
		{
			MYLOG("SEIS", "Timeout waiting for D7S");
			return false;
		}
		delay(500);
	}

	//--- SETTINGS ---
	// setting the D7S to switch the axis at inizialization time
	MYLOG("SEIS", "Setting D7S sensor to switch axis at inizialization time.");
	// AT_PRINTF("EVT: Setting D7S sensor to switch axis at inizialization time.\n");
	D7S.setAxis(SWITCH_AT_INSTALLATION);

	/*********************************************************************/
	/** Calling calibration, this should be done from AT command instead */
	/*********************************************************************/
	if (!calib_rak12027())
	{
		// MYLOG("SEIS", "Calibration failed with timeout");
		AT_PRINTF("EVT: Calibration failed with timeout\n");
		return false;
	}

	// Get saved threshold level

	// Set low threshold
	// D7S.setThreshold(THRESHOLD_LOW);
	threshold_rak12027(threshold_level);

	//--- RESETTING EVENTS ---
	// reset the events shutoff/collapse memorized into the D7S
	D7S.resetEvents();

	//--- READY TO GO ---
	MYLOG("SEIS", "Listening for earthquakes!");

#if MY_DEBUG > 0
	//--- Report status
	report_status();
#endif

	//--- INTERRUPT SETTINGS ---
	// registering event handler
	pinMode(INT1_PIN, INPUT);
	pinMode(INT2_PIN, INPUT);
	attachInterrupt(INT1_PIN, d7s_int1_handler, FALLING);
	attachInterrupt(INT2_PIN, d7s_int2_handler, CHANGE);

	return true;
}

/**
 * @brief Calibration of D7S sensor
 * Should be called if position of sensor is changing
 *
 * @return true if calibrarion succeed
 * @return false if calibration timeout
 */
bool calib_rak12027(void)
{
	//--- INITIALIZZATION ---
	AT_PRINTF("EVT: Initializing the D7S sensor in 2 seconds. Please keep it steady during the initializing process.\n");
	delay(2000);
	MYLOG("SEIS", "Initializing...");
	// start the initial installation procedure
	D7S.initialize();
	// wait until the D7S is ready (the initializing process is ended)
	time_t start_wait_time = millis();
	while (!D7S.isReady())
	{
		if ((millis() - start_wait_time) > 5000)
		{
			MYLOG("SEIS", "Timeout waiting initialization of D7S");
			return false;
		}
		delay(500);
	}
	MYLOG("SEIS", "INITIALIZED!");
	return true;
}

void threshold_rak12027(uint8_t new_threshold)
{
	if (new_threshold == 1)
	{
		D7S.setThreshold(THRESHOLD_LOW);
	}
	else
	{
		D7S.setThreshold(THRESHOLD_HIGH);
	}
}

/**
 * @brief Get events from the D7S after interrupt occured
 *
 * @param is_int1 true if it was INT1, false if it was INT2
 * @return uint8_t event code
 * 			0 no event found
 * 			1 Collapse alert
 * 			2 Shutoff alert
 * 			3 Collapse and Shutoff alert
 * 			4 Earthquake start detected
 * 			5 Earthquake end detected
 */
uint8_t check_event_rak12027(bool is_int1)
{
	MYLOG("SEIS", "Check Event");

#if MY_DEBUG > 0
	//--- Report status
	report_status();
#endif

	uint8_t return_val = 0;
	if (is_int1)
	{
		// Check for collapse event
		if (D7S.isInCollapse() == 1)
		{
			D7S.resetEvents();
			return_val = 1;
		}
		// Check for SI > 5
		if (D7S.isInShutoff() == 1)
		{
			D7S.resetEvents();
			return_val = return_val + 2;
		}
	}
	else
	{
		// Check if earthquake started or ended
		if (D7S.isEarthquakeOccuring())
		{
			earthquake_start = true;
			return_val = 4;
		}
		else
		{
			D7S.resetEvents();
			if (earthquake_start)
			{
				return_val = 5;
			}
		}
	}
	return return_val;
}

/**
 * @brief Read latest saved SI and PGA values
 *
 * @param add_values if true, values will be added to payload, false will just read
 * @return true new values recorded
 * @return values are same as last reading, might be false alert
 */
bool read_rak12027(bool add_values)
{
	// Seismic Intensity vs PGA
	// I = 2.14 log10 (PGV) + 1.89

	MYLOG("SEIS", "Read values");

#if MY_DEBUG > 0
	//--- Report status
	report_status();
#endif

	// get information about the current earthquake
	float currentSI = D7S.getInstantaneusSI();
	float currentPGA = D7S.getInstantaneusPGA();

	MYLOG("SEIS", "Current SI level %.4f", currentSI);
	MYLOG("SEIS", "Current PGA level %.4f", currentPGA);

	float lastSI = D7S.getLastestSI(0);
	float lastPGA = D7S.getLastestPGA(0);

	for (int idx = 0; idx < 5; idx++)
	{
		MYLOG("SEIS", "SI level at %d %.4f", idx, D7S.getLastestSI(idx));
		MYLOG("SEIS", "PGA level at %d %.4f", idx, D7S.getLastestPGA(idx));
	}

	if ((savedSI != 0.0) && (savedPGA != 0.0))
	{
		if ((savedSI == lastSI) && (savedPGA == lastPGA))
		{
			MYLOG("SEIS", "Same as last value, false alert?");
			MYLOG("SEIS", "SI level old %.4f new %.4f", savedSI, lastSI);
			MYLOG("SEIS", "PGA level old %.4f new %.4f", savedPGA, lastPGA);
			return false;
		}
	}
	savedSI = lastSI;
	savedPGA = lastPGA;

	if (add_values)
	{
		MYLOG("SEIS", "Seismic values updated for NB-IoT payload");
	}
	MYLOG("SEIS", "SI level %.4f", lastSI);
	MYLOG("SEIS", "PGA level %.4f", lastPGA);
	return true;
}

/**
 * @brief Get last SI and PGA values
 *
 * @return float[] First 5 are last SI values, last 5 values are the latest PGA values
 */
 void rak12027_get_last(float * last_values)
{
	for (int idx = 0; idx < 5; idx++)
	{
		last_values[idx] = D7S.getLastestSI(idx);
		last_values[idx + 5] = D7S.getLastestPGA(idx);
	}
	return;
}