/*
 * ESP32 YModem driver
 *
 * Copyright (C) LoBo 2017
 *
 * Author: Boris Lovosevic (loboris@gmail.com)
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 */

#include "ymodem.h"

constexpr unsigned int TIMEOUT_MS_VALUE = 1000; 

//------------------------------------------------------------------------
static unsigned short crc16(const unsigned char *buf, unsigned long count)
{
  unsigned short crc = 0;
  int i;

  while(count--) {
    crc = crc ^ *buf++ << 8;

    for (i=0; i<8; i++) {
      if (crc & 0x8000) crc = crc << 1 ^ 0x1021;
      else crc = crc << 1;
    }
  }
  return crc;
}

//--------------------------------------------------------------
static int32_t Receive_Byte (unsigned char *c, uint32_t timeout_ms)
{
	//unsigned char ch;
  //int len = uart_read_bytes(EX_UART_NUM, &ch, 1, timeout / portTICK_RATE_MS);
  //int len = getchar_timeout_ms(TIMEOUT_MS_VALUE);

  unsigned long start = millis();
  //int getCharResult = 1;
  int len = 0;

  while (1) {
    if(Serial.available() > 0)
    {
      *c = Serial.read();
      len = 1;
      break;
    }    
    
    if (millis() - start > timeout_ms)
    {
      //getCharResult = -1;
      break;
    }
  }

  if (len <= 0) return -1;

  return 0;
  //---------------------------------
  // int len = Serial.readBytes(c, 1);
  // if (len <= 0) return -1;

  // //*c = (unsigned char)len;
  // return 0;
  //---------------------------------
}

//------------------------
static void uart_consume()
{
	char ch[64];
  //while (uart_read_bytes(EX_UART_NUM, ch, 64, 100 / portTICK_RATE_MS) > 0) ;
  // for(uint8_t i = 0; i < sizeof(ch); i++)
  // {
  //   getchar_timeout_ms(TIMEOUT_MS_VALUE);
  // }
  Serial.readBytes(ch, 64);
}

//--------------------------------
static uint32_t Send_Byte (char c)
{
  //uart_write_bytes(EX_UART_NUM, &c, 1);
  Serial.write(c);
  return 0;
}

//----------------------------
static void send_CA ( void ) {
  Send_Byte(CA);
  Send_Byte(CA);
}

//-----------------------------
static void send_ACK ( void ) {
  Send_Byte(ACK);
}

//----------------------------------
static void send_ACKCRC16 ( void ) {
  Send_Byte(ACK);
  Send_Byte(CRC16);
}

//-----------------------------
static void send_NAK ( void ) {
  Send_Byte(NAK);
}

//-------------------------------
static void send_CRC16 ( void ) {
  Send_Byte(CRC16);
}


/**
  * @brief  Receive a packet from sender
  * @param  data
  * @param  timeout
  * @param  length
  *    >0: packet length
  *     0: end of transmission
  *    -1: abort by sender
  *    -2: error or crc error
  * @retval 0: normally return
  *        -1: timeout
  *        -2: abort by user
  */
//--------------------------------------------------------------------------
static int32_t Receive_Packet (uint8_t *data, int *length, uint32_t timeout)
{
  int count, packet_size, i;
  unsigned char ch;
  *length = 0;
  
  // receive 1st byte
  if (Receive_Byte(&ch, timeout) < 0) {
	  return -1;
  }

  switch (ch) {
    case SOH:
		packet_size = PACKET_SIZE;
		break;
    case STX:
		packet_size = PACKET_1K_SIZE;
		break;
    case EOT:
        *length = 0;
        return 0;
    case CA:
    	if (Receive_Byte(&ch, timeout) < 0) {
    		return -2;
    	}
    	if (ch == CA) {
    		*length = -1;
    		return 0;
    	}
    	else return -1;
    case ABORT1:
    case ABORT2:
    	return -2;
    default:
    	//vTaskDelay(100 / portTICK_RATE_MS);
      delay(100);
    	uart_consume();
    	return -1;
  }

  *data = (uint8_t)ch;
  uint8_t *dptr = data+1;
  count = packet_size + PACKET_OVERHEAD-1;

  for (i=0; i<count; i++) {
	  if (Receive_Byte(&ch, timeout) < 0) {
		  return -1;
	  }
	  *dptr++ = (uint8_t)ch;;
  }

  if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff)) {
      *length = -2;
      return 0;
  }
  if (crc16(&data[PACKET_HEADER], packet_size + PACKET_TRAILER) != 0) {
      *length = -2;
      return 0;
  }

  *length = packet_size;
  return 0;
}

