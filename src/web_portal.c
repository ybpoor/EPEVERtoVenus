#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "wifi_manager.h"
#include "epever_data.h"
#include "web_portal.h"

static const char *TAG = "web_portal";
static httpd_handle_t s_http_server = NULL;
static TaskHandle_t s_dns_task_handle = NULL;
static bool s_is_running = false;
extern app_config_t g_config;

/* =========================================================================
 * 1. Embedded Captive Portal HTML / CSS / JS UI
 * ========================================================================= */
static const char INDEX_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>EPEVER to Venus OS Gateway</title>"
"<style>"
":root{--bg:#0f172a;--card:#1e293b;--text:#f8fafc;--primary:#0284c7;--border:#334155;--accent:#38bdf8}"
"body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,sans-serif;margin:0;padding:20px;display:flex;justify-content:center}"
".box{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:24px;max-width:480px;width:100%;box-shadow:0 10px 25px rgba(0,0,0,0.4)}"
"h1{font-size:1.4rem;margin-top:0;color:var(--accent);display:flex;align-items:center;gap:8px}"
"label{font-size:0.85rem;font-weight:600;margin-top:14px;display:block;color:#94a3b8}"
"input,select{width:100%;padding:10px;margin-top:4px;border:1px solid var(--border);border-radius:6px;background:#0f172a;color:#fff;box-sizing:border-box;font-size:0.95rem}"
"input:focus,select:focus{border-color:var(--primary);outline:none}"
"button{width:100%;padding:12px;margin-top:20px;background:var(--primary);color:#fff;border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;transition:background .2s}"
"button:hover{background:#0369a1}"
".scan-btn{background:#334155;margin-top:6px;padding:8px;font-size:0.85rem}"
".hint{font-size:0.75rem;color:#64748b;margin-top:4px}"
"#status{margin-top:14px;padding:10px;border-radius:6px;font-size:0.85rem;display:none}"
".success{background:#065f46;color:#34d399}"
".err{background:#7f1d1d;color:#f87171}"
"</style></head><body>"
"<div class='box'>"
"<h1>☀️ EPEVER &rarr; Venus OS</h1>"
"<p style='font-size:0.85rem;color:#94a3b8'>ESP32-C3 SuperMini Gateway Configuration</p>"
"<form id='cfgForm'>"
"<label>Wi-Fi Network (SSID)</label>"
"<div style='display:flex;gap:6px'>"
"<input type='text' id='ssid' name='ssid' required placeholder='Enter or scan SSID'>"
"<button type='button' class='scan-btn' style='width:auto' onclick='scanWifi()'>🔍 Scan</button>"
"</div>"
"<select id='ssidList' style='display:none;margin-top:6px' onchange='pickSsid()'></select>"
"<label>Wi-Fi Password</label>"
"<input type='password' id='pass' name='pass' placeholder='WPA2 Passphrase'>"
"<hr style='border-color:var(--border);margin-top:20px'>"
"<label>Venus OS MQTT Broker Host / IP</label>"
"<input type='text' id='mqtt_host' name='mqtt_host' required value='venus.local' placeholder='e.g. 192.168.1.100 or venus.local'>"
"<div style='display:flex;gap:12px'>"
"<div style='flex:1'><label>MQTT Port</label><input type='number' id='mqtt_port' name='mqtt_port' value='1883'></div>"
"<div style='flex:2'><label>Base Topic</label><input type='text' id='mqtt_topic' name='mqtt_topic' value='solar_charger'></div>"
"</div>"
"<div class='hint'>Each unit publishes to topic: {base}/{index} (e.g. solar_charger/0)</div>"
"<div style='display:flex;gap:12px'>"
"<div style='flex:1'><label>MQTT User (opt)</label><input type='text' id='mqtt_user' name='mqtt_user'></div>"
"<div style='flex:1'><label>MQTT Pass (opt)</label><input type='password' id='mqtt_pass' name='mqtt_pass'></div>"
"</div>"
"<hr style='border-color:var(--border);margin-top:20px'>"
"<label>Active EPEVER Modbus Slave IDs</label>"
"<input type='text' id='slaves' name='slaves' required value='1' placeholder='e.g. 1, 2, 3'>"
"<div class='hint'>Comma-separated slave IDs for cascaded multi-unit RS485 bus.</div>"
"<label>Polling Interval (ms)</label>"
"<input type='number' id='interval' name='interval' value='2000' min='500' max='10000'>"
"<button type='submit' id='subBtn'>Save Settings & Connect</button>"
"</form>"
"<div id='status'></div>"
"</div>"
"<script>"
"function scanWifi(){"
"  const btn=event.target;btn.innerText='Scanning...';"
"  fetch('/api/scan').then(r=>r.json()).then(d=>{"
"    btn.innerText='🔍 Scan';"
"    const sel=document.getElementById('ssidList');sel.innerHTML='<option value=\"\">-- Select Found Network --</option>';"
"    d.forEach(n=>{sel.innerHTML+=`<option value='${n.ssid}'>${n.ssid} (${n.rssi} dBm)</option>`});"
"    sel.style.display='block';"
"  }).catch(e=>{btn.innerText='Retry';alert('Scan failed');});"
"}"
"function pickSsid(){"
"  const sel=document.getElementById('ssidList');"
"  if(sel.value) document.getElementById('ssid').value=sel.value;"
"}"
"document.getElementById('cfgForm').onsubmit=function(e){"
"  e.preventDefault();"
"  const btn=document.getElementById('subBtn');btn.disabled=true;btn.innerText='Saving...';"
"  const st=document.getElementById('status');st.style.display='none';"
"  const payload={"
"    ssid:document.getElementById('ssid').value,"
"    pass:document.getElementById('pass').value,"
"    mqtt_host:document.getElementById('mqtt_host').value,"
"    mqtt_port:parseInt(document.getElementById('mqtt_port').value)||1883,"
"    mqtt_topic:document.getElementById('mqtt_topic').value||'solar_charger',"
"    mqtt_user:document.getElementById('mqtt_user').value,"
"    mqtt_pass:document.getElementById('mqtt_pass').value,"
"    slaves:document.getElementById('slaves').value,"
"    interval:parseInt(document.getElementById('interval').value)||2000"
"  };"
"  fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)})"
"  .then(r=>r.text()).then(t=>{"
"    st.className='success';st.innerText='Settings saved! Rebooting gateway into Station mode...';st.style.display='block';"
"  }).catch(err=>{"
"    btn.disabled=false;btn.innerText='Save Settings & Connect';"
"    st.className='err';st.innerText='Error saving settings: '+err;st.style.display='block';"
"  });"
"};"
"</script></body></html>";

