/******************************************************************************************
* Copyright 2017 Ideetron B.V.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************************/
/****************************************************************************************
* File:     LoRaMAC.cpp
* Author:   Gerben den Hartog
* Compagny: Ideetron B.V.
* Website:  http://www.ideetron.nl/LoRa
* E-mail:   info@ideetron.nl
****************************************************************************************/
/****************************************************************************************
* Created on:         06-01-2017
****************************************************************************************/
/*
*****************************************************************************************
* INCLUDE FILES
*****************************************************************************************
*/

#include <SPI.h>
#include "AES-128.h"
#include "RFM95.h"
#include "Encrypt.h"
#include "LoRaMAC.h"
#include "Struct.h"
#include "Config.h"
#include <Arduino.h>
#include "../STM32LowPowerCal/STM32LowPowerCal.h"
/*
 GLOBAL VARIABLES
*/

/*
*****************************************************************************************
* FUNCTIONS
*****************************************************************************************
*/


/*
*****************************************************************************************
* Description : Function that handles a send and receive cycle with timing for receive slots.
*				This function is only used for Class A motes. The wait times are tested with
*				the iot.semtech.com site.
*
* Arguments   : *Data_Tx pointer to tranmit buffer
*				*Data_Rx pointer to receive buffer
*				*RFM_Command pointer to current RFM state
*				*Session_Data pointer to sLoRa_Session sturct
*				*OTAA_Data pointer to sLoRa_OTAA struct
*				*Message_Rx pointer to sLoRa_Message struct used for the received message information
*				*LoRa_Settings pointer to sSetting struct
*****************************************************************************************
*/
void LORA_Cycle(sBuffer *Data_Tx, sBuffer *Data_Rx, RFM_command_t *RFM_Command, sLoRa_Session *Session_Data,
 									sLoRa_OTAA *OTAA_Data, sLoRa_Message *Message_Control, sSettings *LoRa_Settings)
{
	// calculate Receive_Delay in millis;
	int Receive_Delay_1 = LoRa_Settings->Recieve_Delay1 * 1000;
	int Receive_Delay_2 = LoRa_Settings->Recieve_Delay2 * 1000;

	unsigned long prevTime = 0;
	// save settings for later reset
	unsigned char rx1_ch = LoRa_Settings->Channel_Rx;
	#ifdef US_915
    unsigned char rx1_dr = LoRa_Settings->Datarate_Tx+10;
	#elif defined(EU_868)
	// save settings for later reset
    unsigned char rx1_dr = LoRa_Settings->Datarate_Rx;
	#endif

  	//Transmit
	if(*RFM_Command == NEW_RFM_COMMAND)
  	{
		// Reset ADR  Frame Control options to be sure...
		// do not reset ACK and FoptLength
		Message_Control->Frame_Control = Message_Control->Frame_Control & 0x2F;

		//Lora send data
    	LORA_Send_Data(Data_Tx, Session_Data, Message_Control, LoRa_Settings);
		// reset time after sending data -> this is the RX1 time window
		prevTime = millis();

		// clear Message_Control and Message_Options before receiving new message:
		Message_Control->Frame_Control = 0x00;
		// set direction to down for MIC calculations
		Message_Control->Direction = 0x01;


		// Class C open RX2 immediately after sending data
		if(LoRa_Settings->Mote_Class == CLASS_C)
    	{
			#ifdef US_915
			LoRa_Settings->Channel_Rx = 0x08;    // set Rx2 channel 923.3 MHZ
			LoRa_Settings->Datarate_Rx = SF12BW500;   //set RX2 datarate 12
			#elif defined(EU_868)
			LoRa_Settings->Channel_Rx = CHRX2;    // set Rx2 channel 923.3 MHZ
			LoRa_Settings->Datarate_Rx = SF12BW125;   //set RX2 datarate 12
			#endif
			LORA_Receive_Data(Data_Rx, Session_Data, OTAA_Data, Message_Control, LoRa_Settings);  //BUG DETECT SENDED PACKET ALWAYS (IT DOES UPDATE)
		}

		if(LoRa_Settings->Mote_Class == CLASS_A)
		{
			// gotosleep for receive_delay - 20 ms
			// millis stop while in sleep!
			DEBUG_PRINTLN("MAC: Sleep RX1");
			RFM_Mode_Sleep_LowPower();
			LowPowerCal.begin();
			// sleep 
			// changed from 80 to 120 when debug is on -> to be tested
			LowPowerCal.deepSleep(Receive_Delay_1, 80);
			DEBUG_PRINTLN("MAC: Wakeup");

			//Wait rx1 window delay
			//Receive on RX2 if countinous mode is available
			//check if anything if coming on class C RX2 window in class A no DIO0 flag will be activated
			// do
			// {
			// 	//Poll Rx done for getting message on Class C device!
			// 	//DIO0 flag will only be active while class C
			// 	if(digitalRead(RFM_pins.DIO0))
			// 	{
			// 		LORA_Receive_Data(Data_Rx, Session_Data, OTAA_Data, Message_Control, LoRa_Settings);
			// 	}		
			// }while(millis() - prevTime < Receive_Delay_1);
			// Return if message on RX1
			//if (Data_Rx->Counter > 0)return;

			//Return to datarate and channel for RX1
			LoRa_Settings->Channel_Rx = rx1_ch;    // set RX1 channel
			LoRa_Settings->Datarate_Rx = rx1_dr;   //set RX1 datarate
			// Receive Data RX1 usualy for class A device
			LORA_Receive_Data(Data_Rx, Session_Data, OTAA_Data, Message_Control, LoRa_Settings);

			if (Data_Rx->Counter == 0)
			{
				DEBUG_PRINTLN("MAC: Sleep RX2");
				RFM_Mode_Sleep_LowPower();
				LowPowerCal.begin();
				// 100 ms extra for time spend on RX1 when not receiving data on RX1!
				LowPowerCal.deepSleep(Receive_Delay_2 - Receive_Delay_1, 100); 
				DEBUG_PRINTLN("MAC: Wakeup");

				// Wait rx2 window delay
				// do{
				// 	//Poll Rx done for getting message on Class C device!
				// 	//DIO0 flag will only be active while class C
				// 	if(digitalRead(RFM_pins.DIO0))
				// 	{
				// 		LORA_Receive_Data(Data_Rx, Session_Data, OTAA_Data, Message_Control, LoRa_Settings);
				// 	}
						
				// }while(millis() - prevTime < Receive_Delay_2);
				//Return if message on RX2
				//if (Data_Rx->Counter > 0)return;

				//Configure datarate and channel for RX2
				LoRa_Settings->Channel_Rx = LoRa_Settings->Channel_Rx;   // set RX2 channel
				LoRa_Settings->Datarate_Rx = LoRa_Settings->Datarate_Rx2;   //set RX2 datarate
				//Receive Data RX2
				//If class A timeout will apply
				//If class C continous Rx will happen
				LORA_Receive_Data(Data_Rx, Session_Data, OTAA_Data, Message_Control, LoRa_Settings);
			}
			*RFM_Command = NO_RFM_COMMAND;

			LoRa_Settings->Channel_Rx = rx1_ch;
			LoRa_Settings->Datarate_Rx = rx1_dr;
		}

		if(LoRa_Settings->Mote_Class == CLASS_A_NO_RX)
		{
			// do nothing... do not switch to receive mode.
		}
	}
}

