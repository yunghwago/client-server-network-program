//Christopher Go
//game_server.cpp
//server used to interact with game_client.cpp

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <time.h>

#include <fstream> //used for file input
#include <ctime>    //for time()
#include <cstdlib>  //for srand() and rand()

using namespace std;

const int MAX_ARGS = 2; //max args for the server
const int MAX_NUM = 3;  //max number of people on the leaderboard
const int PORT_ARG = 1; //const used to identify which arg is the portno
const int MAX_PENDING = 5; //max no of pending requests

const int MINPORT = 10270; //hard coded assigned port no
const int MAXPORT = 10279; //hard coded assigned port no

//struct that holds connection info as well as roundCount + name
struct clients
{
  int sock;
  int roundCount;
  string name;
};

//struct for single entry of the leaderboard
//contains user name and score
//score is (guesses)/(letters in word)
struct leaderBoard
{
  vector<string> names;
  vector<float> scores;
};

//global variables for leaderboard and its lock
leaderBoard leadBoard;
pthread_mutex_t boardLock;

//game pthread
void* game(void* newClient);

//receive and send functions for various data types
long receiveLong(clients connectInfo, bool &abort);
void sendLong(long num, clients connectInfo);

string receiveString(clients connectInfo, bool &abort);
void sendString(string msg, clients connectInfo);

char receiveChar(clients connectInfo, bool &abort);

//all leaderboard-related functions
void initBoard(); //constructor for leaderboard
void sendBoard(leaderBoard lBoard, clients connectInfo); //send leaderboard to client
//converts floats into strings while doing so, did this because i didn't know how to
//implement a sendFloat function
void checkBoard(string name, long round, int wordLength);
//insert a high (technically low) score into the leaderboard
void insertBoard(int index, string name, long rounds, int wordLength);
//display the leaderboard on the server
void printBoard(leaderBoard board);

//additional helper functions
bool initializeFile(string filename); //read in file from filename
string getNthLine(const string& filename, int N); //select the Nth line of the filename
void printLine(); //line printing function

int main(int argc, const char * argv[])
{
  //print an error for an incorrect amount of arguments
  if(argc != MAX_ARGS){
    cerr << "Invalid number of arguments." << endl;
    cerr << "Usage: [executable] [IP] [port]" << endl;
    exit(-1);      
  }

  //connect to the server. exit with an appropriate error message if a
  //connection could not be established.

  //assign port number to port number in argument
  unsigned short portNum = (unsigned short)strtoul(argv[PORT_ARG], NULL, 0);
  if(portNum > MAXPORT || portNum < MINPORT){
    cerr << "Port number is either above " << MAXPORT << " or " << endl
         <<"below " << MINPORT << endl;    
    exit(-1);
  }
  int status;            
  int clientSock;        
  
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    cerr << "Error with setting socket value. Now exiting program. " << endl;
    close(sock);
    exit (-1);
  }
 
  initBoard();
  pthread_mutex_init(&boardLock, NULL);
  
  
  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(portNum);
  
  status = ::bind(sock,(struct sockaddr *) &servAddr, sizeof(servAddr));
  if (status < 0){
    cerr << "Error occured when binding socket. Now exiting program. " << endl;
    close(sock);
    exit (-1);
  }
  
  status = listen(sock, MAX_PENDING);
  cerr << "Currently waiting for a new client to connect to the server..." << endl;
  if (status < 0){
    cerr << "Error with listening. Now exiting program. " << endl;
    close(sock);
    exit (-1);
  }
  
  while(true){
    pthread_t tid;    
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    clientSock = accept(sock,(struct sockaddr *) &clientAddr, &addrLen);
    if (clientSock < 0) {
      cerr << "Error with accepting socket. Now exiting program. " << endl;
      close(clientSock);
      exit(-1);
    }
	
    clients *newClient = new clients;
    newClient->sock = clientSock;
    
    status = pthread_create(&tid, NULL, game, (void*)newClient);
    if(status){
      cerr<<"Error creating threads, return code is "<< status << ". Now exiting " <<endl;
      close(clientSock);
      exit(-1);
    }
    cout << "Client thread has started." << endl;
  }
}


