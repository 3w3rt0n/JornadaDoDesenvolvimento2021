/*===========================================================*/
/*                 SmartHome Franzininho                     */
/*===========================================================*/
#define MODO 1 // 0 = Coleta, 1 = Predição

#include "JQ6500_serial.h";

#include <driver/i2s.h>
#include <SPIFFS.h>

//MIC_I2S
extern void i2sInstall();
extern void i2sSet();

#if MODO == 0
//HTTP_SEND
extern void uploadFile();

//WIFI_INIT
extern void wifiInit();
extern void deinitialize_wifi();
void wifi_connect();

//SPIFFS
extern void listSPIFFS();
#endif

#include <esp_wifi.h> //ESP-IDF
#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <tcpip_adapter.h> //ESP-IDF

esp_err_t event_handler(void *ctx, system_event_t *event);

#include <HTTPClient.h>
#include <Ticker.h>
#include <arduinoFFT.h>

//Botoes
#define btn1 5
#define btn2 6

//Rele
#define rele 2

/* ================================================================ */
/*                            FFT                                   */
/* ================================================================ */
arduinoFFT FFT = arduinoFFT();

#define BAND_SIZE 16
#define BLOCK_SIZE 512
#define WINDOW 144 //32


#define amplitudeBaixa 50 //200 default
#define amplitudeMedia 50 //200 default
#define amplitudeAlta  50 //200 default

double vReal[BLOCK_SIZE];
double vImag[BLOCK_SIZE];
int vTemp[BLOCK_SIZE];

int bands[BAND_SIZE];

int mediaalta = 0;
int mediabaixa = 0;

/* ================================================================ */
/*                         NeoPixel                                 */
/* ================================================================ */
#include <Adafruit_NeoPixel.h>

//LED RGB da placa (NeoPixel)
#define PIXELPIN  18
#define NUMPIXELS 1

Adafruit_NeoPixel pixel(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);


/* ================================================================ */
/*                            I2S                                   */
/* ================================================================ */
#define BUF_COUNT         (16) //64 original - problema no wifi

#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (8 * 1024) //16*1024

char i2s_read_buff[I2S_READ_LEN];
int8_t flash_write_buff[I2S_READ_LEN];
size_t bytes_read;

/* ================================================================ */
/*                         TinyML                                   */
/* ================================================================ */
#if MODO == 1
#include <EloquentTinyML.h>
#include "model.h"

float X[784] = {0.0};
float y_pred[4] = {0.0};
String classes[4] = {"Ligar","Desligar","Outros","Franzininho"};


#define NUMBER_OF_INPUTS 49*16
#define NUMBER_OF_OUTPUTS 4
#define TENSOR_ARENA_SIZE 48*1024  //1024

Eloquent::TinyML::TfLite<NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, TENSOR_ARENA_SIZE> ml;
#endif

/* ================================================================ */
/*                           FILE                                   */
/* ================================================================ */
#if MODO == 0
File file;
const char filenameRAW[] = "/recording.raw";
const char filenameFFT[] = "/recording.csv";
#endif

/* ================================================================ */
/*                         JQ6500                                   */
/* ================================================================ */
#if MODO == 1

//JQ6500

#define RX0 44
#define TX0 43

HardwareSerial serial(0);

JQ6500_Serial mp3(0); //TX, RX
#endif

/* ================================================================ */
/*                           SETUP                                  */
/* ================================================================ */
void setup() {

  //Definição dos pinos
  pinMode(btn1, INPUT);
  pinMode(btn2, INPUT);

  pinMode(rele, OUTPUT);

  // Desligar rele por padrao
  digitalWrite(rele, LOW);

  // Inicialização do LED NEOPIXEL
  pixel.begin(); 
  pixel.clear(); 
  pixel.show();
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  while(!Serial) {};
  delay(500);

  #if MODO == 1
  // ======= Inicializar do RN ======= //  
  Serial.println("\n===== Carregar Modelo =====");    
  ml.begin(model);
  Serial.print(" Modelo carregado! [");
  Serial.print(sizeof(model)/1024);
  Serial.println("KB] ");
  Serial.println("===========================");  

  // ======= JQ6500 ======= //  
  Serial.println("\n=== Inicializando JQ6500 ===");    
  serial.begin(9600,SERIAL_8N1,RX0,TX0);  
  mp3.begin(9600);
  mp3.reset();
  delay(100);
  mp3.setLoopMode(MP3_LOOP_NONE);
  mp3.setVolume(29);
  mp3.playFileByIndexNumber(1);
  Serial.println(" JQ6500, Ativo");
  Serial.println("============================");  
  #endif
    
  
  // ======= Inicializar o I2S ========= //
  Serial.println("\n============ I2S ==============");
  i2sInstall();
  delay(100);
  i2sSet();
  delay(100);  
  Serial.println("===============================");

  
  #if MODO == 0
  // ======= SPIFFS ======= //
  Serial.println("\n======== SPIFFS =========");
  if(!SPIFFS.begin(true)){
    Serial.println("Falha na inicializacao do SPIFFS!");
    while(1) yield();
  }  
  Serial.println("Inicializado, ok!");
  Serial.println("=========================");

  //Format SPIFFS
  /*
  bool formatted = SPIFFS.format();
  if(formatted){
    Serial.println("\n\nSuccess formatting");
  }else{
    Serial.println("\n\nError formatting");
  }*/
  #endif

  //Limpar buffer
  i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
  i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
    
}

