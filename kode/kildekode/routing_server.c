#include <stdio.h> // For å ha access til printf()
#include <stdlib.h> // For å kunne allokére med malloc()
#include <string.h> // For å kunne bruke memcpy.

#include <unistd.h> // For å kunne stenge en socket med close().
#include <sys/socket.h> // For det standardisérte socket-API'et.
#include <sys/types.h> // For noen av typene som kreves for å få sockets til å funke.

#include <netinet/in.h>

#include "../print_lib/print_lib.h"
#include "Networking.h" // Inneholder structer.



int PORT = 0;
int N = 0;  // N = antall noder i systemet.

char TCPBuffer [2048];

struct NodeSocket ** nodeSockets;
int currentNodeSocketCount = 0;


void initializeTCPServer(void);
void printBuffer(char * arr, int len);
void printRoutingTables(struct NodeSocket * sockets []);
void savePathRecursivelyToIntArray(int * prev  , int index, int * destinationArray, int currIndex);


int main(int argc, char* argv[]){

    printf("Hello I'm routing_server.c %d \n",argc);

    int i;

    printf("All Arguments.\n");
    for(i = 0; i < argc ; i ++){

        printf("argv[%d] : %s \n", i , argv[i]);

        if(i == 1)
            PORT = atoi(argv[i]);

        if(i == 2){
            N = atoi(argv[i]);
        }
    }


    initializeTCPServer();   

    // 2) Regn ut korteste paths og lagre pathene i NodeSockets structene!!
    FindDijkstrasShortestPaths(nodeSockets, 1);  
    

    // Regn ut Routing tabellene og lagre det i nodeSocketene!
    CalculateRoutingTableForAllNodeSockets(nodeSockets);

    // Print Routing Tables.
    printRoutingTables(nodeSockets);



    sendBackRoutingTablesToAllNodeSockets(nodeSockets);



    freeAllAllocatedMemory();


    

    




    return 0;
}


int nodeConnectionsCountSoFar = 0;