// Receive a file using the ymodem protocol.
//-----------------------------------------------------------------
int Ymodem_Receive (unsigned char* buf, unsigned int maxsize, char* getname)
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD];
  uint8_t *file_ptr;
  char file_size[128];
  unsigned int i, file_len, write_len, session_done, file_done, packets_received, errors, size = 0;
  int packet_length = 0;
  file_len = 0;
  int eof_cnt = 0;
  
  for (session_done = 0, errors = 0; ;) {
    for (packets_received = 0, file_done = 0; ;) {

      switch (Receive_Packet(packet_data, &packet_length, NAK_TIMEOUT)) {
        case 0:  // normal return
          switch (packet_length) {
            case -1:
                // Abort by sender
                send_ACK();
                size = -1;
                goto exit;
            case -2:
                // error
                errors ++;
                if (errors > 5) {
                  send_CA();
                  size = -2;
                  goto exit;
                }
                send_NAK();
                break;
            case 0:
                // End of transmission
            	eof_cnt++;
            	if (eof_cnt == 1) {
            		send_NAK();
            	}
            	else {
            		send_ACKCRC16();
            	}
                break;
            default:
              // ** Normal packet **
              if (eof_cnt > 1) {
          		send_ACK();
              }
              else if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0x000000ff)) {
                errors ++;
                if (errors > 5) {
                  send_CA();
                  size = -3;
                  goto exit;
                }
                send_NAK();
              }
              else {
                if (packets_received == 0) {
                  // ** First packet, Filename packet **
                  if (packet_data[PACKET_HEADER] != 0) {
                    errors = 0;
                    // ** Filename packet has valid data
                    if (getname) {
                      for (i = 0, file_ptr = packet_data + PACKET_HEADER; ((*file_ptr != 0) && (i < 64));) {
                        *getname = *file_ptr++;
                        getname++;
                      }
                      *getname = '\0';
                    }
                    for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && ((int)i < packet_length);) {
                      file_ptr++;
                    }
                    for (i = 0, file_ptr ++; (*file_ptr != ' ') && (i < FILE_SIZE_LENGTH);) {
                      file_size[i++] = *file_ptr++;
                    }
                    file_size[i++] = '\0';
                    if (strlen(file_size) > 0) size = strtol(file_size, NULL, 10);
                    else size = 0;

                    // Test the size of the file
                    if ((size < 1) || (size > maxsize)) {
                      // End session
                      send_CA();
                      if (size > maxsize) size = -9;
                      else size = -4;
                      goto exit;
                    }

                    file_len = 0;
                    send_ACKCRC16();
                  }
                  // Filename packet is empty, end session
                  else {
                      errors ++;
                      if (errors > 5) {
                        send_CA();
                        size = -5;
                        goto exit;
                      }
                      send_NAK();
                  }
                }
                else {
                  // ** Data packet **
                  // Write received data to file
                  if (file_len < size) {
                    file_len += packet_length;  // total bytes received
                    if (file_len > size) {
                    	write_len = packet_length - (file_len - size);
                    	file_len = size;
                    }
                    else write_len = packet_length;

                    memcpy(buf, (char*)(packet_data + PACKET_HEADER), write_len);
                    buf += write_len;

                    // int written_bytes = fwrite((char*)(packet_data + PACKET_HEADER), 1, write_len, ffd);
                    // if (written_bytes != write_len) { //failed
                    //   /* End session */
                    //   send_CA();
                    //   size = -6;
                    //   goto exit;
                    // }

                  }
                  //success
                  errors = 0;
                  send_ACK();
                }
                packets_received++;
              }
          }
          break;
        case -2:  // user abort
          send_CA();
          size = -7;
          goto exit;
        default: // timeout
          if (eof_cnt > 1) {
        	file_done = 1;
          }
          else {
			  errors ++;
			  if (errors > MAX_ERRORS) {
				send_CA();
				size = -8;
				goto exit;
			  }
			  send_CRC16();
          }
      }
      if (file_done != 0) {
    	  session_done = 1;
    	  break;
      }
    }
    if (session_done != 0) break;
  }
exit:
  return size;
}