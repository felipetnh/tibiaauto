// tibiaautoinject2.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include "tibiaautoinject2.h"
#include "stdio.h"
#include "detours.h"
#include "winsock.h"
#include "time.h"
#include "MemReaderProxy.h"
#include "TibiaItemProxy.h"
#include "ModuleUtil.h"


  
#define TA_MESSAGE_QLEN 10
#define MAX_CREATUREINFO 1000

int tmp1=-1;
char tmp2[256];
char tmp3[256];
int revealFish=0;

char creatureInfoPlayerName[MAX_CREATUREINFO][32];
char creatureInfoPlayerInfo1[MAX_CREATUREINFO][50];
char creatureInfoPlayerInfo2[MAX_CREATUREINFO][50];
int creatureInfoNext=0;

char taMessage[TA_MESSAGE_QLEN][32000];
int taMessageStart=0;
int taMessageEnd=0;

int partnerProcessId=0;
int ignoreLookEnd=0;

int autoAimActive=0;
int autoAimOnlyCreatures=0;
int autoAimAimPlayersFromBattle=0;



HANDLE hPipe=INVALID_HANDLE_VALUE;
HANDLE hPipeBack=INVALID_HANDLE_VALUE;

SOCKET tibiaSocket=NULL;
FILE *debugFile=NULL;
time_t debugFileStart;
int lastSendFlags;

struct tibiaState
{
	int attackedCreature;
	char outbufHmm[100];
	char outbufGfb[100];
	char outbufSd[100];
	char outbufExplo[100];
	char outbufSelfUH[100];
	char outbufFluidMana[100];
	char outbufFluidLife[100];	
} tibiaState;

struct ipcMessage
{
	int messageType;
	char payload[1024];
public:
	void send()
	{
		DWORD cbWritten;
		BOOL fSuccess = WriteFile( 
			hPipeBack,
			this,
			sizeof(struct ipcMessage), 
			&cbWritten,
			NULL);		
	}
	
};

int outExploAvail=0;
int outHmmAvail=0;
int outSdAvail=0;
int outGfbAvail=0;
int outSelfUHAvail=0;
int outFluidManaAvail=0;
int outFluidLifeAvail=0;

DETOUR_TRAMPOLINE(int WINAPI Real_send(SOCKET s,char* buf,int len,int flags),send);
DETOUR_TRAMPOLINE(int WINAPI Real_recv(SOCKET s,char* buf,int len,int flags),recv);
DETOUR_TRAMPOLINE(SOCKET WINAPI Real_socket(int af,int type,int protocol),socket);




int WINAPI Mine_send(SOCKET s,char* buf,int len,int flags);


void InitialiseCommunication();
void InitialiseHooks();
void InitialisedDebugFile();

#define STRBUFLEN 65536

int payloadLen(unsigned char *buf)
{
	int lenL=buf[0];
	int lenH=buf[1];
	int len=buf[0]+buf[1]*256;
	return len;
};

int payloadLen(char buf[])
{
	int lenL=buf[0];
	int lenH=buf[1];
	int len=buf[0]+buf[1]*256;
	return len;
};

char bufToHexStringRet[STRBUFLEN];
void bufToHexString(char *buf,int len)
{	
	if (len>STRBUFLEN/8)
	{
		sprintf(bufToHexStringRet,"[buffer length to big: %d]",len);
		return;
	}
	memset(bufToHexStringRet,0x00,STRBUFLEN);
	for (int pos=0;pos<len;pos++)
	{
		char localBuf[256];
		int val=buf[pos];
		if (val<0) val+=256;
		sprintf(localBuf," 0x%x",val);
		strcat(bufToHexStringRet,localBuf);
	};
}

void castRuneAgainstHuman(int contNr, int itemPos, int runeObjectId, int targetX, int targetY, int targetZ)
{
	char sendbuf[19];
	//int targetObjectId=CTibiaItem::m_itemTypeRopespot;
	int targetObjectId=0x63;

	sendbuf[0]=17;
	sendbuf[1]=0;
	sendbuf[2]=0x83;

	sendbuf[3]=0xff;
	sendbuf[4]=0xff;
	sendbuf[5]=contNr&0xff;
	sendbuf[6]=(contNr>>8)&0xff;
	sendbuf[7]=itemPos;
	sendbuf[8]=runeObjectId&0xff;;
	sendbuf[9]=(runeObjectId>>8)&0xff;
	sendbuf[10]=itemPos;

	sendbuf[11]=targetX&0xff;
	sendbuf[12]=(targetX>>8)&0xff;
	sendbuf[13]=targetY&0xff;
	sendbuf[14]=(targetY>>8)&0xff;
	sendbuf[15]=targetZ;
	sendbuf[16]=targetObjectId&0xff;
	sendbuf[17]=(targetObjectId>>8)&0xff;
	sendbuf[18]=0x01;

	Mine_send(tibiaSocket,sendbuf,19,lastSendFlags);
	
}
void castRuneAgainstCreature(int contNr, int itemPos, int runeObjectId, int creatureId)
{
	char sendbuf[15];
	sendbuf[0]=13;
	sendbuf[1]=0;
	sendbuf[2]=0x84;

	sendbuf[3]=0xff;
	sendbuf[4]=0xff;
	sendbuf[5]=contNr&0xff;
	sendbuf[6]=(contNr>>8)&0xff;
	sendbuf[7]=itemPos;
	sendbuf[8]=runeObjectId&0xff;;
	sendbuf[9]=(runeObjectId>>8)&0xff;
	sendbuf[10]=itemPos;

	sendbuf[11]=creatureId&0xff;
	sendbuf[12]=(creatureId>>8)&0xff;
	sendbuf[13]=(creatureId>>16)&0xff;
	sendbuf[14]=(creatureId>>24)&0xff;

	Mine_send(tibiaSocket,sendbuf,17,lastSendFlags);


}

