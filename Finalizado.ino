#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Adafruit_Fingerprint.h>

#define FIREBASE_HOST "********"
#define FIREBASE_AUTH "********"
#define WIFI_SSID "******"
#define WIFI_PASSWORD "********"
#define LED_PIN 5
#define LED_PIN2 4
#define BTN_PIN 14
#define RELAY 2
#define FINGERPRINT_RX 13
#define FINGERPRINT_TX 15



SoftwareSerial Serial2(FINGERPRINT_RX, FINGERPRINT_TX);

// Configuração do sensor biométrico
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

// Configuração do Firebase
FirebaseData firebaseData;

void setup() {
  Serial.begin(9600);
  Serial2.begin(57600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN2, LOW);
  digitalWrite(RELAY, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Sensor biométrico inicializado com sucesso!");
  } else {
    Serial.println("Falha na inicialização do sensor biométrico!");
    while (1) {}
  }
}

void loop() {
  // Captura a digital


  if (digitalRead(BTN_PIN) == LOW) {
    // Chama a função que deseja executar
    storeFingerprint();
  }

  if (Serial.available() > 0) {
    int input = Serial.read();
    if (input == '1') {
      storeFingerprint();
    }
    if (input == '2') {
      emptyDatabase();
    }
  }

  uint8_t id = 0;
  int verifLocal = 0;
  uint8_t status = finger.getImage();

  while (status != FINGERPRINT_OK) {
    Serial.println("========");
    return;
  }
  //if (status != FINGERPRINT_OK) {
  // Serial.println("========");
  // return;
  //}
  status = finger.image2Tz(1);
  if (status != FINGERPRINT_OK) {
    Serial.println("Não foi possível converter a imagem em um modelo");
    return;
  }
  status = finger.fingerSearch();
  if (status == FINGERPRINT_OK) {
    id = finger.fingerID;
    verifLocal = checkFingerprint(status);

  } else {
    Serial.println("Digital não encontrada no banco de dados");
    digitalWrite(LED_PIN2, HIGH);
    delay(3000);
    digitalWrite(LED_PIN2, LOW);
    return;
  }


  // Envia a digital e nome não definido para o firebase para o Firebase
  
  String path = "digitais/id_" + String(id);
  String path2 = "nomes/id_" + String(id);
  Firebase.setString(firebaseData, path, "true");
  
  // Verifica se a digital corresponde a uma pessoa autorizada
  if (Firebase.getBool(firebaseData, path) || verifLocal == 1) {

    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELAY, HIGH);

    if (Firebase.getString(firebaseData, path2)) {
      Serial.println("Nome: " + firebaseData.stringData());
    } else {
      String name = "none";
      Firebase.setString(firebaseData, path2, name);
      Serial.println(firebaseData.errorReason());
    }

    delay(5000);
    digitalWrite(RELAY, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  delay(2500);
}



//===================FUNÇÕES====================//
int printStoredFingerprintsCount() {
  //Manda o sensor colocar em "templateCount" a quantidade de digitais salvas
  finger.getTemplateCount();

  //Exibe a quantidade salva

  return finger.templateCount;
}


void storeFingerprint() {
  uint8_t id = 0;

  Serial.println("Digite o seu nome...!");
  String nome = "none";
  while (nome == "none") {
    if (Serial.available() > 0) {
      nome = Serial.readStringUntil('\n');
      Serial.println("O nome recebido foi: " + nome);
    }
  }  

  //Transforma em inteiro
  int location = printStoredFingerprintsCount() + 1;
  Serial.printf("Digitais cadastradas: %d \n", location - 1);
  //Verifica se a posição é válida ou não
  if (location < 1 || location > 149) {
    //Se chegou aqui a posição digitada é inválida, então abortamos os próximos passos
    Serial.println(F("Posição inválida"));
    return;
  }

  Serial.println(F("Encoste o dedo no sensor"));

  //Espera até pegar uma imagem válida da digital
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(LED_PIN2, HIGH);
  while (finger.getImage() != FINGERPRINT_OK)
    ;

  //Converte a imagem para o primeiro padrão
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    //Se chegou aqui deu erro, então abortamos os próximos passos
    Serial.println(F("Erro image2Tz 1"));
    return;
  }

  Serial.println(F("Tire o dedo do sensor"));
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN2, LOW);
  delay(2000);

  //Espera até tirar o dedo
  while (finger.getImage() != FINGERPRINT_NOFINGER)
    ;

  //Antes de guardar precisamos de outra imagem da mesma digital
  Serial.println(F("Encoste o mesmo dedo no sensor"));

  //Espera até pegar uma imagem válida da digital
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(LED_PIN2, HIGH);
  while (finger.getImage() != FINGERPRINT_OK);

  //Converte a imagem para o segundo padrão
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    //Se chegou aqui deu erro, então abortamos os próximos passos
    Serial.println(F("Erro image2Tz 2"));
    for (int i = 0; i < 4; i++) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(LED_PIN2, LOW);
      delay(1000);
    }
    return;
  }

  //Cria um modelo da digital a partir dos dois padrões
  if (finger.createModel() != FINGERPRINT_OK) {
    //Se chegou aqui deu erro, então abortamos os próximos passos
    Serial.println(F("Erro createModel"));
    for (int i = 0; i < 4; i++) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(LED_PIN2, LOW);
      delay(1000);
    }
    return;
  }

  //Guarda o modelo da digital no sensor
  if (finger.storeModel(location) == FINGERPRINT_OK) {

    uint8_t status = finger.fingerSearch();
    if (status == FINGERPRINT_OK) {
      id = finger.fingerID;

    } else {
      Serial.println("Digital não encontrada no banco de dados");
      for (int i = 0; i < 4; i++) {
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(LED_PIN2, HIGH);
        delay(1000);
        digitalWrite(LED_PIN, LOW);
        digitalWrite(LED_PIN2, LOW);
        delay(1000);
      }
      return;
    }

  } else {
    //Se chegou aqui deu erro, então abortamos os próximos passos
    Serial.println(F("Erro storeModel"));
    for (int i = 0; i < 4; i++) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(LED_PIN2, LOW);
      delay(1000);
    }
    return;
  }

  //Se chegou aqui significa que todos os passos foram bem sucedidos
  Serial.println(F("Sucesso!!!"));
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN2, LOW);

  Serial.println("Aguardando a digital...");

  // Envia a digital para o Firebase
  String path = "digitais/id_" + String(id);
  String path2 = "nomes/id_" + String(id);
  Firebase.setString(firebaseData, path, "true");
  Serial.println("Nome:" + nome);
  Firebase.setString(firebaseData, path2, nome);

}

