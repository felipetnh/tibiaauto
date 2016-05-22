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


// mod_banker.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "mod_banker.h"

#include "ConfigDialog.h"
#include "ConfigData.h"
#include "TibiaContainer.h"
#include "MemConstData.h"

#include <ModuleLoader.h>
#include <MemReader.h>
#include <PackSender.h>
#include <TibiaItem.h>
#include <VariableStore.h>
#include <ModuleUtil.h>
#include <Tlhelp32.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif // ifdef _DEBUG

CToolBankerState globalBankerState = CToolBankerState_notRunning;
int GUIx                           = 0, GUIy = 0, GUIz = 0;

/////////////////////////////////////////////////////////////////////////////
// Tool functions

int RandomTimeBankerSay(int length)
{
	return CModuleUtil::randomFormula(300 + min(length * 100, 1200), 200);//ranges from 300 to 1700
}

int findBanker(CConfigData *);
int moveToBanker(CConfigData *);
void getBalance();
int depositGold();
int changeGold();
int withdrawGold(CConfigData *config, int suggestedWithdrawAmount);
int isDepositing();
int countAllItemsOfType(int, bool);
int shouldBank(CConfigData *);
int canBank(CConfigData *);

// Required to be run more often than the return value is expected to change
int doneAttackingAndLooting()
{
	CMemReader& reader = CMemReader::getMemReader();
	static unsigned int lastAttackTm = 0;
	int ret                          = GetTickCount() - lastAttackTm > 3 * 1000 && !reader.getAttackedCreature();
	if (reader.getAttackedCreature())
		lastAttackTm = GetTickCount();
	return ret;
}

/////////////////////////////////////////////////////////////////////////////
// Tool thread function

int toolThreadShouldStop = 0;
HANDLE toolThreadHandle;

