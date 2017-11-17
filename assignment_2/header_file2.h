#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define CLIENT_ID              30
#define ACC_PER_VAL            0xFFF8
#define NOT_PAID_VAL           0xFFF9
#define NOT_EXIST_VAL          0xFFFA
#define ACC_OK                 0XFFFB

const uint16_t kEndOfPacketId    = 0xFFFF;
const uint16_t kStartOfPacketId  = 0xFFFF;
//const uint32_t kSourceSubNum   = 0xFF;

// 4 payloads.
const uint32_t SourceSubNum1     = 4085546805;
const uint32_t SourceSubNum2     = 4086668821;
const uint32_t SourceSubNum3     = 4086808821;
const uint32_t SourceSubNum_not  = 4085546845;

//technology in payload
const uint8_t technology_2G = 02;
const uint8_t technology_3G = 03;
const uint8_t technology_4G = 04;
const uint8_t technology_5G = 05;

#define SUCCESS 0
#define NOT_PAID 1
#define NOT_EXIST 2
#define TECHNOLOGY_DOES_NOT_MATCH 3


#pragma pack(1)
typedef struct 
{
  uint16_t start_of_packet;
  uint8_t client_id;
  uint16_t packet_type;
  
} preamble;

typedef struct
{ 
  uint8_t technology;
  uint32_t source_subscriber_number;
   
} payload;


typedef struct
{
  preamble pre;
  uint8_t segment_number;
  uint8_t length;
  payload data;
  uint16_t end_of_packet;
} access_permission_request_packet;


typedef struct
{
  preamble pre;
  uint8_t segment_number;
  uint8_t length;
  payload data;
  uint16_t end_of_packet;
  
} not_paid_message;

typedef struct
{
  preamble pre;
  uint8_t segment_number;
  uint8_t length;
  payload data;
  uint16_t end_of_packet;
} subcriber_does_not_exist;

typedef struct
{
  preamble pre;
  uint8_t segment_number;
  uint8_t length;
  payload data;
  uint16_t end_of_packet;
} subcriber_permitted;
#pragma pack(0)


uint8_t GetTechnologyFromDBEntry( char* entry);

uint8_t GetPaidStatusFromDBEntry( char* entry_paid);

bool Search_in_DB(const char *fname, const char* str, uint32_t* subnum, uint8_t* tech, uint8_t* paid/*paid*/);

//functions
int Parse_access_permission_request(access_permission_request_packet ap, int index,
                                    const uint8_t* buffer, const int buffLen, uint8_t* segment_num);

int Parse_not_paid_message(not_paid_message np, int index, const uint8_t* buffer,
                           const int buffLen, uint8_t* segment_num);

int Parse_subcriber_does_not_exist(subcriber_does_not_exist ne, int index, const uint8_t* buffer,
                                   const int buffLen, uint8_t* segment_num);

int Parse_subcriber_permitted(subcriber_permitted sp, int index, const uint8_t* buffer,
                              const int buffLen, uint8_t* segment_num);

int IsSubscriberValid(uint32_t subscriber_number, uint8_t technology);

bool GetSubscriberInfoFromDB(uint32_t subscriber_number, uint32_t* sub_num, uint8_t* tech, uint8_t* paid);

int HandleReceivedPacket(const uint8_t* buffer, const int buffLen, uint8_t* recv_segment_num,
                         payload* received_payload);