void initializeTCPServer(){


    // Lag en NodeSocket liste som alle nodene kan lagres i. ( Vil brukes som Graf for Dijsktras senere ).
    
    nodeSockets = malloc(sizeof(struct NodeSocket *) * N);
    


    // SOCK_STREAM betyr TCP-type socket.
    int ruterServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAdresse;
    
    serverAdresse.sin_family = AF_INET;
    serverAdresse.sin_port = htons(PORT);
    serverAdresse.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0 ( localhost ).


    // Bind andressen ( dvs start socketen på en måte )
    bind(ruterServerSocket, (struct sockaddr *) &serverAdresse, sizeof(serverAdresse) );
    printf("bind() called.\n");

    // Og start å lytte på innkommende connections - N er max antall lovlige connections.
    listen(ruterServerSocket, N);

    printf("listen() called.\n");


    
    while(nodeConnectionsCountSoFar < N){

        printf("N = %d,   nodeConnectionsCountSoFar = %d\n",N, nodeConnectionsCountSoFar);

        int client_socket = accept(ruterServerSocket, NULL, NULL);
        printf("accept() called!\n");

        if(client_socket == -1){

            printf("Accept() called but client_socket has a failure value.\n");

        }else{
            printf("A TCP-client connected!!\n client_socket_id = %d \n", client_socket);

            // Handle Storage of the client structre here:
            nodeSockets[currentNodeSocketCount] = malloc(sizeof(struct NodeSocket) );
            nodeSockets[currentNodeSocketCount]->socketID = client_socket;
            nodeSockets[currentNodeSocketCount]->nodeID = -1;
            nodeSockets[currentNodeSocketCount]->pathFrom1 = NULL;
            nodeSockets[currentNodeSocketCount]->nodes = NULL;
            nodeSockets[currentNodeSocketCount]->shortestDistTo1 = 0;
            currentNodeSocketCount ++;

            int bytesRecieved = recv(client_socket, &TCPBuffer, sizeof(TCPBuffer),0);

            if(bytesRecieved > 0){

                printf("%d bytes Recieved!! : Recieved message from socket: %d   message:%s\n", bytesRecieved, client_socket, TCPBuffer);


                // Finn frem korrekt NodeSocket struct med informasjon om en node basert på socketID'en vi fikk fra socket()!
                struct NodeSocket * currSock = getNodeSocketBySocketId(nodeSockets, currentNodeSocketCount, client_socket);

                // Kopiér inn nodens ID i NodeSocket structen.
                memcpy(&currSock->nodeID, &TCPBuffer, sizeof(int));
                int readIndex = sizeof(int);

                // Tell alle vektene som kom inn fra recv()!!
                while(TCPBuffer[readIndex] != '\0'){
                    readIndex += sizeof(int) * 2;
                }
                int amountOfEdgeWeights = readIndex / 4 / 2;
                printf("amountOfEdgeWeights %d \n", amountOfEdgeWeights);



                // Lag dynamisk allokért struct NodeInfo peker(!) Array.
                currSock->nodes = malloc(sizeof(struct NodeInfo * ) * amountOfEdgeWeights);
                currSock->nodeCount = amountOfEdgeWeights;


                readIndex = sizeof(int);
                // Kopiér inn alle vektene.
                int curEdgeWeightIndex = 0;
                while(TCPBuffer[readIndex] != '\0'){

                    // Allokér dynamisk minne for en struct NodeInfo! For å holde Weight / Edges.
                    currSock->nodes[curEdgeWeightIndex] = malloc( sizeof(struct NodeInfo) );

                    int to     =  TCPBuffer[readIndex];
                    readIndex += 4; 
                    int weight =  TCPBuffer[readIndex];
                    readIndex += 4;
                    printf("nodeID[%d] has Edge/weight;  to:  %d     weight:    %d \n",currSock->nodeID, to,weight );

                    currSock->nodes[curEdgeWeightIndex]->OwnAddress = currSock->nodeID;
                    currSock->nodes[curEdgeWeightIndex]->from = currSock->nodeID;
                    currSock->nodes[curEdgeWeightIndex]->to = to;
                    currSock->nodes[curEdgeWeightIndex]->weight = weight;
                    currSock->nodes[curEdgeWeightIndex]->BasePort = PORT + currSock->nodeID;


                    curEdgeWeightIndex++;
                }



                // Tøm TCP bufferen.
                clearTCPBuffer();



            } else if(bytesRecieved == 0){

                printf("No bytes were recieved, but a sending-connection was made from a client.\n");

            } else{

                printf("An error occured with reception of data!\n");   
            }

                
                




            if(currentNodeSocketCount > N){
                printf("Amount of nodes connected in the router exceeded the amount specified at start of Program: Max Sockets is: %d \n",N);
                exit(0);
            }
           

            

        }


        // Øk antallet noder som har koblet seg til med TCP til nå !! 
        nodeConnectionsCountSoFar++;


    }

    printf("All nodes currently in system: \n");
    printAllNodeSockets(nodeSockets, currentNodeSocketCount);

    
    printAllEdgesAndWeights(nodeSockets, currentNodeSocketCount);



    // Når koden når dette punktet har alle nodene sendt nabo-vekt dataen sin over TCP!



    // 1) LAG tilstrekkelig lagringsplass for korteste paths ! :) 

    // I mitt system lagres korteste paths i NodeSocket. 
    // De allokéres dynamisk.

    printf("Allocating memory for storing Paths in each NodeSocket.");
    int i; 
    for(i = 0; i < currentNodeSocketCount ; i++) {
        nodeSockets[i]->pathFrom1 = malloc(sizeof(int) * currentNodeSocketCount);

        int j;
        for(j = 0; j < currentNodeSocketCount ; j++){
            nodeSockets[i]->pathFrom1[j] = -1;
           // printf("nodesockets[i]->pathFRom1[j] = %d", nodeSockets[i]->pathFrom1[j]);
        }
        printf("\n");
    }
    

}


