//#define ip "192.168.88.217" 
//#define ip "192.168.88.243" 
//Endereco IP do computador onde esta executando o servidor

void uploadFile();

void uploadFile(){ 
  #if MODO == 0
  file = SPIFFS.open(filenameFFT, FILE_READ);
  if(!file){
    Serial.println("Erro ao criar o arquivo temporario!");
    return;
  }

  Serial.println("===> Upload do arquivo CSV para o servidor.");

  HTTPClient client;
  client.begin("http://192.168.88.243:8888/uploadCSV");
  client.addHeader("Content-Type", "audio/wav");
  int httpResponseCode = client.sendRequest("POST", &file, file.size());
  Serial.print("httpResponseCode : ");
  Serial.println(httpResponseCode);

  if(httpResponseCode == 200){
    String response = client.getString();
    Serial.print("Resposta: ");
    Serial.println(response);
  }else{
    Serial.println("Error");
  }
  file.close();  
  client.end();

  file = SPIFFS.open(filenameRAW, FILE_READ);
  if(!file){
    Serial.println("Erro ao criar arquivo temporario!");
    return;
  }

  Serial.println("===> Upload do arquivo RAW para o servidor");

  client.begin("http://192.168.88.243:8888/uploadRAW");
  client.addHeader("Content-Type", "audio/wav");
  httpResponseCode = client.sendRequest("POST", &file, file.size());
  Serial.print("httpResponseCode : ");
  Serial.println(httpResponseCode);

  if(httpResponseCode == 200){
    String response = client.getString();
    Serial.print("Resposta: ");
    Serial.println(response);    
  }else{
    Serial.println("Error");
  }
  
  file.close();  
  client.end();
  #endif
}
