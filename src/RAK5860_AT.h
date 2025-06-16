#ifndef RAK5860_AT_H
#define RAK5860_AT_H

#include <Arduino.h>

#define RAK5860_APN "NBIOT"

bool RAK5860_init();
bool RAK5860_connect();
bool RAK5860_isConnected();
bool send_http_post(const char *payload);
bool RAK5860_receive(char *buffer, size_t len);
int RAK5860_getRSSI();
int convertCSQtoRSSI(int csq);
void RAK5860_check_and_attach();
void log_response(const char* tag, unsigned long timeout = 2000);
extern HardwareSerial& BG77;
void RAK5860_setConnected(bool state);

#endif