void printRoutingTables(struct NodeSocket * nodeSockets []){
    int i;
    for(i = 0; i < N; i++){
        int j; 
        for(j = 0; j < N; j++){
            printf("nodeId: %d -> [%d] = %d\n",nodeSockets[i]->nodeID, nodeSockets[j]->nodeID, nodeSockets[i]->routingTable[j]);
        }
    }
}

void sendBackRoutingTablesToAllNodeSockets(struct NodeSocket * nodeSockets []) {


    printf("\n\nSending back routing Tables over TCP: \n")  ;

    // RoutingTable struktu:
    // len        | from    |  to        |   .... |  from      |    to        | '\0'
    // 4 bytes    | 4 bytes |  4 bytes   |        |  4 bytes   |    4 bytes   |

    
    int bufferIndex = 0;
    int i; 
    for( i = 0 ; i < N; i++){

        printf("\nSending back Routing Table for nodeID:  %d\n", nodeSockets[i]->nodeID);
        clearTCPBuffer();
        bufferIndex = sizeof(int);
        
        int j, validPairCount = 0;
        for(j = 0; j < N ; j++){
            
            
            // Kopiér rute data inn i bufferen som vil sendes over TCP tilbake til Noden.     
            if(nodeSockets[i]->routingTable[j] != -1 && nodeSockets[j]->nodeID != 1){
                // memcpy(dest, src, size_t)
                memcpy(&TCPBuffer[bufferIndex], &nodeSockets[j]->nodeID, sizeof(int));  
                bufferIndex += sizeof(int);
                memcpy(&TCPBuffer[bufferIndex], &nodeSockets[i]->routingTable[j], sizeof(int));     
                bufferIndex += sizeof(int);
                validPairCount++;
            }
            
        }

        // Lagre lengden ( i bytes ) på dataen som sendes over til Noden. 
        // I TCP bufferen som skal sendes over til node.
        int validDataLength = validPairCount * 4 * 2;
        char nullChar = '\0';
        memcpy(&TCPBuffer, &validDataLength, sizeof(int) );
        memcpy(&TCPBuffer[bufferIndex], &nullChar, 1);

        printf("DataSize: %d bytes. + 4 bytes for this int.\n", validDataLength) ;

        int bytesSent = send(nodeSockets[i]->socketID, &TCPBuffer, sizeof(TCPBuffer), 0);

    }

}


void clearTCPBuffer(){
    int i;
    for(i = 0; i < 2048 ; i++ ){
        TCPBuffer [i] = 0;
    }

}


void printBuffer(char * arr, int len){
    int i;
    for(i = 0; i < len-1; i++){
        printf("%d ",arr[i]);
    }
}

void freeAllAllocatedMemory(){

    // Frigjør dynamisk allokért minne fra NodeSockets.
    
    int i;
    for(i = 0; i < currentNodeSocketCount ; i++){
        
        // Frigjør nabopekerene.
        int j; 
        for(j = 0; j < nodeSockets[i]->nodeCount; j++){
            free(nodeSockets[i]->nodes[j]);         
        }

        // Frigjør pathen.
        free(nodeSockets[i]->pathFrom1);

        // frigjør routing tabellen
        free(nodeSockets[i]->routingTable);

        // Frigjør NodeSocketene's array peker til noder.
        free(nodeSockets[i]->nodes);

        // Frigjør noden selv.
        free(nodeSockets[i]);
    }

    // Frigjør pekeren til arrayet av NodeSockets.
    free(nodeSockets);

}


void printAllNodeSockets(struct NodeSocket * sockets [], int len ){

    int i = 0;
    for (; i < len ; i++){

        printf("\nNodeID   : %d ", sockets[i]->nodeID);
        printf("\nSocketID : %d ", sockets[i]->socketID);

        printf("\n\n All neighbours\n");
        int j = 0;
        for(j = 0; j < sockets[i]->nodeCount ; j++){
            printf("From: %d  to: %d  weight: %d\n",  sockets[i]->nodes[j]->from, sockets[i]->nodes[j]->to, sockets[i]->nodes[j]->weight);
        }


        // Print path hvis det finnes en.
        printf("\nprint Path: \n");
        if(sockets[i] != NULL) {
            if(sockets[i]->pathFrom1 != NULL){
                for(j = 0; sockets[i]->pathFrom1[j] != -1; j++){
                    printf("%d ",sockets[i]->pathFrom1[j]);

                }
            }
        }
        
        printf("\n\n");
    
    }  

}



