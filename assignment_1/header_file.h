#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_PAYLOAD_SIZE       0xFF
#define CLIENT_ID              30

#define DATA_ID                0xFFF1
#define ACK_ID                 0xFFF2
#define REJECT_ID              0XFFF3

const uint16_t kEndOfPacketId    = 0xFFFF;
const uint16_t kStartOfPacketId  = 0xFFFF;

// 4 payloads.
const uint8_t first_payload[]  = {1, 1, 1};
const uint8_t second_payload[] = {2, 2, 2};
const uint8_t third_payload[]  = {3, 3, 3};
const uint8_t fourth_payload[] = {4, 4, 4};
const uint8_t fifth_payload[]  = {5, 5, 5};
const uint8_t timerCheck_payload[]  = "no_reply";
//Error
const uint16_t kEndOfPacketId_Error=0x1111;

/*All structure definitions*/
#pragma pack(1)
typedef struct 
{
  uint16_t start_of_packet;
  uint8_t client_id;
  uint16_t packet_type;
  
} preamble;

typedef struct
{
  preamble pre;
  uint8_t segment_number;
  uint8_t length;
  uint8_t payload[255];
  uint16_t end_of_packet;
} data_packet;

typedef struct
{
  preamble pre;
  uint8_t received_seg;
  uint16_t end_of_packet;
} ack_packet;

typedef struct
{
  preamble pre;
  uint16_t rej_sub_code;
  uint8_t received_seg_no;
  uint16_t end_of_packet;
} reject_packet;
#pragma pack(0)


// Error codes
#define SUCCESS        0
#define ERR_CASE_1     1
#define ERR_CASE_2     2
#define ERR_CASE_3     3
#define ERR_CASE_4     4

// Error codes common between server & client
#define ERR_REJ        1
#define ERR_ACK        2

int HandleReceivedPacket(const uint8_t* buffer, const int buffLen, uint8_t* recv_segment_num);
int ParseDataPacket(data_packet d, int index, const uint8_t* buffer, const int buffLen,
                    uint8_t* segment_num);
void ParseAckPacket(ack_packet a, int index, const uint8_t* buffer);
void ParseRejectPacket(reject_packet r, int index, const uint8_t* buffer);

/* This function handles all the packets received on server as well as client*/
int HandleReceivedPacket(const uint8_t* buffer, const int buffLen, uint8_t* segment_num) {
  if (buffer == NULL || buffLen <= 0) {
    printf("[ERROR] Invalid buffer or buffer length\n");
    return -1;
  
  }
  printf("\n");
  printf("Received data: %d bytes\n", buffLen);

 
  data_packet d;
  ack_packet a;
  reject_packet r;
  preamble p;

  int index = 0;
  p.start_of_packet = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  p.client_id = *(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  p.packet_type = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  
  d.pre = p;
  printf("Preamble: \n");
  printf("_start of packet: 0x%x \n", p.start_of_packet);
  printf("_client_id: 0x%x\n", p.client_id);
  printf("_packet_type: 0x%x\n", p.packet_type);
  
  if (p.packet_type == DATA_ID) {
    // SERVER ONLY
    int error = ParseDataPacket(d, index, buffer, buffLen, segment_num); /*Function to parse data packets*/
    return error;
  }
  else if (p.packet_type == ACK_ID) {
    // CLIENT ONLY
    ParseAckPacket(a, index, buffer);                                    /*Function to parse ACK packets*/
  }
  else if (p.packet_type == REJECT_ID) {
    // CLIENT ONLY
    ParseRejectPacket(r, index, buffer);                                 /*Function to parse REJECT packets*/
  }
  else {
    printf("[ERROR] Unknown packet type \n");
  }
}


int next_expected_segment_num=1;
int previous_segment_num=0;
/*Function to parse data packets*/

int ParseDataPacket(data_packet d, int index, const uint8_t* buffer, const int buffLen,
                    uint8_t* segment_num) {
  
  d.segment_number = *(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  // Update segment number
  *segment_num = d.segment_number;
  if(d.segment_number== next_expected_segment_num){
    previous_segment_num=d.segment_number%5;
    next_expected_segment_num = (next_expected_segment_num+1);
    if(d.segment_number==5){
      previous_segment_num=0;
      next_expected_segment_num=1;
    }
    
  }
  else if(d.segment_number==previous_segment_num){
    printf("[ERROR] Duplicate Sequence Number: Current sequence number is :%d and Last sequence number was %d\n", d.segment_number, previous_segment_num);
    return ERR_CASE_4;
    // prev and next values are not changed as its invalid packet
  }
  
  else {
    printf("[ERROR] Wrong Sequence Number \n");
    return ERR_CASE_1;
  }
  // don't update previous seq num yet
  // update next expected seq num
  // if all logic for seg num is okay then update previous seq num.
  // else reset next expected & previous



  d.length = *(uint8_t* )&buffer[index];
  //printf("Bufferlength %d",buffLen);
  //printf(" The value of buffLen - sizeof(preamble)- 2 - sizeof(d.end_of_packet) = %lu \n",(buffLen - sizeof(preamble)- 2 - sizeof(d.end_of_packet)));
  // printf("d.length = %d \n",d.length);
  if (buffLen - sizeof(preamble)- 2 - sizeof(d.end_of_packet) != d.length) {
    printf("[ERROR] Invalid payload length \n");
    return ERR_CASE_2;
  }
  
  if (*(uint16_t* )&buffer[buffLen-2] != kEndOfPacketId) {
    printf("[ERROR] Missing end of packet id.\n");
    return ERR_CASE_3;
  }
  index += sizeof(uint8_t); 

  memcpy(d.payload, &buffer[index], d.length);
  index += d.length;

  /*if(strcmp((char*)(d.payload),(char*)timerCheck_payload)){
    printf("No acknowledgement or reject \n");
  }*/
  d.end_of_packet = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  
  printf("\n");
  printf("Data packet Contents \n");
  printf("_segment number: 0x%x \n", d.segment_number);
  printf("_Length: 0x%x\n", d.length);
  printf("_payload is :\n");
  for (int i = 0 ; i < d.length; i++){
    printf("0x%x  ", d.payload[i]);
  }
  printf("\n");
  printf("_end_of_packet: 0x%x\n", d.end_of_packet);

 
  // If code returns here it means this is S U C C E S S!!!!!! 
  return SUCCESS;
  printf("\n");
}

/*Function to parse ACK packets*/
void ParseAckPacket(ack_packet a, int index, const uint8_t* buffer) {
  a.received_seg=*(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  a.end_of_packet = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  printf("_received segment number: 0x%x \n", a.received_seg);
  printf("_end_of_packet: 0x%x\n", a.end_of_packet);
  printf("\n");
}

/*Function to parse REJECT packets*/
void ParseRejectPacket(reject_packet r,int index, const uint8_t* buffer) {
  r.rej_sub_code = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  r.received_seg_no=*(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  r.end_of_packet = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  printf("_reject sub code : 0x%x \n", r.rej_sub_code);
  printf("_received segment number: 0x%x \n", r.received_seg_no);
  printf("_end_of_packet: 0x%x\n", r.end_of_packet);
  printf("\n");
}