/*
*****************************************************************************************
* Description : Function that is used to build a LoRaWAN data message and then tranmit it.
*
* Arguments   : *Data_Tx pointer to tranmit buffer
*				*Session_Data pointer to sLoRa_Session sturct
*				*LoRa_Settings pointer to sSetting struct
*****************************************************************************************
*/
void LORA_Send_Data(sBuffer *Data_Tx, sLoRa_Session *Session_Data, sLoRa_Message *Message_Control, sSettings *LoRa_Settings)
{
	//Define variables
	unsigned char i;

	//Initialise RFM buffer
	unsigned char RFM_Data[MAX_UPLINK_PAYLOAD_SIZE+65];
	sBuffer RFM_Package = {&RFM_Data[0], 0x00};

	//Initialise Message struct for a transmit message
	//sLoRa_Message Message;

	Message_Control->MAC_Header = 0x00;
	//Frame port always 1 for now
	Message_Control->Frame_Port = 0x01; 

	//Load device address from session data into the message
	Message_Control->DevAddr[0] = Session_Data->DevAddr[0];
	Message_Control->DevAddr[1] = Session_Data->DevAddr[1];
	Message_Control->DevAddr[2] = Session_Data->DevAddr[2];
	Message_Control->DevAddr[3] = Session_Data->DevAddr[3];

	//Set up direction
	Message_Control->Direction = 0x00;

	//Load the 32 bits of the frame counter from the session data into the message control
	Message_Control->Frame_Counter = *Session_Data->Frame_Counter;

	//Set confirmation
	if(LoRa_Settings->Confirm == 0x00)
	{   
		//Unconfirmed
		Message_Control->MAC_Header = Message_Control->MAC_Header | 0x40;
	}
	else
	{
		//Confirmed
		Message_Control->MAC_Header = Message_Control->MAC_Header | 0x80;
	}

	//Build the Radio Package
	//Load mac header
	RFM_Package.Data[0] = Message_Control->MAC_Header;

	//Load device address
	RFM_Package.Data[1] = Message_Control->DevAddr[3];
	RFM_Package.Data[2] = Message_Control->DevAddr[2];
	RFM_Package.Data[3] = Message_Control->DevAddr[1];
	RFM_Package.Data[4] = Message_Control->DevAddr[0];

	//Load frame control
	RFM_Package.Data[5] = Message_Control->Frame_Control;

	//Load frame counter
	RFM_Package.Data[6] = (*Session_Data->Frame_Counter & 0x00FF);
	RFM_Package.Data[7] = ((*Session_Data->Frame_Counter >> 8) & 0x00FF);

	// if frame options are set by Frame_Control add them to the frame
	// last 4:0 in frame control is lenght of Frame_Options
	unsigned char Frame_Options_Lenght =  Message_Control->Frame_Control & 0x0F;

	RFM_Package.Counter = 8;

	if (Frame_Options_Lenght > 0)
	{
		// add frame options into RFM_Package
		for (int i = 0; i < Frame_Options_Lenght; i++)
		{
			RFM_Package.Data[RFM_Package.Counter+i] = Message_Control->Frame_Options[i];
		}
		// move data counter/pointer
		RFM_Package.Counter += Frame_Options_Lenght;
	}

	//If there is data load the Frame_Port field
	//Encrypt the data and load the data
	if(Data_Tx->Counter > 0x00)
	{
		//Load Frame port field
		//RFM_Data[8] = Message.Frame_Port;
		RFM_Package.Data[RFM_Package.Counter] = LoRa_Settings->Mport;

		//Raise package counter
		RFM_Package.Counter++;

		//Encrypt the data
		Encrypt_Payload(Data_Tx, Session_Data->AppSKey, Message_Control);

		//Load Data
		for(i = 0; i < Data_Tx->Counter; i++)
		{
			RFM_Package.Data[RFM_Package.Counter++] = Data_Tx->Data[i];
		}

	}

	//Calculate MIC
	Construct_Data_MIC(&RFM_Package, Session_Data, Message_Control);

	//Load MIC in package
	for(i = 0; i < 4; i++)
	{
	   RFM_Package.Data[RFM_Package.Counter++] = Message_Control->MIC[i];
	}

	//Send Package
	RFM_Send_Package(&RFM_Package, LoRa_Settings);
	DEBUG_PRINT("MAC: RFM_Send_Package called @ ");
	DEBUG_PRINTLN(millis());

	// Raise Frame counter (uint32_t = 32 bits!.., however only 16 bits are send!)
	if(*Session_Data->Frame_Counter != 0xFFFFFFFF)
	{
  	 // Raise frame counter
	   *Session_Data->Frame_Counter = *Session_Data->Frame_Counter + 1;
	}
	else
	{
	   *Session_Data->Frame_Counter = 0x00000001;
	}

	//Change channel for next message if hopping is activated
	if(LoRa_Settings->Channel_Hopping == 0x01)
	{
	if(LoRa_Settings->Channel_Tx < 0x07)
	{
		LoRa_Settings->Channel_Tx++;
	}
	else
	{
		LoRa_Settings->Channel_Tx = 0x00;
	}
	}
}


