#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For å kunne bruke memcpy.

#include <unistd.h> // For å kunne stenge en socket med close(). OG sleep(1) 
#include <sys/socket.h> // For det standardisérte socket-API'et.
#include <sys/types.h> // For noen av typene som kreves for å få sockets til å funke.

#include <netinet/in.h> // Inneholder noen av addresse structene som man bruker med sockets.

#include "../print_lib/print_lib.h"
#include "Networking.h" // Inneholder structer .


int OwnAddress = -1;
int BasePort = -1;

char TCPBuffer [2048];

// 'Boolsk' variabel som sier noe om vi lytter på UDP pakker eller ikke.
// Når denne blir -1 terminerer lytting og noden.
int isListening = 1;


// Self-management prototyper
void initializeNode(int, char **);

// Rutertabell og TCP-relaterte prototyper.
void printRoutingTable(void);
int isDestinationInRoutingTable(int id);
int getNextHopByDestination(int dest);
void fetchRoutingTableFromServer(int serverSocket);

// UDP relaterte prototyper.. 
void listenForUDPPackages(void);
int sendUDPPackagesThroughNetwork(void);
char * constructUDPPackage(int destination, int source, char * message);
void sendPackageToDesination(int nextHop, int destination, int source, char * message);



int neighbourCount = 0; // Antallet nabo-noder.
struct NodeInfo * neighbourNodes; // Array som holder referanser til alle nabonoder og deres 'weights' 

struct RoutingTableNode ** routingTable; // Inneholder informasjon om alle nestehoppene denne noden er ansvarlig for.
int tableSize = 0;


int UDPserverSocket = -1;
struct sockaddr_in udpServerAdresse;

int main(int argc, char * argv[]){

    printf("Hello I'm a node.c! Amount of arguments: %d \n",argc);
    
    initializeNode(argc, argv);


    // Denne funksjonen henter også ut rutingtabellen og lagrer den i 
    // routingTable. 
    openTCPConnectionToRoutingServer();

    // Print noe statistikk for sånn at man kan se om inistialisering
    // av nodens NodeInfo struct ble gjort riktig 
    printNodeInfo();

    UDPserverSocket = openUDPServerConnection();

    if(UDPserverSocket != -1){
        printf("UDP ServerSocket was successfully created! Port: %d \n", BasePort+OwnAddress);
    }

    


    if(OwnAddress == 1){

        // Vent 1 sekund for å vente litt slik at alle UDP serverene er startet.
        sleep(1);
        printf("This is node 1! Preparing sending of UDP packages: \n");
        int result = sendUDPPackagesThroughNetwork();

    }else{

      listenForUDPPackages();

    }

    freeAllAllocatedMemory();

    exit(0);

    return 0;
}









void initializeNode(int argCount, char * argList[]){
    
    printf("Initializing node.\n");
    // Hvis argumentene i programmet er formatert riktig skal
    // argc - 3 være den riktige lengden på  Edge -> Weight Arrayet som jeg kaller neighbourNodes.
    neighbourCount = argCount - 3;
    neighbourNodes = malloc(sizeof(struct NodeInfo) * neighbourCount);
    int currentNeighbourIndex = 0;
    int i;

    printf("Reading arguments:\n");
    for(i = 0; i < argCount ; i++){
        printf("argument %d: %s \n",i, argList[i]);


        if( i == 1 ){
            BasePort = atoi(argList[i]);
        }

        if( i == 2 ){
            OwnAddress = atoi(argList[i]);
        }


        // Lagre Neighbooring Nodes + Weights.
        if( i >= 3){
            
            // Så må vi allokere minne til structen som skal lagres.
            neighbourNodes[currentNeighbourIndex].from = OwnAddress;

            // Leser inn informasjon i structen fra argumentet.
            sscanf(argList[i], "%d:%d", &neighbourNodes[currentNeighbourIndex].to, &neighbourNodes[currentNeighbourIndex].weight);

            currentNeighbourIndex++;

        }

    }
}