struct NodeSocket * getNodeSocketBySocketId(struct NodeSocket * sockets [], int len,  int socketID){
    int i;
    for(i = 0; i < len; i++  ){
        if(sockets[i]->socketID == socketID){
            return sockets[i];
        }
    }

    return NULL;

}


#define INFINITE_MAXIMA 99392392 // Egentlig infinity, trengte bare å gjøre koden litt morsom.



// Min implementasjon av Dijkstras er inspirert av pseudokoden på Wikipedia om algoritmen!
// https://en.wikipedia.org/wiki/Dijkstra's_algorithm
void FindDijkstrasShortestPaths(struct NodeSocket * nodes [], int startNode){

    printf("Dijkstra has started!!");
    int N = currentNodeSocketCount;
    int Q[N];
    

    int dist[N]; // Denne vil holde avstanden til de ulike nodene.
    int prev[N]; // Denne vil holde forrige data.

    int i ;   /// Her er i indeksen til de ulike nodene i NodeSocket nodes[] arrayet.
    for(i = 0; i < N; i++){
        dist[i] = INFINITE_MAXIMA;
        prev[i] = -1;
        Q[i] = 1;
    }

    int startNodeIndex = getIndexOfNodeSocketWithNodeID(startNode);
    printf("\n\nstartNode: %d     startNodeINdex : %d \n\n", startNode, startNodeIndex);

    // Avstanden til startNoden vil alltid være 0.
    dist[startNodeIndex] = 0;


    // Er Q tomt enda? Hvis ikke kalkulér path.
    while(isArrayEmpty(&Q,N) == -1){
        printf("isArrayEmpty  %d \n", isArrayEmpty(&Q,N));

        // Finn min dist[u];
        int u = findIndexInQWithMinDist(&Q, &dist, N);
        
        Q[u] = -1;


        // Bla igjennom alle naboene i noden vi jobber med for øyeblikket.
        int i, alt;
        struct NodeSocket * currentNode = nodes[u]; 
        for (i = 0; i < currentNode->nodeCount; i++ ){

            int v = -1;
            v = getIndexOfNodeSocketWithNodeID(currentNode->nodes[i]->to);

            // Hvis elementet allerede er tatt ut av Q - skip denne runden. 
            int res = isIndexInArrayEmpty(&Q,N,v);
            if(res == 1)
                continue;
            


            int distanceBetweenNodes = lengthBetweenNodes(currentNode, i);
            alt = dist[u] + distanceBetweenNodes;

            
            if(alt < dist[v]){
                dist[v] = alt;
                prev[v] = u;
            }

        }

    }


    // 2) Lagre den korteste path til 1 i hver enkelt NodeSockets[i]->pathFrom1 array!
    for (i = 1; i < N; i++) { 
        savePathRecursivelyToIntArray(prev, i, nodes[i]->pathFrom1, 0); 
        nodes[i]->shortestDistTo1 = dist[i];
        // Reversér arrayet siden det med denne metoden lagres baklengs.
        reversePathArray(nodes[i]->pathFrom1, N);
    } 

    printAllNodeSockets(nodes, N);


}

int isIndexInArrayEmpty(int * arr, int len, int index){
    // Hvis indexen vi skal sjekke er uten for arrayet; return -1.
    if(index > len)
        return 1;

    // Hvis verdien i arrayet til indexen er -1 returner 1 !
    // Jeg definerer en verdi på -1 på indexen i arrayet til å være 'TOM'.
    if(arr[index] == -1){
        return 1;
    }else{
        return -1;
    }

}

int lengthBetweenNodes(struct NodeSocket * node, int index){
    return node->nodes[index]->weight;
}