/* =========================================================================
 * 2. HTTP Handlers
 * ========================================================================= */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

// Redirect captive portal checks (iOS, Android, Windows) to root
static esp_err_t captive_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t api_scan_handler(httpd_req_t *req)
{
    wifi_ap_record_t ap_records[16];
    uint16_t ap_count = 16;
    esp_err_t err = wifi_manager_scan(ap_records, &ap_count);

    cJSON *root = cJSON_CreateArray();
    if (err == ESP_OK) {
        for (int i = 0; i < ap_count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "ssid", (const char *)ap_records[i].ssid);
            cJSON_AddNumberToObject(item, "rssi", ap_records[i].rssi);
            cJSON_AddItemToArray(root, item);
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str ? json_str : "[]", HTTPD_RESP_USE_STRLEN);
    if (json_str) free(json_str);
    return ret;
}

static void delayed_restart_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "Rebooting into normal operation...");
    esp_restart();
}

static esp_err_t api_save_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return httpd_resp_send_500(req);
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    app_config_t new_cfg;
    app_config_set_defaults(&new_cfg);

    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "ssid")) && item->valuestring) {
        strncpy(new_cfg.wifi_ssid, item->valuestring, sizeof(new_cfg.wifi_ssid) - 1);
    }
    if ((item = cJSON_GetObjectItem(json, "pass")) && item->valuestring) {
        strncpy(new_cfg.wifi_pass, item->valuestring, sizeof(new_cfg.wifi_pass) - 1);
    }
    if ((item = cJSON_GetObjectItem(json, "mqtt_host")) && item->valuestring) {
        strncpy(new_cfg.mqtt_host, item->valuestring, sizeof(new_cfg.mqtt_host) - 1);
    }
    if ((item = cJSON_GetObjectItem(json, "mqtt_port"))) {
        new_cfg.mqtt_port = (uint16_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "mqtt_topic")) && item->valuestring) {
        strncpy(new_cfg.mqtt_base_topic, item->valuestring, sizeof(new_cfg.mqtt_base_topic) - 1);
    }
    if ((item = cJSON_GetObjectItem(json, "mqtt_user")) && item->valuestring) {
        strncpy(new_cfg.mqtt_user, item->valuestring, sizeof(new_cfg.mqtt_user) - 1);
    }
    if ((item = cJSON_GetObjectItem(json, "mqtt_pass")) && item->valuestring) {
        strncpy(new_cfg.mqtt_pass, item->valuestring, sizeof(new_cfg.mqtt_pass) - 1);
    }
    if ((item = cJSON_GetObjectItem(json, "interval"))) {
        new_cfg.poll_interval_ms = (uint16_t)item->valueint;
    }

    // Parse slave IDs string e.g. "1, 2, 3"
    if ((item = cJSON_GetObjectItem(json, "slaves")) && item->valuestring) {
        char slaves_buf[64];
        strncpy(slaves_buf, item->valuestring, sizeof(slaves_buf) - 1);
        new_cfg.num_units = 0;
        char *token = strtok(slaves_buf, ", ");
        while (token && new_cfg.num_units < MAX_EPEVER_UNITS) {
            int id = atoi(token);
            if (id > 0 && id <= 247) {
                new_cfg.slave_ids[new_cfg.num_units++] = (uint8_t)id;
            }
            token = strtok(NULL, ", ");
        }
    }

    if (new_cfg.num_units == 0) {
        new_cfg.slave_ids[0] = 1;
        new_cfg.num_units = 1;
    }

    new_cfg.is_configured = (strlen(new_cfg.wifi_ssid) > 0);
    cJSON_Delete(json);

    esp_err_t err = app_config_save(&new_cfg);
    if (err == ESP_OK) {
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        // Spawn background task to restart after brief delay
        xTaskCreate(delayed_restart_task, "restart_task", 2048, NULL, 5, NULL);
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS Save Failed");
        return ESP_FAIL;
    }
}

