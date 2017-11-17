// Server Code 

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "header_file2.h"

// Socket initialization.
int socket_connection;
struct sockaddr_in server_address;
uint8_t buffer[1024];
int nBytes = 0;
socklen_t addr_size = 0;
struct sockaddr_in incoming;

void CreateAndSendNotPacket(uint8_t segment_number, const payload received_payload);
void CreateAndSendAccOk(uint8_t segment_number, const payload received_payload);
void CreateAndSendNotExistPacket(uint8_t segment_number, const payload received_payload);
void CreateAndSendNotPaidPacket(uint8_t segment_number, const payload received_payload);

int main() {
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
  while (1) {
    int bytes = recvfrom(socket_connection, buffer, 1024, 0,(struct sockaddr* )&incoming, &addr_size);
    if( bytes < 0){
      printf("Receive Failed");
      return 1;
    }

    uint8_t segment_number;
    payload received_payload;
    int error = HandleReceivedPacket(buffer, bytes, &segment_number, &received_payload);
    //printf("Error is %d \n",error);
  
    switch (error) {
      case SUCCESS: {
        printf("Case success \n");
        //send subscriber permitted to access network packet
        CreateAndSendAccOk(segment_number, received_payload); 
      } break;

      case NOT_PAID: {
        CreateAndSendNotPaidPacket(segment_number, received_payload);
       } break;

       case TECHNOLOGY_DOES_NOT_MATCH:
       case NOT_EXIST: {
         CreateAndSendNotExistPacket(segment_number, received_payload);
       } break;
     
       default:{
         printf("error \n");
         break; 
       }
     } //end_switch
  } // end_while   

  return 0;
}


/* This function sends ACC_OK packet to client after finding the subscriber
   number and technology in the database for which the subscriber has paid for.
 */
void CreateAndSendAccOk(uint8_t segment_number, const payload received_payload) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = ACC_OK;
  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t)+ sizeof(payload);  //+ sizeof( payload) = length
  uint8_t* buff = (uint8_t* )malloc(total_size);
  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  // Copy other fields.
  index += sizeof(preamble);
  buff[index] = segment_number;
  index += sizeof(uint8_t);
  buff[index] = sizeof(payload);
  index += sizeof(uint8_t);
  payload data;
  data.technology = received_payload.technology;
  data.source_subscriber_number = received_payload.source_subscriber_number;
  memcpy(&buff[index], &data, sizeof(payload));
  index+=sizeof(payload);

  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending 'Acc Ok' packet\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_connection, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&incoming, sizeof(incoming));
  if (err < 0) {
    perror("cannot send\n");
  }
}

/* This function sends a 'not exist' packet if user does not exist in DB.
 */
void CreateAndSendNotExistPacket(uint8_t segment_number, const payload received_payload) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = NOT_EXIST_VAL;
  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t)+ sizeof(payload);  //+ sizeof( payload) = length
  uint8_t* buff = (uint8_t* )malloc(total_size);
  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  // Copy other fields.
  index += sizeof(preamble);
  buff[index] = segment_number;
  index += sizeof(uint8_t);
  buff[index] = sizeof(payload);
  index += sizeof(uint8_t);
  payload data;
  data.technology = received_payload.technology;
  data.source_subscriber_number = received_payload.source_subscriber_number;
  memcpy(&buff[index], &data, sizeof(payload));
  index+=sizeof(payload);

  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending 'NOT EXIST'packet\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_connection, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&incoming, sizeof(incoming));
  if (err < 0) {
    perror("cannot send\n");
  }
}

/* This function sends a 'not paid' packet to the client.
 */
void CreateAndSendNotPaidPacket(uint8_t segment_number, const payload received_payload) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = NOT_PAID_VAL;
  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t)+ sizeof(payload);  //+ sizeof( payload) = length
  uint8_t* buff = (uint8_t* )malloc(total_size);
  int index = 0;
  // Copy preamble
  memcpy(buff, &p, sizeof(preamble));
  // Copy other fields.
  index += sizeof(preamble);
  buff[index] = segment_number;
  index += sizeof(uint8_t);
  buff[index] = sizeof(payload);
  index += sizeof(uint8_t);
  payload data;
  data.technology = received_payload.technology;
  data.source_subscriber_number = received_payload.source_subscriber_number;
  memcpy(&buff[index], &data, sizeof(payload));
  index+=sizeof(payload);

  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending 'NOT PAID' packet\n");  
  for (int i=0; i<total_size; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_connection, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&incoming, sizeof(incoming));
  if (err < 0) {
    perror("cannot send\n");
  }
}





