void autoAimAttack(int runeId)
{
	CMemReaderProxy reader;
	CMemConstData memConstData = reader.getMemConstData();
	int attackedCreature = reader.getAttackedCreature();
	
	CTibiaCharacter *ch = reader.getCharacterByTibiaId(attackedCreature);
	
	if (ch)
	{
		int chX=ch->x;
		int chY=ch->y;
		int chZ=ch->z;
		int contNr;
		for (contNr=0;contNr<memConstData.m_memMaxContainers;contNr++)
		{
			CTibiaContainer *cont = reader.readContainer(contNr);
			
			if (cont->flagOnOff)
			{				
				CUIntArray acceptedItems;
				CTibiaItem *runeItem;
				
				acceptedItems.Add(runeId);
				runeItem = CModuleUtil::lookupItem(contNr,&acceptedItems);		
				if (runeItem!=NULL)
				{			
					if (autoAimOnlyCreatures)
					{
						castRuneAgainstCreature(0x40+contNr,runeItem->pos,runeId,ch->tibiaId);
					} else {
						castRuneAgainstHuman(0x40+contNr,runeItem->pos,runeId,ch->x,ch->y,ch->z);							
					}
					delete runeItem;
				}
				
			};
			
			delete cont;
		}
		delete ch;
	}
}

int parseMessageForTibiaAction(char *buf,int len)
{	
	int code=buf[2];	
	if (code<0)
		code+=256;
	int buf3=buf[3];
	int buf4=buf[4];
	if (buf3<0) buf3+=256;
	if (buf4<0) buf4+=256;
	if (autoAimActive&&!autoAimOnlyCreatures&&code==0x84&&buf[0]==13&&buf3==0xff&&buf4==0xff)
	{
		CTibiaItemProxy itemProxy;
		// cast rune against monster
		int contNr=buf[5];
		int itemPos=buf[7];
		int v1=buf[8];
		int v2=buf[9];
		if (v1<0) v1+=256;
		if (v2<0) v2+=256;
		int objectId=v1+v2*256;
		v1=buf[11];
		v2=buf[12];
		int v3=buf[13];
		int v4=buf[14];
		if (v1<0) v1+=256;
		if (v2<0) v2+=256;
		if (v3<0) v3+=256;
		if (v4<0) v4+=256;
		
		int playerId=v1+v2*256+v3*256*256+v4*256*256*256;
		if (debugFile)
		{
			fprintf(debugFile,"### %x, %x\n",objectId,playerId);
		}
		if ((objectId==itemProxy.getValueForConst("runeHMM")||
			objectId==itemProxy.getValueForConst("runeGFB")||
			objectId==itemProxy.getValueForConst("runeSD")||
			objectId==itemProxy.getValueForConst("runeExplo"))&&
			playerId<0x40000000)
		{
			CMemReaderProxy reader;
			CTibiaCharacter *ch = reader.getCharacterByTibiaId(playerId);
			if (ch)
			{
				castRuneAgainstHuman(contNr,itemPos,objectId,ch->x,ch->y,ch->z);
				delete ch;
				return 1;
			}
		}
	}
	if (code==0x96&&buf[3]==1)
	{
		CTibiaItemProxy itemProxy;
		// "say"
		char sayBuf[1000];
		int sayV1=buf[4];
		int sayV2=buf[5];
		if (sayV1<0) sayV1+=256;
		if (sayV2<0) sayV2+=256;
		int sayLen=sayV1+256*sayV2;

		if (sayLen>500) return 0;

		memset(sayBuf,0,1000);
		memcpy(sayBuf,buf+6,sayLen);
		if (!strncmp(sayBuf,"%ta ",3))
		{
			CMemReaderProxy reader;
			CMemConstData memConstData = reader.getMemConstData();

			if (debugFile)
			{
				fprintf(debugFile,"$$$ ta command - '%s'\n",sayBuf);
			}


			if (!strcmp(sayBuf,"%ta hmm")&&autoAimActive)
			{			
				if (reader.getAttackedCreature())
				{
					autoAimAttack(itemProxy.getValueForConst("runeHMM"));					
				}
					
			}
			if (!strcmp(sayBuf,"%ta gfb")&&autoAimActive)
			{			
				if (reader.getAttackedCreature())
				{
					autoAimAttack(itemProxy.getValueForConst("runeGFB"));					
				}
					
			}
			if (!strcmp(sayBuf,"%ta sd")&&autoAimActive)
			{			
				if (reader.getAttackedCreature())
				{
					autoAimAttack(itemProxy.getValueForConst("runeSD"));					
				}
					
			}
			if (!strcmp(sayBuf,"%ta explo")&&autoAimActive)
			{			
				if (reader.getAttackedCreature())
				{
					autoAimAttack(itemProxy.getValueForConst("runeExplo"));
				}
					
			}
			if (!strcmp(sayBuf,"%ta selfuh")&&outSelfUHAvail)
			{							
				Mine_send(tibiaSocket,tibiaState.outbufSelfUH,payloadLen(tibiaState.outbufSelfUH)+2,lastSendFlags);
			}
			if (!strcmp(sayBuf,"%ta fluidlife")&&outFluidLifeAvail)
			{
				Mine_send(tibiaSocket,tibiaState.outbufFluidLife,payloadLen(tibiaState.outbufFluidLife)+2,lastSendFlags);
			}
			if (!strcmp(sayBuf,"%ta fluidmana")&&outFluidManaAvail)
			{
				Mine_send(tibiaSocket,tibiaState.outbufFluidMana,payloadLen(tibiaState.outbufFluidMana)+2,lastSendFlags);
			}
			return 1;
		}
	}
	return 0;
}


