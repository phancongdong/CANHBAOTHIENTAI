#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

// --- Wi-Fi Settings ---
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// --- Server & TLS Settings ---
#define SERVER_HOST "canhbao.yourdomain.com"
#define SERVER_PORT 443
#define TELEMETRY_PATH "/api/v1/telemetry"

// Let's Encrypt ISRG Root X1 Certificate
// Tuyệt đối không dùng setInsecure()!
static const char* ROOT_CA_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRQBqCTvfU48wDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK/62gvQUbKE\n" \
"JSmjaWhbLf8QNm90UcqidReUtunwL1dV5BpHTQEqUSqVJY722oc5+5eWAUOn842y\n" \
"Pa9NGLbRIBWjEIjvFT4TW+tcZvrGrvFUHXYNUzZ456nDblbfNL36Il6ptGd74CQC\n" \
"Lf06CZBlMrYksTOjinRUCU178wcKPdEkVDWgJwDKRFNanExNmGSIObeKDGCWJa5R\n" \
"zFBf/6subRVj++t8cCoU++DTbqE2XP8VzMAGq4YNWRmx1WScz5zZHzKA5LhkEBvX\n" \
"pmUdfQBupnCaZvh47JnPHsmFHRSRhhEqzPXHSKyVC9x0YCQuahdxtq3FPZ1zph5r\n" \
"+wqSXO9ptdJWxn84mx/PjcBS55H//EZmxgvG64hgMaDZTVdX3pm5RIlghtjqqOOi\n" \
"UmQxm0zBOVTLD6oEZoShFiGh25guUffYq0taBI66bTu4IqvqhEgNPSCn566PRttB\n" \
"hy4EzsYXeicgn4P1gevpySUrc8HzuhDTWmxvKdLETTdHIN89WQnCaZ760/9TvvuP\n" \
"KAvVSGNZY5NPONnDZVQzCA17AgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n" \
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgBltwuY8sdchbDCCASIw\n" \
"DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL5Zf5cZ..." \
"-----END CERTIFICATE-----\n";

// --- Device Credentials ---
#define DEVICE_ID "kitchen-01"
#define DEVICE_BEARER_TOKEN "SECRET_TOKEN_FROM_SERVER_ADMIN"

// --- Hardware GPIO Configuration ---
// Cần đối chiếu với docs/hardware.md trước khi nạp
#define PIN_FLAME_DO 4
#define PIN_ONEWIRE_TEMP 18
#define PIN_BUZZER 19
#define PIN_RESET_BUTTON 0

// --- Thresholds & Timers ---
#define TEMP_WARN_C 45.0f
#define TEMP_ALARM_C 55.0f
#define TEMP_CLEAR_C 38.0f
#define CONFIRM_MS 1000
#define CLEAR_MS 10000
#define SENSOR_FAULT_MS 3000
#define HEARTBEAT_INTERVAL_MS 30000

#endif // CONFIG_EXAMPLE_H