int openTCPConnectionToRoutingServer(){
                                // SOCK_STREAM betyr TCP-type socket.
    int socketToRouter = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAdresse;
    
    serverAdresse.sin_family = AF_INET;
    serverAdresse.sin_port = htons(BasePort);
    serverAdresse.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0 ( localhost ).

    int socketStatus = connect(socketToRouter, (struct sockaddr *) &serverAdresse, sizeof(serverAdresse) );

    if(socketStatus != -1){
        // Alt gikk bra og dermed kan vi sende data direkte!

        // #1 Forberéd dataen som skal sendes.

        // Protokoll:
        // OwnAdress |    Edge      Weight    |          |     Edge      Weight    |     '\0'
        // 4 bytes   |    4 bytes   4 bytes   |     ...  |     4 bytes   4 bytes   |     1 byte.

        char edgeWeightList [2048];
        int i; 
        printf("\n1024 first bytes of edgeWeightList BEFORE network-preparation =  \n");
        for(i = 0; i< 2048; i++){
            edgeWeightList[i] = 0;
        }


        int curIndex = 0;
          printf("Looping through neighbours: \n ");
          for(i = 0; i < neighbourCount; i++){

            printf("Dealing with neighbour[%d]  to= %d     weight= %d \n" , i, neighbourNodes[i].to, neighbourNodes[i].weight);

                edgeWeightList[curIndex] = neighbourNodes[i].to;
                curIndex += sizeof(int);
                edgeWeightList[curIndex] = neighbourNodes[i].weight;
                curIndex += sizeof(int);
                

            printf("Index after adding weight: curIndex = %d\n", curIndex);

         }

        // Set nullchar på  enden av edgeWeightList for denne Noden.
        edgeWeightList [curIndex] = '\0';
        printf("\n");
        printf("256 first bytes of edgeWeightList after network-preparation =  ");

        for(i = 0; i< 256; i++){
            printf("%d",edgeWeightList[i]);
        }


        printf("\n");
        printf("Connection with RoutingServer successfull! \n ");



        /// Sett alle chars i TCP-bufferen til 0, hvis den har noe 'skitten data'.
        printf("\n1024 first bytes of TCPBuffer BEFORE network-preparation =  \n");
        for(i = 0; i< 2048; i++){
            TCPBuffer[i] = 0;
        }

        int lastIndex = 0;
        memcpy(&TCPBuffer, &OwnAddress, sizeof(int));
        lastIndex = sizeof(int);

    
        memcpy(&TCPBuffer[lastIndex], &edgeWeightList, curIndex);



        printf("\n2048 first bytes of TCPBuffer after network-preparation =  \n");
        for(i = 0; i< 2048; i++){
            printf("%d",TCPBuffer[i]);
        }



        // #2 Send informasjonen fra denne Noden MED TCP socketen til ruter_serveren!
        int bytesSent = send(socketToRouter,TCPBuffer, sizeof(TCPBuffer),0);
        printf("returnvalue from send() : %d\n", bytesSent);


        for(i = 0; i < 2048 ; i++){
            TCPBuffer[i] = 0;
        }

        
        fetchRoutingTableFromServer(socketToRouter);

        printRoutingTable();


        // #5 Terminér TCP-tilkoblingen med ruter_serveren.
        close(socketToRouter);

    }else{

        // Error!
        printf("Det skjedde en feil ved opprettelsen av TCP-router-klient-socketen!\n");
        exit(0);
        return -1;

    }

    return 0;
}


void fetchRoutingTableFromServer(int serverSocket){


        // #3 Vent på ruter-data sendt med TCP fra hovedprogrammet.
        int bytesRecieved = recv(serverSocket, &TCPBuffer, sizeof(TCPBuffer),0 );

        printf("returnvalue from recv() : %d\n", bytesRecieved);
        int i; 
        for(i = 0; i < 2048 ; i++){
            printf("%d",TCPBuffer[i]);
        }

        int readIndex = 0; 
      
        // #4 Lagre rute-tabellen

        // Først størrelsen som lagres i Table size
        memcpy(&tableSize, &TCPBuffer, sizeof(int));
        tableSize = tableSize / 2 / 4; 


        printf("\nAmount of nextHops stored in the routing table: %d \n", tableSize);
        routingTable = malloc(sizeof(struct RoutingTableNode * ) * tableSize);

        
        readIndex = sizeof(int);

        for(i = 0; i < tableSize ; i++){

            // allokér minne til en struct RoutingTableNode
            routingTable[i] = (struct RoutingTableNode * ) malloc(sizeof(struct RoutingTableNode));

            // Sett noen default verdier så valgrind ikke klager.
            routingTable[i]->destination = 0;
            routingTable[i]->nextHop = 0;

            // Kopiér inn en int som henviser til destinasjon
            memcpy(&routingTable[i]->destination, &TCPBuffer[readIndex], sizeof(int));
            readIndex += sizeof(int);

            // Kopiér inn en int som henviser til neste Node hvis man skal til destinasjonen.
            memcpy(&routingTable[i]->nextHop, &TCPBuffer[readIndex], sizeof(int));
            readIndex += sizeof(int);            
        }



}


