// client Code

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/time.h>
#include "header_file.h"
#include <signal.h>
#define VERBOSE_LOGGING  (0)


//socket
int socket_client;
struct sockaddr_in server_address;
struct sockaddr_storage incoming;
char buffer_client[1024];
int nBytes = 0;
socklen_t addr_size = 0;
int err = 0;

uint8_t segment_ = 0;
uint8_t length_ = 0;
struct itimerval timer_retry;


void handle_timer(int sig);
void AllCorrect();
void lengthMismatch();
void duplicatePacket();
void outOfSequence();
void EndOfPacketError();
void timerCheck();
int GetTestOption();



void CreateAndSendPacket(int segment_num, int len_payload,  const uint8_t* data, bool fake_length_error);
void CreateAndSendEOPErrorPacket(int segment_num, int len_payload, const uint8_t* data, bool fake_length_error);
int ReceivePacket(uint8_t segment_num);
const uint8_t* all_payloads[5] = {first_payload, second_payload, third_payload, fourth_payload, fifth_payload};
void CreateAndSendPacketTimer(int segment_num, int len_payload, const uint8_t* data);

void TimerCase();
void StartTimer();
void CancelTimer();



int main() {
  // create socket
  socket_client = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_client < 0) {
    perror("cannot create socket\n");
    return 0;
  } else {
    printf("Socket created\n");
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(7822);
  server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
  memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
  addr_size = sizeof(incoming);
  
  //timer

  timer_retry.it_interval.tv_sec = 0;
  timer_retry.it_interval.tv_usec = 0;
  timer_retry.it_value.tv_sec = 3;
  timer_retry.it_value.tv_usec = 0;

 /* Choose the input. All the functions have data packets to be sent to server */
  int count=1;
  int choice=0;
  int loop_continue = false;
  do {
    int choice = GetTestOption();
    switch(choice) {
     
      case 1:
      AllCorrect();
      loop_continue = true;
      break;

      case 2:
      //Packet out of sequence
      outOfSequence();
      loop_continue = true;      
      break;

      case 3:
      //Length mismatch
      lengthMismatch();
      loop_continue = true;      
      break;

      case 4:
      //Missing end of packet 
      EndOfPacketError();
      loop_continue = true;      
      break;

      case 5:
      //Duplicate packet
      duplicatePacket();
      loop_continue = true;      
      break;
    
      case 6:
      //server does not respond
      TimerCase();
      loop_continue = true;      
      break;
      
      case 7:
      loop_continue = false;
      break;

      default:
      printf("Wrong input. Please select valid options.\n");
      loop_continue = true;
      break;
    }
  } while (loop_continue);


 return 0;
} 

/*This function creates and sends packet to the server */
void CreateAndSendPacket(int segment_num, int len_payload, const uint8_t* data, bool fake_length_error) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = DATA_ID;

  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t) + len_payload;
  if(fake_length_error==true){
  total_size = total_size- len_payload + 3;
  }
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  // Copy other fields.
  index += sizeof(preamble);

#if VERBOSE_LOGGING
  printf("[TEST] preamble_index= %d\n", index);
#endif

  buff[index] = segment_num;
  index += sizeof(uint8_t);
  buff[index] = len_payload;
  index += sizeof(uint8_t);

  memcpy(&buff[index], data, 3);
  index += 3;
/*
  if (fake_length_error)
    index += sizeof(uint8_t);*/

  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending this packet\n");  
  for (int i=0; i<len_payload+9; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0, (struct sockaddr* )&server_address,
                   sizeof(server_address));
  if (err < 0) {
    perror("cannot receive\n");
  }
}


/*Inputs All correct packets*/
void AllCorrect() {
  //segment_ = 1;
  //length_ = 3;
  CreateAndSendPacket(1 /* segment_num */,
                      3 /* payload length */,
                      first_payload, false);
  int bytes = ReceivePacket(segment_);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
 
  CreateAndSendPacket(2, 3, second_payload, false);
  bytes = ReceivePacket(segment_);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(3, 3, third_payload, false);
  bytes = ReceivePacket(3);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(4, 3, fourth_payload, false);
  bytes = ReceivePacket(4);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(5, 3, fifth_payload, false);
  bytes = ReceivePacket(5);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
}

/*case in which timer is implemented*/
void TimerCase() {
  segment_ = 1;
  length_ = 3;
  CreateAndSendPacketTimer(segment_/* segment_num */,
                      length_  /* payload length */,
                      all_payloads[segment_-1]);
  StartTimer();
  int bytes = ReceivePacket(segment_);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  CancelTimer();
  segment_++;

  CreateAndSendPacketTimer(segment_/* segment_num */,
                      length_  /* payload length */,
                      all_payloads[segment_-1]);
  StartTimer();
  bytes = ReceivePacket(segment_);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  CancelTimer();
  
  }

/*This starts the timer*/
void StartTimer() {
  if (0 > setitimer(ITIMER_REAL, &timer_retry, NULL))
     printf("Timer Error \n");

  signal(SIGALRM, handle_timer);  
}

// If code reaches here, it means client did not receive ACK/REJECT packet.
void handle_timer(int sig) {
  static int retry_cnt = 0;
  ++retry_cnt;

  printf("Timer fired, retry count: %d, for segment number %u\n", retry_cnt, segment_);
  if (retry_cnt <= 2) {
   
   CreateAndSendPacketTimer(segment_/* segment_num */,
                      length_  /* payload length */,
                      all_payloads[segment_-1]);
   
  } else {
    printf("\n[ERROR] Server does not respond\n");
    retry_cnt = 0;
  }
}