void* game(void* newClient)
{  
  clients *new_Client;
  new_Client = (clients*)newClient;
  
  srand(time(NULL)); 
  new_Client->roundCount = 0;   
  string name; //username
  long roundCount = 0; //# of rounds so far 
  bool won = false; //set to true if all letters have been guessed
  
  string victoryMess = "You won! You have won "; //send this to the client when they win 
  bool exit = false;
  
  int NUM_WORDS = 57127; //hardcoded number of lines in the file
  string filename = "/home/fac/lillethd/cpsc3500/projects/p4/words.txt"; //hard coded file directory
  string wordToGuess = ""; //holder for random word selected from file
  const int MAX_WORD_LENGTH = 50; //hardcoded max word length
  char guessedArray [MAX_WORD_LENGTH];
  int lettersGuessed = 0; //letters guessed correctly
  char clientGuess; //letter that the client is currently choosing to guess
  pthread_detach(pthread_self());
  
  name = receiveString(*new_Client, exit); //get client name
  
  cout <<  name << " has connected! " << endl;
  
  //get random word from text file and initialize other values
  //initialize lettersGuessed array
  for(int i = 0; i < MAX_WORD_LENGTH; i++)
    guessedArray[i] = '_';
  
  //randomly generate n between 0 and NUM_WORDS
  srand(time(0));
  int random = (rand() % 57127) + 1;
  
  //get line based on random value
  wordToGuess = getNthLine(filename, random);
  
  cout << "Word to guess: " << wordToGuess << endl;
  sendLong(wordToGuess.length(), *new_Client);
  
  while(lettersGuessed != wordToGuess.length() && !exit){
    new_Client->name = name;
    cout << endl << endl;
    
    do{
      clientGuess = receiveChar(*new_Client, exit);
      
      if(!exit){
        cout << name << " guessed " << clientGuess << endl;
        for(int i = 0; i < wordToGuess.length(); i++){
          if(wordToGuess[i] == clientGuess){
            cout << clientGuess << " was a correct guess for letter "
                 << i << endl;
            lettersGuessed++;
          }
        }              
      }
      else{
        won = true;
        break;
      }
      sendLong(lettersGuessed, *new_Client);
      roundCount++;
      cout << "Rounds: " << roundCount << endl;
      cout << "Letters correctly guessed: " << lettersGuessed << endl;
      if(lettersGuessed == wordToGuess.length()){
        won = true;
        sendString(victoryMess, *new_Client);
      }
      
    }while(!won);
    
    if(!exit){
      cout << name << " has won!" << endl;
      
      pthread_mutex_lock(&boardLock);
      checkBoard(name, roundCount, wordToGuess.length());
      pthread_mutex_unlock(&boardLock);
      
      cout << "Current Leaderboard: "<< endl;
      printBoard(leadBoard);
      sendBoard(leadBoard, *new_Client);
    }
    
  }
  if(exit){
    cerr << endl << "A user has left before the game has ended! " << endl;
  }
  
  cout << endl << "Now waiting for a new client to connect!" << endl;
  close(new_Client->sock);
  pthread_exit(NULL);
}

//helper Functions
//all leaderboard-related functions
//constructor for leaderboard
void initBoard()
{
  for(int i = 0; i < MAX_NUM; i++){
    leadBoard.scores.push_back(0);
    leadBoard.names.push_back("-----");
  }
}

//send leaderboard data to client
void sendBoard(leaderBoard lBoard, clients connectInfo)
{
  string stringHolder;
  for(int i = 0; i < MAX_NUM; i++){
    //change this to send float eventually
    stringHolder = to_string(lBoard.scores[i]); //this line is why I'm using C++11
    sendString(stringHolder, connectInfo);
    sendString(lBoard.names[i], connectInfo);
  }
}

void checkBoard(string name, long round, int wordLength)
{
  int indexToPass = 0;
  bool leader = false;
  
  for(int i = (MAX_NUM-1); i >= 0; i--){
    if(leadBoard.scores[i] > round || leadBoard.scores[i] == 0){
      leader = true;
      indexToPass = i;
    }
  }
  if(leader)
    insertBoard(indexToPass, name, round, wordLength);  
}