int openUDPServerConnection(){

    int socketToRouter = socket(AF_INET, SOCK_DGRAM, 0);

    if(socketToRouter == -1 ){
        printf("\nCreating the socket failed!\n");
        exit(EXIT_FAILURE);
    }

    udpServerAdresse.sin_family = AF_INET;
    udpServerAdresse.sin_port = htons(BasePort+OwnAddress);
    udpServerAdresse.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0 ( localhost ).

    if ( bind(socketToRouter, (struct sockaddr *) &udpServerAdresse, sizeof(udpServerAdresse)) < 0 ){ 
        printf("\nBinding the socket failed!!! :/ Try another port number! :)  \n");
        exit(EXIT_FAILURE); 
    } 

    return socketToRouter;
}


int sendUDPPackagesThroughNetwork(){


        ////  1) les Data.txt & legg inn i et array.

        FILE * dataFile = fopen("data.txt","r");

        char messageBuffer[2048];
        while(!feof(dataFile)){


            // rens messageBufferen!
            int i; 
            for(i = 0; i < 2048; i++){
                messageBuffer[i] = 0;
            }

            
            int destination = 0; 
        
            fscanf(dataFile, "%d", &destination);


            // Les 1 og 1 character i messageBufferen frem til \n eller filens ende!!
            char nextChar = fgetc(dataFile);
            int currReadIndex = 0;
            while(nextChar != '\n' && feof(dataFile) == 0) {
                messageBuffer[currReadIndex] = nextChar;
                currReadIndex++;
                nextChar = fgetc(dataFile);
            }


            messageBuffer[currReadIndex] = '\0';
            // Hvis destination nå er 0 betyr det at alle meldingene er lest fra data.txt.
            if(destination != 0){
                // Legg til 0 terminering må meldinge bufferen.
                messageBuffer[currReadIndex] = '\0';


                ////  2) Konstruér pakker
                printf("\nRead with fscanf dest: %d   message: %s  \n", destination, messageBuffer );    


                /// Hent nextHop 
                int nextHop = getNextHopByDestination(destination);

                // Konstuér UDP-pakken.
                char * package = constructUDPPackage(destination, OwnAddress, messageBuffer);

                // bruk print_pkt funksjonen for å sjekke om pakken er korrekt laget.
                print_pkt(package);

                // Send pakken av gårde dit den skal.
                sendPackageToDesination(nextHop, destination, OwnAddress, package);


                // Frigjør pakken.
                free(package);


               
            }else{

                // Bryt ut av fillesingen
                break;
            }

        }


        fclose(dataFile);


       



    return -1;
}


char * constructUDPPackage(int destination, int source, char message []){
   
    // Format  2 bytes = packet length | 2 bytes = dest address  | 2 bytes = source address | message that must be '\0' terminated.
    
    printf("Constructing package: dest: %d    src: %d     msg:  %s\n", destination, source, message);
    printf("message to be sent: \n");

    int i; 
    for(i = 0; i < strlen(message) ; i++){
        printf("%c", message[i]);
    }

    printf("\n");

    int packageLength = 2 + 2 + 2 + strlen(message) + 1;

    printf("Suggested package length: %d \n", packageLength);

    char * pckg = malloc(2 + 2 + 2 + packageLength + 1);

    char nullByte = '\0';
    memcpy(&pckg[0],&packageLength,2);
    memcpy(&pckg[2],&destination,2);
    memcpy(&pckg[4],&source,2);
    memcpy(&pckg[6],message, packageLength);
    memcpy(&pckg[6+packageLength],&nullByte,1);

    printf("Printing Package Just after copying it. &pckg[6] -> packageLength\n");
    printf("%s", &pckg[6]);
    printf("\nPrinting package OVER.\n");

    return pckg;


}



void printPackage(char * package){

    // read len
    short len = 0 , dest = 0 , source = 0;
    memcpy(&len, &package[0], 2);
    memcpy(&dest, &package[2],  2);
    memcpy(&source, &package[4], 2);

    printf("PackageLength: %d dest: %d   source: %d ", len, dest, source);

    printf("\n data in package: \n");
    int index ;
    for( index = 6 ; index < len - 7 ; index++){
        printf("%c", package[index]);
    }
    printf("\n");

}


int getPackageLength(char * package){
    short len;
    memcpy(&len, &package[0], 2);
    return len;
}



