// This is a UDP Client Code. The client code creates a socket and sends
// messages to the server over udp connection. To run this program, compile it
// using 'gcc' as 
//  gcc client.c - client.o
// 
// Run the binary as
//  ./client.o
//
// Author: Shubhra Rajadhyaksha

#include "header_file.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>


// When enabled, turns on verbose logging for debugging purpose.
#define VERBOSE_LOGGING  (0)

#define _MAX_BUFFER_SIZE_ (1024)

// Creating global sockets and buffers.
int socket_client;
struct sockaddr_in server_address;
struct sockaddr_storage incoming;

char buffer_client[_MAX_BUFFER_SIZE_];
int nBytes = 0;
socklen_t addr_size = 0;
int err = 0;

// This is used to track the segment number and length of payload in each
// outgoing & incoming packet.
uint8_t segment_ = 0;
uint8_t length_ = 0;
// Timer struct use to create the retry timer.
struct itimerval timer_retry;

// All payloads container. These payloads are defined in header_file.h and reused
// here as they are also needed in the server code for parsing logic.
const uint8_t* all_payloads[5] = {
  first_payload,
  second_payload,
  third_payload,
  fourth_payload,
  fifth_payload
};



// ********************* Function protoypes. ********************

/* Timer interrupt handler. This handler is called when the timer set using
 * setitimer() fires.
 */
void TimerHandler(int sig);

/* Send all expected (to the server) packets. Server will ACK each packet,
 * after which client sends the next segment number packet. The first packet
 * will start with segment number 1.
 */
void AllCorrect();

/* Send a packet with a length that does not match the actual length of the
 * payload. Server should detect this anomaly and reply with a
 * REJECT packet.
 */
void LengthMismatch();

/* Client will send a duplicate packet i.e. two packets with the same segment
 * number. Since server expects a successive segment number, sending this packet
 * should return an error from the server. For example, client will send the
 * following sequence numbers: 1 2 2 3. Here 2 is the duplicate packet.
 *
 */
void DuplicatePacket();

/* Client will send an out of sequence packet. This simulates an error condition.
 * Server should send a REJECT packet in response. Example of an out of sequence
 * packet queue is with segment numbers as follows: 1 2 6 3 4 5. Here 6 is out of
 * sequence.
*/
void OutOfSequence();

/* Client does not fill the end of packet identifier while sending the packet.
 * Server should return a REJECT packet in response.
 */
void EndOfPacketError();

/* Gets test option from user via stdin.
 */
int GetTestOption();

/* Creates and Sends packet on the socket. This function follows the packet
 * structure defined in header_file.h. This is a wrapper over the POSIX sendto()
 * function call.
 *
 * Params: segment number, payload length, pointer to payload, flag to indicate
 * if the packet should simulate the length mismatch error. 
 */
void CreateAndSendPacket(int segment_num, int len_payload,  const uint8_t* data,
                         bool fake_length_error);

/* Similar to the CreateAndSendPacket() above, except this function does not
 * append the 'end of packet' identifier.
 *
 */
void CreateAndSendEOPErrorPacket(int segment_num, int len_payload,
                                 const uint8_t* data,
                                 bool fake_length_error);

/* Receives packet with expected segment number specified by |segment_num|.
 * This is a wrapper over the POSIX recvfrom() function call.
 */
int ReceivePacket(uint8_t segment_num);

/* Create and send packet with timer.
 */
void CreateAndSendPacketTimer(int segment_num, int len_payload, const uint8_t* data);

/* Simulates timer case.
 */
void TimerCase();

/* Initiates a timer using setitimer() */
void StartTimer();

/* Cancels timer if it exists, noop otherwise */
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
      OutOfSequence();
      loop_continue = true;      
      break;

      case 3:
      LengthMismatch();
      loop_continue = true;      
      break;

      case 4:
      EndOfPacketError();
      loop_continue = true;      
      break;

      case 5:
      DuplicatePacket();
      loop_continue = true;      
      break;
    
      case 6:
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

void CreateAndSendPacket(int segment_num, int len_payload, const uint8_t* data,
                         bool fake_length_error) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = DATA_ID;

  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) +
                      sizeof(uint16_t) + len_payload;

  if (fake_length_error==true) {
    total_size = total_size- len_payload + 3;
  }

  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  memcpy(buff, &p, sizeof(preamble));
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

  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending this packet\n");  
  for (int i=0; i<len_payload+9; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&server_address, sizeof(server_address));

  if (err < 0) {
    perror("cannot receive\n");
  }
}