void loop() {   

  //Ligar ou desligar o rele pelos botoes
  if(digitalRead(btn1) || digitalRead(btn2)){
    digitalWrite(rele, !digitalRead(rele));
  };

  bytes_read = 0;
  
  i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);

  uint32_t dac_value = 0;
  
  int tempalta = mediaalta;
  int tempbaixa = mediabaixa;
  int calta = 0;
  int cbaixa = 0;
  
  uint32_t j = 0;

  for (int i = 0; i < bytes_read; i+=2) {
      dac_value = ((((uint16_t) (i2s_read_buff[i + 1] & 0xf) << 8) | ((i2s_read_buff[i + 0]))));
      int left = (dac_value * 256) / 2048;
      
      if(left > 256){     
        flash_write_buff[j++] = (left - 512);     
        mediabaixa += (left - 512);
        cbaixa++;
      }else{      
        flash_write_buff[j++] = left;   
        mediaalta += left;
        calta++;   
      }     

      
      //Serial.print("Valor l: ");
      //Serial.print(left);     
      //Serial.print(" - flash_write: ");  
      //Serial.print(",");
      //Serial.println(flash_write_buff[j-1]);  
      
  }    

  mediabaixa = mediabaixa/cbaixa;
  mediaalta = mediaalta/calta;

  
  Serial.print("Nivel do som ambiente: ");
  //Serial.print(mediabaixa);           
  //Serial.print(",");
  Serial.println(mediaalta);      

  if(mediaalta < 10){
    pixel.setPixelColor(0, pixel.Color(0, 50, 0));
    pixel.show();  
  }

  if(mediaalta < 25 && mediaalta > 10){
    pixel.setPixelColor(0, pixel.Color(0, 80, 0));
    pixel.show();  
  }

  if(mediaalta > 35){
    pixel.setPixelColor(0, pixel.Color(80, 0, 0));
    pixel.show();  
  }
  

  
  // ==== Gravacao ==== //  
  if( (mediabaixa < -25 && mediabaixa > -35 )|| ( mediaalta > 25 && mediaalta < 35 ) ){
    pixel.setPixelColor(0, pixel.Color(0, 100, 0));
    pixel.show();     

    int flash_wr_size = 0;      
    flash_wr_size += bytes_read;  
    
    //Serial.println(" *** Inicio da gravacao *** ");
    while (flash_wr_size <= I2S_READ_LEN) {
        // Ler dados do INMP-441
        i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
        
        for (int i = 0; i < bytes_read; i += 2) {
            dac_value = ((((uint16_t) (i2s_read_buff[i + 1] & 0xf) << 8) | ((i2s_read_buff[i + 0]))));
            int l = dac_value * 256 / 2048;
      
            if(l > 256){          
              flash_write_buff[j++] = (l - 512);
            }else{         
              flash_write_buff[j++] = l;
            }      

            /*Serial.print(l);     
            Serial.print(",");     
            Serial.println(flash_write_buff[j-1]);       */
        }
        
        // Atualizar quantidade de bytes lidos
        flash_wr_size += bytes_read;

        //Exibir progresso da gravacao
        Serial.print("Progresso da gravacao: ");
        if((flash_wr_size * 100 / I2S_READ_LEN)>100){
          Serial.println("100%");
        }else{
          Serial.print(flash_wr_size * 100 / I2S_READ_LEN);
          Serial.println("%");
        }
        
    }

    //Serial.println("==================================");
    //Serial.print("flash_wr_size: ");Serial.println(flash_wr_size);
    //Serial.print("I2S_READ_LEN: ");Serial.println(I2S_READ_LEN);
    //Serial.print("J: ");Serial.println(j);
    //Serial.println("==================================");

    #if MODO == 0
    //Gravar audio no arquivo
    if(SPIFFS.exists(filenameRAW)){
      Serial.println("Arquivo temporario removido!");
      SPIFFS.remove(filenameRAW);  
    }

    file = SPIFFS.open(filenameRAW, FILE_WRITE);
    if(!file){
      Serial.println("Erro ao criar arquivo temporario!");
    }
        
    file.write((const byte*) flash_write_buff, I2S_READ_LEN);
    file.close();
    #endif
    
    
    //Serial.println(" *** Gravacao finalizada *** ");  
    mediaalta = 0;
    mediabaixa = 0;  

    pixel.setPixelColor(0, pixel.Color(0, 0, 0));
    pixel.show();
        
    pixel.setPixelColor(0, pixel.Color(100, 100, 100));
    pixel.show();
    Serial.println(" *** Calcular FFT da gravacao *** ");
    calcularFFT();
    pixel.setPixelColor(0, pixel.Color(0, 0, 0));
    pixel.show();

    //Limpar buffer
    i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
    

    #if MODO == 1
    uint32_t start = micros();
    ml.predict(X, y_pred);
    uint32_t timeit = micros() - start;

    Serial.print("\n\nTempo:");
    Serial.print(timeit);
    Serial.println(" ms (INFERENCIA)");

    Serial.println("Predicao: ");
    
    for (int i=0; i<4;i++) {
      Serial.print(y_pred[i]*100);
      Serial.print(i == 3 ? '\n' : ',');
      Serial.print(' ');
    }      
    Serial.print("Classe predita: ");
    Serial.println(classes[ml.probaToClass(y_pred)]);

    switch (ml.probaToClass(y_pred)) {
      
      //0 - Ligar
      case 0:      
        digitalWrite(rele, HIGH); 
        mp3.playFileByIndexNumber(random(4, 7));
        pixel.setPixelColor(0, pixel.Color(255, 0, 0));
        pixel.show();
        delay(500);
        pixel.setPixelColor(0, pixel.Color(0, 0, 0));
        pixel.show();
        break;
        
      //1 - Desligar
      case 1:
        digitalWrite(rele, LOW);
        mp3.playFileByIndexNumber(random(7, 10));
        pixel.setPixelColor(0, pixel.Color(0, 0, 255));
        pixel.show();
        delay(500);
        pixel.setPixelColor(0, pixel.Color(0, 0, 0));
        pixel.show();
        break;
        
      //3 - Franzininho
      case 3:        
        mp3.playFileByIndexNumber(random(1, 4));
        pixel.setPixelColor(0, pixel.Color(100, 0, 0));
        pixel.show();
        delay(500);
        pixel.setPixelColor(0, pixel.Color(0, 100, 0));
        pixel.show();
        delay(500);
        pixel.setPixelColor(0, pixel.Color(0, 0, 100));
        pixel.show();
        delay(500);
        pixel.setPixelColor(0, pixel.Color(0, 0, 0));
        pixel.show();
        break;
        
      //2 - Outros
      default:        
        mp3.playFileByIndexNumber(random(10, 13));
        break;
    }

    delay(100); //delay para executar o audio

    #else
    
    pixel.setPixelColor(0, pixel.Color(100, 0, 0));
    pixel.show();
    Serial.println("Conectando a rede WIFI...");
    wifiInit();
    delay(10000);
    Serial.println(" *** Enviando arquivo de audio para o servidor... *** ");
    uploadFile();
    deinitialize_wifi();
    pixel.setPixelColor(0, pixel.Color(0, 0, 0));
    pixel.show();   
    #endif 
    
    
  }

  delay(50);
  
}

