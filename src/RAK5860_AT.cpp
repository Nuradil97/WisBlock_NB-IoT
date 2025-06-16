// RAK5860 NB-IoT modem driver (refactored)
#include "RAK5860_AT.h"
#include "app.h"

HardwareSerial& BG77 = Serial1;

static bool connected = false;
static bool in_reset = false;
int last_rssi = -1;

int RAK5860_getRSSI() {
    BG77.println("AT+CSQ");
    delay(200);
    unsigned long timeout = millis() + 2000;
    char line[64];
    size_t idx = 0;

    while (millis() < timeout) {
        if (BG77.available()) {
            char c = BG77.read();
            if (c == '\n' || c == '\r') {
                if (idx > 0) {
                    line[idx] = '\0';
                    if (strstr(line, "+CSQ:")) {
                        int rssi = 0;
                        if (sscanf(line, "+CSQ: %d", &rssi) == 1) {
                            last_rssi = rssi;
                            return rssi;
                        }
                    }
                    idx = 0;
                }
            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }
    return -1; // unknown
}

int convertCSQtoRSSI(int csq) {
    if (csq == 99) return -999; // not known or not detectable
    return -113 + 2 * csq;
}

bool wait_for_connect(const char* tag, unsigned long timeout = 10000) {
    char buf[128];
    size_t idx = 0;
    unsigned long start = millis();

    while (millis() - start < timeout) {
        while (BG77.available()) {
            char c = BG77.read();
            Serial.print(c);
            if (c == '\r' || c == '\n') {
                if (idx > 0) {
                    buf[idx] = '\0';
                    MYLOG(tag, "%s", buf);
                    if (strstr(buf, "CONNECT")) return true;
                    idx = 0;
                }
            } else if (idx < sizeof(buf) - 1) {
                buf[idx++] = c;
            }
        }
    }
    return false;
}

bool wait_for_ok(unsigned long timeout = 3000) {
    char line[64] = {0};
    size_t idx = 0;
    unsigned long deadline = millis() + timeout;

    while (millis() < deadline) {
        while (BG77.available()) {
            char c = BG77.read();
            Serial.print(c);  // Optional debug output

            if (c == '\r' || c == '\n') {
                if (idx > 0) {
                    line[idx] = '\0';
                    MYLOG("NB", "%s", line);
                    if (strstr(line, "OK")) return true;
                    idx = 0;
                }
            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }
    return false; // timeout
}


void log_response(const char* tag, unsigned long timeout) {
    unsigned long last_received = millis();
    char line[128];
    size_t idx = 0;

    while (millis() - last_received < timeout) {
        while (BG77.available()) {
            char c = BG77.read();
            Serial.print(c);
            if (c == '\r' || c == '\n') {
                if (idx > 0) {
                    line[idx] = '\0';
                    if (strstr(line, "RDY") && !in_reset) {
                        in_reset = true;
                        MYLOG(tag, "BG77 RDY detected – reinitializing modem once");
                        delay(1000);
                        RAK5860_init();
                        delay(500);
                        RAK5860_connect();
                        in_reset = false;
                        return;
                    }
                    MYLOG(tag, "%s", line);
                    idx = 0;
                }
            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
            last_received = millis();
        }
    }
}

bool RAK5860_init() {
    static bool modem_initialized = false;
    if (modem_initialized) return true;

    pinMode(WB_IO1, OUTPUT);
    digitalWrite(WB_IO1, HIGH);
    delay(200);
    digitalWrite(WB_IO1, LOW);

    pinMode(WB_IO2, OUTPUT);
    digitalWrite(WB_IO2, HIGH);

    delay(500);
    BG77.begin(115200);
    delay(2000);
    BG77.println("AT");
    log_response("NB", 3000);

    modem_initialized = true;
    return true;
}

bool RAK5860_connect() {
    BG77.println("AT+CFUN=0");
    if (!wait_for_ok()) {
    MYLOG("NB", "Failed to set CFUN=0");
    return false;
    }

    BG77.println("AT+CFUN=1");
    if (!wait_for_ok()) {
    MYLOG("NB", "Failed to set CFUN=1");
    return false;
    }

    delay(8000);

    
    BG77.println("AT+CPIN?");
    unsigned long simTimeout = millis() + 3000;
    char simResp[64];
    size_t simIdx = 0;
    bool simReady = false;

    while (millis() < simTimeout) {
        if (BG77.available()) {
            char c = BG77.read();
            if (c == '\n' || c == '\r') {
                if (simIdx > 0) {
                    simResp[simIdx] = '\0';
                    MYLOG("NB", "%s", simResp);
                    if (strstr(simResp, "READY")) {
                        simReady = true;
                        break;
                    }
                    simIdx = 0;
                }
            } else if (simIdx < sizeof(simResp) - 1) {
                simResp[simIdx++] = c;
            }
        }
    }

    if (!simReady) {
        MYLOG("NB", "SIM not ready");
        return false;
    }

    
    BG77.print("AT+CGDCONT=1,\"IP\",\"");
    BG77.print(RAK5860_APN);
    BG77.println("\"");
    if (!wait_for_ok()) {
    MYLOG("NB", "Failed to set CGDCONT");
    return false;
    }

    BG77.println("AT+COPS=0");
    if (!wait_for_ok()) {
    MYLOG("NB", "Failed to set COPS=0");
    return false;
    }

    BG77.println("AT+CGATT=1");
    if (!wait_for_ok()) {
    MYLOG("NB", "Failed to set CGATT=1");
    return false;
    }

    BG77.println("AT+CGATT?");
    unsigned long attachTimeout = millis() + 10000;
    char line[64];
    int attached = 0;
    size_t idx = 0;

    while (millis() < attachTimeout && !attached) {
        if (BG77.available()) {
            char c = BG77.read();
            if (c == '\r' || c == '\n') {
                if (idx > 0) {
                    line[idx] = '\0';
                    if (strstr(line, "+CGATT: 1")) {
                        attached = 1;
                    }
                    idx = 0;
                }
            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }

    connected = attached;

    return attached;
}


bool RAK5860_isConnected() {
    return connected;
}

bool send_http_post(const char *payload) {
    BG77.println("AT+QHTTPURL=84,30");
    if (!wait_for_connect("QHTTPURL", 5000)) {
        MYLOG("NB", "QHTTPURL CONNECT not received – aborting URL input");
        return false;
    }

    BG77.println("https://iot.beeline.kz/api/v1/integrations/http/ee2b3f5d-15ed-bc9b-7aed-9065f77e87cc");
    delay(2000);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+QHTTPPOST=%d,60,60", strlen(payload));
    BG77.println(cmd);
    if (!wait_for_connect("QHTTPPOST", 20000)) {
        MYLOG("NB", "CONNECT not received – aborting POST");
        return false;
    }

    BG77.print(payload);
    delay(500);
    log_response("NB", 10000);

    BG77.println("AT+CGATT=0");
    if (!wait_for_ok()) {
    MYLOG("NB", "Failed to set CGATT=1");
    return false;
    }

    return true;
}

bool RAK5860_receive(char *buffer, size_t len) {
    size_t idx = 0;
    unsigned long timeout = millis() + 5000;
    while (millis() < timeout && idx < len - 1) {
        if (BG77.available()) {
            buffer[idx++] = BG77.read();
        }
    }
    buffer[idx] = '\0';
    return idx > 0;
}

void RAK5860_setConnected(bool state) {
    connected = state;
}