DWORD WINAPI toolThreadProc(LPVOID lpParam)
{
	CMemReader& reader = CMemReader::getMemReader();
	CConfigData *config = (CConfigData *)lpParam;

	int persistentShouldGo      = 0;
	time_t lastPathNotFoundTm   = 0;
	time_t lastBankerSuccessTm  = 0;
	int bankerPauseAfterSuccess = 5;
	int modRuns                 = 0;
	while (!toolThreadShouldStop)
	{
		Sleep(400);
		modRuns++;
		if (time(NULL) - lastBankerSuccessTm <= bankerPauseAfterSuccess)
			continue;

		if (!persistentShouldGo && shouldBank(config))
			persistentShouldGo = 1;
		const char* controller = CVariableStore::getVariable("walking_control");
		if (!persistentShouldGo
		    && config->stopByBanker
		    && (!strcmp(controller, "seller") || !strcmp(controller, "depotwalker"))
		    && canBank(config))
			persistentShouldGo = 1;
		if (persistentShouldGo && modRuns % 5 == 0 && !canBank(config))//do not go to bank if can't go anymore
			persistentShouldGo = 0;

		bool control    = strcmp(CVariableStore::getVariable("walking_control"), "banker") == 0;
		int modpriority = atoi(CVariableStore::getVariable("walking_priority"));
		// if wants control
		if (persistentShouldGo)
		{
			//if no path found let other modules work and wait 10 secs before trying again
			if (time(NULL) - lastPathNotFoundTm > 10)
			{
				//if should have control, take it
				if (!control)
				{
					if (atoi(config->modPriorityStr) > modpriority)
					{
						CVariableStore::setVariable("walking_control", "banker");
						CVariableStore::setVariable("walking_priority", config->modPriorityStr);
					}
					else
					{
						globalBankerState = CToolBankerState_halfSleep;
					}
				}
			}
			else
			{
				globalBankerState = CToolBankerState_noPathFound;
				//if has control, give it up
				if (control)
				{
					CVariableStore::setVariable("walking_control", "");
					CVariableStore::setVariable("walking_priority", "0");
				}
			}
		}
		else     // if doesn't want control
		{
			globalBankerState = CToolBankerState_notRunning;
			//if has control, give it up
			if (control)
			{
				CVariableStore::setVariable("walking_control", "");
				CVariableStore::setVariable("walking_priority", "0");
			}
		}
		if (doneAttackingAndLooting() && strcmp(CVariableStore::getVariable("walking_control"), "banker") == 0)
		{
			if (findBanker(config))
			{
				globalBankerState = CToolBankerState_walking;
				if (moveToBanker(config))
				{
					globalBankerState = CToolBankerState_talking;
					//AfxMessageBox("Yup, found the banker!");
					if (config->changeGold)
					{
						if (changeGold())
							lastBankerSuccessTm = time(NULL);
					}
					else
					{
						if (depositGold())
						{
							// High Priority Task
							CVariableStore::setVariable("walking_control", "banker");
							CVariableStore::setVariable("walking_priority", "9");
							int suggestedWithdrawAmount = atoi(CVariableStore::getVariable("banker_suggestion"));

							if (config->cashOnHand || suggestedWithdrawAmount > 0)
								withdrawGold(config, suggestedWithdrawAmount);
							getBalance();
							CVariableStore::setVariable("walking_control", "banker");
							CVariableStore::setVariable("walking_priority", config->modPriorityStr);
							lastBankerSuccessTm = time(NULL);
						}
					}
					persistentShouldGo = 0;
				}
			}
			else
			{
				lastPathNotFoundTm = time(NULL);
			}
		}
	}
	if (strcmp(CVariableStore::getVariable("walking_control"), "banker") == 0)
	{
		CVariableStore::setVariable("walking_control", "");
		CVariableStore::setVariable("walking_priority", "0");
	}
	globalBankerState    = CToolBankerState_notRunning;
	toolThreadShouldStop = 0;
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CMod_bankerApp construction

CMod_bankerApp::CMod_bankerApp()
{
	m_configDialog = NULL;
	m_started      = 0;
	m_configData   = new CConfigData();
}

CMod_bankerApp::~CMod_bankerApp()
{
	if (m_configDialog)
	{
		m_configDialog->DestroyWindow();
		delete m_configDialog;
	}
	delete m_configData;
}

char * CMod_bankerApp::getName()
{
	return "Auto banker";
}

int CMod_bankerApp::isStarted()
{
	return m_started;
}

void CMod_bankerApp::start()
{
	superStart();
	if (m_configDialog)
	{
		m_configDialog->disableControls();
		m_configDialog->activateEnableButton(true);
	}

	DWORD threadId;

	toolThreadShouldStop = 0;
	toolThreadHandle     = ::CreateThread(NULL, 0, toolThreadProc, m_configData, 0, &threadId);
	m_started            = 1;
}

void CMod_bankerApp::stop()
{
	toolThreadShouldStop = 1;
	while (toolThreadShouldStop)
	{
		Sleep(50);
	};
	m_started = 0;

	if (m_configDialog)
	{
		m_configDialog->enableControls();
		m_configDialog->activateEnableButton(false);
	}
}

void CMod_bankerApp::showConfigDialog()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!m_configDialog)
	{
		m_configDialog = new CConfigDialog(this);
		m_configDialog->Create(IDD_CONFIG);
		configToControls();
		if (m_started)
			disableControls();
		else
			enableControls();
		m_configDialog->m_enable.SetCheck(m_started);
	}
	m_configDialog->ShowWindow(SW_SHOW);
}

void CMod_bankerApp::configToControls()
{
	if (m_configDialog)
		m_configDialog->configToControls(m_configData);
}

void CMod_bankerApp::controlsToConfig()
{
	if (m_configDialog)
	{
		delete m_configData;
		m_configData = m_configDialog->controlsToConfig();
	}
}

void CMod_bankerApp::disableControls()
{
	if (m_configDialog)
		m_configDialog->disableControls();
}

void CMod_bankerApp::enableControls()
{
	if (m_configDialog)
		m_configDialog->enableControls();
}

char *CMod_bankerApp::getVersion()
{
	return "2.0";
}

int CMod_bankerApp::validateConfig(int showAlerts)
{
	return 1;
}

void CMod_bankerApp::resetConfig()
{
	if (m_configData)
	{
		delete m_configData;
		m_configData = NULL;
	}
	m_configData = new CConfigData();
}