void AllCorrect() {
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

void StartTimer() {
  if (0 > setitimer(ITIMER_REAL, &timer_retry, NULL))
     printf("Timer Error \n");

  signal(SIGALRM, TimerHandler);  
}

// If code reaches here, it means client did not receive ACK/REJECT packet.
void TimerHandler(int sig) {
  static int retry_cnt = 0;
  ++retry_cnt;

  printf("Timer fired, retry count: %d, for segment number %u\n",
          retry_cnt,segment_);

  if (retry_cnt <= 2) {
   CreateAndSendPacketTimer(segment_/* segment_num */,
                      length_  /* payload length */,
                      all_payloads[segment_-1]);
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

void OutOfSequence(){
  CreateAndSendPacket(1 /* segment_num */, 3 /* payload length */,
                      first_payload, false);

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


void LengthMismatch(){
  CreateAndSendPacket(1 /* segment_num */, 3 /* payload length */,
                      first_payload, false);

  int bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  
  // Simulate error case by sending mismatching length.
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
void DuplicatePacket(){
  CreateAndSendPacket(1, 3 , first_payload, false);
  int bytes = ReceivePacket(1);
  if (bytes < 0) {
    printf("[ERROR] Error receiving packet from server\n");
    return;
  }
  
  // Send duplicate packet with segment number = 1.
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

int ReceivePacket(uint8_t segment_num) {
  uint8_t buffer[_MAX_BUFFER_SIZE_];
  int bytes = recvfrom(socket_client, buffer, _MAX_BUFFER_SIZE_, 0, NULL, 0);
  if (bytes < 0) {
    printf("[ERROR] Packet receive error\n");
    return bytes;
  }

  return HandleReceivedPacket(buffer, bytes, &segment_num);
}

int GetTestOption() {
  int choice;
  printf("Enter the case which you want to check for \n\n");
  printf("1.All 5 Correct packets \n");
  printf("2.Demonstrate packet out of sequence error\n");
  printf("3.Demonstrate length mismatch error\n");
  printf("4.Demonstrate missing end of identifier error\n");
  printf("5.Demonstrate duplicate packet error\n");
  printf("6.Demonstrate case in which server does not respond\n");
  printf("7.Exit the program \n\n");

  printf("Your choice: ");
  scanf("%d",&choice);
    
  return choice;
}

void CreateAndSendEOPErrorPacket(int segment_num, int len_payload,
                                 const uint8_t* data, bool fake_length_error) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = DATA_ID;

  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) +
                      sizeof(uint16_t) + len_payload;
  
  if (fake_length_error==true) {
    total_size = total_size- len_payload + 3;
  }
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  // Copy each field of the packet and update the packet index correctly to fill
  // the next field.
  // Preamble.
  memcpy(buff, &p, sizeof(preamble));
  index += sizeof(preamble);
  
  // Segment number
  buff[index] = segment_num;
  index += sizeof(uint8_t);

  // Length of payload
  buff[index] = len_payload;
  index += sizeof(uint8_t);

  // Payload
  memcpy(&buff[index], data, 3);
  index += 3;

  // End of Packet
  memcpy(&buff[index], &kEndOfPacketId_Error, sizeof(kEndOfPacketId_Error));

  printf("Sending this packet\n");  
  for (int i=0; i<len_payload+9; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0,
                   (struct sockaddr* )&server_address, sizeof(server_address));

  if (err < 0) {
    perror("cannot receive\n");
  }
}


void CreateAndSendPacketTimer(int segment_num, int len_payload,
                              const uint8_t* data) {
  preamble p;
  p.start_of_packet = kStartOfPacketId;
  p.client_id = CLIENT_ID;
  p.packet_type = DATA_ID;

  // Create new buffer that goes on the wire.
  size_t total_size = sizeof(preamble) + 2 * sizeof(uint8_t) + sizeof(uint16_t) + len_payload;
  
  uint8_t* buff = (uint8_t* )malloc(total_size);

  int index = 0;
  
  // Copy fields into the outgoing buffer and increment the index correctly for
  // the next field.
  // Preamble
  memcpy(buff, &p, sizeof(preamble));
  index += sizeof(preamble);

  // Segment number
  buff[index] = segment_num;
  index += sizeof(uint8_t);

  // Payload length
  buff[index] = len_payload;
  index += sizeof(uint8_t);

  // Payload
  memcpy(&buff[index], data, 3);
  index += 3;

  // End of packet
  memcpy(&buff[index], &kEndOfPacketId, sizeof(kEndOfPacketId));

  printf("Sending this packet\n");  
  for (int i=0; i<len_payload+9; ++i)
    printf("0x%x |  ", buff[i]);
  printf("\n");
 
  int err = sendto(socket_client, (const void*)buff, total_size, 0,
                                   (struct sockaddr* )&server_address,
                                   sizeof(server_address));
  if (err < 0) {
    perror("cannot receive\n");
  }

  // Once the send is successful, start a timer. We cannot set the timer before
  // this point, because we should make sure that there is no error sending the
  // packet.
  StartTimer();
}