// Returnerer indexen til elementet med den korteste distansen.
int findIndexInQWithMinDist(int * Q, int * dist, int len){
    int i;
    int minDist = INFINITE_MAXIMA;
    int indexOfMinDist = -1;
    for(i = 0; i < len; i++){

        // Sjekk at elementet er i Q og sjekk deretter om dist er mindre.
        if(Q[i] != -1 && dist[i] <= minDist){
            minDist = dist[i];
            indexOfMinDist = i;
        }
    }
    
    return indexOfMinDist;
}
   

// Hvis ETT av elementene i arrayet vi får inn som argument er -1 ( som jeg bruker i programmet til å peke på tomhet )    
// Så returnerer funksjonen -1, som betyr; NEI arrayet er ikke tomt.
int isArrayEmpty(int * arr , int len){
    int i; 
    for(i = 0; i < len; i++){
        if(arr[i] != -1){
            return -1;
        }
    }

    return 1;
}

void CalculateRoutingTableForAllNodeSockets(struct NodeSocket * nodes []){

    // RoutingTable er lagret inni hver enkelts NodeSocket som en int *  !!


    int i; 
    for(i = 0; i < N; i++){

        // Allokér minne for routingTable
        nodes[i]->routingTable   = malloc(sizeof(int) * N);

        int j;

        nodes[i]->routingTable[0] = -1;

        // Sett the first node which tells us about the startingNode
        for(j = 1; j < N; j++){

            int k;
            // Sett standard verdi for routingtabellen.
            nodes[i]->routingTable[j] = -1;

            // Bla igjennom alle de korteste pathene til alle nodes.
            for(k = 0; k < N; k++){
                // Hvis noen av nodene har denne noden's ID:
                if(nodes[j]->pathFrom1[k] == nodes[i]->nodeID){
                    // Lagre den.
                    if(nodes[j]->pathFrom1[k+1] != -1){
                        nodes[i]->routingTable[j] = nodes[j]->pathFrom1[k+1];
                    }
                }
            }
            
            // Hvis denne noden er nevnt i en annen nodes path.
            // Må denne noden's routingTable vite om det.
        }

    }

}


int getIndexOfNodeSocketWithNodeID(int nodeID){
    int i ;
    for(i = 0; i < N;i++){
        if(nodeSockets[i]->nodeID == nodeID)
            return i;
    }
    // return -1 Hvis noe feilet. Hvis indexen ikke finnes.
    return -1;
}

void printAllEdgesAndWeights(struct NodeSocket * sockets [], int len){
    printf("All Edges / Weights \n (nodeID)  ---- weight --->  (nodeTargetID)  \n");
    int i = 0, j = 0;
    for (; i < len ; i++){
        for(j = 0; j < sockets[i]->nodeCount ; j++){
            printf(" (%d)  ----- %d ------>  (%d)  \n", sockets[i]->nodes[j]->from, sockets[i]->nodes[j]->weight, sockets[i]->nodes[j]->to);
            print_weighted_edge( (short)sockets[i]->nodes[j]->from, (short)sockets[i]->nodes[j]->to, sockets[i]->shortestDistTo1);
            
        }
    }  

}


void savePathRecursivelyToIntArray(int * prev  , int index, int * destinationArray, int currIndex) { 
      
    if(prev[index] == -1){
        // Legg til den siste indexen til pathen.
        destinationArray[currIndex] = 1;
        return;
    }
    destinationArray[currIndex] = nodeSockets[index]->nodeID;
    currIndex++;
    savePathRecursivelyToIntArray(prev, prev[index], destinationArray, currIndex); 
    
    return;
} 
  
int reversePathArray(int * array, int len ){

    int temp[len];
    int i;
    int amountOfNodesInPath = 0;
    for(i = 0; i < len; i++){
        if(array[i] != -1){
            temp[i] = array[i];
            amountOfNodesInPath++;
        }
    }

    for(i = 0; i < amountOfNodesInPath-1; i++){
        array[i] = temp[amountOfNodesInPath-i-1];
    }
    array[amountOfNodesInPath-1] = temp[0];
}








