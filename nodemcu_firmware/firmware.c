/********************************************************
* 
* SolarFI ver 1.3.0 - Firmware per scheda NodeMCU
* 
* Lettore produzione fotovoltaico per inverter Soltec
* Invio dei dati a server HTTP remoto
*
* Scheda: NodeMCU
* Programmatore: Arduino IDE via USB
*
* 21/05/2018
* Gianluca Ghettini 
*
********************************************************/


#include <ESP8266WiFi.h>
 
// configurazione WIFI (nome rete e password) 
const char* ssid = "*************";
const char* password = "*************";

// host di destinazione (es. http://www.miosito.com)
const char* host = "*************";

// identificativo della board NodeMCU. Qualsiasi stringa è ok
#define DEVICEID "*************";



const int debug = 0; 
int gpiod2 = 5;
unsigned char buff[1024];
unsigned char buffout[1024];
unsigned char payload[1024];
char printout[1024];
char scratch[1024];

#define MAX_INVERTERS 32
#define MAX_RETRY 5

typedef struct inverter
{
	int valid;
	char name[64];
	int address;
	int retry;
} t_inverter;

t_inverter inverters[MAX_INVERTERS];



/********************************************************
* 
* Cerca il prossimo slot libero nella lista degli inverter
* Ritorna con l'indice dello slot libero o -1 se la lista
* è piena
*
********************************************************/
int nextempty()
{
	int slot = 0;
	while(slot < MAX_INVERTERS && inverters[slot].valid == 1) slot++;
	if(slot == MAX_INVERTERS) return -1;
	else return slot;
}





/********************************************************
* 
* mette il transceiver RS485 in modo "ascolto"
*
********************************************************/
void readenable()
{
	digitalWrite(gpiod2, LOW);
}



/********************************************************
* 
* mette il transceiver RS485 in modo "scrittura"
*
********************************************************/
void writeenable()
{
	digitalWrite(gpiod2, HIGH);
}



/********************************************************
* 
* Costruisce un pacchetto MODBUS e lo invia sul bus
*
********************************************************/
void sendpacket(int addr, int command, int payloadLength)
{
	// breve lampeggio del led 
	digitalWrite(LED_BUILTIN, HIGH);
	delay(50);
	digitalWrite(LED_BUILTIN, LOW);
	delay(50);


	// costruzione del pacchetto MODBUS
	int p = 0;
	buff[p++] = 0xAA;
	buff[p++] = 0xAA;
	buff[p++] = 0x01;
	buff[p++] = 0x00;
	buff[p++] = (addr >> 8) & 0xff;
	buff[p++] = (addr >> 0) & 0xff;
	buff[p++] = (command >> 8) & 0xff;
	buff[p++] = (command >> 0) & 0xff;
	buff[p++] = payloadLength;
	for(int b = 0; b < payloadLength; b++)
	{
		buff[p++] = payload[b];
	}
	int crc = 0;
	for(int c = 0; c < p; c++)
	{
		crc += buff[c];
	}
	buff[p++] = (crc >> 8) & 0xff;
	buff[p++] = (crc >> 0) & 0xff;
	p++;



	writeenable(); // mette il transceiver RS485 in modo "scrittura"

	// invia il pacchetto MODBUS
	int wrote = 0;
	while(wrote < p)
	{
		wrote += Serial.write(buff, p); 
	}
	Serial.flush();

	readenable(); // mette il transceiver RS485 in modo "ascolto"
}




/********************************************************
* 
* Attende per e legge il pacchetto MODBUS di risposta 
* inviato dell'inverter
*
********************************************************/
int readpacket(int max_timeout)
{
	int timeout = 0;
	int p = 0;

	// attende una risposta...
	while(Serial.available() == 0)
	{
		if(timeout++ > max_timeout) return -1; // timeout!
		delay(1);
	}

	// risposta in arrivo! leggi i byte e mettili in "buffout"
	while(Serial.available() > 0)
	{
		int howmany = Serial.available();
		for(int i = 0; i < howmany; i++) buffout[p++] = Serial.read();
		delay(10);
	}

	// pacchetto più lungo di 11 byte? errore
	if(p < 11) return -2;
	
	// controllo CRC
	int crcRead = (buffout[p - 2] << 8) | buffout[p - 1];
	int crc = 0;
	for(int c = 0; c < p - 2; c++)
	{
		crc += buffout[c];
	}
	if(crc != crcRead) return -3; // pacchetto malformato!


	// tutto ok. estrai dalla risposta il payload e mettilo in "payload"
	int len = buffout[8];
	for(int d = 0; d < len; d++)
	{
		payload[d] = buffout[9 + d];
	}
	
	return len;
}




/********************************************************
* 
* Inizializza la board NodeMCU
*
* - gpio dei led
* - seriale
* - inzializza la lista degli inverter
* - invia il comando di reset sul bus
*
********************************************************/
void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);
	Serial.begin(9600);
	pinMode(gpiod2, OUTPUT);
	delay(1000);

	for(int i = 0; i < MAX_INVERTERS; i++)
	{
		inverters[i].valid = 0;      
	}
	
	// invio comando di reset (RESET)
	sendpacket(0, 4, 0);
	sendpacket(0, 4, 0);
	sendpacket(0, 4, 0);
}