void CMod_bankerApp::loadConfigParam(const char *paramName, char *paramValue)
{
	if (!strcmp(paramName, "BankerName"))
		sprintf(m_configData->banker.bankerName, "%s", paramValue);
	if (!strcmp(paramName, "BankerPos"))
		sscanf(paramValue, "(%d,%d,%d)", &(m_configData->banker.bankerX), &(m_configData->banker.bankerY), &(m_configData->banker.bankerZ));
	if (!strcmp(paramName, "DepositTrigger"))
		m_configData->minimumGoldToBank = atoi(paramValue);
	if (!strcmp(paramName, "CashOnHand"))
		m_configData->cashOnHand = atoi(paramValue);
	if (!strcmp(paramName, "ModPriority"))
		strcpy(m_configData->modPriorityStr, paramValue);
	if (!strcmp(paramName, "ChangeGold"))
		m_configData->changeGold = atoi(paramValue);
	if (!strcmp(paramName, "CapsLimit"))
		m_configData->capsLimit = atoi(paramValue);
	if (!strcmp(paramName, "StopByBanker"))
		m_configData->stopByBanker = atoi(paramValue);
	if (!strcmp(paramName, "DrawUpTo"))
		m_configData->drawUpTo = atoi(paramValue);
}

char *CMod_bankerApp::saveConfigParam(const char *paramName)
{
	static char buf[1024];
	buf[0] = '\0';
	if (!strcmp(paramName, "BankerName"))
		sprintf(buf, "%s", m_configData->banker.bankerName);
	if (!strcmp(paramName, "BankerPos"))
		sprintf(buf, "(%d,%d,%d)", m_configData->banker.bankerX, m_configData->banker.bankerY, m_configData->banker.bankerZ);
	if (!strcmp(paramName, "DepositTrigger"))
		sprintf(buf, "%d", m_configData->minimumGoldToBank);
	if (!strcmp(paramName, "CashOnHand"))
		sprintf(buf, "%d", m_configData->cashOnHand);
	if (!strcmp(paramName, "ModPriority"))
		strcpy(buf, m_configData->modPriorityStr);
	if (!strcmp(paramName, "ChangeGold"))
		sprintf(buf, "%d", m_configData->changeGold);
	if (!strcmp(paramName, "CapsLimit"))
		sprintf(buf, "%d", m_configData->capsLimit);
	if (!strcmp(paramName, "StopByBanker"))
		sprintf(buf, "%d", m_configData->stopByBanker);
	if (!strcmp(paramName, "DrawUpTo"))
		sprintf(buf, "%d", m_configData->drawUpTo);

	return buf;
}

static const char *configParamNames[] =
{
	"BankerName",
	"BankerPos",
	"DepositTrigger",
	"CashOnHand",
	"ModPriority",
	"ChangeGold",
	"CapsLimit",
	"StopByBanker",
	"DrawUpTo",
	NULL,
};
const char **CMod_bankerApp::getConfigParamNames()
{
	return configParamNames;
}


int findBanker(CConfigData *config)
{
	CMemReader& reader = CMemReader::getMemReader();
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	if (config->targetX == self.x && config->targetY == self.y && config->targetZ == self.z)
	{
		memset(config->path, 0, 15);
		return 1;
	}
	GUIx = config->banker.bankerX;
	GUIy = config->banker.bankerY;
	GUIz = config->banker.bankerZ;
	struct point nearestBank = CModuleUtil::findPathOnMap(self.x, self.y, self.z, config->banker.bankerX, config->banker.bankerY, config->banker.bankerZ, 0, config->path, 3);
	if (nearestBank.x && nearestBank.y && nearestBank.z)
	{
		config->targetX = nearestBank.x;
		config->targetY = nearestBank.y;
		config->targetZ = nearestBank.z;
		return 1;
	}
	return 0;
}

int shouldKeepWalking()
{
	static time_t lastAttackTime = 0;
	CMemReader& reader = CMemReader::getMemReader();
	if (!reader.getAttackedCreature())
	{
		const char *var = CVariableStore::getVariable("autolooterTm");
		if (strcmp(var, "") == 0)
		{
			if (lastAttackTime < time(NULL) - 3)
				return 1;
			else
				return 0;
		}
	}
	lastAttackTime = time(NULL);
	return 0;
}

