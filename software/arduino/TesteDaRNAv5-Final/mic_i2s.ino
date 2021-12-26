
#define I2S_WS 33
#define I2S_SD 35
#define I2S_SCK 34

void i2sInstall();
void i2sSet();

void i2sInstall(){

  esp_err_t err;

  Serial.println("Configurando o I2S...");
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = BUF_COUNT, 
    .dma_buf_len = 1024,  
    .use_apll = 1
  };

  Serial.println("Instalando drivers do I2S...");
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  
  if (err != ESP_OK) {
    Serial.printf("Falha na instalacao do driver I2S: %d\n", err);
    while (true);
  }
  Serial.println("Driver instalado com sucesso!");

}

void i2sSet(){

  esp_err_t err;

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  Serial.println("Configurando pinos do I2S...");
  err = i2s_set_pin(I2S_PORT, &pin_config);
  
  if (err != ESP_OK) {
    Serial.printf("Falha na configuracao dos pinos I2S: %d\n", err);
    while (true);
  }
  Serial.println("Pinos configurados com sucesso!");
}

