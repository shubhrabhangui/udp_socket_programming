// Server Code 

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "header_file.h"

// Socket initialization.
  int socket_connection;
  struct sockaddr_in server_address;
  uint8_t buffer[1024];
  int nBytes = 0;
  socklen_t addr_size = 0;
  struct sockaddr_in incoming;

void SendAckPacket(uint8_t segment_num);
void SendRejectPacket(uint8_t segment_number, uint16_t rej_sub_code );
int checkbytes();
 
int main()
{
  

  // House-keeping info
  uint8_t segment_number = 0;

  if ((socket_connection = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("cannot create socket");
    return 0;
  }

  memset(&server_address, 0 , sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(7822);
  server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
  memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

  if (bind(socket_connection, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
    perror("bind failed");
    return 0;
  }

  
  
  addr_size = sizeof(incoming);
  //printf("before while loop \n");

 /* To send reject packets after receiving error data packet*/
  while (1)
  {
    int bytes = recvfrom(socket_connection, buffer, 1024, 0,(struct sockaddr* )&incoming, &addr_size);
    if( bytes < 0){
      printf("Receive Failed");
      return 1;
    }
   
    int error = HandleReceivedPacket(buffer, bytes, &segment_number);
    //printf("Error is %d \n",error);
    switch (error) {
      case SUCCESS: {
        printf("Case success \n");
        SendAckPacket(segment_number);
        
      } break;

      case ERR_CASE_1: {
        SendRejectPacket(segment_number, 1);
      } break;

      case ERR_CASE_2: {
        SendRejectPacket(segment_number, 2);
      } break;

      case ERR_CASE_3: {
        SendRejectPacket(segment_number, 3);
      } break;

      case ERR_CASE_4: {
        SendRejectPacket(segment_number, 4);
      } break; 

    }
  }

  return 0;
}

/* Function to send reject packet by taking segment number and reject sub code*/
void SendRejectPacket(uint8_t segment_number, uint16_t rej_sub_code ) {
  
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = REJECT_ID;
  size_t total_size = sizeof(preamble) + sizeof(uint8_t) + 2*sizeof(uint16_t);
  uint8_t* buff = (uint8_t* )malloc(total_size);
  int index = 0;
  memcpy(buff, &p, sizeof(preamble));
  index += sizeof(preamble);
  buff[index] = rej_sub_code;
  index += sizeof(uint16_t);
  buff[index] = segment_number;
  index += sizeof(uint8_t);
  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));
  
  printf("Sending Reject\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_connection, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&incoming, sizeof(incoming));
  if (err < 0) {
    perror("cannot send\n");
  }
}

/* Function to send acknowlegment packet to client by taking segment number*/
void SendAckPacket(uint8_t segment_num) {
  
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = ACK_ID;
  size_t total_size = sizeof(preamble) + sizeof(uint8_t) + sizeof(uint16_t);
  uint8_t* buff = (uint8_t* )malloc(total_size);
  int index = 0;
  memcpy(buff, &p, sizeof(preamble));
  index += sizeof(preamble);
  buff[index] = segment_num;
  index += sizeof(uint8_t);
  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));
  
  printf("Sending Acknowledgement\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_connection, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&incoming, sizeof(incoming));
  if (err < 0) {
    perror("cannot send\n");
  }
}