int HandleReceivedPacket(const uint8_t* buffer, const int buffLen, uint8_t* segment_num,
                         payload* received_payload) {
  if (buffer == NULL || buffLen <= 0) {
    printf("[ERROR] Invalid buffer or buffer length\n");
    return -1;
  
  }
  printf("\n");
  printf("Received data: %d bytes\n", buffLen);
  
  preamble p;

  int index = 0;
  p.start_of_packet = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  p.client_id = *(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  p.packet_type = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  printf("Preamble: \n");
  printf("_start of packet: 0x%x \n", p.start_of_packet);
  printf("_client_id: 0x%x\n", p.client_id);
  printf("_packet_type: 0x%x\n", p.packet_type);

  *segment_num = *(uint8_t* )&buffer[index];
  printf("_segment number: 0x%x \n", *segment_num);
  index += sizeof(uint8_t);
  printf("_Length: 0x%x\n", *(uint8_t* )&buffer[index]);
  index += sizeof(uint8_t);

  received_payload->technology = *(uint8_t* )&buffer[index];
  printf("_Technology: 0x%x \n", received_payload->technology);
  index += sizeof(uint8_t);
  received_payload->source_subscriber_number = *(uint32_t* )&buffer[index];
  printf("_Source Subscriber Number is: 0x%x\n", received_payload->source_subscriber_number);
  index += sizeof(uint32_t);

  printf("_end_of_packet: 0x%x\n",*(uint16_t* )&buffer[index]);
  
  if (p.packet_type == ACC_PER_VAL) {
    // SERVER ONLY
    return IsSubscriberValid(received_payload->source_subscriber_number,
                             received_payload->technology);
  }
  else if (p.packet_type == NOT_PAID_VAL) {
    // CLIENT ONLY
   
    return NOT_PAID;
  }
  else if (p.packet_type == NOT_EXIST_VAL) {
    // CLIENT ONLY
    return NOT_EXIST;
  }
  else if (p.packet_type == ACC_OK) {
    // CLIENT ONLY
    return ACC_OK;
  }
  else {
    printf("[ERROR] Unknown packet type \n");
    // TODO Defint this code 
    return 3;
  }

  return SUCCESS;
}

int IsSubscriberValid(uint32_t subscriber_number, uint8_t technology) {
  uint32_t subscriber_num_from_db = 0;
  uint8_t technology_from_db = 0;
  uint8_t paid=0;
  if (!GetSubscriberInfoFromDB(subscriber_number, &subscriber_num_from_db, &technology_from_db, &paid) ||
      subscriber_number != subscriber_num_from_db) {
    printf("subscriber not in db\n");
    return NOT_EXIST;
  }

  if (technology != technology_from_db)
    return TECHNOLOGY_DOES_NOT_MATCH;

  // Paid
  if (!paid)
    return NOT_PAID;

  printf("DB has subs num %u, tech %u, paid %u\n", subscriber_num_from_db, technology_from_db, paid);

  return SUCCESS;
}

bool GetSubscriberInfoFromDB(uint32_t subscriber_number, uint32_t* sub_num, uint8_t* tech, uint8_t* paid/*paid*/) {
  char str[10];
  printf("Sub num= %u\n", subscriber_number);
  if (snprintf(str, 10, "%u", subscriber_number) < 0) {
    printf("[ERROR] snprint error");
    return false;
  }

  return Search_in_DB("verification_database.txt", str, sub_num, tech, paid);
}


bool Search_in_DB(const char *fname, const char* subscriber_number, uint32_t *sub_num, uint8_t* tech, uint8_t* paid) {
  FILE *fp;
  char temp[100];
  bool value = false;

  printf("Finding %s in %s\n", subscriber_number, fname);
  if ((fp = fopen(fname, "r")) == NULL) {
    printf("[ERROR] Cannot open file %s\n", fname);
    return false;
  }	

  while (fgets(temp, 100, fp) != NULL) {
    if ((strstr(temp, subscriber_number)) != NULL) {
        printf("Entry found in db: %s\n", temp);
        value = true;
        break;
    }
  }

  if (value) {
    char* token;
    // Tokenize entry in db.
    *sub_num = (uint32_t)atoi(strtok(temp, ","));
    *tech = (uint8_t)atoi(strtok(NULL, ","));
    *paid = (uint8_t)atoi(strtok(NULL, ","));  
  }

  if (fp) {
    fclose(fp);
  }

  return value;
}


int Parse_access_permission_request(access_permission_request_packet ap, int index, const uint8_t* buffer, const int buffLen, uint8_t* segment_num){

   ap.segment_number = *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   ap.length = *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   payload data;
   data.technology= *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   data.source_subscriber_number=*(uint32_t* )&buffer[index];
   index += sizeof(uint32_t);
   ap.data =data;
   ap.end_of_packet = *(uint16_t* )&buffer[index];
   index += sizeof(uint16_t);
   printf("Access Permission Request Packet Contents \n");
   printf("_segment number: 0x%x \n", ap.segment_number);
   printf("_Length: 0x%x\n", ap.length);
   printf("_payload is :\n");
   printf("_Technology: 0x%x \n", ap.data.technology);
   printf("_Source Subscriber Number is: 0x%x\n", ap.data.source_subscriber_number);
   printf("_end_of_packet: 0x%x\n", ap.end_of_packet);

  return SUCCESS;
}
  

  
int Parse_not_paid_message(not_paid_message np, int index, const uint8_t* buffer, const int buffLen, uint8_t* segment_num){
// TODO fix all spaces and indenation in all code files.
   np.segment_number = *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   np.length = *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   payload data;
   data.technology= *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   data.source_subscriber_number=*(uint32_t* )&buffer[index];
   index += sizeof(uint32_t);
   np.data =data;
   np.end_of_packet = *(uint16_t* )&buffer[index];
   index += sizeof(uint16_t);
   printf("Subscriber has not paid message Contents \n");
   printf("_segment number: 0x%x \n", np.segment_number);
   printf("_Length: 0x%x\n", np.length);
   printf("_payload is :\n");
   printf("_Technology: 0x%x \n", np.data.technology);
   printf("_Source Subscriber Number is: 0x%x\n", np.data.source_subscriber_number);
   printf("_end_of_packet: 0x%x\n", np.end_of_packet);

  return SUCCESS;
}



int Parse_subcriber_does_not_exist(subcriber_does_not_exist ne, int index, const uint8_t* buffer, const int buffLen, uint8_t* segment_num){
  ne.segment_number = *(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  ne.length = *(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  payload data;
  data.technology= *(uint8_t* )&buffer[index];
  index += sizeof(uint8_t);
  data.source_subscriber_number=*(uint32_t* )&buffer[index];
  index += sizeof(uint32_t);
  ne.data =data;
  ne.end_of_packet = *(uint16_t* )&buffer[index];
  index += sizeof(uint16_t);
  printf("Subscriber does not exist message content : \n");
  printf("_segment number: 0x%x \n", ne.segment_number);
  printf("_Length: 0x%x\n", ne.length);
  printf("_payload is :\n");
  printf("_Technology: 0x%x \n", ne.data.technology);
  printf("_Source Subscriber Number is: 0x%x\n", ne.data.source_subscriber_number);
  printf("_end_of_packet: 0x%x\n", ne.end_of_packet);

  return SUCCESS;
}

  

int Parse_subcriber_permitted (subcriber_permitted sp, int index, const uint8_t* buffer, const int buffLen, uint8_t* segment_num){

   sp.segment_number = *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   sp.length = *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   payload data;
   data.technology= *(uint8_t* )&buffer[index];
   index += sizeof(uint8_t);
   data.source_subscriber_number=*(uint32_t* )&buffer[index];
   index += sizeof(uint32_t);
   sp.data =data;
   sp.end_of_packet = *(uint16_t* )&buffer[index];
   index += sizeof(uint16_t);
   printf("Subscriber permitted message contents: \n");
   printf("_segment number: 0x%x \n", sp.segment_number);
   printf("_Length: 0x%x\n", sp.length);
   printf("_payload is :\n");
   printf("_Technology: 0x%x \n", sp.data.technology);
   printf("_Source Subscriber Number is: 0x%x\n", sp.data.source_subscriber_number);
   printf("_end_of_packet: 0x%x\n", sp.end_of_packet);

  return SUCCESS;
}
