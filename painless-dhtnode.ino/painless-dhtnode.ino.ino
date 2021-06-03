//************************************************************
// painless-dhtnode
// implementazione di un nodo (esp8266) all'interno di una mesh radio
// accoppiato con un economico sensore dht22
//
//
// nodemcu(s) pinout
// https://www.reddit.com/r/esp8266/comments/9q3314/nodemcu_v1_v2_v3_size_and_pinout_comparison/
// esp8266 mesh
// https://gitlab.com/painlessMesh
// task scheduler
// https://github.com/arkhipenko/TaskScheduler/wiki/API-Task
// adafruit's dht sensor library
// https://github.com/adafruit/DHT-sensor-library
//
//************************************************************

#include "painlessMesh.h"
#include "DHT.h"

//IMPOSTAZIONI DELLA RETE MESH
#define   MESH_PREFIX     "iotnetwork"
#define   MESH_PASSWORD   "iotnetwork"
#define   MESH_PORT       5555

//led   16=led vicino porta usb  2=led vicino antenna    (attivi bassi)
#define LED1           16 //la scheda lolin v3 non ha questo link collegato alla GPIO 16
#define LED2           2
//dati sensore   (da pinout D8=15, oppure D1=5)
#define DHTPIN         D1
#define DHTTYPE        DHT22
// nodo passivo //impostare true al nodo che sniffa
#define PASV           true
//echo su seriale
bool serialEcho = false;

DHT dht(DHTPIN, DHTTYPE);
float t=0.0; //variabile temperatura
float h=0.0; //variabile umidità

//DEFINIZIONE DI UN OGGETTO PAINLESSMESH PER GESTIRE LA RETE
painlessMesh  mesh;

//DEFINIZIONE DELLO SCHEDULER PER L'USO DEI TASK
Scheduler userScheduler; // scheduler

// DEFINIZIONE DEI TASK
void checker();
Task taskChecker( TASK_SECOND * 1 , TASK_FOREVER, &checker );
void sender();
Task taskSender( TASK_SECOND * 1 , TASK_FOREVER, &sender );
void blinker();
Task taskBlinker( TASK_SECOND / 30, 2, &blinker );
void sensing();
Task taskSensing( TASK_SECOND * 2 , TASK_FOREVER, &sensing );

// FUNZIONI CHE SONO USATE NEI TASK

//accende il led appena rileva un altro nodo collegato (attiva la mesh)
void checker() {
  if(PASV){
    if(mesh.getNodeList().size() > 0) digitalWrite(LED2, LOW);
    else digitalWrite(LED2, HIGH);
  }
}

// led blinker
void blinker() {
  boolean state = digitalRead(LED2);
  digitalWrite(LED2, !state);
  if(taskBlinker.isLastIteration()) digitalWrite(LED2, HIGH); // assicura che il led non rimanga acceso
 
}

//INVIO DATI DAI NODI COLLEGATI AI DHT22 (se non configurato come passivo)
void sender() {
  #if(!PASV) //solo i nodi che non sono configurati come passivi inviano i risultati della misurazione
    //String msg = (String)random(1024);  // dati di esempio
    String msg = (String)t + "-" + (String)h;
    //String msg = (String)mesh.getNodeList().size()+1;
    mesh.sendBroadcast(msg); //INVIA IL MESSAGGIO IN BROADCAST
    taskSender.setInterval( random( TASK_SECOND * 1, TASK_SECOND * 5 )); //randomizza intervallo temporale di invio
    if(serialEcho) Serial.print("snd from " + (String)mesh.getNodeId() + " = " + msg + "\n");
    taskBlinker.restart();              // blinka in trasmissione
   #endif
}

//FUNZIONE PER IL MONITORAGGIO DI UMIDITA' E TEMPERATURA
void sensing() {
  h = dht.readHumidity();
  t = dht.readTemperature();
}


// CALLBACKS 

//CALLBACK ASSOCIATA ALL'EVENTO DI RICEZIONE DI UN MESSAGGIO (usata da tutti i nodi quando nella rete mesh si verifica l'evento di ricezione di un messaggio)
void receivedCallback( uint32_t from, String &msg ) {
  // se è un comando diretto, lo esegue
  if(msg=="serial") serialEcho=!serialEcho;
  else if(msg=="topology") mesh.sendSingle(from, mesh.subConnectionJson()); //restituisce SOLO al nodo (mesh.sendSinlge() invia un messaggio solo ad un destinatario) che ha inviato il messaggio "topology", la topologia della rete in un messaggio in formato JSON
  else if(serialEcho) Serial.printf("rcv from %u = %s\n", from, msg.c_str()); //altrimenti si limita a fare l'eco sul seriale (se abilitato) la variabile serialEcho la lascio a true solo nel nodo sniffer, in modo che solo lui stampa su seriale
 
  #if(PASV)
    taskBlinker.restart();
  #endif
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> startHere: new Connection from = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  checker();   // controlla se isolato
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}


// START
void setup() {
  Serial.begin(115200);
  // INIT SENSORE
  dht.begin();
  
  // pin led
  pinMode(LED1, OUTPUT); digitalWrite(LED1, HIGH); //sulla scheda lolin V3 esiste solo un LED2, quello collegato alla GPIO 16 non esiste
  pinMode(LED2, OUTPUT); digitalWrite(LED2, HIGH);
  
  // DEBUG SU SERIAL HARDWARE
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  
  // PROCEDURE DI INIT
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  Serial.println("Connecting to MESH");

  //si associa ad ogni evento la relativa callback
  //si setta per ogni evento che siverifica nella rete mesh come deve reagire.
  mesh.onReceive(&receivedCallback);//quando un nodo riceve un messaggio viene chiamata la callback receivedCallback
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  
  // TASKS vengno ripetuti in successione nel loop  
  userScheduler.addTask( taskChecker );
  userScheduler.addTask( taskSender );
  userScheduler.addTask( taskBlinker );
  userScheduler.addTask( taskSensing );
  taskChecker.enable();
  taskSender.enable();
  taskSensing.enable();
 
  // PASSIVE STA (da capire il funzionamento in bridge con AP vicino)
  //solo il nodo passivo ha la variabile serialEcho a true, in modo tale che quando riceve un messaggio lo stampa su seriale
  #if(PASV)
    serialEcho = true;
    //mesh.stationManual( .... );
    //mesh.setHostname("meshroot");
  #endif
}

void loop() {
  // it will run the user scheduler as well
  mesh.update();
}