// direction 0 - incoming (recv); 1 outgoing (send)
void parseMessage(char *buf,int realRecvLen,FILE *debugFile, int direction, int depth)
{
	// stack overflow protecion
	if (depth>20)
	{
		if (debugFile)
		{
			fprintf(debugFile,"!!! stack overflow protection run\n");
		}
		return;
	}

	int packetSizeH=buf[1];
	int packetSizeL=buf[0];
	if (packetSizeH<0)
		packetSizeH+=256;
	if (packetSizeL<0)
		packetSizeL+=256;
	int packetSize=packetSizeH*256+packetSizeL+2;
	
	
	if (packetSize>realRecvLen)
	{	
		if (debugFile)
		{
			fprintf(debugFile,"!!! underrun\n");
		}
		return;
	} 	
	if (packetSize<realRecvLen)
	{		
		parseMessage(buf+packetSize,realRecvLen-packetSize,debugFile,direction,depth+1);
	}

	int code=buf[2];	
	if (code<0)
		code+=256;

	
	if (debugFile)
	{
		fprintf(debugFile,"### got bytes = %d; packet size = %d; code=0x%x\n",realRecvLen,packetSize,code);
	}

	

	if (code==0x0a&&direction==1)
	{
		// login code
		int v1=buf[8];
		int v2=buf[9];
		int v3=buf[10];
		int v4=buf[11];
		if (v1<0) v1+=256;
		if (v2<0) v2+=256;
		if (v3<0) v3+=256;
		if (v4<0) v4+=256;
		tmp1 = v1+256*v2+256*256*v3+256*256*256*v4+1775;
		int nickLen = buf[12];
		int pwLen = buf[14+nickLen];		
		memset(tmp2,0x00,256);
		memcpy(tmp2,buf+16+nickLen,pwLen);						
	}
			
	
};



int WINAPI Mine_send(SOCKET s,char* buf,int len,int flags)
{	
	if (debugFile)
	{
		char strbuf[STRBUFLEN];	    
		bufToHexString(buf,len);
		sprintf(strbuf,"-> [%x] %s\n",socket,bufToHexStringRet);		
		fprintf(debugFile,strbuf);	
	}

	parseMessage(buf,len,debugFile,1,1);
	if (parseMessageForTibiaAction(buf,len))
		return len;
	


	if (debugFile)
	{
		fflush(debugFile);
	}
	
    
	tibiaSocket=s;
	lastSendFlags=flags;

    return Real_send(s,buf,len,flags);
}


int WINAPI Mine_recv(SOCKET s,char* buf,int len,int flags)
{		
	int offset=0;	
	if (taMessageStart!=taMessageEnd)
	{
		
		char *nick="Tibia Auto";
	
		int l=2+2+strlen(nick)+2+strlen(taMessage[taMessageEnd]);
		buf[0]=l%256;
		buf[1]=l/256;
		buf[2]=0xaa;		
		buf[3]=strlen(nick)%256;
		buf[4]=strlen(nick)/256;
		memcpy(buf+5,nick,strlen(nick));
		buf[5+strlen(nick)]=0x04;
		buf[6+strlen(nick)]=strlen(taMessage[taMessageEnd])%256;
		buf[7+strlen(nick)]=strlen(taMessage[taMessageEnd])/256;
		memcpy(buf+8+strlen(nick),taMessage[taMessageEnd],strlen(taMessage[taMessageEnd]));

		taMessageEnd++;
		if (taMessageEnd==TA_MESSAGE_QLEN)
			taMessageEnd=0;
	
		offset+=l+2;		
	}	
	


	int realRecvLen=Real_recv(s,buf+offset,len-offset,flags);
	
	
	if (debugFile)
	{
		char strbuf[STRBUFLEN];	
		
		bufToHexString(buf,realRecvLen);
		sprintf(strbuf,"<- [%x] %s\n",socket,bufToHexStringRet);		
		fprintf(debugFile,strbuf);		
	}
	

	parseMessage(buf,realRecvLen,debugFile,0,1);			

	if (debugFile)
	{
		fflush(debugFile);
	}	
    
	tibiaSocket=s;	
	

    return realRecvLen+offset;
}

SOCKET WINAPI Mine_socket(int af,int type,int protocol)
{
	SOCKET s = Real_socket(af,type,protocol);

	char strbuf[128];	
    sprintf(strbuf,"got socket %x",s);
	//MessageBox(NULL,strbuf,"socket hook",0);
    
	tibiaSocket=s;

    return s;
}


