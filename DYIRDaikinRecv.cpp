
#include <DYIRDaikinRecv.h>

//decode
#define SAMPLE_DELAY_TIME 10//uS
#define IDLE_TIMER_COUNT ((1000*13)/SAMPLE_DELAY_TIME)//SAMPLE_DELAY_TIME*100*13
#define BUFFER_SIZE 310
#define NORMAL_INPUT_STATE 1
//
#define SIGNAL_TIMEOUT__COUNT (6000/SAMPLE_DELAY_TIME)
#define PACKET_TIMEOUT__COUNT (40000/SAMPLE_DELAY_TIME)

// 0:none,1:start,2:packet,3:stop,4 packet error,5:wake
#define SIGNAL_PATTERN_START 1
#define SIGNAL_PATTERN_PACKET 2
#define SIGNAL_PATTERN_STOP 3
#define SIGNAL_PATTERN_PACKET_ERROR 4
#define SIGNAL_PATTERN_WAKUP 5
//
#define SIGNAL_PAIRED	2

//
#define DEBUG_IR_PRINT
//~ #define DEBUG_IR_PRINT_DECODED_DATA

uint8_t DYIRDaikinRecv::begin(uint8_t pin,uint8_t *buffer,uint8_t buffer_size)
{
	if (buffer_size <24) {
		return 0;
	}
	irPin = pin;
	pinMode(irPin,INPUT);
	pinMode(3,OUTPUT);
	PORTD |= B00001000;
	irReceiveDataP0 = buffer;
	memset(irReceiveDataP0,0,buffer_size);
	hasPacket = 0;
	packetCounter =0;
	signalCounter = 0;
	bitCounter = 0;
	irPatternStateMachine = 0;
	wakePatternCounter = 0;
	packetLength = 3;
	irState = irLastState = digitalRead(irPin);
	irReceiveDataLen = 0;
	return 1;
}


uint8_t DYIRDaikinRecv::decode() {
}
/*
 * 0:none 2:paired
*/
uint8_t DYIRDaikinRecv::isSignalLowHighPaired()
{
	irStateBufIdx = irSignalState;
	irStateBuf[irStateBufIdx] = irSignalState;
	irStateDurationBuf[irStateBufIdx] = irSignalDuation;
	if (irSignalState == 1) {
		return 2;
	}
	return 0;
}

// 0:none,1:start,2:packet,3:stop,4 packet error,5:wake
uint8_t DYIRDaikinRecv::decodePerPacket() {

	if (irPatternStateMachine == 0) {
		//detect 3 packets pattern that first have 4 zero pattern,it is 3 packets protocol
		if (isZeroMatched(irStateDurationBuf[0],irStateDurationBuf[1]) == 1) {
			wakePatternCounter++;
			if ((wakePatternCounter > 3)) {
				if (hasWakupPattern == 0) {
					DYIRDAIKIN_DEBUG_PRINTLN("wake");
					hasWakupPattern = 1;
					PORTD &= B11110111;
				}else {
					PORTD |= B00001000;
				}
				return SIGNAL_PATTERN_WAKUP;
			}
		}
		if (hasWakupPattern == 1) {
			if (isStopMatched(irStateDurationBuf[0],irStateDurationBuf[1]) == 1) {
				return (SIGNAL_PATTERN_STOP + SIGNAL_PATTERN_WAKUP);
			}
		}
		//first detect start pattern, it is 2 packets protocol
		if (isStartMatched(irStateDurationBuf[0],irStateDurationBuf[1]) == 1) {
			DYIRDAIKIN_DEBUG_PRINTLN("SB");
			wakePatternCounter = 0;
			irPatternStateMachine = 1;
			packetLength = (hasWakupPattern == 1 ? 3: 2);
			receiveBufferBitPtr = 0;
			receiveBufferIndex = 0;
			return SIGNAL_PATTERN_START;
		}
		return 0;
	}

	if (irPatternStateMachine == 1) {
		if (isStopMatched(irStateDurationBuf[0],irStateDurationBuf[1]) == 1) {
			irPatternStateMachine = 0;
			packetCounter++;
			return SIGNAL_PATTERN_STOP;
		}
		if (isZeroMatched(irStateDurationBuf[0],irStateDurationBuf[1]) == 1) {
			fillBitToByte(receiveBuffer, 0,receiveBufferBitPtr, receiveBufferIndex);
		}else if (isOneMatched(irStateDurationBuf[0],irStateDurationBuf[1]) == 1) {
			fillBitToByte(receiveBuffer, 1,receiveBufferBitPtr, receiveBufferIndex);
		}else {
			fillBitToByte(receiveBuffer, 1,receiveBufferBitPtr, receiveBufferIndex);//return 4;
		}
		return SIGNAL_PATTERN_PACKET;
	}
	return 0;
}