int moveToBanker(CConfigData *config)
{
	CMemReader& reader = CMemReader::getMemReader();	

	static int positionFound = 0;
	CTibiaCharacter self;
	if (shouldKeepWalking())
	{
		//Find a location close enough to NPC
		if (!positionFound)
		{
			reader.readSelfCharacter(&self);
			CModuleUtil::executeWalk(self.x, self.y, self.z, config->path);
			if (self.x == config->targetX && self.y == config->targetY && self.z == config->targetZ)
				positionFound = 1;
		}
		else     //Approach NPC after finding them
		{
			for (int i = 0; i < reader.m_memMaxCreatures; i++)
			{
				CTibiaCharacter mon;
				reader.readVisibleCreature(&mon, i);
				// since banker.bankerName may include city, match first part of name
				if (mon.tibiaId == 0)
				{
					break;
				}
				int len = strlen(mon.name);
				if (strncmp(config->banker.bankerName, mon.name, len) == 0 && (config->banker.bankerName[len] == 0 || config->banker.bankerName[len] == ' '))
				{
					for (int tries = 0; tries < 2; tries++) // should only need 1 try, but we'd need to start over if we don't make it
					{
						reader.readSelfCharacter(&self);
						reader.readVisibleCreature(&mon, i);

						struct point nearestBank = point(0, 0, 0);
						int rad                  = 2;
						while (nearestBank.x == 0 && rad <= 3)//find paths increasingly farther away
						{
							nearestBank = CModuleUtil::findPathOnMap(self.x, self.y, self.z, mon.x, mon.y, mon.z, 0, config->path, rad++);
						}
						if (nearestBank.x && nearestBank.y && nearestBank.z == self.z)
						{
							CModuleUtil::executeWalk(self.x, self.y, self.z, config->path);
							if (CModuleUtil::waitToStandOnSquare(nearestBank.x, nearestBank.y))
							{
								return 1;
							}
						}
					}
				}
			}
			positionFound = 0;
		}
	}
	return 0;
}

void getBalance()
{

	Sleep(RandomTimeBankerSay(strlen("balance")));
	CPackSender::sayNPC("balance");
}

int depositGold()
{
	
	CMemReader& reader = CMemReader::getMemReader();


	CTibiaCharacter self;
	 reader.readSelfCharacter(& self);
	float origcaps        = self.cap;

	int moneycount = countAllItemsOfType(CTibiaItem::getValueForConst("GP"), true);
	moneycount += countAllItemsOfType(CTibiaItem::getValueForConst("PlatinumCoin"), true) * 100;
	moneycount += countAllItemsOfType(CTibiaItem::getValueForConst("CrystalCoin"), true) * 10000;
	Sleep(RandomTimeBankerSay(strlen("hi")));
	CPackSender::say("hi");
	Sleep(500);//Give time for NPC window to open
	Sleep(RandomTimeBankerSay(strlen("deposit all")));
	CPackSender::sayNPC("deposit all");
	Sleep(RandomTimeBankerSay(strlen("yes")));
	CPackSender::sayNPC("yes");
	if (CModuleUtil::waitForCapsChange(origcaps) || moneycount == 0)//return success if caps changed or if we might have had no money to begin with
		return 1;
	return 0;
}

int withdrawGold(CConfigData *config, int suggestedWithdrawAmount)
{
	
	CMemReader& reader = CMemReader::getMemReader();

	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	char withdrawBuf[32];

	sprintf(withdrawBuf, "withdraw %d", config->cashOnHand + suggestedWithdrawAmount);
	Sleep(RandomTimeBankerSay(strlen(withdrawBuf)));
	CPackSender::sayNPC(withdrawBuf);
	Sleep(RandomTimeBankerSay(strlen("yes")));
	CPackSender::sayNPC("yes");

	if (CModuleUtil::waitForCapsChange(self.cap))
	{
		return 1;
	}
	return 0;
}

int changeGold()
{
	CMemReader& reader = CMemReader::getMemReader();

	

	CTibiaCharacter self;
	 reader.readSelfCharacter(& self);
	float origcaps        = self.cap;
	int retval = 0;

	int goldId    = CTibiaItem::getValueForConst("GP");
	int goldCount = countAllItemsOfType(goldId, true);

	char buf[128];
	Sleep(RandomTimeBankerSay(strlen("hi")));
	CPackSender::say("hi");
	Sleep(500);//Give time for NPC window to open
	if (goldCount >= 100)
	{
		Sleep(RandomTimeBankerSay(strlen("change gold")));
		CPackSender::sayNPC("change gold");
		sprintf(buf, "%d", goldCount / 100);
		Sleep(RandomTimeBankerSay(strlen(buf)));
		CPackSender::sayNPC(buf);
		Sleep(RandomTimeBankerSay(strlen("yes")));
		CPackSender::sayNPC("yes");
	}

	if (CModuleUtil::waitForCapsChange(origcaps))
		retval = 1;

	int platId    = CTibiaItem::getValueForConst("PlatinumCoin");
	int platCount = countAllItemsOfType(platId, true);
	if (platCount >= 100)
	{
		Sleep(RandomTimeBankerSay(strlen("change platinum")));
		CPackSender::sayNPC("change platinum");
		Sleep(RandomTimeBankerSay(strlen("crystal")));
		CPackSender::sayNPC("crystal");
		sprintf(buf, "%d", platCount / 100);
		Sleep(RandomTimeBankerSay(strlen(buf)));
		CPackSender::sayNPC(buf);
		Sleep(RandomTimeBankerSay(strlen("yes")));
		CPackSender::sayNPC("yes");
	}

	if (CModuleUtil::waitForCapsChange(origcaps))
		retval = 1;

	return retval;
}

