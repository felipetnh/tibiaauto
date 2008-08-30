/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/


// mod_looter.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "mod_looter.h"

#include "ConfigDialog.h"
#include "ConfigData.h"
#include "TibiaContainer.h"

#include "MemReaderProxy.h"
#include "PackSenderProxy.h"
#include "ModuleUtil.h"
#include "TibiaItemProxy.h"
#include "SendStats.h"

#include "commons.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

/////////////////////////////////////////////////////////////////////////////
// CMod_looterApp

BEGIN_MESSAGE_MAP(CMod_looterApp, CWinApp)
	//{{AFX_MSG_MAP(CMod_looterApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Tool functions

int containerNotFull(int containerNr)
{
	int ret;
	CMemReaderProxy reader;
	CTibiaContainer *cont = reader.readContainer(containerNr);
	ret=(cont->size>cont->itemsInside&&cont->flagOnOff);
	delete cont;
	return ret;
}

/////////////////////////////////////////////////////////////////////////////
// Tool thread function

int toolThreadShouldStop=0;
HANDLE toolThreadHandle;

DWORD WINAPI toolThreadProc( LPVOID lpParam )
{		
	CMemReaderProxy reader;
	CTibiaItemProxy itemProxy;
	CPackSenderProxy sender;
	CConfigData *config = (CConfigData *)lpParam;
	int lastAttackedMonster = 0;

	while (!toolThreadShouldStop)
	{			
		Sleep(100);
		if (reader.getConnectionState()!=8) continue; // do not proceed if not connected

		/*** killed monster opening part ***/
		if (config->m_autoOpen&&lastAttackedMonster&&reader.getAttackedCreature()==0)
		{		
			//::MessageBox(NULL,"x 1","x",0);			
			CTibiaCharacter *self = reader.readSelfCharacter();
			CTibiaCharacter *attackedCh = reader.getCharacterByTibiaId(lastAttackedMonster);
			
			if (attackedCh)
			{
												
				if (!attackedCh->hpPercLeft&&abs(self->x-attackedCh->x)<=1&&abs(self->y-attackedCh->y)<=1&&self->z==attackedCh->z)
				{									
					//Sleep(1000);
					// the creature is dead and we can try to open its corpse					
					int corpseId = itemOnTopCode(attackedCh->x-self->x,attackedCh->y-self->y);
					
				
					// now close containers 7-9 which we use for auto-opening purposes
					sender.closeContainer(7);
					CModuleUtil::waitForOpenContainer(7,0);
					sender.closeContainer(8);
					CModuleUtil::waitForOpenContainer(8,0);
					sender.closeContainer(9);
					CModuleUtil::waitForOpenContainer(9,0);
					CTibiaContainer *cont7 = reader.readContainer(7);
					CTibiaContainer *cont8 = reader.readContainer(8);
					CTibiaContainer *cont9 = reader.readContainer(9);
					
					if (!cont7->flagOnOff&&!cont8->flagOnOff&&!cont9->flagOnOff)
					{
						// now open corpse and all its containers 
						// but only when 7-9 are closed (to avoid mixing corpses)

						delete self;
						self = reader.readSelfCharacter();
						delete attackedCh;
						attackedCh = reader.getCharacterByTibiaId(lastAttackedMonster);
						if (attackedCh)
						{
							CModuleUtil::waitForCreatureDisappear(attackedCh->x-self->x,attackedCh->y-self->y,attackedCh->tibiaId);
							int corpseId = itemOnTopCode(attackedCh->x-self->x,attackedCh->y-self->y);							
							sender.openContainerFromFloor(corpseId,attackedCh->x,attackedCh->y,attackedCh->z,itemOnTopIndex(attackedCh->x-self->x,attackedCh->y-self->y),9);
							CModuleUtil::waitForOpenContainer(9,1);
							
							delete cont9;
							cont9 = reader.readContainer(9);						
							if (corpseId&&cont9->flagOnOff)
							{
								
								int currentExtraContainerNr = 8;
								int itemNr;
								for (itemNr=0;itemNr<cont9->itemsInside&&currentExtraContainerNr>=7;itemNr++)
								{
									CTibiaItem *item = (CTibiaItem *)cont9->items.GetAt(itemNr);
									if (item->objectId==itemProxy.getValueForConst("bagbrown"))
									{
										if (currentExtraContainerNr==7)
										{
											sender.openContainerFromContainer(item->objectId,0x40+9,itemNr,currentExtraContainerNr);
											CModuleUtil::waitForOpenContainer(7,1);										
											delete cont8;
											cont8=reader.readContainer(7);										
											currentExtraContainerNr--;
										}
										if (currentExtraContainerNr==8)
										{
											sender.openContainerFromContainer(item->objectId,0x40+9,itemNr,currentExtraContainerNr);
											CModuleUtil::waitForOpenContainer(8,1);										
											delete cont8;
											cont8=reader.readContainer(8);		
											currentExtraContainerNr--;
										}
										
										
									} 
								} // for itemNr (open extra containers)
								
								if (corpseId&&cont9->flagOnOff)
								{
									FILE *lootStatsFile = NULL;
									// time,rand,creature,name,pos,objectId,count,bagopen,checksum
									int killNr=rand();
									lootStatsFile=fopen("tibiaauto-stats-loot.txt","a+");
									if (lootStatsFile)
									{
										int i,len;
										char statChName[128];
										for (i=0,strcpy(statChName,attackedCh->name),len=strlen(statChName);i<len;i++)
										{
											if (statChName[i]=='[')
												statChName[i]='\0';
										}
										int tm=time(NULL);
										int killNr=rand();
										int checksum;
										
										checksum = CModuleUtil::calcLootChecksum(tm,killNr,strlen(statChName),-1,corpseId,0,2);
										fprintf(lootStatsFile,"%d,%d,'%s',%d,%d,%d,%d,%d\n",tm,killNr,statChName,-1,corpseId,0,2,checksum);
										
										CTibiaContainer *lootCont = cont9;
										int itemNr;
										for (itemNr=0;itemNr<lootCont->itemsInside;itemNr++)
										{												
											CTibiaItem *lootItem = (CTibiaItem *)lootCont->items.GetAt(itemNr);
											
											checksum = CModuleUtil::calcLootChecksum(tm,killNr,strlen(statChName),itemNr,lootItem->objectId,(lootItem->quantity?lootItem->quantity:1),2);
											if (checksum<0) checksum*=-1;
											fprintf(lootStatsFile,"%d,%d,'%s',%d,%d,%d,%d,%d\n",tm,killNr,statChName,itemNr,lootItem->objectId,lootItem->quantity?lootItem->quantity:1,2,checksum);
										}
										
										
										if (cont8->flagOnOff)
										{
											lootCont = cont8;
											int itemNr;
											for (itemNr=0;itemNr<lootCont->itemsInside;itemNr++)
											{												
												CTibiaItem *lootItem = (CTibiaItem *)lootCont->items.GetAt(itemNr);
												
												checksum = CModuleUtil::calcLootChecksum(tm,killNr,strlen(statChName),100+itemNr,lootItem->objectId,(lootItem->quantity?lootItem->quantity:1),2);
												if (checksum<0) checksum*=-1;
												fprintf(lootStatsFile,"%d,%d,'%s',%d,%d,%d,%d,%d\n",tm,killNr,statChName,100+itemNr,lootItem->objectId,lootItem->quantity?lootItem->quantity:1,2,checksum);
											}								
										}
										if (cont7->flagOnOff)
										{
											lootCont = cont7;
											int itemNr;
											for (itemNr=0;itemNr<lootCont->itemsInside;itemNr++)
											{												
												CTibiaItem *lootItem = (CTibiaItem *)lootCont->items.GetAt(itemNr);
												
												checksum = CModuleUtil::calcLootChecksum(tm,killNr,strlen(attackedCh->name),100+itemNr,lootItem->objectId,(lootItem->quantity?lootItem->quantity:1),2);
												if (checksum<0) checksum*=-1;
												fprintf(lootStatsFile,"%d,%d,'%s',%d,%d,%d,%d,%d\n",tm,killNr,attackedCh->name,100+itemNr,lootItem->objectId,lootItem->quantity?lootItem->quantity:1,2,checksum);
											}
										}
										
										fclose(lootStatsFile);
									}												
								}
								
								
								
							} // if corpseId && cont9 open
						} // if (attackedCh)
						
						
					} // if (7-9 containers closed)
					
					delete cont7;
					delete cont8;
					delete cont9;
					
					
					
				}
				delete self;
				if (attackedCh)
					delete attackedCh;
			}
		}
		lastAttackedMonster = reader.getAttackedCreature();

		/*** moving part ***/

		/**
		 * mode 0 - ignore
		 * mode 1 - carrying
		 * mode 2 - external
		 */

		int containerCarrying=-1;

		// find carrying container
		if (containerCarrying==-1&&config->m_mode1==1&&containerNotFull(0)) containerCarrying=0;
		if (containerCarrying==-1&&config->m_mode2==1&&containerNotFull(1)) containerCarrying=1;
		if (containerCarrying==-1&&config->m_mode3==1&&containerNotFull(2)) containerCarrying=2;
		if (containerCarrying==-1&&config->m_mode4==1&&containerNotFull(3)) containerCarrying=3;
		if (containerCarrying==-1&&config->m_mode5==1&&containerNotFull(4)) containerCarrying=4;
		if (containerCarrying==-1&&config->m_mode6==1&&containerNotFull(5)) containerCarrying=5;
		if (containerCarrying==-1&&config->m_mode7==1&&containerNotFull(6)) containerCarrying=6;
		if (containerCarrying==-1&&config->m_mode8==1&&containerNotFull(7)) containerCarrying=7;
		if (containerCarrying==-1&&config->m_mode9==1&&containerNotFull(8)) containerCarrying=8;
		if (containerCarrying==-1&&config->m_mode10==1&&containerNotFull(9)) containerCarrying=9;


		if (containerCarrying!=-1)
		{
			// find items in the other containers
			CUIntArray acceptedItems;
			
			if (config->m_lootFood)
			{
				int p;
				for (p=0;p<itemProxy.getItemsFoodArray()->GetSize();p++)
				{
					acceptedItems.Add(itemProxy.getItemsFoodArray()->GetAt(p));
				}
			}
			if (config->m_lootGp)
			{
				acceptedItems.Add(itemProxy.getValueForConst("GP"));
			}
			if (config->m_lootWorms)
			{
				acceptedItems.Add(itemProxy.getValueForConst("worms"));
			}
			if (config->m_lootCustom)
			{
				int i;
				for (i=0;i<itemProxy.getItemsLootedCount();i++)
				{					
					
					acceptedItems.Add(itemProxy.getItemsLootedId(i));
				}
			}
			
			
			int extraSleep=0;
			if (config->m_mode1==2&&CModuleUtil::loopItemFromSpecifiedContainer(0,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode2==2&&CModuleUtil::loopItemFromSpecifiedContainer(1,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode3==2&&CModuleUtil::loopItemFromSpecifiedContainer(2,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode4==2&&CModuleUtil::loopItemFromSpecifiedContainer(3,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode5==2&&CModuleUtil::loopItemFromSpecifiedContainer(4,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode6==2&&CModuleUtil::loopItemFromSpecifiedContainer(5,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode7==2&&CModuleUtil::loopItemFromSpecifiedContainer(6,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode8==2&&CModuleUtil::loopItemFromSpecifiedContainer(7,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode9==2&&CModuleUtil::loopItemFromSpecifiedContainer(8,&acceptedItems,containerCarrying)) extraSleep=1;
			if (config->m_mode10==2&&CModuleUtil::loopItemFromSpecifiedContainer(9,&acceptedItems,containerCarrying)) extraSleep=1;
			if (extraSleep) Sleep(1000);
		}
				
	}
	toolThreadShouldStop=0;
	return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CMod_looterApp construction

CMod_looterApp::CMod_looterApp()
{
	m_configDialog =NULL;
	m_started=0;
	m_configData = new CConfigData();	
}

CMod_looterApp::~CMod_looterApp()
{
	if (m_configDialog)
	{
		delete m_configDialog;
	}
	delete m_configData;	
}

char * CMod_looterApp::getName()
{
	return "Auto looter";
}


int CMod_looterApp::isStarted()
{
	return m_started;
}


void CMod_looterApp::start()
{	
	superStart();
	if (m_configDialog)
	{
		m_configDialog->disableControls();
		m_configDialog->activateEnableButton(true);
	}

		
	FILE *f=fopen("tibiaauto-stats-loot.txt","r");
		
	if (f)
	
	{
		fseek(f,0,SEEK_END);

		int flen=ftell(f);
		fclose(f);		
		if (flen>1024*100)
		{
			CSendStats info;
			info.DoModal();				
		}		
	}
	

	DWORD threadId;
		
	toolThreadShouldStop=0;
	toolThreadHandle =  ::CreateThread(NULL,0,toolThreadProc,m_configData,0,&threadId);				
	m_started=1;
}

void CMod_looterApp::stop()
{
	toolThreadShouldStop=1;
	while (toolThreadShouldStop) {
		Sleep(50);
	};
	m_started=0;
	
	if (m_configDialog)
	{
		m_configDialog->enableControls();
		m_configDialog->activateEnableButton(false);
	}
} 

void CMod_looterApp::showConfigDialog()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());	

	if (!m_configDialog)
	{
		m_configDialog = new CConfigDialog(this);
		m_configDialog->Create(IDD_CONFIG);
		configToControls();
	}
	m_configDialog->ShowWindow(SW_SHOW);
}


void CMod_looterApp::configToControls()
{
	if (m_configDialog)
	{		
		
		m_configDialog->configToControls(m_configData);
	}
}


void CMod_looterApp::controlsToConfig()
{
	if (m_configDialog)
	{
		delete m_configData;
		m_configData = m_configDialog->controlsToConfig();
	}
}


void CMod_looterApp::disableControls()
{
	if (m_configDialog)
	{
		m_configDialog->disableControls();
	}
}

void CMod_looterApp::enableControls()
{
	if (m_configDialog)
	{
		m_configDialog->enableControls();
	}
}


char *CMod_looterApp::getVersion()
{
	return "1.4";
}


int CMod_looterApp::validateConfig(int showAlerts)
{
	return 1;
}

void CMod_looterApp::resetConfig()
{
	m_configData = new CConfigData();
}

void CMod_looterApp::loadConfigParam(char *paramName,char *paramValue)
{
	if (!strcmp(paramName,"mode/cont1")) m_configData->m_mode1=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont2")) m_configData->m_mode2=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont3")) m_configData->m_mode3=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont4")) m_configData->m_mode4=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont5")) m_configData->m_mode5=atoi(paramValue);

	if (!strcmp(paramName,"mode/cont6")) m_configData->m_mode6=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont7")) m_configData->m_mode7=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont8")) m_configData->m_mode8=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont9")) m_configData->m_mode9=atoi(paramValue);
	if (!strcmp(paramName,"mode/cont10")) m_configData->m_mode10=atoi(paramValue);

	if (!strcmp(paramName,"loot/worms")) m_configData->m_lootWorms=atoi(paramValue);
	if (!strcmp(paramName,"loot/gp")) m_configData->m_lootGp=atoi(paramValue);
	if (!strcmp(paramName,"loot/food")) m_configData->m_lootFood=atoi(paramValue);
	if (!strcmp(paramName,"loot/custom")) m_configData->m_lootCustom=atoi(paramValue);	
	if (!strcmp(paramName,"other/autoOpen")) m_configData->m_autoOpen=atoi(paramValue);	
}

char *CMod_looterApp::saveConfigParam(char *paramName)
{
	static char buf[1024];
	buf[0]=0;
	
	if (!strcmp(paramName,"mode/cont1")) sprintf(buf,"%d",m_configData->m_mode1);
	if (!strcmp(paramName,"mode/cont2")) sprintf(buf,"%d",m_configData->m_mode2);
	if (!strcmp(paramName,"mode/cont3")) sprintf(buf,"%d",m_configData->m_mode3);
	if (!strcmp(paramName,"mode/cont4")) sprintf(buf,"%d",m_configData->m_mode4);
	if (!strcmp(paramName,"mode/cont5")) sprintf(buf,"%d",m_configData->m_mode5);

	if (!strcmp(paramName,"mode/cont6")) sprintf(buf,"%d",m_configData->m_mode6);
	if (!strcmp(paramName,"mode/cont7")) sprintf(buf,"%d",m_configData->m_mode7);
	if (!strcmp(paramName,"mode/cont8")) sprintf(buf,"%d",m_configData->m_mode8);
	if (!strcmp(paramName,"mode/cont9")) sprintf(buf,"%d",m_configData->m_mode9);
	if (!strcmp(paramName,"mode/cont10")) sprintf(buf,"%d",m_configData->m_mode10);

	if (!strcmp(paramName,"loot/worms")) sprintf(buf,"%d",m_configData->m_lootWorms);
	if (!strcmp(paramName,"loot/gp")) sprintf(buf,"%d",m_configData->m_lootGp);
	if (!strcmp(paramName,"loot/food")) sprintf(buf,"%d",m_configData->m_lootFood);
	if (!strcmp(paramName,"loot/custom")) sprintf(buf,"%d",m_configData->m_lootCustom);
	if (!strcmp(paramName,"other/autoOpen")) sprintf(buf,"%d",m_configData->m_autoOpen);

	return buf;
}

char *CMod_looterApp::getConfigParamName(int nr)
{
	switch (nr)
	{
	case 0: return "mode/cont1";
	case 1: return "mode/cont2";
	case 2: return "mode/cont3";
	case 3: return "mode/cont4";
	case 4: return "mode/cont5";
	case 5: return "mode/cont6";
	case 6: return "mode/cont7";
	case 7: return "mode/cont8";
	case 8: return "mode/cont9";
	case 9: return "mode/cont10";
	case 10: return "loot/worms";
	case 11: return "loot/gp";
	case 12: return "loot/food";
	case 13: return "loot/custom";
	case 14: return "other/autoOpen";	
	default:
		return NULL;
	}
}