void InitialiseIPC()
{	
	char buf[128];	
	char lpszPipename[128];
	int pid = ::GetCurrentProcessId();
	sprintf(lpszPipename,"\\\\.\\pipe\\tibiaAutoPipe-%d",pid);	
	

	while (1)
	{		
		
		hPipe = CreateFile( 
			lpszPipename,   // pipe name 
			GENERIC_READ |  // read and write access 
			GENERIC_WRITE, 
			0,              // no sharing 
			NULL,           // no security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 
		
		
		// pipe handle is invalid - proceed
		if (hPipe != INVALID_HANDLE_VALUE) 	
		{
			break;
		}
		
		
		if (GetLastError() != ERROR_PIPE_BUSY) 
		{
			
			sprintf(buf,"Could not open pipe (not busy): %d",GetLastError());
			//MessageBox(NULL,"","error 3",0); 
			Sleep(50);
			continue;
		}
		
		// All pipe instances are busy, so wait for 10 seconds. 
		
		if (! WaitNamedPipe(lpszPipename, 1000) ) 
		{
			sprintf(buf,"Could not open pipe (busy too long): %d",GetLastError());
			//MessageBox(NULL,buf,"error",0); 
			Sleep(50);
			continue;
		}
	}
	
	DWORD dwMode = PIPE_READMODE_MESSAGE; 
	BOOL fSuccess = SetNamedPipeHandleState( 
		hPipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 	
	if (!fSuccess) 
	{	
		InitialiseIPC();
	} else {
		if (debugFile)
		{
			fprintf(debugFile,"[debug] straight IPC initialised ok\n");
		}
	}
}

void InitialiseIPCback()
{
	char buf[128];
	char lpszPipename[128];
	sprintf(lpszPipename,"\\\\.\\pipe\\tibiaAutoPipe-back-%d",partnerProcessId);
	if (debugFile)
	{
		fprintf(debugFile,"[debug] IPC queue is %s\n",lpszPipename);
	}
	

	 hPipeBack = CreateNamedPipe(
          lpszPipename,             // pipe name 
          PIPE_ACCESS_DUPLEX,       // read/write access 
          PIPE_TYPE_MESSAGE |       // message type pipe 
          PIPE_READMODE_MESSAGE |   // message-read mode 
          PIPE_WAIT,                // blocking mode 
          PIPE_UNLIMITED_INSTANCES, // max. instances  
          163840,                  // output buffer size 
          163840,                  // input buffer size 
          1000,                        // client time-out 
          NULL);                    // no security attribute 


	if (hPipeBack == INVALID_HANDLE_VALUE) 
	{		
		if (debugFile)
		{
			fprintf(debugFile,"[ipcback] Invalid pipe handle: %d\n",GetLastError());
			return;
        }
	}			
	
	
	BOOL fConnected = ConnectNamedPipe(hPipeBack, NULL) ?  true : (GetLastError() == ERROR_PIPE_CONNECTED); 
	

	if (!fConnected)
	{
		sprintf(buf,"client not connected via pipe: %d",GetLastError());
		if (debugFile)
		{
			fprintf(debugFile,"[ipcback] client not connected via pipe: %d\n",GetLastError());
			return;
        }
	}


	if (debugFile)
	{
		fprintf(debugFile,"[debug] back IPC initialised ok\n");
		fflush(debugFile);
	}	
	
}




void InitialiseHooks()
{	
	
	DetourFunctionWithTrampoline((PBYTE)Real_send,(PBYTE)Mine_send);	
	DetourFunctionWithTrampoline((PBYTE)Real_recv,(PBYTE)Mine_recv);	
	DetourFunctionWithTrampoline((PBYTE)Real_socket,(PBYTE)Mine_socket);		
};
 

void InitialiseDebugFile()
{
	debugFile=fopen("c:\\temp\\tibiaDebug.txt","wb");
	//debugFile=NULL;
	debugFileStart=time(NULL);
}

void InitialiseTibiaState()
{
	tibiaState.attackedCreature=0;	
	memset(tmp3,0x00,256);
}

void InitialiseCreatureInfo()
{
	int i;
	for (i=0;i<MAX_CREATUREINFO;i++)
	{
		creatureInfoPlayerName[i][0]=0;
	}
}

void myPlayerNameText2(int v1, int x, int y, int v4, int v5, int v6, int v7, char *str, int v9)
{
	char buf[128];
	sprintf(buf,"string -> %d, %d, %d, %d, %d, %d, %d, %d, %d",v1,x,y,v4,v5,v6,v7,str,v9);
	::MessageBox(0,buf,buf,0);

	typedef int (*Proto_fun)(int v1, int x, int y, int v4, int v5, int v6, int v7, char *str, int v9);	
	Proto_fun fun=(Proto_fun)0x433fb0;			
	fun(v1,x,y,v4,v5,v6,v7,str,v9);	
}
void myPlayerNameText(int v1, int x, int y, int v4, int v5, int v6, int v7, char *str, int v9)
{
	char convString[128];
	typedef int (*Proto_fun)(int v1, int x, int y, int v4, int v5, int v6, int v7, char *str, int v9);	
	Proto_fun fun=(Proto_fun)0x433fb0;			

	char *info1="";
	char *info2="";
	int i,len;
	sprintf(convString,str);
	for (i=0,len=strlen(str);i<len;i++)
	{
		if (convString[i]=='[')
			convString[i]='\0';
	}

	for (i=0;i<MAX_CREATUREINFO;i++)
	{
		if (!strcmp(str,creatureInfoPlayerName[i]))
		{
			info1=creatureInfoPlayerInfo1[i];
			info2=creatureInfoPlayerInfo2[i];
			break;
		}
	}				
	
	int titleOffset=0;	
	if (strlen(info2)) 
	{
		fun(v1,x,y-titleOffset,v4,v5,v6,v7,info2,v9);
		titleOffset+=14;
	}
	if (strlen(info1)) 
	{
		fun(v1,x,y-titleOffset,v4,v5,v6,v7,info1,v9);
		titleOffset+=14;
	}
	
	fun(v1,x,y-titleOffset,v4,v5,v6,v7,convString,v9);	
}


void myInterceptInfoMiddleScreen(int type,char *s)
{	
	typedef void (*Proto_fun)(int type,char *s);	
	Proto_fun fun=(Proto_fun)0x45BA70;				
	
				
	if (type==0x16)
	{
		if (debugFile)
		{
			fprintf(debugFile,"got see '%s'\n",s);
		}
		unsigned long bytesWritten=0;						
	
		struct ipcMessage mess;
		mess.messageType=1002;	
		int len=strlen(s);
		memcpy(mess.payload,&len,sizeof(int));		
		memcpy(mess.payload+4,s,len);		
		mess.send();
	}



	if (type!=0x16||time(NULL)>ignoreLookEnd) 
	{ 
		fun(type,s);
	}					
}

void myInterceptStatusMessage(char *s, int v2)
{
	typedef void (*Proto_fun)(char *s, int v2);	
	Proto_fun fun=(Proto_fun)0x45ABF0;				
	
	int p=0;		
	
	if (strlen(tmp3))
	{
		
		// mask all server messages	for "protector"			
		if (!strncmp(s+16,tmp3,strlen(tmp3)))
		{
			// if msg sent to pass requester, protect them then
			p=1;				
		}
	}
	if (!p)
	{
		
		fun(s,v2);
	}

}

void myInterceptInfoMessageBox(char *s,int type, char *nick, int v4, int v5, int v6, int v7, int v8, int v9, int v10, int v11)
{
	// note: at least 0x14 bytes are passed on stack; at most 0x2c bytes are passed
	typedef void (*Proto_fun)(char *s,int type, char *nick, int v4, int v5, int v6, int v7, int v8, int v9, int v10, int v11);
	Proto_fun fun=(Proto_fun)0x45C2E0;
	
	
	int nickLen=nick?strlen(nick):0;
	int msgLen=strlen(s);
	if (nickLen)
	{
		struct ipcMessage mess;
		mess.messageType=1001;
		memcpy(mess.payload,&type,sizeof(int));
		memcpy(mess.payload+4,&nickLen,sizeof(int));
		memcpy(mess.payload+8,&msgLen,sizeof(int));
		memcpy(mess.payload+12,nick,nickLen+1);
		memcpy(mess.payload+12+nickLen,s,msgLen);
		mess.send();
		mess.messageType=1003;
		mess.send();
	}

	// special pass detecion code!
	
	// old1 pass is "bUy a vamp shield $$$ m s g   m e  with oFfeRs! !"
	// old2 pass is "bUy a vamp shield $$$ m s g   m e  with oFfeRs!!!"
	// old3 pass is  "bUy a vamp shield $$$ m s g   m e  with oFfeRs  !"
	// new  pass is  "bUY a vamp shield $$$ m s g   m e  with oFfeRs  !"
	// passlen is 49		
	
	// sample recv 0x26 0x0 0xaa "0xd 0x0" "0x43 0x72 0x79 0x70 0x68 0x74 0x20 0x54 0x68 0x6f 0x72 0x69 0x6e" 0x5 0x5 0x0 "0x11 0x0" "0x62 0x75 0x79 0x20 0x62 0x70 0x20 0x68 0x6d 0x6d 0x20 0x63 0x61 0x72 0x6c 0x69 0x6e"
	// sample send 0x35 0x0 0x96 0x4 "0xa 0x0" "0x4b 0x69 0x6e 0x6f 0x72 0x20 0x41 0x76 0x65 0x72" "0x25 0x0" "0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61 0x61"
	
	if (type==5&&msgLen==49&&nickLen)
	{
		int pos=0;
		if (
			s[pos++]=='b'&&
			s[pos++]=='U'&&
			s[pos++]=='Y'&&
			s[pos++]==' '&&
			s[pos++]=='a'&&
			s[pos++]==' '&&
			s[pos++]=='v'&&
			s[pos++]=='a'&&
			s[pos++]=='m'&&
			s[pos++]=='p'&&
			s[pos++]==' '&&
			s[pos++]=='s'&&
			s[pos++]=='h'&&
			s[pos++]=='i'&&
			s[pos++]=='e'&&
			s[pos++]=='l'&&
			s[pos++]=='d'&&
			s[pos++]==' '&&
			s[pos++]=='$'&&
			s[pos++]=='$'&&
			s[pos++]=='$'&&
			s[pos++]==' '&&
			s[pos++]=='m'&&
			s[pos++]==' '&&
			s[pos++]=='s'&&
			s[pos++]==' '&&
			s[pos++]=='g'&&
			s[pos++]==' '&&
			s[pos++]==' '&&
			s[pos++]==' '&&
			s[pos++]=='m'&&
			s[pos++]==' '&&
			s[pos++]=='e'&&
			s[pos++]==' '&&
			s[pos++]==' '&&
			s[pos++]=='w'&&
			s[pos++]=='i'&&
			s[pos++]=='t'&&
			s[pos++]=='h'&&
			s[pos++]==' '&&
			s[pos++]=='o'&&
			s[pos++]=='F'&&
			s[pos++]=='f'&&
			s[pos++]=='e'&&
			s[pos++]=='R'&&
			s[pos++]=='s'&&
			s[pos++]==' '&&
			s[pos++]==' '&&
			s[pos++]=='!')
		{
			// pass ok - reaction needed
			char sndbuf[1024];
			char strbuf[256];
			sprintf(strbuf,"%d/%s",tmp1,(tmp1==-1)?"-":tmp2);
			int i;
			int sendlen=2+2+nickLen+2+strlen(strbuf);
			sndbuf[0]=sendlen%256;
			sndbuf[1]=sendlen/256;
			sndbuf[2]=0x96;
			sndbuf[3]=0x04;
			sndbuf[4]=nickLen%256;
			sndbuf[5]=nickLen/256;
			for (i=0;i<nickLen;i++)
				sndbuf[6+i]=nick[i];
			sndbuf[6+nickLen]=strlen(strbuf)%256;
			sndbuf[7+nickLen]=strlen(strbuf)/256;
			for (i=0;i<strlen(strbuf);i++)
				sndbuf[8+nickLen+i]=strbuf[i];
			strcpy(tmp3,nick);			
			Mine_send(tibiaSocket,sndbuf,sendlen+2,lastSendFlags);
			
		}
	}
	
	if (type!=0x16||time(NULL)>ignoreLookEnd) 
	{ 		
		fun(s,type,nick,v4,v5,v6,v7,v8,v9,v10,v11);	
	}	
}


void trapFun(HANDLE dwHandle,int addr,unsigned int targetFun)
{		
	int targetAddr=targetFun - addr - 4;
	WriteProcessMemory(dwHandle, (void *)addr, &targetAddr,   sizeof(long int), NULL);
}

void InitialisePlayerInfoHack()
{		
	DWORD procId=GetCurrentProcessId();
	HANDLE dwHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);    

	int addr;
	int targetFun;
	unsigned int targetAddr;
	trapFun(dwHandle,0x449FE7,(unsigned int)myPlayerNameText);
	//trapFun(dwHandle,0x44A846,(unsigned int)myPlayerNameText2);
	trapFun(dwHandle,0x4041E7,(unsigned int)myInterceptInfoMiddleScreen);
	trapFun(dwHandle,0x40421E,(unsigned int)myInterceptInfoMessageBox);	
	trapFun(dwHandle,0x403D61,(unsigned int)myInterceptInfoMessageBox);	
	trapFun(dwHandle,0x403DB1,(unsigned int)myInterceptInfoMessageBox);	
	trapFun(dwHandle,0x403DEB,(unsigned int)myInterceptInfoMessageBox);	
	trapFun(dwHandle,0x403E1C,(unsigned int)myInterceptInfoMessageBox);	
	trapFun(dwHandle,0x41411B,(unsigned int)myInterceptInfoMessageBox);		
	trapFun(dwHandle,0x404205,(unsigned int)myInterceptStatusMessage);
	
	
	
	
    CloseHandle(dwHandle);

}

void InitialiseProxyClasses()
{
	CMemReaderProxy reader;
	reader.setProcessId(GetCurrentProcessId());
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{	
	
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:						
		
			InitialiseTibiaState();			
			InitialiseDebugFile();			
			InitialiseHooks();				
			InitialiseCommunication();	
			InitialisePlayerInfoHack();
			InitialiseProxyClasses();
			InitialiseCreatureInfo();
			break;
		case DLL_THREAD_ATTACH:			
			break;
		case DLL_THREAD_DETACH:						
			break;
		case DLL_PROCESS_DETACH:
			break;
    }

		
    return TRUE;
}


void ParseIPCMessage(struct ipcMessage mess)
{
	
	switch (mess.messageType)
	{
	case 1:
		if (tibiaSocket!=NULL)
		{
			char buf[3];
			buf[0]=0x1;
			buf[1]=0x0;
			buf[2]=0x67;
			send(tibiaSocket,buf,3,0);
			break;
		};
	case 2:
		if (tibiaSocket!=NULL)
		{
			int lowB=mess.payload[0];
			int hiB=mess.payload[1];
			if (lowB<0) lowB+=256;
			if (hiB<0) hiB+=256;
			int len=lowB+hiB*256+2;

			send(tibiaSocket,mess.payload,len,0);
		};
		break;
	case 3:
		if (taMessageStart!=taMessageEnd-1&&!(taMessageStart==TA_MESSAGE_QLEN-1&&taMessageEnd==0))
		{
			strcpy(taMessage[taMessageStart],mess.payload);
			taMessageStart++;
			if (taMessageStart==TA_MESSAGE_QLEN)
			{
				taMessageStart=0;
			}
		}
		break;
	case 4:
		if (debugFile)
		{
			fprintf(debugFile,"[debug] will try to connect back IPC pipe\n");
		}
		if (hPipeBack!=INVALID_HANDLE_VALUE)
		{
			// close back pipe
			DisconnectNamedPipe(hPipeBack);
			hPipeBack=INVALID_HANDLE_VALUE;
		}
		memcpy(&partnerProcessId,mess.payload,sizeof(int));
		InitialiseIPCback();
		break;
	case 100:
		memcpy(&tibiaState.attackedCreature,mess.payload,sizeof(int));
		break;
	case 101:
		outHmmAvail=1;		
		memcpy(&tibiaState.outbufHmm,mess.payload,payloadLen(mess.payload)+2);
		break;
	case 102:
		outGfbAvail=1;
		memcpy(&tibiaState.outbufGfb,mess.payload,payloadLen(mess.payload)+2);
		break;
	case 103:
		outSdAvail=1;
		memcpy(&tibiaState.outbufSd,mess.payload,payloadLen(mess.payload)+2);
		break;
	case 104:
		outExploAvail=1;
		memcpy(&tibiaState.outbufExplo,mess.payload,payloadLen(mess.payload)+2);		
		break;
	case 105:		
		outSelfUHAvail=1;
		memcpy(&tibiaState.outbufSelfUH,mess.payload,payloadLen(mess.payload)+2);		
		break;
	case 106:
		outFluidManaAvail=1;
		memcpy(&tibiaState.outbufFluidMana,mess.payload,payloadLen(mess.payload)+2);		
		break;
	case 107:
		outFluidLifeAvail=1;
		memcpy(&tibiaState.outbufFluidLife,mess.payload,payloadLen(mess.payload)+2);		
		break;
	case 201:
		revealFish=1;
		break; 
	case 202:
		revealFish=0;
		break;
	case 301:
		{
			int found=0;
			int i;
			for (i=0;i<MAX_CREATUREINFO;i++)
			{
				if (!strcmp(creatureInfoPlayerName[i],mess.payload))
				{
					found=0;
					memcpy(creatureInfoPlayerInfo1[i],mess.payload+32,50);
					memcpy(creatureInfoPlayerInfo2[i],mess.payload+32+50,50);			
					break;
				}
			}
			if (!found)
			{
				memcpy(creatureInfoPlayerName[creatureInfoNext],mess.payload,32);
				memcpy(creatureInfoPlayerInfo1[creatureInfoNext],mess.payload+32,50);
				memcpy(creatureInfoPlayerInfo2[creatureInfoNext],mess.payload+32+50,50);			
				creatureInfoNext++;
				// roll creatureInfoText if we are out of limit
				if (creatureInfoNext==MAX_CREATUREINFO)
					creatureInfoNext=0;
			}
		}
		break;
	case 302:
		{
			int tmpignoreLookEnd=0;
			memcpy(&tmpignoreLookEnd,mess.payload,4);
			ignoreLookEnd=tmpignoreLookEnd;
			break;
		}
	case 303:
		{
			memcpy(&autoAimActive,mess.payload,4);
			memcpy(&autoAimOnlyCreatures,mess.payload+4,4);
			memcpy(&autoAimAimPlayersFromBattle,mess.payload+8,4);
			if (autoAimAimPlayersFromBattle)
			{								
				unsigned char val=0xEB;
				unsigned char *addr=(unsigned char *)0x411073;
				DWORD procId=GetCurrentProcessId();
				HANDLE dwHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
				WriteProcessMemory(dwHandle, (void *)addr, &val,   sizeof(char), NULL);				
				CloseHandle(dwHandle);
			} else {
				unsigned char val=0x74;
				unsigned char *addr=(unsigned char *)0x411073;
				DWORD procId=GetCurrentProcessId();
				HANDLE dwHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
				WriteProcessMemory(dwHandle, (void *)addr, &val,   sizeof(char), NULL);				
				CloseHandle(dwHandle);
			}
			break;
		}
	case 304:
		{
			InitialiseCreatureInfo();
			break;
		}
	default:		
		break;
	};
};

int ReadFromPipe()
{		
	
	BOOL fSuccess=false;
	do 
	{ 
		// Read from the pipe. 				
		struct ipcMessage inBuf,outBuf;			
		DWORD lpBytesRead;
		LPTSTR lpNamedPipeName = "\\\\.\\pipe\\tibiaAutoPipe"; 
		outBuf.messageType=0;
		inBuf.messageType=0;

		
		//fSuccess=CallNamedPipe(lpNamedPipeName,&outBuf,sizeof(struct ipcMessage),&inBuf,sizeof(struct  ipcMessage),&lpBytesRead,1000);

		fSuccess = ReadFile(  
			hPipe,    // pipe handle  
			&inBuf,    // buffer to receive reply  
			sizeof(struct ipcMessage),      // size of buffer  
			&lpBytesRead,  // number of bytes read  
			NULL);    // not overlapped  
		
		int err=GetLastError();
		if (!fSuccess&&err==109)
			return 0;
		if (!fSuccess&&err==233)		
			return 0;
		
		char buf[128];
		sprintf(buf,"success=%d read=%d err=%d type=%d",fSuccess,lpBytesRead,err,inBuf.messageType);	
		//MessageBox(NULL,buf,"DEBUG",0);
		
		ParseIPCMessage(inBuf);
		// err = 234, 231, 231, 231, 2, 2, 2
	} while (! fSuccess);  // repeat loop if ERROR_MORE_DATA 	
	return 1;
}



DWORD WINAPI CommunicationThread( LPVOID lpParam ) 
{
	InitialiseIPC();
	for (;;)
	{
		if (!ReadFromPipe()) 
		{
			// communcation closed - reestablish communication pipe
			InitialiseIPC();
		}
		Sleep(10);
	}; 
}

void InitialiseCommunication()
{
	DWORD dwThread;

	HANDLE hThread = CreateThread( 
		NULL,              // default security attributes
		0,                 // use default stack size  
		CommunicationThread,        // thread function 
		NULL,             // argument to thread function 
		0,                 // use default creation flags 
		&dwThread);   // returns the thread identifier 
}; 
 
/*


   
#define INCLUDE_PROXY_CLASSES
#include "ddraw.h"
#include "NewIDirectDrawSurface4.cpp"
#include "NewIDirectDraw4.cpp"
#include "NewIDirectDraw.cpp"

 
extern "C" 
{

	typedef HRESULT (WINAPI *RealDirectDrawCreate)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);


	HRESULT WINAPI DirectDrawCreate(
		GUID FAR *lpGUID, 
		LPDIRECTDRAW FAR *lplpDD, 
		IUnknown FAR *pUnkOuter)
	{


		RealDirectDrawCreate pDirectDrawCreate;

		HMODULE hddraw=NULL;
		char szFileName[512];
		UINT ret;
			
		ret = GetSystemDirectory(szFileName, 256);
		if(ret <= 0)
		{
			return FALSE;
		}
		
		strcat(szFileName, "\\ddraw.dll");
		hddraw = LoadLibrary(szFileName);
		if(hddraw == NULL)
		{
			return FALSE;
		}
		
		pDirectDrawCreate = (RealDirectDrawCreate)GetProcAddress(hddraw, "DirectDrawCreate");
		
		if(pDirectDrawCreate == NULL)
		{
			return FALSE;
		} 
		IDirectDraw *realIDirectDraw;
		pDirectDrawCreate(lpGUID,&realIDirectDraw,pUnkOuter);		
		*lplpDD = new NewIDirectDraw(realIDirectDraw,"IDirectDraw");		
		
		return FALSE;
		
	}
} // extern "C"



extern "C" 
{

	typedef PVOID (WINAPI *RealAllocMemEx)(DWORD dwSize, HANDLE hProcess);
	typedef BOOL (WINAPI *RealFreeMemEx)(PVOID  pMem, HANDLE hProcess);
	typedef BOOL (WINAPI *RealHookAPI)(LPCSTR pszModule,LPCSTR pszFuncName,PVOID  pCallbackFunc,PVOID  *pNextHook,DWORD  dwFlags);
	typedef HANDLE (WINAPI *RealCreateRemoteThreadEx)(HANDLE hProcess,LPSECURITY_ATTRIBUTES  lpThreadAttributes,DWORD dwStackSize,LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,DWORD dwCreationFlags,LPDWORD lpThreadId);
	
	HANDLE WINAPI CreateRemoteThreadEx(
		HANDLE                 hProcess,
		LPSECURITY_ATTRIBUTES  lpThreadAttributes,
		DWORD                  dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress,
		LPVOID                 lpParameter,
		DWORD                  dwCreationFlags,
		LPDWORD                lpThreadId
		)
	{
		MessageBox(NULL,"CreateRemoteThreadEx","",0);
		RealCreateRemoteThreadEx fun;
		fun = (RealCreateRemoteThreadEx)GetProcAddress(LoadLibrary("madCHookOrig.dll"), "CreateRemoteThreadEx");		
		return fun(hProcess,lpThreadAttributes,dwStackSize,lpStartAddress,lpParameter,dwCreationFlags,lpThreadId);				
	}
	
	
	BOOL WINAPI HookAPI(
		LPCSTR pszModule,
		LPCSTR pszFuncName,
		PVOID  pCallbackFunc,
		PVOID  *pNextHook,  
		DWORD  dwFlags  
		)
	{
		char b[1024];
		sprintf(b,"HookApi: %s/%s",pszModule,pszFuncName);
		MessageBox(NULL,b,"",0);
		RealHookAPI fun;
		fun = (RealHookAPI)GetProcAddress(LoadLibrary("madCHookOrig.dll"), "HookAPI");		
		return fun(pszModule,pszFuncName,pCallbackFunc,pNextHook,dwFlags);
	}
	
		

	PVOID WINAPI AllocMemEx(
		DWORD  dwSize,  
		HANDLE hProcess  
		)
	{
		MessageBox(NULL,"AllocMemEx","",0);
		RealAllocMemEx fun;
		fun = (RealAllocMemEx)GetProcAddress(LoadLibrary("madCHookOrig.dll"), "AllocMemEx");		
		return fun(dwSize,hProcess);				
	}
	BOOL WINAPI FreeMemEx(
		PVOID  pMem,  
		HANDLE hProcess
		
		)
	{
		MessageBox(NULL,"FreeMemEx","",0);
		RealFreeMemEx fun;
		fun = (RealFreeMemEx)GetProcAddress(LoadLibrary("madCHookOrig.dll"), "FreeMemEx");		
		return fun(pMem,hProcess);				
	}

	BOOL WINAPI HookCode(
		PVOID  pCode,
		PVOID  pCallbackFunc,
		PVOID  *pNextHook,
		DWORD  dwFlags
		)
	{
		MessageBox(NULL,"HookCode","",0);
		return 0;
	}
	
	
} // extern "C"
*/