/********************************************************
* 
* Connette la board alla wifi o esce se già connesso
*
********************************************************/
int connectWifiReliable()
{
	while(WiFi.status() != WL_CONNECTED)
	{     
		digitalWrite(LED_BUILTIN, HIGH);
		
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid, password);

		int count = 0;
		while(WiFi.status() != WL_CONNECTED)
		{
			digitalWrite(LED_BUILTIN, HIGH);
			delay(100);
			digitalWrite(LED_BUILTIN, LOW);
			delay(100);

			if(count++ > 200) // dopo 200 tentativi effettua il reboot
			{
				digitalWrite(LED_BUILTIN, HIGH);
				WiFi.disconnect();
				while(1); // questo ciclo stretto triggera il watchdog che effettua il reboot
			}
		}
	}

	digitalWrite(LED_BUILTIN, LOW);
	return 1;
}





/********************************************************
* 
* Invia una stringa di dati arbitrari ad un server HTTP
* remoto.
* Utilizza HTTP 1.1
*
********************************************************/
void sendhttp(String data)
{    
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(host, httpPort)) {
		return;
	}
	
	// We now create a URI for the request
	String url = "/solar/solar.php" + data;

	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
							 "Host: " + host + "\r\n" + 
							 "Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			client.stop();
			return;
		}
	}
	
	while(client.available()){
		String line = client.readStringUntil('\r');
	}  
}





/********************************************************
* 
* Interroga gli inverter, recupera i dati, invia i dati
* al server. Per ogni interrogazione controlla anche la
* presenza di nuovi inverter
*
********************************************************/
void getdata()
{
	int discover = 1;
	for(int slot = 0; slot < MAX_INVERTERS; slot++)
	{
		discovernew(); // controlla se ci sono nuovi inverter sul bus
		
		if(inverters[slot].valid == 1)
		{
			if(debug) sendhttp("?debug=getdatafor:" + String(slot));
			
			discover = 1;
						
			sendpacket(inverters[slot].address, 0x0102, 0); // invio del pacchetto DATA_REQUEST all'inverter
			int res = readpacket(1000); // attesa e recupero della risposta

			if(debug) sendhttp("?debug=datarequest:" + String(res));
			
			if(res > 0) // abbiamo una risposta dall'inverter?
			{
				// si, costruisci la stringa da inviare al server
				char* name = inverters[slot].name;
				char *ptr = printout;
				for(int p = 0; p < res; p++)
				{
					ptr += sprintf(ptr, "%02x", payload[p]);
				}

				// invia i dati al server
				sendhttp("?data=" + String(name) + ":" + String(printout));    
			}
			else if(res == -1)
			{
				// no, l'inverter non ha riposto in tempo (timeout)
				inverters[slot].retry++;
				if(inverters[slot].retry == MAX_RETRY) // superato il numero massimo di tentativi falliti?
				{
					inverters[slot].valid = 0; // si, rimuovi l'inverter dalla lista
				}
			}
			else if(res == -2)
			{
				// si, ma il pacchetto di risposta è parziale. Non fare nulla
			}
			else if(res == -3)
			{
				// si, ma il pacchetto di risposta è corrotto (errore CRC). Non fare nulla
			}
		}     
	}
}



/********************************************************
* 
* Trova nuovi inverter sul bus
*
********************************************************/
int discovernew()
{
	if(debug) sendhttp("?debug=discovernew");
		 
	int slot = nextempty();

	if(debug) sendhttp("?debug=slotfound:" + String(slot));
		
	if(slot < 0) return -1; // abbiamo terminato gli slot disponibili, abbandona
	
	sendpacket(0, 0x0000, 0); // invia pacchetto di richiesta del seriale (SERIAL_REQUEST)
	int res = readpacket(1000); // leggi risposta

	if(debug) sendhttp("?debug=serialrequest:" + String(res));

	if(res > 0)
	{
		// scegli l'indirizzo del nuovo inverter
		int address = slot + 1;

		// copia il nome
		for(int n = 0; n < res; n++) scratch[n] = payload[n];
		scratch[res] = 0;

		payload[res] = address & 0xff;
		
		sendpacket(0, 0x0001, res + 1); // invia pacchetto di assegnazione dell'indirizzo (ADDRESS_SET)
		res = readpacket(1000); // lettura risposta

		if(debug) sendhttp("?debug=pairingrequest:" + String(res));

		if(res == 1) // il pacchetto di risposta è lungo 1 byte?
		{
			// inverter confermato! registrato in lista
			inverters[slot].valid = 1;
			inverters[slot].retry = 0;
			inverters[slot].address = address;
			strcpy(inverters[slot].name, scratch);

			if(debug) sendhttp("?debug=paired:" + String(inverters[slot].name));
			
			return 1;
		}
	}
	return -1;
}
 

/********************************************************
* 
* Ciclo principale:
*
* - mantiene la connessione in wifi
* - invia un keepalive al server (ping hello)
* - recupera dati dagli inverter e invia dati al server
*
********************************************************/
void loop()
{
	delay(1000);

	connectWifiReliable(); // connetti in wifi, se siamo già connessi esce subito

	sendhttp("?hello="+String(DEVICEID)); // invia ping keepalive

	if(debug) sendhttp("?debug=hellofrom:"+String(DEVICEID));

	getdata(); // recupera dati dagli inverter e invia dati al server
}