/*
*****************************************************************************************
* Description : Function that is used to receive a LoRaWAN message and retrieve the data from the RFM
*               Also checks on CRC, MIC and Device Address
*               This function is used for Class A and C motes.
*
* Arguments   : *Data_Rx pointer to receive buffer
*				*Session_Data pointer to sLoRa_Session sturct
*				*OTAA_Data pointer to sLoRa_OTAA struct
*				*Message_Rx pointer to sLoRa_Message struct used for the received message information
*				*LoRa_Settings pointer to sSetting struct
*****************************************************************************************
*/
void LORA_Receive_Data(sBuffer *Data_Rx, sLoRa_Session *Session_Data, sLoRa_OTAA *OTAA_Data, sLoRa_Message *Message_Control, sSettings *LoRa_Settings)
{
	// remember this takes time to process -> RX1 and RX2 window will be too large!
	// correct every window with 20 milliseconds!
	DEBUG_PRINT("MAC: LORA_Received_Data called @ ");
	DEBUG_PRINTLN(millis());
	// DEBUG_PRINT("Receive DataRate: ");
	// DEBUG_PRINTLN(LoRa_Settings->Datarate_Rx);

	unsigned char i;

    //Initialise RFM buffer
	unsigned char RFM_Data[MAX_DOWNLINK_PAYLOAD_SIZE + 65];
	sBuffer RFM_Package = {&RFM_Data[0], 0x00};

	unsigned char MIC_Check;
  	unsigned char Address_Check;

	unsigned char Frame_Options_Length;

	unsigned char Data_Location;

	message_t Message_Status = NO_MESSAGE;

	//If it is a type A device switch RFM to single receive
	if(LoRa_Settings->Mote_Class == CLASS_A)
	{
		Message_Status = RFM_Single_Receive(LoRa_Settings);
	}

	if(LoRa_Settings->Mote_Class == CLASS_A_NO_RX)
	{
		// do nothing
	}

	if(LoRa_Settings->Mote_Class == CLASS_C)
	{
		//Switch RFM to standby
		RFM_Switch_Mode(RFM_MODE_STANDBY);
		Message_Status = NEW_MESSAGE;
	}

	//If there is a message received get the data from the RFM
	if(Message_Status == NEW_MESSAGE)
	{
		DEBUG_PRINTLN("MAC: LORA_Received_Data called : NEW_MESSAGE");
		Message_Status = RFM_Get_Package(&RFM_Package);

		//If mote class C switch RFM back to continuous receive
		if(LoRa_Settings->Mote_Class == CLASS_C)
		{
			//Switch RFM to Continuous Receive
			RFM_Continuous_Receive(LoRa_Settings);
		}
	}

	//if CRC ok breakdown package
	if(Message_Status == CRC_OK)
	{
		//Get MAC_Header
    	Message_Control->MAC_Header = RFM_Data[0];

		//Data message
		// First Three bits of MAC_Header are the message type bits:
		// 000x 0x00 Join request
		// 001x 0x02 Join accept
		// 010x 0x40 Unconfirmed data up
		// 011x 0x60 Unconfirmed data Down
		// 100x 0x80 Confirmed data up
		// 101x 0xA0 Confirmed data Down

		if(Message_Control->MAC_Header == 0x40 || Message_Control->MAC_Header == 0x60 || Message_Control->MAC_Header == 0x80 || Message_Control->MAC_Header == 0xA0)
		{
			//Get device address from received data
			Message_Control->DevAddr[0] = RFM_Data[4];
			Message_Control->DevAddr[1] = RFM_Data[3];
			Message_Control->DevAddr[2] = RFM_Data[2];
			Message_Control->DevAddr[3] = RFM_Data[1];

			//Get frame control field (one bytes)
			Message_Control->Frame_Control = RFM_Data[5];

			 //Get frame counter
			Message_Control->Frame_Counter = RFM_Data[7];
			Message_Control->Frame_Counter = (Message_Control->Frame_Counter << 8) + RFM_Data[6];

			//Lower Package length with 4 to remove MIC length
			RFM_Package.Counter -= 4;

			//Calculate MIC
			Construct_Data_MIC(&RFM_Package, Session_Data, Message_Control);

			MIC_Check = 0x00;

      		//Compare MIC
			for(i = 0x00; i < 4; i++)
			{

				if(RFM_Data[RFM_Package.Counter + i] == Message_Control->MIC[i])
				{
					MIC_Check++;
				}
			}

			//Check MIC
			if(MIC_Check == 0x04)
			{
				Message_Status = MIC_OK;
			}
			else
			{
				Message_Status = WRONG_MESSAGE;
			}

			//Check Address
			Address_Check = 0;

			if(MIC_Check == 0x04)
			{
					for(i = 0x00; i < 4; i++)
					{
						if(Session_Data->DevAddr[i] == Message_Control->DevAddr[i])
						{
							Address_Check++;
						}
					}
			}

			if(Address_Check == 0x04)
			{
				Message_Status = ADDRESS_OK;
			}
			else
			{
				Message_Status = WRONG_MESSAGE;
			}

			//if the address is OK then decrypt the data
			//Send the data to USB??
			if(Message_Status == ADDRESS_OK)
			{
				DEBUG_PRINTLN("MAC: CRC and ADDRESS are ok, decrypt data");
				Data_Location = 8;

				//Get length of frame options field
				Frame_Options_Length = (Message_Control->Frame_Control & 0x0F);

				DEBUG_PRINT("MAC: FCtrl ADR bit: ");
				DEBUG_PRINTLN((Message_Control->Frame_Control &0x80) >> 7);
				DEBUG_PRINT("MAC: FCtrl ACK bit: ");
				DEBUG_PRINTLN((Message_Control->Frame_Control &0x20) >> 5);
				DEBUG_PRINT("MAC: FCtrl FPending bit: ");
				DEBUG_PRINTLN((Message_Control->Frame_Control &0x10) >> 4);
				DEBUG_PRINT("MAC: FCtrl Frame_Options_Length: ");
				DEBUG_PRINTLN(Frame_Options_Length);

				//Add length of frame options field to data location
				Data_Location = Data_Location + Frame_Options_Length;

				// put frame options into message
				if (Frame_Options_Length > 0)
				{
					DEBUG_PRINT("MAC: FOpts data:");
					for (int i = 0; i < Frame_Options_Length; i++)
					{
						Message_Control->Frame_Options[i] = RFM_Data[Data_Location - Frame_Options_Length + i];
						DEBUG_PRINT(" ");
						DEBUG_PRINT(RFM_Data[Data_Location - Frame_Options_Length + i], HEX);
						
					}
					DEBUG_PRINTLN("<EOD");
				}

				//Check if there is data in the package
				if(RFM_Package.Counter == Data_Location)
				{
					// there is no data
					DEBUG_PRINTLN("MAC: NO DATA");
					Data_Rx->Counter = 0x00;
				}
				else
				{
					//Get port field when there is data
					Message_Control->Frame_Port = RFM_Data[8];

					//Calculate the amount of data in the package
					Data_Rx->Counter = (RFM_Package.Counter - Data_Location -1);

					//Correct the data location by 1 for the Fport field
					Data_Location = (Data_Location + 1);
				}

				//Copy and decrypt the data
				if(Data_Rx->Counter != 0x00)
				{
					for(i = 0; i < Data_Rx->Counter; i++)
					{
						Data_Rx->Data[i] = RFM_Data[Data_Location + i];
					}

					//Check frame port fields. When zero it is a mac command message encrypted with NwkSKey
					if(Message_Control->Frame_Port == 0x00)
					{
						Encrypt_Payload(Data_Rx, Session_Data->NwkSKey, Message_Control);
					}
					else
					{
						Encrypt_Payload(Data_Rx, Session_Data->AppSKey, Message_Control);
					}

					Message_Status = MESSAGE_DONE;
				}
			}
		}

		if(Message_Status == WRONG_MESSAGE)
		{
			Data_Rx->Counter = 0x00;
 		}
	}
}
/*
*****************************************************************************************
* Description : Functions that is used to generate device nonce used in the join request function
*				This is based on reading RSSI multiple times;
*
* Arguments   : *Devnonce pointer to the devnonce arry of withc is unsigned char[2]
*****************************************************************************************
*/