uint8_t DYIRDaikinRecv::dumpPackets() {

  for(;;) {
	irState = digitalRead(irPin);
	if (irState == 1) {
		return 0;
	}else {
		break;
	}
  }
  //start sniffer
  signalCounter = 0;
  bitCounter = 0;
  duationCounter = 0;
  //
  signalTimeoutCounter = 0;
  packetTimeoutCounter = 0;
  //
  irLastState = irState = 0;

  //searching ir data
  uint8_t result = 0;
  while (1) {
    irState = digitalRead(irPin);
    if (irState != irLastState) {
		irSignalState = irLastState;
		irSignalDuation = duationCounter;
		signalCounter++;
		if (isSignalLowHighPaired() == SIGNAL_PAIRED) {
			bitCounter++;
			signalTimeoutCounter = 0;
			result = decodePerPacket();
			if (result == SIGNAL_PATTERN_PACKET_ERROR) {
				//packet is fail ,restart
				irPatternStateMachine = 0;
				DYIRDAIKIN_DEBUG_PRINTLN("packet detect is error,restart");
				return 0;
			}
			if (result == SIGNAL_PATTERN_WAKUP) {

			}
			if (result == SIGNAL_PATTERN_START) {
				signalCounter = 0;
				bitCounter = 0;
			}
			//
			//if ((signalCounter == 2) && (result == 1)) {
				//DYIRDAIKIN_DEBUG_PRINT(irStateBuf[0],DEC);
				//DYIRDAIKIN_DEBUG_PRINT(",");
				//DYIRDAIKIN_DEBUG_PRINTLN(irStateDurationBuf[0],DEC);
				//DYIRDAIKIN_DEBUG_PRINT(irStateBuf[1],DEC);
				//DYIRDAIKIN_DEBUG_PRINT(",");
				//DYIRDAIKIN_DEBUG_PRINTLN(irStateDurationBuf[1],DEC);
			//}
		}
		//clear counterS
		duationCounter = 0;
		signalTimeoutCounter = 0;
		packetTimeoutCounter= 0;
    }

    if (signalTimeoutCounter > SIGNAL_TIMEOUT__COUNT) {
		if (irLastState == 1) {
			irSignalState = irLastState;
			irSignalDuation = duationCounter;
			signalCounter++;
			if (isSignalLowHighPaired() == SIGNAL_PAIRED) {
				bitCounter++;
				result = decodePerPacket();
				if (result == SIGNAL_PATTERN_STOP) {
				#ifdef DEBUG_IR_PRINT
					DYIRDAIKIN_DEBUG_PRINTLN("=Decoded=");
					DYIRDAIKIN_DEBUG_PRINT("=wave counter:");
					DYIRDAIKIN_DEBUG_PRINTLN(signalCounter,DEC);
					DYIRDAIKIN_DEBUG_PRINT("=bit counter:");
					DYIRDAIKIN_DEBUG_PRINTLN(bitCounter,DEC);
					DYIRDAIKIN_DEBUG_PRINT("=byte counter:");
					irReceiveDataLen = receiveBufferIndex;
					DYIRDAIKIN_DEBUG_PRINTLN(irReceiveDataLen,DEC);
					DYIRDAIKIN_DEBUG_PRINTLN("--");
					for (int idx = 0;idx < receiveBufferIndex;idx++) {
						DYIRDAIKIN_DEBUG_PRINT(receiveBuffer[idx],HEX);
					}
				#endif
					hasWakupPattern = 0;
					return 1;

				}else if (result == (SIGNAL_PATTERN_STOP + SIGNAL_PATTERN_WAKUP)) {
					DYIRDAIKIN_DEBUG_PRINTLN("=WAKEUP+STOP=1=");
					return 0;
				}else {
					DYIRDAIKIN_DEBUG_PRINTLN("=Timeout=2=");
					return 0;
				}
			}else if (result == SIGNAL_PATTERN_WAKUP) {
				DYIRDAIKIN_DEBUG_PRINTLN("=WAKEUP=RETURN=2=");
				return 0;
			}else {
			#ifdef DEBUG_IR_PRINT
				DYIRDAIKIN_DEBUG_PRINTLN("=Timeout=1=");
			#endif
				hasWakupPattern = 0;
				return 0;
			}
			duationCounter = 0;
			signalCounter = 0;
		}
		signalTimeoutCounter = 0;
	}//signal timeout
	if (packetTimeoutCounter > PACKET_TIMEOUT__COUNT) {
		#ifdef DEBUG_IR_PRINT
			DYIRDAIKIN_DEBUG_PRINTLN("=Packet=Timeout=0=");
		#endif
		hasWakupPattern = 0;
		return 0;
	}

    irLastState = irState;
	_delay_us(SAMPLE_DELAY_TIME);
	signalTimeoutCounter++;
	packetTimeoutCounter++;
    duationCounter++;
  //------------------------------------------------------

  }//while
    return 0;
}