/*To cancel the timer */
void CancelTimer() {
  if (0 > setitimer(ITIMER_REAL, NULL, &timer_retry)) {
    printf("Failed to cancel timer\n");
  }

  printf("Timer cancelled\n");
}

/*
  int all_payloads[5] = {&first, .....};
*/



/* One of the packets has end of packet identifier error*/
void EndOfPacketError() {
  CreateAndSendPacket(1 /* segment_num */,
                      3 /* payload length */,
                      first_payload, false);

  int bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(2, 3, second_payload, false);
  bytes = ReceivePacket(2);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(3, 3, third_payload, false);
  bytes = ReceivePacket(3);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(4, 3, fourth_payload, false);
  bytes = ReceivePacket(4);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendEOPErrorPacket(5, 3, fifth_payload, false);
  bytes = ReceivePacket(5);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  
  
}

/* One of the packets has out of sequence error*/
void outOfSequence(){
  CreateAndSendPacket(1 /* segment_num */, 3 /* payload length */, first_payload, false);

  int bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  //Out Of Sequence Error
  CreateAndSendPacket(3, 3, third_payload, false);
  bytes = ReceivePacket(2);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  CreateAndSendPacket(2, 3, second_payload, false);
  bytes = ReceivePacket(2);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(3, 3, third_payload, false);
  bytes = ReceivePacket(3);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(4, 3, fourth_payload, false);
  bytes = ReceivePacket(4);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  CreateAndSendPacket(5, 3, fifth_payload, false);
  bytes = ReceivePacket(5);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  
}

/* One of the packets has length mismatch error*/
void lengthMismatch(){

CreateAndSendPacket(1 /* segment_num */,
                      3 /* payload length */,
                      first_payload, false);

  int bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  //Error case
  CreateAndSendPacket(2, 6, second_payload, true);
  bytes = ReceivePacket(2);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(3, 3, third_payload, false);
  bytes = ReceivePacket(3);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(4, 3, fourth_payload, false);
  bytes = ReceivePacket(4);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(5, 3, fifth_payload, false);
  bytes = ReceivePacket(5);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
}

/* One of the packets is duplicate packet */
void duplicatePacket(){
  CreateAndSendPacket(1, 3 , first_payload, false);
  int bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  
  CreateAndSendPacket(1, 3, first_payload, false);
  bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(2, 3, second_payload, false);
  bytes = ReceivePacket(2);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }

  CreateAndSendPacket(3, 3, third_payload, false);
  bytes = ReceivePacket(3);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  CreateAndSendPacket(4, 3, fourth_payload, false);
  bytes = ReceivePacket(4);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  CreateAndSendPacket(5, 3, fifth_payload, false);
  bytes = ReceivePacket(5);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  
}
/* Recvfrom function*/
int ReceivePacket(uint8_t segment_num) {
  uint8_t buffer[1024];
  int bytes = recvfrom(socket_client, buffer, 1024, 0, NULL, 0);
  if (bytes < 0) {
    printf("[ERROR] Packet receive error\n");
    return bytes;
  }

  return HandleReceivedPacket(buffer, bytes, &segment_num); /* This function is common between server and client (written in header file)*/
}

/* Function to get the choice */
int GetTestOption() {
    int choice;
    printf("Enter the case which you want to check for \n");
    printf("1.All 5 Correct packets \n");
    printf("2.Demonstrate packet out of sequence error\n");
    printf("3.Demonstrate length mismatch error\n");
    printf("4.Demonstrate missing end of identifier error\n");
    printf("5.Demonstrate duplicate packet error\n");
    printf("6.Demonstrate case in which server does not respond\n");
    printf("7.Exit the program \n");
    scanf("%d",&choice);
    return choice;
}

/*This function has one of the packets with wrong end of packet identifier*/
void CreateAndSendEOPErrorPacket(int segment_num, int len_payload, const uint8_t* data, bool fake_length_error) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = DATA_ID;

  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t) + len_payload;
  if(fake_length_error==true){
  total_size = total_size- len_payload + 3;
  }
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  // Copy other fields.
  index += sizeof(preamble);

  buff[index] = segment_num;
  index += sizeof(uint8_t);
  buff[index] = len_payload;
  index += sizeof(uint8_t);

  memcpy(&buff[index], data, 3);
  index += 3;
/*
  if (fake_length_error)
    index += sizeof(uint8_t);*/

  memcpy(&buff[index], &kEndOfPacketId_Error, sizeof(kEndOfPacketId_Error));

  printf("Sending this packet\n");  
  for (int i=0; i<len_payload+9; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0, (struct sockaddr* )&server_address,
                   sizeof(server_address));
  if (err < 0) {
    perror("cannot receive\n");
  }
}

/* creates and sends timer packet */
void CreateAndSendPacketTimer(int segment_num, int len_payload, const uint8_t* data) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = DATA_ID;

  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t) + len_payload;
  
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  // Copy other fields.
  index += sizeof(preamble);
  buff[index] = segment_num;
  index += sizeof(uint8_t);
  buff[index] = len_payload;
  index += sizeof(uint8_t);

  memcpy(&buff[index], data, 3);
  index += 3;

  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending this packet\n");  
  for (int i=0; i<len_payload+9; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0, (struct sockaddr* )&server_address,
                   sizeof(server_address));
  if (err < 0) {
    perror("cannot receive\n");

  }
  StartTimer() ;
}

