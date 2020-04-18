//Christopher Go
//game_client.cpp
//client to send user input to p4_server.cpp

//included libraries
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <string>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>

using namespace std;

const int MAX_ARGS = 3; //maximum amount of arguments to be passed in at all
const int CLIENT_ARGS = 2; //arguments = [IP] [portno]
const int SERVER_ARGS = 1; //arguments = [portno]
const int LEADERBOARD_SPOTS = 3; //leaderboard spots

const int MINPORT = 10270; //hard coded assigned port no
const int MAXPORT = 10279; //hard coded assigned port no

//struct for single entry in leaderboard
struct leaderBoard{
  vector<string> names;
  vector<string> score;
};

//send and receive functions for various data types
void sendLong(long num, int sock);
long receiveLong(int sock);

void sendChar(char guess, int sock);

void sendString(string msg, int sock);
string receiveString(int sock);

//leaderboard-related functions
void printBoard(leaderBoard board); //print the leaderboard
leaderBoard receiveBoard(int sock); //recieve information for the leaderboard

//line printing function
void printLine();

int main(int argc, const char * argv[])
{
  int status;
  bool won = false; //is the game over yet?
  string nameHolder; //username
  string victoryMess; //victory message that displays when the user has won
  long roundCount = 0; //amount of rounds that have elapsed so far
  bool goodInput; //whether or not the user input is a letter

  char guess; //user's single letter guess
  char lettersGuessed [27]; //letters guessed so far. 27 because that's how many letters are
  //in the alphabet
  string wordToGuess; //the actual word the user needs to guess
  long wordToGuessLength = 0; //# of letters the user needs to guess correctly
  int numGuessed = 0; //# of letters guessed correctly so far
  long correctGuessed = 0;
  bool duplicate; //has the guess been guessed before?
  
  //print an error for an incorrect amount of arguments
  if(argc != MAX_ARGS){
    cerr << "Invalid number of arguments." << endl;
    cerr << "Usage: [executable] [IP] [port]" << endl;
    exit(-1);
  }

  //connect to the server. exit with an appropriate error message if a
  //connection could not be established.
  
  //assign port number to port number in argument
  unsigned short portNum = (unsigned short)strtoul(argv[CLIENT_ARGS], NULL, 0);
  if(portNum > MAXPORT || portNum < MINPORT){
    cerr << "Port number is either above " << MAXPORT << " or " << endl
         << "below " << MINPORT << endl;
    exit(-1);
  }
  
  unsigned long servIP;

  //convert dotted decimal address to int
  status = inet_pton(AF_INET, argv[SERVER_ARGS], (void *) &servIP);
  if(status <= 0)
    exit(-1);
  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET; //always AF_INET
  servAddr.sin_addr.s_addr = servIP;
  servAddr.sin_port = htons(portNum);


  //initializing the socket & checking if there is an error
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    cerr << "Error occured on server side with socket" << endl;
    exit (-1);
  }

  cout << "Now attempting to connect to the server." << endl;

  //connect to the IP Address with the given socket
  status = connect(sock, (struct sockaddr*) &servAddr, sizeof(servAddr));
  if (status < 0) {
    cerr << "Error with connect" << endl;
    exit (-1);
  }
  cout << "Connected successfully!" << endl;

  //start the actual game
  printLine();
  cout << "HANGMAN" << endl;
  printLine();
  
  //initialize letters guessed array
  for(int i = 0; i < 27; i++)
    lettersGuessed[i] = '_';
  //receiving name of user & sending it to the server
  cout << "What is your name: ";
  cin >> nameHolder;
  sendString(nameHolder, sock);
  wordToGuessLength = receiveLong(sock);
  cout << "Welcome " << nameHolder << endl;
  printLine();

  //run the game until the user has either force-exited or won
  do{    
    cout << "Turn: " << (roundCount+1) << endl;    
    goodInput = false;
    while(!goodInput){
      cout << "Please guess a letter from A-Z." << endl;      
      cin >> guess;

      //check if user guess is valid
      if(isalpha(guess)){// is the guess a letter?
        //convert letter guessed to upper case
        guess = toupper(guess);
        duplicate = false;
        int i = 0;
        //has the letter already been guessed?
        for(int i = 0; i < 27; i++){
          if(guess == lettersGuessed[i]){
            duplicate = true;            
          }
        }
        if(duplicate){        
          cout << "Letter has already been guessed" << endl;          
        }
        else{
          lettersGuessed[numGuessed] = guess;
          numGuessed++;
          goodInput = true;          
        }
      }
      else
        cout << "Input is not valid." << endl;      
    }
    printLine();
    sendChar(guess, sock);
    
    //count the amount of letters guessed correctly
    correctGuessed = receiveLong(sock);
    roundCount++;
    cout << "Letters correct: " << correctGuessed << "/" << wordToGuessLength << endl;

    //win condition
    if(correctGuessed == wordToGuessLength){ 
      won = true;
      victoryMess = receiveString(sock);
    }
  }while(!won);
  
  cout << victoryMess << "in " << roundCount << " turns!" << endl;

  leaderBoard currBoard = receiveBoard(sock);
  
  printBoard(currBoard);
  
  status = close(sock);
  if (status < 0){
    cerr << "Error occured when closing the socket." << endl;
    exit (-1);
  }
}


