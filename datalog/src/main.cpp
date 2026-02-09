#include <SPI.h>
#include <FS.h>
#include <SD.h>


// PINAGEM DO SEU PROJETO

// SD Card (HSPI)
#define SD_CS   5
#define SD_SCK  18
#define SD_MOSI 23
#define SD_MISO 19

// UART Baud Rate (deve ser igual ao do dispositivo que envia)
#define UART_BAUD   115200

// Arquivo de log
#define LOG_FILE "/datalog.txt"

// Usar HSPI (SPI dedicado)
SPIClass sdSPI(HSPI);

// Buffer para receber dados
String inputBuffer = "";
bool sdCardOK = false;
unsigned long recordCount = 0;

// ===============================
// FUNÇÕES AUXILIARES
// ===============================

// Adiciona dados ao arquivo de log (append)
void appendToLog(const char * message) {
  File file = SD.open(LOG_FILE, FILE_APPEND);
  if (!file) {
    return;
  }

  file.println(message);
  file.close();
  recordCount++;
}

// Cria o cabeçalho do arquivo se não existir
void createLogHeader() {
  if (!SD.exists(LOG_FILE)) {
    File file = SD.open(LOG_FILE, FILE_WRITE);
    if (file) {
      file.println("TEMP,UMID,ANGULO,ALERTA");
      file.close();
    }
  }
}

// Processa os dados recebidos
void processData(String data) {
  data.trim();  // Remove espaços e \n\r
  
  if (data.length() == 0) return;
  
  // Verifica se é dado no formato correto (contém números e vírgulas)
  // Formato esperado: 31.2,47.4,33.2,0
  
  // Conta vírgulas para validar formato
  int commaCount = 0;
  bool hasDigit = false;
  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',') commaCount++;
    if (isDigit(data.charAt(i))) hasDigit = true;
  }
  
  // Se tem 3 vírgulas e pelo menos um dígito, é dado válido
  if (commaCount == 3 && hasDigit) {
    // Formato válido, salva no SD
    if (sdCardOK) {
      appendToLog(data.c_str());
    }
  }
}

// ===============================
// SETUP
// ===============================
void setup() {
  // Serial para receber dados (UART0 - pinos TX=1, RX=3)
  Serial.begin(UART_BAUD);
  delay(1000);

  // Inicializa o barramento SPI customizado
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // Inicializa o SD usando esse SPI
  if (!SD.begin(SD_CS, sdSPI, 10000000)) {  // 10 MHz
    sdCardOK = false;
  } else {
    sdCardOK = true;

    // Cria cabeçalho do arquivo de log
    createLogHeader();
  }
}

// ===============================
// LOOP
// ===============================
void loop() {
  // Lê dados da Serial (UART0 - pinos TX=1, RX=3)
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n') {
      // Linha completa recebida, processa
      processData(inputBuffer);
      inputBuffer = "";
    } else if (c != '\r') {
      // Adiciona caractere ao buffer (ignora \r)
      inputBuffer += c;
    }
  }
}

