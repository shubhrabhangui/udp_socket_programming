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
#include "header_file2.h"
#include <signal.h>





//Indentation to be checked again

#define VERBOSE_LOGGING  (0)


//socket
int socket_client;
struct sockaddr_in server_address;
struct sockaddr_storage incoming;
char buffer_client[1024];
int nBytes = 0;
socklen_t addr_size = 0;
int err = 0;
struct itimerval timer_retry;

//static struct itimerval timer;
int GetTestOption();
void CreateAndSendaccess_permission_request_packet(uint8_t segment_number,
                                                   uint8_t technology,
                                                   const uint32_t source_subscriber_number);
void Subscriber_permitted_to_access();
void subscriber_wrong_technology();
void Subscriber_not_paid();
void subscriber_not_exist();
void ReceivePacket();
void CreateAndSendPacketTimer(uint8_t segment_number, uint8_t technology,const uint32_t source_subscriber_number);

void TimerCase();
void TimerCase();
void StartTimer();
void CancelTimer();
void handle_timer(int sig);

uint8_t segment_ = 0;
int all_technology[4]= {04,03,02,05};
int all_subscribers[4]= {4085546805, 4086668821, 4086808821, 4085546845};

int main(){
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

  timer_retry.it_interval.tv_sec = 0;
  timer_retry.it_interval.tv_usec = 0;
  timer_retry.it_value.tv_sec = 3;
  timer_retry.it_value.tv_usec = 0;

  int choice=0;
  int loop_continue = false;
  do {
    int choice = GetTestOption();
    switch(choice) {
      case 1:
      //subscriber permitted to access network
      Subscriber_permitted_to_access();
      loop_continue = true;
      break;

      case 2:
      //subscriber has not paid
      Subscriber_not_paid();
      loop_continue = true;      
      break;

      case 3:
      //subscriber does not exist
      subscriber_not_exist();
      loop_continue = true;      
      break;
      
      case 4:
      //subscriber does not exist
      subscriber_wrong_technology();
      loop_continue = true;      
      break;

      case 5:
      //server does not respond
      TimerCase();
      loop_continue = true;      
      break;
  
      
      case 6:
      //exit
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

int GetTestOption() {
    int choice;
    printf("\n");
    printf("Enter the case which you want to check for \n");
    printf("1.Subscriber permitted to access network \n");
    printf("2.Subscriber has not paid\n");
    printf("3.Subscriber does not exist\n");
    printf("4.Subcriber number match but technology doesnot match \n");
    printf("5.Demonstrate timer: Case in which server does not respond \n");
    printf("6.Exit the program \n");
    scanf("%d",&choice);
    return choice;
}


void CreateAndSendaccess_permission_request_packet(
    uint8_t segment_number, uint8_t technology,
    const uint32_t source_subscriber_number) {

  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = ACC_PER_VAL;
  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t) + sizeof(payload);
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  index += sizeof(preamble);
  
  buff[index] = segment_number;
  index += sizeof(uint8_t);
  
  buff[index] = sizeof(payload);
  index += sizeof(uint8_t);
  
  payload data;
  data.technology = technology;
  data.source_subscriber_number = source_subscriber_number;
  memcpy(&buff[index], &data, sizeof(payload));
  index += sizeof(payload);
  
  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending this packet\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&server_address, sizeof(server_address));
  if (err < 0) {
    perror("cannot receive\n");
  }
}


void Subscriber_permitted_to_access(){
  CreateAndSendaccess_permission_request_packet(1/*segment_number*/, technology_4G,SourceSubNum1);
  ReceivePacket();
}


void Subscriber_not_paid(){
  CreateAndSendaccess_permission_request_packet(1, technology_3G,  SourceSubNum2);
  ReceivePacket();
}

void subscriber_not_exist(){
  CreateAndSendaccess_permission_request_packet(1, technology_3G,  SourceSubNum_not);
  ReceivePacket();
}

void subscriber_wrong_technology(){
  CreateAndSendaccess_permission_request_packet(1, technology_3G,  SourceSubNum3);
  ReceivePacket();
}

void ReceivePacket() {
  uint8_t buffer[1024];
  int bytes = recvfrom(socket_client, buffer, 1024, 0, NULL, 0);
  if (bytes < 0) {
    printf("[ERROR] Packet receive error: %d\n", bytes);
    return;
  }

  uint8_t segment_number;
  payload received_payload;
  int errorcode = HandleReceivedPacket(buffer, bytes, &segment_number, &received_payload);
  switch (errorcode) {

    case TECHNOLOGY_DOES_NOT_MATCH:
    case NOT_EXIST:
      printf("The subscriber %u doesnot exist for segment number %d or technology mismatch\n", received_payload.source_subscriber_number, segment_number);
      break;

    case ACC_OK:
      printf("The subscriber %u permitted to access the network for segment number %d \n", received_payload.source_subscriber_number, segment_number);
      break;

    
    case NOT_PAID:
      printf("The subscriber %u has not paid for segment number %d \n", received_payload.source_subscriber_number, segment_number);
      break;

    default:
    // some error message.
    break;
  } 
}

void CreateAndSendPacketTimer(uint8_t segment_number, uint8_t technology,const uint32_t source_subscriber_number) {
   
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = ACC_PER_VAL;
  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t) + sizeof(payload);
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  index += sizeof(preamble);
  
  buff[index] = segment_number;
  index += sizeof(uint8_t);
  
  buff[index] = sizeof(payload);
  index += sizeof(uint8_t);
  
  payload data;
  data.technology = technology;
  data.source_subscriber_number = source_subscriber_number;
  memcpy(&buff[index], &data, sizeof(payload));
  index += sizeof(payload);
  
  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending this packet\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&server_address, sizeof(server_address));
  if (err < 0) {
    perror("cannot receive\n");
  }
  StartTimer() ;
}




void TimerCase() {
  segment_=1;
  CreateAndSendPacketTimer(1/*segment_number*/, all_technology[segment_-1], all_subscribers[segment_-1]);
  StartTimer();
  ReceivePacket();
  CancelTimer();
  segment_++;

  }

void StartTimer() {
  if (0 > setitimer(ITIMER_REAL, &timer_retry, NULL))
     printf("Timer Error \n");

  signal(SIGALRM, handle_timer);  
}

void handle_timer(int sig) {
  static int retry_cnt = 0;
  ++retry_cnt;

  printf("Timer fired, retry count: %d\n", retry_cnt);
  if (retry_cnt <= 2) {
   //printf("Retry Count= %d \n",retry_cnt);
   
   // CreateAndSendPacket(segment_, length_, all_payloads[segment_ + 1], false);
   CreateAndSendPacketTimer(segment_/*segment_number*/, all_technology[segment_-1], all_subscribers[segment_-1]);
   
  } else {
    printf("\n[ERROR] Server does not respond\n");
    retry_cnt = 0;
  }
}

void CancelTimer() {
  if (0 > setitimer(ITIMER_REAL, NULL, &timer_retry)) {
    printf("Failed to cancel timer\n");
  }

  printf("Timer cancelled\n");
}