void calcularFFT(){ 

  #if MODO == 0
  file = SPIFFS.open(filenameFFT, FILE_WRITE);
  if(!file){
    Serial.println("Erro ao criar arquivo temporario!");
  }
  #endif

  int j = 128;

  for(int c=0; c<(BLOCK_SIZE - WINDOW); c++){ //Janela de 64
              
      vReal[c] = flash_write_buff[j];
      vTemp[c] = flash_write_buff[j]; 
      vImag[c] = 0.0;

      j++;    
  }

  int linha = 0;
  
  while((I2S_READ_LEN - 640) > j){
    
    for(int c=(BLOCK_SIZE - WINDOW); c < BLOCK_SIZE; c++){   
          
        vReal[c] = flash_write_buff[j];
        vTemp[c] = flash_write_buff[j]; 
        vImag[c] = 0.0;

        j++;
    }
    
    FFT.Windowing(vReal, BLOCK_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, BLOCK_SIZE, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, BLOCK_SIZE);
    for (int i = 0; i < BAND_SIZE; i++) {
      bands[i] = 0;
    }

    for (int i = 2; i < (BLOCK_SIZE/2); i++){ 
      if (vReal[i] > 2000) { 
        if (i<=2 )             bands[0] = max(bands[0], (int)(vReal[i]/amplitudeBaixa)); // 125Hz
        if (i >3   && i<=5 )   bands[1] = max(bands[1], (int)(vReal[i]/amplitudeBaixa)); // 250Hz
        if (i >5   && i<=7 )   bands[2] = max(bands[2], (int)(vReal[i]/amplitudeBaixa)); // 500Hz
        if (i >7   && i<=15 )  bands[3] = max(bands[3], (int)(vReal[i]/amplitudeBaixa)); // 1000Hz
        if (i >15  && i<=30 )  bands[4] = max(bands[4], (int)(vReal[i]/amplitudeBaixa)); // 2000Hz
        if (i >30  && i<=45 )  bands[5] = max(bands[5], (int)(vReal[i]/amplitudeBaixa)); // 4000Hz
        if (i >45  && i<=60 )  bands[6] = max(bands[6], (int)(vReal[i]/amplitudeMedia)); // 5000Hz
        if (i >60  && i<=75 )  bands[7] = max(bands[7], (int)(vReal[i]/amplitudeMedia)); // 6000Hz
        if (i >75  && i<=90 )  bands[8] = max(bands[8], (int)(vReal[i]/amplitudeMedia)); // 7000Hz
        if (i >90  && i<=105 ) bands[9] = max(bands[9], (int)(vReal[i]/amplitudeMedia)); // 8000Hz
        if (i >105 && i<=120 ) bands[10] = max(bands[10], (int)(vReal[i]/amplitudeAlta)); // 9000Hz
        if (i >120 && i<=135 ) bands[11] = max(bands[11], (int)(vReal[i]/amplitudeAlta)); // 10000Hz
        if (i >135 && i<=150 ) bands[12] = max(bands[12], (int)(vReal[i]/amplitudeAlta)); // 11000Hz
        if (i >150 && i<=165 ) bands[13] = max(bands[13], (int)(vReal[i]/amplitudeAlta)); // 12000Hz
        if (i >165 && i<=180 ) bands[14] = max(bands[14], (int)(vReal[i]/amplitudeAlta)); // 13000Hz
        if (i >200           ) bands[15] = max(bands[15], (int)(vReal[i]/amplitudeAlta)); // 16000Hz
      }      
    }

    #if MODO == 1
    X[linha+0]  = bands[0] / 255.0;
    X[linha+1]  = bands[1] / 255.0;
    X[linha+2]  = bands[2] / 255.0;
    X[linha+3]  = bands[3] / 255.0;
    X[linha+4]  = bands[4] / 255.0;
    X[linha+5]  = bands[5] / 255.0;
    X[linha+6]  = bands[6] / 255.0;
    X[linha+7]  = bands[7] / 255.0;
    X[linha+8]  = bands[8] / 255.0;
    X[linha+9]  = bands[9] / 255.0;
    X[linha+10] = bands[10] / 255.0;
    X[linha+11] = bands[11] / 255.0;
    X[linha+12] = bands[12] / 255.0;
    X[linha+13] = bands[13] / 255.0;
    X[linha+14] = bands[14] / 255.0;
    X[linha+15] = bands[15] / 255.0;

    #else
    
    file.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    bands[0], bands[1], bands[2], bands[3], bands[4], bands[5], bands[6], bands[7],
    bands[8], bands[9], bands[10],bands[11],bands[12],bands[13],bands[14],bands[15]);
    #endif
 
    
    for (int i = WINDOW; i < BLOCK_SIZE; i++) { 
      vReal[i - WINDOW] = vTemp[i]; 
      vTemp[i - WINDOW] = vTemp[i];
      vImag[i - WINDOW] = 0.0;
    } 

    linha = linha + 16;  
    
  }

  #if MODO == 0
  file.close();
  #endif

}
