#include "drivers.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static String g_host = "103.178.235.184";
static int g_port = 80;
static String g_token = "secret-token-kitchen-01";

void initTransport(const char* host, int port, const char* bearer_token) {
    g_host = host;
    g_port = port;
    g_token = bearer_token;
}

TransportResult postEvent(const TelemetryEvent& event) {
    if (WiFi.status() != WL_CONNECTED) {
        return TransportResult::RETRYABLE_FAILURE;
    }

    HTTPClient http;
    String url = (g_port == 443 ? "https://" : "http://") + g_host + ":" + String(g_port) + "/api/v1/telemetry";

    bool begin_ok = false;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (g_port == 443) {
        secureClient.setTimeout(5000);
        begin_ok = http.begin(secureClient, url);
    } else {
        plainClient.setTimeout(5000);
        begin_ok = http.begin(plainClient, url);
    }

    if (!begin_ok) {
        return TransportResult::RETRYABLE_FAILURE;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + g_token);
    http.setTimeout(5000);

    std::string payload = serializeJson(event);
    int httpCode = http.POST((uint8_t*)payload.c_str(), payload.length());

    TransportResult res = TransportResult::RETRYABLE_FAILURE;

    if (httpCode > 0) {
        if (httpCode == 200) {
            String resp = http.getString();
            if (resp.indexOf("\"duplicate\":true") >= 0) {
                res = TransportResult::DUPLICATE_ACK;
            } else {
                res = TransportResult::SENT;
            }
        } else if (httpCode == 409) {
            res = TransportResult::PERMANENT_FAILURE;
        } else if (httpCode == 401 || httpCode == 403 || httpCode == 422) {
            res = TransportResult::PERMANENT_FAILURE;
        } else if (httpCode == 429 || httpCode >= 500) {
            res = TransportResult::RETRYABLE_FAILURE;
        }
    } else {
        res = TransportResult::RETRYABLE_FAILURE;
    }

    http.end();
    return res;
}

#else
// Native mock
void initTransport(const char* host, int port, const char* bearer_token) {}
TransportResult postEvent(const TelemetryEvent& event) {
    return TransportResult::SENT;
}
#endif