/* xorshift mix */
static uint32_t mix32(uint32_t x) 
{
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

/* Fallback RSSI entropy collector */
static uint32_t rssi_entropy_pool(unsigned int samples)
{
  uint32_t pool = 0xA5A5A5A5u ^ (uint32_t)millis();

  RFM_Init();

  for (unsigned int i = 0; i < samples; ++i) {
	//Switch RFM to Single reception
	RFM_Switch_Mode(RFM_MODE_RXSINGLE);
	
    delay(3); /* allow RSSI update */

    uint8_t r = RFM_Get_Rssi(); /* RegRssiValue */
	
    uint8_t s = (uint8_t)(r ^ (r << 3));
    pool ^= ((uint32_t)s << ((i & 3) * 8));
    pool = pool * 1664525u + 1013904223u;
    pool = mix32(pool);

    /* mix in micros low bits for jitter if available */
    pool ^= (uint32_t)(micros() & 0xFFFF);
    delay(2);
  }

  pool ^= (uint32_t)millis();
  pool = mix32(pool);
  RFM_Switch_Mode(RFM_MODE_STANDBY);
  return pool;
}

/* fallback to RSSI mixing */
void arduino_hal_getRandomBytes(uint8_t *buf, size_t len)
{
  size_t off = 0;

  while (off < len) {
    uint32_t word = 0;
    word = rssi_entropy_pool(8);

    /* mix in timers */
    word ^= ((uint32_t)millis() << 16) ^ (uint32_t)(micros() & 0xFFFF);
    word = mix32(word);

    size_t tocopy = (len - off) < 4 ? (len - off) : 4;
    for (size_t i = 0; i < tocopy; ++i) {
      buf[off + i] = (uint8_t)(word & 0xFF);
      word >>= 8;
    }
    off += tocopy;
  }
}

static void Generate_DevNonce(unsigned char *DevNonce)
{
	// generate by using RSSI measurements
	arduino_hal_getRandomBytes(DevNonce, 2);

	DEBUG_PRINT("DevNonce: ");
  	DEBUG_PRINT(DevNonce[0], HEX);
  	DEBUG_PRINTLN(DevNonce[1], HEX);
}
/*
*****************************************************************************************
* Description : Function that is used to send a join request to a network.
*
* Arguments   : *OTAA_Data pointer to sLoRa_OTAA struct
*				*LoRa_Settings pointer to sSetting struct
*****************************************************************************************
*/
void LoRa_Send_JoinReq(sLoRa_OTAA *OTAA_Data, sSettings *LoRa_Settings)
{
	unsigned char i;

	//Initialise RFM data buffer
	unsigned char RFM_Data[23];
	sBuffer RFM_Package = { &RFM_Data[0], 0x00};

	//Initialise message sturct
	sLoRa_Message Message;

	Message.MAC_Header = 0x00; //Join request
	Message.Direction = 0x00; //Set up Direction

	//Construct OTAA Request message
	//Load Header in package
	RFM_Data[0] = Message.MAC_Header;

	//Load AppEUI in package
	for(i = 0x00; i < 8; i++)
	{
		RFM_Data[i+1] = OTAA_Data->AppEUI[7-i];
	}

	//Load DevEUI in package
	for(i= 0x00; i < 8; i++)
	{
		RFM_Data[i+9] = OTAA_Data->DevEUI[7-i];
	}

	//Generate DevNonce
	Generate_DevNonce(OTAA_Data->DevNonce);

	//Load DevNonce in package
	RFM_Data[17] = OTAA_Data->DevNonce[0];
	RFM_Data[18] = OTAA_Data->DevNonce[1];

	//Set length of package
	RFM_Package.Counter = 19;

	//Get MIC
	Calculate_MIC(&RFM_Package, OTAA_Data->AppKey, &Message);

	//Load MIC in package
	RFM_Data[19] = Message.MIC[0];
	RFM_Data[20] = Message.MIC[1];
	RFM_Data[21] = Message.MIC[2];
	RFM_Data[22] = Message.MIC[3];

	//Set length of package to the right length
	RFM_Package.Counter = 23;

	//Send Package
	RFM_Send_Package(&RFM_Package, LoRa_Settings);
}

bool LORA_join_Accept(sBuffer *Data_Rx,sLoRa_Session *Session_Data, sLoRa_OTAA *OTAA_Data, sLoRa_Message *Message, sSettings *LoRa_Settings)
{

	bool joinStatus = false;
	unsigned char i;

    // Initialise RFM buffer
	unsigned char RFM_Data[64];
	sBuffer RFM_Package = {&RFM_Data[0], 0x00};
	unsigned char MIC_Check;

	message_t Message_Status = NO_MESSAGE;

	// RFM to single receive, check if a messages is received
	Message_Status = RFM_Single_Receive(LoRa_Settings);

  	// If there is a message received get the data from the RFM
	if(Message_Status == NEW_MESSAGE) 
	{
		Message_Status = RFM_Get_Package(&RFM_Package);
	}

	// If CRC ok breakdown package
	if(Message_Status == CRC_OK)
	{
		// Get MAC_Header
		Message->MAC_Header = RFM_Data[0];

		// Join Accept message
		if(Message->MAC_Header == 0x20)
		{
			// Copy the data into the data array
			for(i = 0x00; i < RFM_Package.Counter; i++)
				Data_Rx->Data[i] = RFM_Package.Data[i];

			// Set data counter
			Data_Rx->Counter = RFM_Package.Counter;

			//Decrypt the data
			for(i = 0x00; i < ((Data_Rx->Counter - 1) / 16); i++)
				AES_Encrypt(&(Data_Rx->Data[(i*16)+1]),OTAA_Data->AppKey);

			// Calculate MIC
			// Remove MIC from number of bytes
			Data_Rx->Counter -= 4;

			// Get MIC
			Calculate_MIC(Data_Rx, OTAA_Data->AppKey, Message);

			// Clear MIC check counter
			MIC_Check = 0x00;

			// Compare MIC
			for(i = 0x00; i < 4; i++)
				if(Data_Rx->Data[Data_Rx->Counter + i] == Message->MIC[i])
					MIC_Check++;

			// Check if MIC compares
			if(MIC_Check == 0x04)
				Message_Status = MIC_OK;
			else
				Message_Status = WRONG_MESSAGE;

			// Get Key's and data from package when MIC is OK
			if(Message_Status == MIC_OK)
			{
				// Get AppNonce
				for(i = 0; i< 3; i++)
					OTAA_Data->AppNonce[i] = Data_Rx->Data[i+1];

				// Get Net ID
				for(i = 0; i< 3; i++)
					OTAA_Data->NetID[i] = Data_Rx->Data[i + 4];

				// Get session Device address
				for(i = 0; i< 4; i++)
					Session_Data->DevAddr[3-i] = Data_Rx->Data[i + 7];

				// Calculate Network Session Key
				Session_Data->NwkSKey[0] = 0x01;

				// Load AppNonce
				for(i = 0; i < 3; i++)
					Session_Data->NwkSKey[i+1] = OTAA_Data->AppNonce[i];

				// Load NetID
				for(i = 0; i < 3; i++)
					Session_Data->NwkSKey[i+4] = OTAA_Data->NetID[i];

				// Load Dev Nonce
				Session_Data->NwkSKey[7] = OTAA_Data->DevNonce[0];
				Session_Data->NwkSKey[8] = OTAA_Data->DevNonce[1];

				// Pad with zeros
				for(i = 9; i <= 15; i++)
					Session_Data->NwkSKey[i] = 0x00;

				// Copy to AppSkey
				for(i = 0x00; i < 16; i++) 
					Session_Data->AppSKey[i] = Session_Data->NwkSKey[i];

				// Change first byte of AppSKey
				Session_Data->AppSKey[0] = 0x02;

				// Calculate the keys
				AES_Encrypt(Session_Data->NwkSKey,OTAA_Data->AppKey);
				AES_Encrypt(Session_Data->AppSKey,OTAA_Data->AppKey);

				// Reset Frame counter
				*Session_Data->Frame_Counter = 0x0001;

				DEBUG_PRINT(F("NwkSKey: "));
				for(byte i = 0; i < 16 ;++i) DEBUG_PRINT(Session_Data->NwkSKey[i],HEX);
				DEBUG_PRINTLN();
				DEBUG_PRINT(F("AppSKey: "));
				for(byte i = 0; i < 16 ;++i) DEBUG_PRINT(Session_Data->AppSKey[i],HEX);
				DEBUG_PRINTLN();

				// debug DLSettings field
				DEBUG_PRINTLN("Join DL Settings: ");
				// DEBUG_PRINT("RFU: (Reserved for Further Use) ");
				// DEBUG_PRINTLN((Data_Rx->Data[11] & 0x80) >> 7 , HEX);
				DEBUG_PRINT("RX1DROffset: ");
				DEBUG_PRINTLN((Data_Rx->Data[11] & 0x70) >> 4 , HEX);
				DEBUG_PRINT("RX2DataRate: ");
				DEBUG_PRINTLN(Data_Rx->Data[11] & 0x0F , HEX);

				// debug RXDelay
				DEBUG_PRINT("RXDelay: ");
				DEBUG_PRINTLN(Data_Rx->Data[12], HEX);

				/*
				The RXDelay field sets the downlink RX1 delay and follows the same convention as the Del
				1523 field in the RXTimingSetupReq command.
				*/

				// debug if CLlist (Channel Frequency) is available and size:
				DEBUG_PRINT("Length of CFList: ");
				DEBUG_PRINTLN(Data_Rx->Counter-13);
				// print frequency list
				DEBUG_PRINTLN("Frequencies in CFList: ");
				#ifdef DEBUG_LORA
					if (Data_Rx->Counter-13 > 0)
					{
						// number of bytes per frequency = 3
						for (int i = 13; i < Data_Rx->Counter - 1; i+=3)
						{
							unsigned int freq = (Data_Rx->Data[i]) + (Data_Rx->Data[i+1] << 8) + (Data_Rx->Data[i+2] << 16);
							DEBUG_PRINTLN(freq * 100);
						}
					}
					// The CFListType SHALL be equal to zero (0) to indicate that the CFList contains a list of frequencies.
					// DEBUG_PRINT("CFListType: (should be 0): ");
					// DEBUG_PRINTLN(Data_Rx->Data[Data_Rx->Counter-1], HEX);
				#endif

				//Clear Data counter
				Data_Rx->Counter = 0x00;

				joinStatus = true;
			}
		}
	}
	return joinStatus;
}