uint8_t DYIRDaikinRecv::checkSum(uint8_t *buffer,uint8_t len)
{
	uint8_t sum = 0;
	for (uint8_t i =0;i< (len - 1);i++) {
		sum = (uint8_t)(sum + buffer[i]);
	}
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT(sum,HEX);
	//~ DYIRDAIKIN_DEBUG_PRINT("-");
	//~ DYIRDAIKIN_DEBUG_PRINT(buffer[len],HEX);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	if (buffer[len] == sum) {
		return 1;
	}
	return 0;
}

void DYIRDaikinRecv::fillBitToByte(uint8_t *buffer, uint8_t value, int bitPtr, int bufferIndex) {
	if (bitPtr>7) {
		bufferIndex++;
		bitPtr = 0;
	}else {
		buffer[bufferIndex] = buffer[bufferIndex] >> 1;
	}
	if (value == 1) {
		buffer[bufferIndex] = buffer[bufferIndex] | B10000000;
	}
	if (value == 0) {
		buffer[bufferIndex] = buffer[bufferIndex] & B01111111;
	}
	bitPtr++;
}
//
void DYIRDaikinRecv::printARCState(uint8_t *recvData) {
	//~ static byte vFanTable[] = { 0x30,0x40,0x50,0x60,0x70,0xa0,0xb0};
	uint8_t temperature= (recvData[6] & B01111110) >> 1;
	uint8_t fan = (recvData[8] & 0xf0);
	if (fan == 0x30) fan = 0;
	if (fan == 0x40) fan = 1;
	if (fan == 0x50) fan = 2;
	if (fan == 0x60) fan = 3;
	if (fan == 0x70) fan = 4;
	if (fan == 0xa0) fan = 5;
	if (fan == 0xb0) fan = 6;

	uint8_t swing = (recvData[8] & 0x0f) >> 4;
	uint8_t powerState =  (recvData[5] & 0x01);
	uint8_t timerOn =  (recvData[5] & 0x02) >> 1;
	uint16_t timerOnValue = (uint16_t)recvData[10]|(uint16_t)(recvData[11] & B00000111)<<8;
	//~ AAAAAAAA AAAXBBBB BBBBBBBX
	uint8_t timerOff =  (recvData[5] & 0x04) >> 2;
	uint16_t timerOffValue = (uint16_t)((recvData[11] & B11110000) >> 4)|(uint16_t)(recvData[12] & B01111111)<<4;
	uint8_t mode = (recvData[5] & B01110000) >> 4;
	//
	uint16_t timeNow = 0;
	if (packetLength == 3) {
		timeNow = (uint16_t)recvData[5]|(uint16_t)(recvData[6] & B00000111)<<8;
	}

	//~ static byte vModeTable[] = { 0x6,0x3,0x2};
	if (mode == 0x6) mode = 0;
	if (mode == 0x3) mode = 1;
	if (mode == 0x2) mode = 2;

	uint8_t econo = (recvData[16] & B00000100) >> 2;

	DYIRDAIKIN_DEBUG_PRINT("\r\n===\r\n");
	DYIRDAIKIN_DEBUG_PRINT("Power:");
	DYIRDAIKIN_DEBUG_PRINT(powerState,DEC);
	DYIRDAIKIN_DEBUG_PRINTLN();
	DYIRDAIKIN_DEBUG_PRINT("Mode:");
	DYIRDAIKIN_DEBUG_PRINT(mode,DEC);
	DYIRDAIKIN_DEBUG_PRINTLN();
	DYIRDAIKIN_DEBUG_PRINT("Fan:");
	DYIRDAIKIN_DEBUG_PRINT(fan,DEC);
	DYIRDAIKIN_DEBUG_PRINTLN();
	DYIRDAIKIN_DEBUG_PRINT("Temperature:");
	DYIRDAIKIN_DEBUG_PRINT(temperature,DEC);
	DYIRDAIKIN_DEBUG_PRINTLN();
	DYIRDAIKIN_DEBUG_PRINT("Swing:");
	DYIRDAIKIN_DEBUG_PRINT(swing,DEC);
	DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT("Econo:");
	//~ DYIRDAIKIN_DEBUG_PRINT(econo,DEC);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT("Timer On:");
	//~ DYIRDAIKIN_DEBUG_PRINT(timerOn,DEC);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT("Timer On Value:");
	//~ DYIRDAIKIN_DEBUG_PRINT((timerOnValue / 60),DEC);
	//~ DYIRDAIKIN_DEBUG_PRINT(":");
	//~ DYIRDAIKIN_DEBUG_PRINT((timerOnValue % 60),DEC);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT("Timer Off:");
	//~ DYIRDAIKIN_DEBUG_PRINT(timerOff,DEC);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT("Timer Off Value:");
	//~ DYIRDAIKIN_DEBUG_PRINT((timerOffValue / 60),DEC);
	//~ DYIRDAIKIN_DEBUG_PRINT(":");
	//~ DYIRDAIKIN_DEBUG_PRINT((timerOffValue % 60),DEC);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
	//~ DYIRDAIKIN_DEBUG_PRINT("Timer Now:");
	//~ DYIRDAIKIN_DEBUG_PRINT((timeNow / 60),DEC);
	//~ DYIRDAIKIN_DEBUG_PRINT(":");
	//~ DYIRDAIKIN_DEBUG_PRINT((timeNow % 60),DEC);
	//~ DYIRDAIKIN_DEBUG_PRINTLN();
}

uint8_t DYIRDaikinRecv::isOneMatched(uint16_t lowTimeCounter,uint16_t highTimecounter)
{
	if ((lowTimeCounter > 15 && lowTimeCounter < 56) && (highTimecounter >= (lowTimeCounter + lowTimeCounter)  && highTimecounter < 150)) {
		return 1;
	}
	return 0;
}

uint8_t DYIRDaikinRecv::isZeroMatched(uint16_t lowTimeCounter,uint16_t highTimecounter)
{

	if ((lowTimeCounter > 20 && lowTimeCounter < 60) && (highTimecounter > 15 && highTimecounter < 40)) {
		return 1;
	}
	return 0;
}

uint8_t DYIRDaikinRecv::isStartMatched(uint16_t lowTimeCounter,uint16_t highTimecounter)
{
	if ((lowTimeCounter > 50 && lowTimeCounter < 400) && (highTimecounter > 70  && highTimecounter < 250)) {
		return 1;
	}
	return 0;
}

uint8_t DYIRDaikinRecv::isStopMatched(uint16_t lowTimeCounter,uint16_t highTimecounter)
{
	if ((lowTimeCounter > 20 && lowTimeCounter < 500) && (highTimecounter > 200)) {
		return 1;
	}
	return 0;
}