/* =========================================================================
 * 3. DNS Captive Portal Server (Port 53 UDP)
 * ========================================================================= */
static void dns_server_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    uint8_t tx_buffer[128];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create DNS socket");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ESP_LOGE(TAG, "DNS socket bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS Captive Portal responder listening on port 53");

    while (s_is_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&client_addr, &addr_len);

        if (len > 12) {
            // Echo header with Response flags
            memcpy(tx_buffer, rx_buffer, len);
            tx_buffer[2] = 0x81; // Standard response
            tx_buffer[3] = 0x80;
            tx_buffer[7] = 1;    // 1 Answer

            // Append Answer: Type A, Class IN, TTL 60, IP 192.168.4.1
            int idx = len;
            tx_buffer[idx++] = 0xc0;
            tx_buffer[idx++] = 0x0c; // Pointer to question name
            tx_buffer[idx++] = 0x00; tx_buffer[idx++] = 0x01; // Type A
            tx_buffer[idx++] = 0x00; tx_buffer[idx++] = 0x01; // Class IN
            tx_buffer[idx++] = 0x00; tx_buffer[idx++] = 0x00; tx_buffer[idx++] = 0x00; tx_buffer[idx++] = 0x3c; // TTL 60
            tx_buffer[idx++] = 0x00; tx_buffer[idx++] = 0x04; // Data length 4
            tx_buffer[idx++] = 192;  tx_buffer[idx++] = 168;  tx_buffer[idx++] = 4;   tx_buffer[idx++] = 1;  // IP

            sendto(sock, tx_buffer, idx, 0, (struct sockaddr *)&client_addr, addr_len);
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

/* =========================================================================
 * 4. Web Portal Start / Stop
 * ========================================================================= */
esp_err_t web_portal_start(void)
{
    if (s_is_running) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;

    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server (%s)", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
    httpd_register_uri_handler(s_http_server, &uri_root);

    httpd_uri_t uri_scan = { .uri = "/api/scan", .method = HTTP_GET, .handler = api_scan_handler };
    httpd_register_uri_handler(s_http_server, &uri_scan);

    httpd_uri_t uri_save = { .uri = "/api/save", .method = HTTP_POST, .handler = api_save_handler };
    httpd_register_uri_handler(s_http_server, &uri_save);

    // Captive portal detection endpoints
    const char *redirect_paths[] = {
        "/generate_204", "/hotspot-detect.html", "/ncsi.txt", "/connecttest.txt"
    };
    for (int i = 0; i < 4; i++) {
        httpd_uri_t redir = { .uri = redirect_paths[i], .method = HTTP_GET, .handler = captive_redirect_handler };
        httpd_register_uri_handler(s_http_server, &redir);
    }

    s_is_running = true;
    xTaskCreate(dns_server_task, "dns_task", 3072, NULL, 5, &s_dns_task_handle);

    ESP_LOGI(TAG, "Web Portal & Captive Portal successfully started");
    return ESP_OK;
}

void web_portal_stop(void)
{
    if (!s_is_running) return;
    s_is_running = false;
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }
}

bool web_portal_is_running(void)
{
    return s_is_running;
}
