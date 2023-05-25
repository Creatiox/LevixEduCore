/**
 * SPDX-FileCopyrightText: 2023 Creatiox
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LEVIX_EDU_CORE_H
#define LEVIX_EDU_CORE_H

#include <Arduino.h>
#include <WiFi.h>

/// @brief Prototipo de función para métodos wifiOnConnected() y wifiOnDisconnected().
typedef void (*LevixWiFiCallback)();

class LevixEduCore {
  public:
  LevixEduCore();

  virtual void init() = 0;

  /**
   * @brief Obtener la referencia a la UART del monitor serie. Ej: levix.terminal().print().
   *
   * @return HardwareSerial& UART Terminal
   */
  HardwareSerial& terminal();

  /**
   * @brief Reiniciar Levix.
   */
  virtual void powerRestart();

  /**
   * @brief Obtener motivo del último reinicio.
   *
   * @return esp_reset_reason_t Motivo de reinicio
   */
  virtual esp_reset_reason_t powerGetResetReason();

  /**
   * @brief Obtener motivo de haber despertado del modo suspensión.
   *
   * @return esp_sleep_wakeup_cause_t Motivo despertar del modo suspensión
   */
  virtual esp_sleep_wakeup_cause_t powerGetWakeupReason();

  // TODO agregar opciones para despertar con gpios
  virtual bool powerTimedDeepSleep(uint32_t seconds);

  /**
   * @brief Habilitar WiFi. Cada 10s intenta conectarse a la red establecida. Solamente se puede
   * utilizar redes de 2.4GHz.
   *
   * @param ssid SSID de la red
   * @param password Contraseña de la red
   * @return true OK
   * @return false No se pudo habilitar el periférico o ya se encuentra encendido
   */
  virtual bool wifiEnable(const char* ssid, const char* password);

  /**
   * @brief Establecer una IP estática para el WiFi. Permite cambiar la configuración en cualquier
   * momento.
   *
   * @param ip IP local
   * @param gateway IP gateway
   * @param mask Máscara de red
   * @param dns1 [Opcional] DNS 1
   * @param dns2 [Opcional] DNS 2
   * @return true OK
   * @return false No se pudo cambiar la configuración
   */
  virtual bool wifiSetStaticIP(IPAddress ip, IPAddress gateway, IPAddress mask,
                               IPAddress dns1 = (uint32_t)0, IPAddress dns2 = (uint32_t)0);

  /**
   * @brief Establecer IP dinámica para el WiFi. Busca una IP disponible
   * automáticamente.
   *
   * @return true OK
   * @return false No se pudo cambiar la configuración
   */
  virtual bool wifiSetDynamicIP();

  /**
   * @brief Deshabilitar WiFi. Se desconecta de la red y apaga el periférico.
   *
   * @return true OK
   * @return false No se pudo apagar el periférico o ya se encuentra
   * apagado
   */
  virtual bool wifiDisable();

  /**
   * @brief Establecer función externa para ejecutarla cuando Levix se conecte a una red WiFi.
   *
   * @param connected_function Función
   */
  virtual void wifiOnConnected(LevixWiFiCallback connected_function);

  /**
   * @brief Establecer función externa para ejecutarla cuando Levix se desconecte de una red WiFi.
   *
   * @param connected_function Función
   */
  virtual void wifiOnDisconnected(LevixWiFiCallback disconnected_function);

  /**
   * @brief Consultar si el WiFi se encuentra conectado a la red.
   *
   * @return true Conectado
   * @return false Desconectado
   */
  virtual bool isWifiConnected();

  protected:
  HardwareSerial& _uart_terminal;
  esp_reset_reason_t _last_reset_reason;
  esp_sleep_wakeup_cause_t _last_wakeup_reason;

  virtual void wifiInit();
  static bool _wifi_enabled;
  static const char* _wifi_ssid;
  static const char* _wifi_pass;
  static LevixWiFiCallback _wifi_connected_cb;
  static LevixWiFiCallback _wifi_disconnected_cb;
  static bool _wifi_connected;

  static void wifiCallback(arduino_event_id_t event);

  static TimerHandle_t _wifi_timer_handle;
  static StaticTimer_t _wifi_timer_buffer;
  static void wifiTimer(void* pvParameters);

  static const uint16_t _wifi_task_size;
  static TaskHandle_t _wifi_task_handle;
  static StackType_t _wifi_task_stack[];
  static StaticTask_t _wifi_task_static_task;
  static void wifiTask(void* pvParameters);
};

#endif // LEVIX_EDU_CORE_H