void sendPackageToDesination(int nextHop, int destination, int source, char * package){

     struct sockaddr_in udpClientAddress;

        int outSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if(outSocket < 0){
            printf("UDP-Connection from  %d -> %d failed!\n", OwnAddress, nextHop);
            exit(EXIT_FAILURE);
        }

        // Fyller inn klientens UDP-socket informasjon.
        udpClientAddress.sin_family = AF_INET; 
        udpClientAddress.sin_port = htons(BasePort + nextHop); 
        udpClientAddress.sin_addr.s_addr = INADDR_ANY;


        printf("Sent UDP-message from %d --> %d \n", OwnAddress , nextHop);
        printf("Package:  %s \n", package);

        printPackage(package);

        
        int bytesSent = sendto(outSocket, package , getPackageLength(package), MSG_CONFIRM, (struct sockaddr *) &udpClientAddress, sizeof(udpClientAddress));

        printf("%d bytes were sent:   message was:  %s  \n",bytesSent, &package[6]);


}


void listenForUDPPackages(){


    while(isListening == 1){
    

        char UDPBuffer [2048]; 
        
        int len = sizeof(struct sockaddr); 
        int bytesRecieved = recvfrom(UDPserverSocket, UDPBuffer , 2048 , MSG_CONFIRM, (struct sockaddr *) &udpServerAdresse, &len);
        printf("\nbytesRecieved = %d \n", bytesRecieved);

        int packageLen = 0;
        memcpy(&packageLen, &UDPBuffer[0],2);
        int dest = 0;
        memcpy(&dest, &UDPBuffer[2],2);
        int source = 0;
        memcpy(&source, &UDPBuffer[4],2);
        char * message = &UDPBuffer[6];

        //printf("UDP package recieved: \n msg Len: %d     dest:  %d     source:   %d     \n", packageLen, dest, source);
        printf("Message : %s ", message);

        char messageBuffer [2048];
        memcpy(&messageBuffer, &UDPBuffer[6], packageLen + 1);

        // Bygg opp UDP pakke. OBS !! Denne returnerer en peker som må deallokeres.
        char * package = constructUDPPackage(dest, source, message);


        if(dest == OwnAddress){
            // Hvis pakken skal til denne noden.

            print_received_pkt((short) OwnAddress, (unsigned char *) package );
            
            if(strcmp(message, " QUIT") == 0){
                isListening = -1;
                printf("Quit signal recieved. Terminating node.\n");
            }else{
                printf("Message for this Node : %s ", message);
            }
        
        
        }else if (isDestinationInRoutingTable(dest) == 1){
             // Hvis pakken skal videre.

            printf("The message is supposed to go elsewhere.  \n", messageBuffer);
            printf("The message is now in char message []  = %s \n", messageBuffer);

            print_forwarded_pkt((short) OwnAddress, (unsigned char * ) package);

            // Melding som skal videre; 
            printf("Message after construction: %s ", &message); 

            int nextHop = getNextHopByDestination(dest);

            // Send av gårde pakken til neste node.
            sendPackageToDesination(nextHop, dest, source, package);

             

         }

         // Frigjør minnet som ble allokért for denne pakken!!
        free(package);
    }
}




int isDestinationInRoutingTable(int id){
    int i; 
    printf("\n\n");
    for( i = 0 ; i < tableSize; i++){
        if(id == routingTable[i]->destination){
            return 1;
        }
    }

    return -1;
}

int getNextHopByDestination(int dest){
    int i; 
    printf("\n\n");
    for( i = 0 ; i < tableSize; i++){
        if(dest == routingTable[i]->destination){
            return routingTable[i]->nextHop;
        }
    }
    return -1;
}

void printRoutingTable(){
    int i; 
    printf("\n\n");
    for( i = 0 ; i < tableSize; i++){
        printf("routingTable[%d] destination:%d -- nextHop -> %d \n", i , routingTable[i]->destination, routingTable[i]->nextHop);
    }
}


void freeAllAllocatedMemory (){
    int i;
    for(i = 0; i < tableSize ; i++){
        // Frigjør pekerene til RoutingTableNode
        free((void *) routingTable[i]);
    }
    // Frigjør hele Routing Tabelet
    free(routingTable);
    free(neighbourNodes);
}



int printNodeInfo(){
    printf("Node Information:::::\n----------------\n");
    printf("OwnAddress: %d \n", OwnAddress);
    printf("BasePort  : %d \n", BasePort);
    printf("\n");
    int i = 0; 
    for(; i < neighbourCount ; i++){
        printf("frm: %d  to:   %d   weight: %d \n", neighbourNodes[i].from, neighbourNodes[i].to, neighbourNodes[i].weight);
    }
}