//helper Functions

//send and recieve functions for various data types
void sendLong(long num, int sock)
{
  long temp = htonl(num);
  int bytesSent = send(sock, (void *) &temp, sizeof(long), 0);
  if (bytesSent != sizeof(long))
    exit(-1);
}

long receiveLong(int sock)
{
  int bytesLeft = sizeof(long);
  long networkInt;
  char *bp = (char*) &networkInt;

  while(bytesLeft > 0)
    {
      int bytesRecv = recv(sock, (void*)bp, bytesLeft, 0);
      if(bytesRecv <= 0){
        break;
      }
      else{
        bytesLeft = bytesLeft - bytesRecv;
        bp = bp + bytesRecv;
      }
    }
  networkInt = ntohl(networkInt);
  return networkInt;
}

void sendString(string msg, int sock)
{
  long msgSize = (msg.length() + 1);
  char msgSend[msgSize];
  strcpy(msgSend, msg.c_str());

  sendLong(msgSize, sock);
  int bytesSent =  send(sock, (void *)msgSend, msgSize, 0);
  if (bytesSent != msgSize)
    exit(-1);
}

string receiveString(int sock)
{
  int bytesLeft = receiveLong(sock);
  char msg[bytesLeft];
  char *bp = (char*)&msg;
  string temp;
  while(bytesLeft > 0){
    int bytesRecv = recv(sock, (void*)bp, bytesLeft, 0);
    if(bytesRecv <= 0)
      break;
    else{
      bytesLeft = bytesLeft - bytesRecv;
      bp = bp + bytesRecv;
    }
  }
  temp = string(msg);
  return temp;
}

void sendChar(char num, int sock)
{
  char temp = num;
  int bytesSent = send(sock, (void *) &temp, sizeof(char), 0);
  if (bytesSent != sizeof(char))
    exit(-1);
}

char receiveChar(int sock)
{
  int bytesLeft = sizeof(long);
  char networkInt;
  char *bp = (char*) &networkInt;

  while(bytesLeft > 0){
    int bytesRecv = recv(sock, (void*)bp, bytesLeft, 0);
    if(bytesRecv <= 0){
      break;
    }
    else{
      bytesLeft = bytesLeft - bytesRecv;
      bp = bp +
        bytesRecv;
    }
  }
  networkInt = ntohl(networkInt);
  return networkInt;  
}

//functions related to printing the leaderboard

leaderBoard receiveBoard(int sock)
{
  leaderBoard tempBoard;
  for(int i = 0; i < LEADERBOARD_SPOTS; i++){
    //CHANGE THIS TO RECEIVE FLOAT EVENTUALLY
    tempBoard.score.push_back(receiveString(sock));
    tempBoard.names.push_back(receiveString(sock));
  }
  return tempBoard;
}

void printBoard(leaderBoard board)
{
  cout << endl;
  cout << "Leaderboard" << endl;
  printLine();
  for(int i = 0; i < LEADERBOARD_SPOTS; i++){
    if(board.score[i] != "0.000000")
      cout << i+1 << ". " << board.names[i] <<  " " << board.score[i] << endl;
  }
}

//small line printing function
void printLine(){
  for(int i = 0; i < 20; i++)
    cout << "-";
  cout << endl;
}