int isDepositing()
{
	CMemReader& reader = CMemReader::getMemReader();
	const char *var = CVariableStore::getVariable("cavebot_depositing");
	return strcmp(var, "true") == 0;
}

int countAllItemsOfType(int objectId, bool includeSlots)
{
	CMemReader& reader = CMemReader::getMemReader();
	
	int contNr;
	int ret         = 0;
	int openContNr  = 0;
	int openContMax = reader.readOpenContainerCount();
	for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
	{
		CTibiaContainer *cont = reader.readContainer(contNr);

		if (cont->flagOnOff)
		{
			openContNr++;
			ret += cont->countItemsOfType(objectId);
		}
		delete cont;
	}
	if (includeSlots)
	{
		
		for (int slotNr = 0; slotNr < 10; slotNr++)   // Loops through all 10 inventory slots(backwards)
		{
			CTibiaItem *item = reader.readItem(reader.m_memAddressSlotArrow + slotNr * reader.m_memLengthItem);
			if (item->objectId == objectId)
				ret += item->quantity ? item->quantity : 1;
			delete item;
		}
	}
	return ret;
}

int shouldBank(CConfigData *config)
{
	
	CMemReader& reader = CMemReader::getMemReader();
	

	CTibiaCharacter self;
	 reader.readSelfCharacter(& self);
	int belowCaps         = self.cap < config->capsLimit;


	int goldId       = CTibiaItem::getValueForConst("GP");
	int platId       = CTibiaItem::getValueForConst("PlatinumCoin");
	int crystalId    = CTibiaItem::getValueForConst("CrystalCoin");
	int goldCount    = countAllItemsOfType(goldId, true);
	int platCount    = countAllItemsOfType(platId, true);
	int crystalCount = countAllItemsOfType(crystalId, true);

	int totalCash = goldCount + platCount * 100 + crystalCount * 10000;
	int canChange = goldCount >= 100 || platCount >= 100;

	if (belowCaps)
		if (!config->changeGold && totalCash > config->cashOnHand || config->changeGold && canChange)
			return 1;
	if (!config->changeGold)
	{
		if (totalCash >= config->minimumGoldToBank || totalCash < config->cashOnHand && config->drawUpTo)
			return 1;
		else
			return 0;
	}
	return 0;
}

int canBank(CConfigData *config)
{
	
	CMemReader& reader = CMemReader::getMemReader();
	

	CTibiaCharacter self;
	 reader.readSelfCharacter(& self);


	int goldId       = CTibiaItem::getValueForConst("GP");
	int platId       = CTibiaItem::getValueForConst("PlatinumCoin");
	int crystalId    = CTibiaItem::getValueForConst("CrystalCoin");
	int goldCount    = countAllItemsOfType(goldId, true);
	int platCount    = countAllItemsOfType(platId, true);
	int crystalCount = countAllItemsOfType(crystalId, true);

	int totalCash = goldCount + platCount * 100 + crystalCount * 10000;
	int canChange = goldCount >= 100 || platCount >= 100;

	if (!config->changeGold && (totalCash > config->cashOnHand || totalCash < config->cashOnHand && config->drawUpTo) || config->changeGold && canChange)
		return 1;
	return 0;
}

void CMod_bankerApp::getNewSkin(CSkin newSkin)
{
	skin = newSkin;

	if (m_configDialog)
	{
		m_configDialog->DoSetButtonSkin();
		m_configDialog->Invalidate();
	}
}
