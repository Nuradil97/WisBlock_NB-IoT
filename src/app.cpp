// Refactored NB-IoT Seismic Sensor main logic
// Based on RAK4630 + RAK5860 + RAK12027 (Slot D)

#include "app.h"
#include "RAK5860_AT.h"

#ifdef NRF52_SERIES
SoftwareTimer delayed_sending;
#endif

char g_ble_dev_name[10] = "RAK-SEISM";
bool has_rak1901 = false;
bool has_rak12002 = false;
bool rejoin_network = false;
bool send_pending = false;

void send_delayed(TimerHandle_t unused) {
    api_wake_loop(STATUS);
    delayed_sending.stop();
}

void setup_app(void) {
    Serial.begin(115200);
    time_t serial_timeout = millis();
    while (!Serial && (millis() - serial_timeout < 5000)) {
        delay(100);
        digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    }
    digitalWrite(LED_GREEN, LOW);

    delayed_sending.begin(60000, send_delayed, NULL, false);

    AT_PRINTF("Seismic Sensor with NB-IoT\n");
    AT_PRINTF("Built with RAK's WisBlock\n");
    AT_PRINTF("SW Version %d.%d.%d\n", g_sw_ver_1, g_sw_ver_2, g_sw_ver_3);
    AT_PRINTF("============================\n");

    if (!RAK5860_init()) {
        MYLOG("NB", "Failed to initialize RAK5860");
        return;
    }

    delay(1000);
    MYLOG("NB", "RAK5860 initialized");

#ifdef NRF52_SERIES
    g_enable_ble = true;
#endif
}

bool init_app(void) {
    pinMode(WB_IO2, OUTPUT);
    digitalWrite(WB_IO2, LOW);

    MYLOG("APP", "init_app");
    Wire.begin();
    Wire.setClock(400000);

    read_threshold_settings();
    bool init_result = init_rak12027();
    MYLOG("SEIS", "RAK12027_SLOT = %d", RAK12027_SLOT);
    MYLOG("APP", "RAK12027 %s", init_result ? "success" : "failed");

    if (init_result) Serial.println("+EVT: RAK12027 OK");

    init_user_at();
    return init_result;
}

void send_earthquake_payload() {
    float jma_intensity = 0.0;
    if (savedPGA > 0 && savedSI > 0) {
        jma_intensity = 0.5 + 1.72 * log10(savedPGA * 100) + 0.4 * log10(savedSI * 100);
        MYLOG("APP", "JMA intensity = %.2f", jma_intensity);
    }

    float battery_voltage = read_batt() / 1000.0;
    MYLOG("APP", "Battery voltage = %.2f V", battery_voltage);

    const int max_retries = 3;
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        MYLOG("NB", "Sending attempt %d of %d", attempt, max_retries);
        if (!RAK5860_connect()) {
            MYLOG("NB", "Connection failed");
            continue;
        }
		int csq = RAK5860_getRSSI();
		MYLOG("APP", "Raw CSQ = %d", csq);
		int rssi = convertCSQtoRSSI(csq);
		MYLOG("APP", "RSSI (dBm) = %d", rssi);

		char payload[256];
		snprintf(payload, sizeof(payload),
        "{\"device\":\"RAK4630\",\"si\":%.2f,\"pga\":%.2f,\"jma\":%.2f,"
        "\"shutoff\":%d,\"collapse\":%d,\"batt\":%.2f,\"rssi\":%d}",
        savedSI, savedPGA, jma_intensity, shutoff_alert, collapse_alert, battery_voltage, rssi);

        if (send_http_post(payload)) {
            MYLOG("NB", "Data sent: %s", payload);
			RAK5860_setConnected(false);
            return;
        }
        delay(1000);
    }
    MYLOG("NB", "All attempts failed. Will retry later.");
    send_pending = true;
}

void app_event_handler(void) {
    if ((g_task_event_type & SEISMIC_EVENT) == SEISMIC_EVENT) {
        g_task_event_type &= N_SEISMIC_EVENT;
        MYLOG("SEIS", "SEISMIC_EVENT triggered");

        uint8_t event_code = check_event_rak12027(false);
        MYLOG("SEIS", "check_event_rak12027 returned: %d", event_code);

        if (event_code == 4) {
            MYLOG("SEIS", "Earthquake started");
            read_rak12027(false);
        } else if (event_code == 5) {
            MYLOG("SEIS", "Earthquake ended");
            if (read_rak12027(true)) {
                MYLOG("SEIS", "Seismic data captured");
                send_earthquake_payload();
            }
        }
    }

    if ((g_task_event_type & STATUS) == STATUS) {
        g_task_event_type &= N_STATUS;
        MYLOG("APP", "Timer wakeup");

        if (send_pending) {
            MYLOG("NB", "Retrying failed earthquake transmission");
            send_earthquake_payload();
            send_pending = false;
        }
    }
}

void lora_data_handler(void) {
    // Required by WisBlock-API, not used in NB-IoT mode
}
