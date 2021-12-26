void wifiInit();
void deinitialize_wifi();
void wifi_connect();
esp_err_t event_handler(void *ctx, system_event_t *event);


esp_err_t event_handler(void *ctx, system_event_t *event)
{
  if (event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
    printf("Endereco IP " IPSTR "\n",
    IP2STR(&event->event_info.got_ip.ip_info.ip));
    printf("Connectado com sucesso...\n");
  }
 
  if (event->event_id == SYSTEM_EVENT_STA_START) {
    ESP_ERROR_CHECK(esp_wifi_connect());
  }
  
  return ESP_OK;
}


void wifiInit(){
  nvs_flash_init();
  tcpip_adapter_init();
  esp_event_loop_init(event_handler, NULL);
  wifi_init_config_t confi = WIFI_INIT_CONFIG_DEFAULT();
  wifi_mode_t mod = WIFI_MODE_STA;
  wifi_config_t wifi_config = {
        .sta = {
            {.ssid = "wifi"},
            {.password = "password123"}
        },
    };

  if( esp_wifi_init(&confi) != ESP_OK )
    Serial.println("Falha ao inicializar o Wifi");
  if( esp_wifi_set_mode(mod) != ESP_OK )
    Serial.println("Falha ao configurar o modo do WiFi");
  if( esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_config) != ESP_OK )
    Serial.println("Falha a configura o WiFi");
  if( esp_wifi_start() != ESP_OK )
    Serial.println("Falha ao iniciar o WiFi");
  else
    Serial.println("WiFi iniciado com sucesso!");
}

void wifi_connect(){
  if( esp_wifi_connect() != ESP_OK )
    Serial.println("Falha ao conectar o WiFi!");
  else
    Serial.println("WiFi Conectado!");
}

void deinitialize_wifi() {
    esp_wifi_stop();
    esp_wifi_deinit();
    Serial.println("Wifi desativado!");
}