void insertBoard(int index, string name, long rounds, int wordLength)
{
  int roundTemp = rounds;
  string nameTemp = name;

  //score = guesses/wordlength
  float score = (float)rounds/(float)wordLength;

  bool placed = false;
  //first place
  if(!placed){
    if(score < leadBoard.scores[0] || leadBoard.scores[0] == 0){
      leadBoard.scores[2] = leadBoard.scores[1];
      leadBoard.scores[1] = leadBoard.scores[0];
      leadBoard.scores[0] = score;
      
      leadBoard.names[2] = leadBoard.names[1];
      leadBoard.names[1] = leadBoard.names[0];
      leadBoard.names[0] = name;
      placed = true;
    }
  }
  //second place
  if(!placed){
    if((score < leadBoard.scores[1] && score > leadBoard.scores[0])|| leadBoard.scores[1] == 0){
      leadBoard.scores[2] = leadBoard.scores[1];
      leadBoard.scores[1] = score;
      
      leadBoard.names[2] = leadBoard.names[1];
      leadBoard.names[1] = name;
      placed = true;
    }
  }
  //third place
  if(!placed){
    if((score < leadBoard.scores[2] && score > leadBoard.scores[0] && score > leadBoard.scores[1])
       || leadBoard.scores[1] == 0){
      leadBoard.scores[2] = score;
      
      leadBoard.names[2] = name;
      placed = true;
    }
  }
}

void printBoard(leaderBoard board)
{
  cout << endl;
  cout << "Leaderboard" << endl;
  printLine();
  for(int i = 0; i < MAX_NUM; i++){
    if(board.scores[i] != 0)
      cout << i+1 << ". " << board.names[i] <<  " " << board.scores[i] << endl;
  }
}

long receiveLong(clients connectInfo, bool &abort)
{
    int bytesLeft = sizeof(long);
    long networkInt;
    char *bp = (char*) &networkInt;
    
    while(bytesLeft > 0){
      int bytesRecv = recv(connectInfo.sock, (void*)bp, bytesLeft, 0);
      if(bytesRecv <= 0){
        abort = true;
        break;
      }
      else{
        bytesLeft = bytesLeft - bytesRecv;
        bp = bp + bytesRecv;
      }
    }
    if(!abort){
        networkInt = ntohl(networkInt);
        return networkInt;
    }
    else
      return 0;
}

void sendLong(long num, clients connectInfo)
{
  long temp = htonl(num);
    int bytesSent = send(connectInfo.sock, (void *) &temp, sizeof(long), 0);
    if (bytesSent != sizeof(long)){
      cerr << "Error sending long! Now exiting. ";
      close(connectInfo.sock);
      exit(-1);
    }
}

string receiveString(clients connectInfo, bool &abort)
{
  int bytesLeft = receiveLong(connectInfo, abort);
  if(!abort){
    char msg[bytesLeft];
    char *bp = (char*)&msg;
    string temp;
    while(bytesLeft > 0){
      int bytesRecv = recv(connectInfo.sock, (void*)bp, bytesLeft, 0);
      if(bytesRecv <= 0){
        abort = true;
        break;
      }
      else{
        bytesLeft = bytesLeft - bytesRecv;
        bp = bp + bytesRecv;
      }
    }
    if(!abort){
      temp = string(msg);
      return temp;
    }
  }
  return "-----";
}

void sendString(string msg, clients connectInfo)
{
  long msgSize = (msg.length() + 1);
  char msgSend[msgSize];
  strcpy(msgSend, msg.c_str());
  
  sendLong(msgSize, connectInfo);
  int bytesSent =  send(connectInfo.sock, (void *)msgSend, msgSize, 0);
  if (bytesSent != msgSize){
    close(connectInfo.sock);
    exit(-1);    
  }
}

char receiveChar(clients connectInfo, bool &abort)
{
  int bytesLeft = sizeof(char);
  char networkInt;
  char *bp = (char*) &networkInt;

  while(bytesLeft > 0){
    int bytesRecv = recv(connectInfo.sock, (void*)bp, bytesLeft, 0);
    if(bytesRecv <= 0){
      abort = true;
      break;
    }
    else{
      bytesLeft = bytesLeft - bytesRecv;
      bp = bp + bytesRecv;
    }
  }
  if(!abort){
    return networkInt;
  }
  else
    return 0;
}


string getNthLine(const string& filename, int N)
{
  ifstream in(filename.c_str());

  string s;

  //skip N lines
  for(int i = 0; i < N; ++i)
    std::getline(in, s);

  getline(in,s);
  return s;
}

//small line printing function
void printLine(){
  for(int i = 0; i < 20; i++)
    cout << "-";
  cout << endl;
}
