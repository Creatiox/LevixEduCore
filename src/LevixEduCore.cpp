/**
 * SPDX-FileCopyrightText: 2023 Creatiox
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "LevixEduCore.h"

/* ---------------------------------------------------------------------------------------------- */
/*                                           PIN Public                                           */
/* ---------------------------------------------------------------------------------------------- */
LevixEduCore::LevixEduCore()
    : _uart_terminal(Serial) {
  // Guardar últimas causas de reinicio
  _last_reset_reason  = esp_reset_reason();
  _last_wakeup_reason = esp_sleep_get_wakeup_cause();
}

HardwareSerial& LevixEduCore::terminal() { return _uart_terminal; }

void LevixEduCore::powerRestart() {
  log_w("--- RESTARTING LEVIX ---");
  ESP.restart();
}

esp_reset_reason_t LevixEduCore::powerGetResetReason() { return _last_reset_reason; }
esp_sleep_wakeup_cause_t LevixEduCore::powerGetWakeupReason() { return _last_wakeup_reason; }

bool LevixEduCore::powerTimedDeepSleep(uint32_t seconds) {
  if (esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000 * 1000) != ESP_OK) {
    log_e("Failed to set timer, too many seconds");
    return false;
  }

  // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);

  log_w("Goodbye!");
  esp_deep_sleep_start();
  return true;
}

bool LevixEduCore::wifiEnable(const char* ssid, const char* password) {
  if (_wifi_enabled) {
    log_w("WiFi already connected");
    return true;
  }

  if (!WiFi.mode(wifi_mode_t::WIFI_MODE_STA)) {
    log_e("Failed to change to STA mode");
    return false;
  }

  _wifi_ssid = ssid;
  _wifi_pass = password;

  if (!xTimerStart(_wifi_timer_handle, 0)) {
    log_e("Failed to start WiFi timer");
    return false;
  }

  vTaskResume(_wifi_task_handle);

  _wifi_enabled = true;
  return true;
}

bool LevixEduCore::wifiSetStaticIP(IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns1,
                                   IPAddress dns2) {
  return WiFi.config(ip, gateway, mask, dns1, dns2);
}

bool LevixEduCore::wifiSetDynamicIP() {
  // Si este metodo recibe 0's, activa DHCP
  return WiFi.config((uint32_t)0, (uint32_t)0, (uint32_t)0);
}

bool LevixEduCore::wifiDisable() {
  if (!_wifi_enabled) {
    log_w("Already disconnected");
    return true;
  }

  if (!xTimerStop(_wifi_timer_handle, 0)) {
    log_e("Failed to stop WiFi timer");
    return false;
  }

  vTaskSuspend(_wifi_task_handle);

  if (!WiFi.disconnect()) {
    log_e("Failed to disconnect!");
    return false;
  }

  if (!WiFi.mode(wifi_mode_t::WIFI_MODE_NULL)) {
    log_e("Failed to change to NULL mode!");
    return false;
  }

  _wifi_enabled = false;
  return true;
}

void LevixEduCore::wifiOnConnected(LevixWiFiCallback connected_function) {
  _wifi_connected_cb = connected_function;
}

void LevixEduCore::wifiOnDisconnected(LevixWiFiCallback disconnected_function) {
  _wifi_disconnected_cb = disconnected_function;
}

bool LevixEduCore::isWifiConnected() { return _wifi_connected; }

/* ---------------------------------------------------------------------------------------------- */
/*                                          PIN Protected                                         */
/* ---------------------------------------------------------------------------------------------- */
void LevixEduCore::wifiInit() {
  // Configuración WiFi
  WiFi.onEvent(wifiCallback);
  WiFi.setAutoReconnect(false); // No reconectar automaticamente (manejado en wifiTask)

  _wifi_timer_handle = xTimerCreateStatic("WifiRc",                 // Nombre
                                          pdMS_TO_TICKS(10 * 1000), // Período
                                          pdTRUE,                   // Auto-recargar al expirar
                                          (void*)0,                 // ID (no utilizado)
                                          wifiTimer,                // Función callback
                                          &_wifi_timer_buffer);     // Datos internos

  if (_wifi_timer_handle == NULL)
    log_e("Failed to create WiFi timer");

  _wifi_task_handle = xTaskCreateStaticPinnedToCore(wifiTask,                // Función
                                                    "WifiRc",                // Nombre
                                                    _wifi_task_size,         // Tamaño stack
                                                    NULL,                    // Parámetros
                                                    1,                       // Prioridad
                                                    _wifi_task_stack,        // Buffer stack
                                                    &_wifi_task_static_task, // Buffer tarea
                                                    ARDUINO_RUNNING_CORE);   // Core

  if (_wifi_task_handle == NULL)
    log_e("Failed to create WiFi task");

  vTaskSuspend(_wifi_task_handle);
}

bool LevixEduCore::_wifi_enabled                      = false;
const char* LevixEduCore::_wifi_ssid                  = nullptr;
const char* LevixEduCore::_wifi_pass                  = nullptr;
LevixWiFiCallback LevixEduCore::_wifi_connected_cb    = nullptr;
LevixWiFiCallback LevixEduCore::_wifi_disconnected_cb = nullptr;
bool LevixEduCore::_wifi_connected                    = false;

void LevixEduCore::wifiCallback(arduino_event_id_t event) {
  // Eventos WiFi
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
    {
      log_i("WiFi started");
    } break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
    {
      log_w("WiFi stopped");
      _wifi_connected = false;
      if (_wifi_disconnected_cb != nullptr)
        _wifi_disconnected_cb();
    } break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    {
      log_i("WiFi connected");
    } break;

    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    {
      log_i("WiFi changed auth mode");
    } break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    {
      log_i("WiFi got IP");
      _wifi_connected = true;
      if (_wifi_connected_cb != nullptr)
        _wifi_connected_cb();
    } break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    {
      log_w("WiFi lost IP");
      _wifi_connected = false;
      if (_wifi_disconnected_cb != nullptr)
        _wifi_disconnected_cb();
    } break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    {
      log_w("WiFi disconnected");
      _wifi_connected = false;
      if (_wifi_disconnected_cb != nullptr)
        _wifi_disconnected_cb();
    } break;

    default: break;
  }
}

TimerHandle_t LevixEduCore::_wifi_timer_handle = nullptr;
StaticTimer_t LevixEduCore::_wifi_timer_buffer = {0};

void LevixEduCore::wifiTimer(void* pvParameters) {
  xTaskNotify(_wifi_task_handle, 0, eNotifyAction::eNoAction);
}

const uint16_t LevixEduCore::_wifi_task_size                = 3 * 1024;
TaskHandle_t LevixEduCore::_wifi_task_handle                = nullptr;
StackType_t LevixEduCore::_wifi_task_stack[_wifi_task_size] = {0};
StaticTask_t LevixEduCore::_wifi_task_static_task           = {0};

void LevixEduCore::wifiTask(void* pvParameters) {
  uint32_t dummy;

  for (;;) {
    // Bloquear indefinidamente hasta recibir notificación para reconectar
    xTaskNotifyWait(0, 0, &dummy, portMAX_DELAY);

    if (!_wifi_connected) {
      log_i("Connecting to %s...", _wifi_ssid);
      WiFi.begin(_wifi_ssid, _wifi_pass);
    }
  }
}