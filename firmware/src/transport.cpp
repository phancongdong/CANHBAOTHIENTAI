#include "drivers.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static String g_host;
static int g_port = 443;
static const char* g_ca_cert = nullptr;
static String g_token;

void initTransport(const char* host, int port, const char* ca_cert, const char* bearer_token) {
    g_host = host;
    g_port = port;
    g_ca_cert = ca_cert;
    g_token = bearer_token;
}

TransportResult postEvent(const TelemetryEvent& event) {
    if (WiFi.status() != WL_CONNECTED) {
        return TransportResult::RETRYABLE_FAILURE;
    }

    WiFiClientSecure client;
    // Strict TLS: Use Root CA, NEVER setInsecure()!
    if (g_ca_cert != nullptr) {
        client.setCACert(g_ca_cert);
    } else {
        // Without CA cert, fail immediately to prevent MITM
        return TransportResult::PERMANENT_FAILURE;
    }
    client.setTimeout(5000); // 5s network timeout

    HTTPClient https;
    String url = "https://" + g_host + ":" + String(g_port) + "/api/v1/telemetry";

    if (!https.begin(client, url)) {
        return TransportResult::RETRYABLE_FAILURE;
    }

    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + g_token);
    https.setTimeout(5000);

    std::string payload = serializeJson(event);
    int httpCode = https.POST((uint8_t*)payload.c_str(), payload.length());

    TransportResult res = TransportResult::RETRYABLE_FAILURE;

    if (httpCode > 0) {
        if (httpCode == 200) {
            String resp = https.getString();
            if (resp.indexOf("\"duplicate\":true") >= 0) {
                res = TransportResult::DUPLICATE_ACK;
            } else {
                res = TransportResult::SENT;
            }
        } else if (httpCode == 409) {
            // Conflict / hash mismatch on same sequence
            res = TransportResult::PERMANENT_FAILURE;
        } else if (httpCode == 401 || httpCode == 403 || httpCode == 422) {
            // Bad token or invalid schema -> do not retry endlessly
            res = TransportResult::PERMANENT_FAILURE;
        } else if (httpCode == 429 || httpCode >= 500) {
            // Rate limited or server error -> retryable
            res = TransportResult::RETRYABLE_FAILURE;
        }
    } else {
        // Connection error / timeout
        res = TransportResult::RETRYABLE_FAILURE;
    }

    https.end();
    return res;
}

#else
// Native mock transport
static TransportResult g_mock_result = TransportResult::SENT;

void initTransport(const char* host, int port, const char* ca_cert, const char* bearer_token) {}

void setMockTransportResult(TransportResult res) {
    g_mock_result = res;
}

TransportResult postEvent(const TelemetryEvent& event) {
    return g_mock_result;
}
#endif