void emptyDatabase() {
  delay(1000);

  Serial.println(F("Apagando banco de digitais..."));

  //Apaga todas as digitais
  if (finger.emptyDatabase() != FINGERPRINT_OK) {
    Serial.println(F("Erro ao apagar banco de digitais"));
  } else {
    String path = "digitais/";
    String path2 = "nomes/";
    //String value = " ";
    Firebase.set(firebaseData, path, NULL);
    Firebase.set(firebaseData, path2, NULL);
    Serial.println(F("Banco de digitais apagado com sucesso!!!"));
  }
  delay(3000);
}

int checkFingerprint(uint8_t verif) {
  Serial.println(F("Verificando"));

  /*Espera até pegar uma imagem válida da digital
    while (finger.getImage() != FINGERPRINT_OK);

    //Converte a imagem para o padrão que será utilizado para verificar com o banco de digitais
    if (finger.image2Tz() != FINGERPRINT_OK)
    {
    //Se chegou aqui deu erro, então abortamos os próximos passos
    Serial.println(F("Erro image2Tz"));

    }*/

  //Procura por este padrão no banco de digitais
  if (verif != FINGERPRINT_OK) {
    //Se chegou aqui significa que a digital não foi encontrada
    Serial.println(F("Digital não encontrada"));
    return 0;
  }

  //Se chegou aqui a digital foi encontrada
  //Mostramos a posição onde a digital estava salva e a confiança
  //Quanto mais alta a confiança melhor
  Serial.print(F("Digital encontrada com confiança de "));
  Serial.print(finger.confidence);
  Serial.print(F(" na posição "));
  Serial.println(finger.fingerID);
  return 1;
}
