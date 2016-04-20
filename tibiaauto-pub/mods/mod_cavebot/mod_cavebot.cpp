/*
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

// mod_cavebot.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "mod_cavebot.h"

#include "ConfigDialog.h"
#include "ConfigData.h"
#include "SendStats.h"
#include "SharedMemory.h"
//#include "TileReader.h"

#include <ModuleUtil.h>
#include <TibiaStructures.h>
#include <TibiaTile.h>
#include <TibiaMap.h>
#include <TibiaContainer.h>
#include <PackSender.h>
#include <MemConstData.h>
#include <MemReader.h>
#include <MemUtil.h>
#include <VariableStore.h>
#include <IPCBackPipe.h>
#include <TileReader.h>
#include <IpcMessage.h>
#include <time.h>
#include <Tlhelp32.h>

#include <iostream>
#include <fstream>
#include <time.h>
#include <sstream>
#include <string>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif // ifdef _DEBUG

// NOTE: those delete trampolines are used to ensure 'secure' delete operation
//       and force revealing any memory issues faster
template <class T>
void deleteAndNull(T *&ptr)
{
	delete ptr;
	ptr = NULL;
}

CToolAutoAttackStateAttack globalAutoAttackStateAttack     = CToolAutoAttackStateAttack_notRunning;
CToolAutoAttackStateLoot globalAutoAttackStateLoot         = CToolAutoAttackStateLoot_notRunning;
CToolAutoAttackStateWalker globalAutoAttackStateWalker     = CToolAutoAttackStateWalker_notRunning;
CToolAutoAttackStateDepot globalAutoAttackStateDepot       = CToolAutoAttackStateDepot_notRunning;
CToolAutoAttackStateTraining globalAutoAttackStateTraining = CToolAutoAttackStateTraining_notRunning;

#define MAX_LOOT_ARRAY 1300

#define GET 0
#define MAKE 1

int waypointTargetX            = 0, waypointTargetY = 0, waypointTargetZ = 0;
int actualTargetX              = 0, actualTargetY = 0, actualTargetZ = 0;
int depotX                     = 0, depotY = 0, depotZ = 0;
time_t lastDepotPathNotFoundTm = 0;
int persistentShouldGo         = 0;
time_t lastSendAttackTm        = 0;

time_t currentPosTM          = 0;
int creatureAttackDist       = 0;
time_t firstCreatureAttackTM = 0;
int currentWaypointNr        = 0;
time_t walkerStandingEndTm   = 0;

time_t lastTAMessageTm = 0;
int taMessageDelay     = 4;
int forwardBackDir     = 1;//forward and back direction
time_t autolooterTm    = 0;
DWORD lootThreadId;
DWORD queueThreadId;
CTibiaQueue<Corpse> corpseQueue;

const int CONTAINER_TIME_CUTOFF = 10;
time_t containerTimes[32];//used to differentiate between carried containers and other containers


/**
 * Register cavebot debug.
 */
/*
   void registerDebug(int argNum,...){
        char * args[]=&argNum+sizeof(argNum);
        CString buf="";
        while (argNum--){
                buf+=args[argNum];
                if (argNum) buf+="\t";
        }
        CModuleUtil::masterDebug("tibiaauto-debug-cavebot.txt",buf.GetBuffer(2000));
   }*/
void registerDebug(const char* buf1, const char* buf2 = "", const char* buf3 = "", const char* buf4 = "", const char* buf5 = "", const char* buf6 = "")
{
	CModuleUtil::masterDebug("tibiaauto-debug-cavebot.txt", buf1, buf2, buf3, buf4, buf5, buf6);
}

std::string intstr(DWORD value)
{
	std::stringstream ss;
	ss << value;
	return ss.str();
}

inline int findInIntArray(int v, CUIntArray& arr)
{
	for (int i = arr.GetSize() - 1; i >= 0; i--)
	{
		if (arr.ElementAt(i) == v)
			return i;
	}
	return -1;
}

static map<int*, int> setTime;
//Creates a random percentage based off of another player's stats that will not change until MAKE is used(GET creates a number if none already present)
int RandomVariableTime(int &pt, int minTime, int maxTime, int command)
{
	if (!setTime[&pt])
		command = MAKE;
	if (command == MAKE)
		setTime[&pt] = CModuleUtil::randomFormula(minTime, maxTime, minTime - 1);
	return setTime[&pt];
}

int binarySearch(unsigned int f, CUIntArray& arr)
{
	int ASCENDING = 1;
	int e         = arr.GetSize();
	int s         = 0;
	while (s < e)
	{
		int m = (e + s) / 2; //m==s iff e-s==1
		if (arr[m] == f)
			return m;
		else if ((arr[m] > f) == ASCENDING)
			e = m;
		else if ((arr[m] < f) == ASCENDING)
			s = m + 1;
	}
	return -1;
}

void quickSort(CUIntArray& arr, int s, int e)
{
	int ASCENDING = 1;
	if (e - s <= 1)
		return;
	int size = arr.GetSize();
	ASSERT(s >= 0 && e <= size);
	int rotator = e - 1;

	//select random pivot
	int pivot = s + 1 + rand() % (e - s - 1);

	//switch pivot to start
	int tmp = arr[s];
	arr[s]     = arr[pivot];
	arr[pivot] = tmp;

	//set index
	pivot = s;
	while (pivot != rotator)
	{
		//check if should switch rotator and pivot
		if ((arr[pivot] >= arr[rotator] == (pivot < rotator)) == ASCENDING)
		{
			tmp          = arr[rotator];
			arr[rotator] = arr[pivot];
			arr[pivot]   = tmp;

			tmp     = rotator;
			rotator = pivot;
			pivot   = tmp;
		}
		//moves rotator 1 closer to pivot
		rotator += (pivot - rotator) / abs(pivot - rotator);
	}
	quickSort(arr, s, pivot);
	quickSort(arr, pivot + 1, e);
	return;
}

void sortArray(CUIntArray& arr)
{
	quickSort(arr, 0, arr.GetSize());
}

/**
 * Dump full info about a creature (for debuging purposes)
 */
void dumpCreatureInfo(char *info, unsigned int tibiaId)
{
	CMemReader& reader = CMemReader::getMemReader();
	int i;
	int nr = -1;
	for (i = 0; i < reader.m_memMaxCreatures; i++)
	{
		CTibiaCharacter ch;
		reader.readVisibleCreature(&ch, i);

		if (ch.tibiaId == tibiaId)
		{
			nr = i;
			break;
		}
		else if (ch.tibiaId == 0)
		{
			registerDebug("dumpCreatureInfo: Creature ID not found.");
			return;
		}
	}

	int offset = reader.m_memAddressFirstCreature + nr * reader.m_memLengthCreature;

	registerDebug(info);

	if (nr != -1)
	{
		for (i = 0; i < reader.m_memLengthCreature; i += 4)
		{
			char buf[128];
			int val = CMemUtil::getMemUtil().GetMemIntValue(offset + i);
			sprintf(buf, "offset %d -> %d", i, val);
			registerDebug(buf);
		}
	}
	else
	{
		registerDebug("Unable to find creature");
	}
	registerDebug("end of creature dump");
}

/*
 * Checks whether or not we can do something productive by going to the depot.
 */
int depotCheckCanGo(CConfigData *config)
{
	int ret = 0;
	CMemReader& reader = CMemReader::getMemReader();
	
	
	CUIntArray depotListObjects;
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	int i;
	for (i = 0; i < MAX_DEPOTTRIGGERCOUNT && config->depotTrigger[i].itemName[0] > 0; i++)
	{
		int objectId = CTibiaItem::getItemId(config->depotTrigger[i].itemName);
		//Track the items we check here so we do not check them again in looted item list
		depotListObjects.Add(objectId);
		int contNr;
		int totalQty    = 0;
		int openContNr  = 0;
		int openContMax = reader.readOpenContainerCount();
		for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
		{
			CTibiaContainer *cont = reader.readContainer(contNr);

			if (cont->flagOnOff)
			{
				openContNr++;
				totalQty += cont->countItemsOfType(objectId);
			}

			deleteAndNull(cont);
		}
		// check whether we can deposit something
		if (config->depotTrigger[i].when >= config->depotTrigger[i].remain &&
		    (totalQty > config->depotTrigger[i].remain))
		{
			ret = 1;
			goto exitDCG;
		}
		// check whether we can restack something
		if (config->depotTrigger[i].when <= config->depotTrigger[i].remain &&
		    (totalQty < config->depotTrigger[i].remain))
		{
			ret = 1;
			goto exitDCG;
		}
	}
	if (config->depositLootedItemList)
	{
		// If not returned yet then find another looted item not already checked in the depot list
		int contNr;
		int openContNr  = 0;
		int openContMax = reader.readOpenContainerCount();
		for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
		{
			CTibiaContainer *cont = reader.readContainer(contNr);
			if (cont->flagOnOff)
			{
				openContNr++;
				int itemNr;
				for (itemNr = 0; itemNr < cont->itemsInside; itemNr++)
				{
					CTibiaItem* item  = (CTibiaItem *)cont->items.GetAt(itemNr);
					int lootListIndex = CTibiaItem::getLootItemIndex(item->objectId);
					if (lootListIndex != -1)
					{
						if (findInIntArray(item->objectId, depotListObjects) == -1)
						{
							ret = 1;
							deleteAndNull(cont);
							goto exitDCG;
						}
					}
				}
			}
			deleteAndNull(cont);
		}
	}


exitDCG:
	return ret;
}

/*
 * Checks whether we should go back to depot or not.
 */
int depotCheckShouldGo(CConfigData *config)
{
	int ret = 0;
	CMemReader& reader = CMemReader::getMemReader();
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	if (self.cap < config->depotCap)
	{
		if (depotCheckCanGo(config))
		{
			ret = 1;
			goto exitDSG;
		}
		ret = 0;
		goto exitDSG;
	}

	int i;
	for (i = 0; i < MAX_DEPOTTRIGGERCOUNT && strlen(config->depotTrigger[i].itemName); i++)
	{
		int objectId = CTibiaItem::getItemId(config->depotTrigger[i].itemName);
		int contNr;
		int totalQty = 0;
		//Count number of this item in open containers
		int openContNr  = 0;
		int openContMax = reader.readOpenContainerCount();
		for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
		{
			CTibiaContainer *cont = reader.readContainer(contNr);

			if (cont->flagOnOff)
			{
				openContNr++;
				totalQty += cont->countItemsOfType(objectId);
			}

			deleteAndNull(cont);
		}
		// check whether we should deposit something
		if (config->depotTrigger[i].when >= config->depotTrigger[i].remain &&
		    (totalQty >= config->depotTrigger[i].when || globalAutoAttackStateDepot != CToolAutoAttackStateDepot_notRunning))
		{
			ret = 1;
			goto exitDSG;
		}
		// check whether we should restack something
		if (config->depotTrigger[i].when <= config->depotTrigger[i].remain &&
		    (totalQty <= config->depotTrigger[i].when || globalAutoAttackStateDepot != CToolAutoAttackStateDepot_notRunning))
		{
			ret = 1;
			goto exitDSG;
		}
	}

exitDSG:
	return ret;
}

/**
 * Do checks concerning 'go to depot' functionality.
 */
void depotCheck(CConfigData *config)
{
	CMemReader& reader = CMemReader::getMemReader();

	if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_notFound)
		lastDepotPathNotFoundTm = time(NULL);

	if (config->debug)
		registerDebug("Depot check");
	if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_notRunning)
	{
		CTibiaCharacter self;
		reader.readSelfCharacter(&self);
		if (!persistentShouldGo && depotCheckShouldGo(config))
			persistentShouldGo = 1;
		const char* controller = CVariableStore::getVariable("walking_control");
		if (!persistentShouldGo
		    && config->stopByDepot
		    && (!strcmp(controller, "seller") || !strcmp(controller, "banker"))
		    && depotCheckCanGo(config))
			persistentShouldGo = 1;

		bool control    = strcmp(controller, "depotwalker") == 0;
		int modpriority = atoi(CVariableStore::getVariable("walking_priority"));
		if (persistentShouldGo)
		{
			if (time(NULL) - lastDepotPathNotFoundTm > 10)
			{
				//if should have control, take it
				if (!control && atoi(config->depotModPriorityStr) > modpriority)
				{
					CVariableStore::setVariable("walking_control", "depotwalker");
					CVariableStore::setVariable("walking_priority", config->depotModPriorityStr);
				}
			}
			else
			{
				//if has control, give it up
				if (control)
				{
					CVariableStore::setVariable("walking_control", "");
					CVariableStore::setVariable("walking_priority", "0");
				}
			}
		}
		else     // if doesn't want control
		{       //if has control, give it up
			if (control)
			{
				CVariableStore::setVariable("walking_control", "");
				CVariableStore::setVariable("walking_priority", "0");
			}
		}
		if (strcmp(CVariableStore::getVariable("walking_control"), "depotwalker") == 0)
		{
			if (config->debug)
				registerDebug("We should go to a depot");
			/**
			 * Ok then - we should do the depositing.
			 */

			uint8_t path[15];
			struct point nearestDepot = CModuleUtil::findPathOnMap(self.x, self.y, self.z, 0, 0, 0, MAP_POINT_TYPE_DEPOT, path);
			depotX = nearestDepot.x;
			depotY = nearestDepot.y;
			depotZ = nearestDepot.z;
			if (depotX && depotY && depotZ)
			{
				// depot found - go to it
				globalAutoAttackStateDepot = CToolAutoAttackStateDepot_walking;
				// reset goto target
				waypointTargetX = depotX;
				waypointTargetY = depotY;
				waypointTargetZ = depotZ;
				if (config->debug)
					registerDebug("Depot found - going to it (x,y,z)=", intstr(waypointTargetX).c_str(), intstr(waypointTargetY).c_str(), intstr(waypointTargetZ).c_str());
			}
			else
			{
				// depot not found - argh
				globalAutoAttackStateDepot = CToolAutoAttackStateDepot_notFound;
				depotX                     = depotY = depotZ = 0;
				waypointTargetX            = waypointTargetY = waypointTargetZ = 0;
				if (config->debug)
					registerDebug("Depot not found - depot state: not found");
			}
			return;
		}
	}
	if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_walking)
	{
		if (!depotCheckCanGo(config))
		{
			globalAutoAttackStateDepot = CToolAutoAttackStateDepot_notRunning;
			persistentShouldGo         = 0;
			depotX                     = depotY = depotZ = 0;
			waypointTargetX            = waypointTargetY = waypointTargetZ = 0;
		}
		else
		{
			if (config->debug)
				registerDebug("Walking to a depot section");
			CTibiaCharacter self;
			reader.readSelfCharacter(&self);
			// check whether the depot is reachable
			uint8_t path[15];
			if (config->debug)
				registerDebug("findPathOnMap: depot walker");
			CModuleUtil::findPathOnMap(self.x, self.y, self.z, depotX, depotY, depotZ, MAP_POINT_TYPE_DEPOT, path);
			if (!path[0] || !CTibiaMap::getTibiaMap().isPointAvailable(depotX, depotY, depotZ))
			{
				if (config->debug)
					registerDebug("Path to the current depot not found: must find a new one");
				// path to the current depot not found, must find a new depot
				struct point nearestDepot = CModuleUtil::findPathOnMap(self.x, self.y, self.z, 0, 0, 0, MAP_POINT_TYPE_DEPOT, path);
				depotX = nearestDepot.x;
				depotY = nearestDepot.y;
				depotZ = nearestDepot.z;
				if (depotX && depotY && depotZ)
				{
					if (config->debug)
						registerDebug("New depot found");
					// depot found - go to it
					globalAutoAttackStateDepot = CToolAutoAttackStateDepot_walking;
					// reset goto target
					waypointTargetX = depotX;
					waypointTargetY = depotY;
					waypointTargetZ = depotZ;
				}
				else
				{
					if (config->debug)
						registerDebug("No new depot found");
					// depot not found - argh
					globalAutoAttackStateDepot = CToolAutoAttackStateDepot_notFound;
					depotX                     = depotY = depotZ = 0;
					waypointTargetX            = waypointTargetY = waypointTargetZ = 0;
				}
			}
			else
			{
				if (config->debug)
					registerDebug("Currently selected depot is reachable");
			}
		}
	}
	if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_notFound)
	{
		// not depot found - but try finding another one
		CTibiaCharacter self;
		reader.readSelfCharacter(&self);
		uint8_t path[15];
		struct point nearestDepot = CModuleUtil::findPathOnMap(self.x, self.y, self.z, 0, 0, 0, MAP_POINT_TYPE_DEPOT, path);
		depotX = nearestDepot.x;
		depotY = nearestDepot.y;
		depotZ = nearestDepot.z;
		if (depotX && depotY && depotZ)
		{
			// depot found - go to it
			globalAutoAttackStateDepot = CToolAutoAttackStateDepot_walking;
			// reset goto target
			waypointTargetX = depotX;
			waypointTargetY = depotY;
			waypointTargetZ = depotZ;
			if (config->debug)
				registerDebug("Depot found - going to it (x,y,z)=", intstr(waypointTargetX).c_str(), intstr(waypointTargetY).c_str(), intstr(waypointTargetZ).c_str());
		}
		else
		{
			// depot not found - argh
			globalAutoAttackStateDepot = CToolAutoAttackStateDepot_notFound;
			depotX                     = depotY = depotZ = 0;
			waypointTargetX            = waypointTargetY = waypointTargetZ = 0;
			persistentShouldGo         = 0;
			if (config->debug)
				registerDebug("Depot not found - depot state: not found (again)");
		}
	}
}

int depotDepositOpenChest(int x, int y, int z)
{
	CMemReader& reader = CMemReader::getMemReader();

	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	int depotContNr       = -1;
	int count             = reader.mapGetPointItemsCount(point(x - self.x, y - self.y, 0));
	int pos               = 0;
	if (count > 10)
		count = 10;    // ? should have written down why I put this here

	int lockerId = 0;
	for (pos = 0; pos < count; pos++)
	{
		int tileId       = reader.mapGetPointItemId(point(x - self.x, y - self.y, 0), pos);
		CTibiaTile* tile = CTileReader::getTileReader().getTile(tileId);
		if (!tile->notMoveable && !tile->isDepot && pos != count - 1)
		{
			//leave at least 1 object that is either movable or the depot box on the tile(For depositing in container in house)
			int qty = reader.mapGetPointItemExtraInfo(point(x - self.x, y - self.y, 0), pos, 1);
			CPackSender::moveObjectFromFloorToFloor(tileId, x, y, z, self.x, self.y, self.z, qty ? qty : 1);
			Sleep(CModuleUtil::randomFormula(400, 200));
			pos--;
			count--;
		}
		if (tile->isDepot)
		{
			lockerId = tileId;
			break;
		}
	}
	if (!lockerId && count)
		lockerId = reader.itemOnTopCode(x - self.x, y - self.y); //use the topmost item as the depot container(useful for bags within houses)
	if (lockerId)
	{
		// this is the depot chest so open it
		depotContNr = reader.findNextClosedContainer();
		CPackSender::openContainerFromFloor(lockerId, x, y, z, depotContNr);
		CModuleUtil::waitForOpenContainer(depotContNr, true);
		Sleep(CModuleUtil::randomFormula(700, 200));
		CTibiaContainer *cont = reader.readContainer(depotContNr);
		int depotSlotNr       = 0, depotId = 0;
		if (cont->flagOnOff && cont->itemsInside > depotSlotNr)
			depotId = ((CTibiaItem *)cont->items.GetAt(depotSlotNr))->objectId;
		if (depotId)
		{
			CPackSender::openContainerFromContainer(depotId, depotContNr + 0x40, depotSlotNr, depotContNr);
			CModuleUtil::waitForItemChange(depotContNr, depotSlotNr, depotId, ((CTibiaItem*)cont->items.GetAt(depotSlotNr))->quantity);
		}
		else
		{
			depotContNr = -1;
		}
		deleteAndNull(cont);
	}
	return depotContNr;
}

// returns quantity actually moved, tries to move as much of the item as possible.
// can be used for more than just depot
int depotTryMovingItemToContainer(int objectId, int sourceContNr, int sourcePos, int qty, int destContNr, int shouldCheckCaps = 0)
{
	CMemReader& reader = CMemReader::getMemReader();

	CTibiaTile *tile = CTileReader::getTileReader().getTile(objectId);
	if (tile == NULL)
		return 0;

	CTibiaContainer *destCont = reader.readContainer(destContNr);
	CTibiaContainer *srcCont  = reader.readContainer(sourceContNr);
	if (!destCont->flagOnOff || !srcCont->flagOnOff)
	{
		deleteAndNull(destCont);
		deleteAndNull(srcCont);
		return 0;
	}
	int qtyToMove = qty;
	int destPos   = destCont->size - 1;
	if (destCont->itemsInside >= destCont->size)//if no space, find location of a stack in the container
	{
		qtyToMove = 0;
		if (tile->stackable)  //
		{
			for (int itemNr = 0; itemNr < destCont->itemsInside; itemNr++)
			{
				CTibiaItem *item = (CTibiaItem *)destCont->items.GetAt(itemNr);
				if (item->objectId == objectId)
				{
					qtyToMove = min(qty, 100 - item->quantity);
					destPos   = itemNr;
					if (qtyToMove)
						break;
				}
			}
		}
	}
	if (qtyToMove)
	{
		if (shouldCheckCaps) // do more checking and retries in case we do not have enough caps
		{
			CTibiaCharacter self;
			reader.readSelfCharacter(&self);
			CPackSender::moveObjectBetweenContainers(objectId, 0x40 + sourceContNr, sourcePos, 0x40 + destContNr, destPos, qtyToMove);
			if (CModuleUtil::waitForCapsChange(self.cap))
			{
				Sleep(CModuleUtil::randomFormula(300, 100));
			}
			else  // if caps not changed, then move 1 item and get weight
			{
				if (qtyToMove != 1)
				{
					CPackSender::moveObjectBetweenContainers(objectId, 0x40 + sourceContNr, sourcePos, 0x40 + destContNr, destPos, 1);
					if (CModuleUtil::waitForCapsChange(self.cap))
					{
						Sleep(CModuleUtil::randomFormula(300, 100));
					}
					else  // exit if fails to move even 1 item
					{
						qtyToMove = 0;
						goto exitTryMoving;
					}
					CTibiaCharacter selfAfter;
					reader.readSelfCharacter(&selfAfter);
					float weight  = self.cap - selfAfter.cap;
					if (weight > 0)  // if we have a valid weight,
					{
						qtyToMove = min(qtyToMove, int(selfAfter.cap / (weight + .01)));
						CPackSender::moveObjectBetweenContainers(objectId, 0x40 + sourceContNr, sourcePos, 0x40 + destContNr, destPos, qtyToMove);
						if (CModuleUtil::waitForCapsChange(selfAfter.cap))
						{
							Sleep(CModuleUtil::randomFormula(300, 100));
						}
						else
						{
							qtyToMove = 0;
							goto exitTryMoving;
						}
					}
				}
				else
				{
					qtyToMove = 0;
					goto exitTryMoving;
				}
			}
		}
		else
		{
			CPackSender::moveObjectBetweenContainers(objectId, 0x40 + sourceContNr, sourcePos, 0x40 + destContNr, destPos, qtyToMove);
			CModuleUtil::waitForItemChange(sourceContNr, sourcePos, objectId, ((CTibiaItem *)srcCont->items.GetAt(sourcePos))->quantity);
			Sleep(CModuleUtil::randomFormula(300, 100));
		}
	}
exitTryMoving:
	deleteAndNull(destCont);
	deleteAndNull(srcCont);
	return qtyToMove;
}

/**
 * Move the item to some container in the chest.
 */
void depotDepositMoveToChest(int objectId, int sourceContNr, int sourcePos, int qty, int depotContNr, int depotContNr2)
{
	if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_noSpace)
		return;

	CMemReader& reader = CMemReader::getMemReader();

	CTibiaContainer *depotChest = reader.readContainer(depotContNr);

	if (!depotChest->flagOnOff)
	{
		// shouldn't happen...
		deleteAndNull(depotChest);
		return;
	}

	/**
	 * If secondary depot container is open than we can try to deposit to it (to avoid
	 * massive close/open of containers.
	 */
	qty -= depotTryMovingItemToContainer(objectId, sourceContNr, sourcePos, qty, depotContNr2);
	if (qty == 0)
	{
		deleteAndNull(depotChest);
		return;
	}


	/**
	 * Scan through all containers in the depot chest to look for an empty one
	 */
	int itemNr;
	int count        = depotChest->itemsInside;
	CTibiaTile *tile = CTileReader::getTileReader().getTile(objectId);
	for (itemNr = 0; itemNr < count; itemNr++)
	{
		CTibiaItem *item = (CTibiaItem *)depotChest->items.GetAt(itemNr);
		//look for place directly in depot if needed
		if (tile->stackable)
		{
			if (item->objectId == objectId &&
			    (item->quantity + qty < 100 || depotChest->itemsInside < depotChest->size))
			{
				qty -= depotTryMovingItemToContainer(objectId, sourceContNr, sourcePos, qty, depotContNr);
				if (qty == 0)
				{
					deleteAndNull(depotChest);
					return;
				}
			}
		}

		if (CTileReader::getTileReader().getTile(item->objectId)->isContainer)
		{
			// that object is a container, so open it
			CPackSender::closeContainer(depotContNr2);
			CModuleUtil::waitForOpenContainer(depotContNr2, false);
			CPackSender::openContainerFromContainer(item->objectId, 0x40 + depotContNr, itemNr, depotContNr2);
			CModuleUtil::waitForOpenContainer(depotContNr2, true);

			qty -= depotTryMovingItemToContainer(objectId, sourceContNr, sourcePos, qty, depotContNr2);
			if (qty == 0)
			{
				deleteAndNull(depotChest);
				return;
			}
		}
	}
	qty -= depotTryMovingItemToContainer(objectId, sourceContNr, sourcePos, qty, depotContNr);
	if (qty == 0)
	{
		deleteAndNull(depotChest);
		return;
	}

	deleteAndNull(depotChest);

	// if we are here this means that the depot is full
	globalAutoAttackStateDepot = CToolAutoAttackStateDepot_noSpace;
}

// take item from depot chest at (contNr,pos) and find empty space in my backpacks
int depotDepositTakeFromChest(int objectId, int srcContNr, int srcPos, int qtyToPickup, int depotContNr, int depotContNr2)
{
	if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_noSpace)
		return 0;

	CMemReader& reader = CMemReader::getMemReader();

	
	CTibiaTile* tile           = CTileReader::getTileReader().getTile(objectId);
	int qtyToPickupOrig        = qtyToPickup;
	for (int contNr = 0; contNr < reader.m_memMaxContainers && qtyToPickup; contNr++)
	{
		if (contNr == depotContNr || contNr == depotContNr2)
			continue;                                          //only scan through carried containers
		qtyToPickup -= depotTryMovingItemToContainer(objectId, srcContNr, srcPos, qtyToPickup, contNr, 1);
	}
	if (qtyToPickupOrig == qtyToPickup)
		globalAutoAttackStateDepot = CToolAutoAttackStateDepot_noSpace;
	return qtyToPickupOrig - qtyToPickup;
}

int countAllItemsOfType(int objectId, int depotContNr, int depotContNr2)   // excludes depotCont containers from search
{
	CMemReader& reader = CMemReader::getMemReader();
	
	int contNr;
	int ret         = 0;
	int openContNr  = 0;
	int openContMax = reader.readOpenContainerCount();
	for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
	{
		CTibiaContainer *cont = reader.readContainer(contNr);
		if (contNr == depotContNr || contNr == depotContNr2)
		{
			if (cont->flagOnOff)
				openContNr++;
			deleteAndNull(cont);
			continue;
		}


		if (cont->flagOnOff)
		{
			openContNr++;
			ret += cont->countItemsOfType(objectId);
		}

		deleteAndNull(cont);
	}
	return ret;
}

void listAllItemsInContainers(CUIntArray& sortedObjectIDs, CUIntArray& outObjectIDs, int depotContNr = -1, int depotContNr2 = -1)   // excludes depotCont containers from search
{       //If objectIDs is empty, then list all objects
	CMemReader& reader = CMemReader::getMemReader();
	
	bool listAll               = FALSE;
	if (sortedObjectIDs.IsEmpty())
		listAll = TRUE;
	// Gather all item IDs from all open containers
	int contNr;
	int ret         = 0;
	int openContNr  = 0;
	int openContMax = reader.readOpenContainerCount();
	for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
	{
		CTibiaContainer *cont = reader.readContainer(contNr);
		if (contNr == depotContNr || contNr == depotContNr2)
		{
			if (cont->flagOnOff)
				openContNr++;
			deleteAndNull(cont);
			continue;
		}
		if (cont->flagOnOff)
		{
			openContNr++;
			for (int itemNr = 0; itemNr < cont->itemsInside; itemNr++)
			{
				CTibiaItem* item = (CTibiaItem*)(cont->items.GetAt(itemNr));
				outObjectIDs.Add(item->objectId);
			}
		}
		deleteAndNull(cont);
	}
	// Remove duplicate IDs and IDs not in sortedObjectIDs
	sortArray(outObjectIDs);
	unsigned int lastVal;
	if (outObjectIDs.GetSize() > 0)
		lastVal = ~outObjectIDs.GetAt(outObjectIDs.GetSize() - 1);                       //ensure lastVal is different
	for (int i = outObjectIDs.GetSize() - 1; i >= 0; i--)
	{
		if (lastVal == outObjectIDs.GetAt(i))
		{
			outObjectIDs.RemoveAt(i);
		}
		else
		{
			lastVal = outObjectIDs.GetAt(i);
			if (!listAll)
				if (binarySearch(lastVal, sortedObjectIDs) == -1)
					outObjectIDs.RemoveAt(i);
		}
	}
}

/**
 * We are nearby a depot, so do the depositing.
 */
void depotDeposit(CConfigData *config)
{
	CMemReader& reader = CMemReader::getMemReader();
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	int depotContNr           = -1;
	int depotContNr2          = -1;
	/*
	 * Now we must do:
	 * 1. locate depot chest (optional)
	 * 2. open depot chest and find place for item (optional)
	 * 3. deposit items
	 */
	//In Tibia 9.40 depot container changed to having no slots available in the "Locker"
	if (!config->depotDropInsteadOfDeposit)
	{
		globalAutoAttackStateDepot = CToolAutoAttackStateDepot_opening;
		for (int x = -1; x <= 1; x++)
		{
			for (int y = -1; y <= 1; y++)
			{
				if (x || y)
				{
					if (CTibiaMap::getTibiaMap().getPointType(self.x + x, self.y + y, self.z) == MAP_POINT_TYPE_DEPOT)//is depot spot
					{
						depotContNr = depotDepositOpenChest(self.x + x, self.y + y, self.z);
						if (depotContNr != -1)
						{
							x = 999;
							y = 999;
						}                                   // exit loop
					}
				}
			}
		}
		if (depotContNr == -1)
		{
			// can't open depot chest ?!?!?
			// so cancel depositing and proceed to walking
			globalAutoAttackStateDepot = CToolAutoAttackStateDepot_walking;
			return;
		}

		depotContNr2 = reader.findNextClosedContainer(depotContNr);
	}

	// do the depositing
	globalAutoAttackStateDepot = CToolAutoAttackStateDepot_depositing;

	// High Priority Task
	CVariableStore::setVariable("walking_control", "depotwalker");
	CVariableStore::setVariable("walking_priority", "9");
	CUIntArray alreadyManagedList; //save itemIDs for later use
	for (int i = 0; i < MAX_DEPOTTRIGGERCOUNT && strlen(config->depotTrigger[i].itemName); i++)
	{
		int objectToMove = CTibiaItem::getItemId(config->depotTrigger[i].itemName);
		if (objectToMove)
		{
			alreadyManagedList.Add(objectToMove);
			int contNr;
			int totalQty = countAllItemsOfType(objectToMove, depotContNr, depotContNr2);

			if (config->depotTrigger[i].when > config->depotTrigger[i].remain && totalQty > config->depotTrigger[i].remain)
			{
				// deposit to depot
				int openContNr  = 0;
				int openContMax = reader.readOpenContainerCount();
				for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
				{
					if (contNr == depotContNr || contNr == depotContNr2)
						continue;                                          //only scan through carried containers
					CTibiaContainer *cont = reader.readContainer(contNr);

					if (cont->flagOnOff)
					{
						openContNr++;
						int itemNr;
						for (itemNr = cont->itemsInside - 1; itemNr >= 0; itemNr--)
						{
							CTibiaContainer *contCheck = reader.readContainer(contNr);
							if (!contCheck->flagOnOff)
							{
								deleteAndNull(contCheck);
								deleteAndNull(cont);
								goto DPfinish;
							}
							delete contCheck;
							CTibiaItem *item = (CTibiaItem *)cont->items.GetAt(itemNr);
							if (item->objectId == objectToMove)
							{
								int itemQty = item->quantity ? item->quantity : 1;
								if (totalQty - itemQty < config->depotTrigger[i].remain)
									itemQty = totalQty - config->depotTrigger[i].remain;
								if (itemQty > 0)
								{
									if (!config->depotDropInsteadOfDeposit)
									{
										// move to the depot chest
										depotDepositMoveToChest(objectToMove, contNr, item->pos, itemQty, depotContNr, depotContNr2);
									}
									else
									{
										// move onto the floor
										CTibiaCharacter selftmp;
										reader.readSelfCharacter(&selftmp);
										CPackSender::moveObjectFromContainerToFloor(objectToMove, 0x40 + contNr, item->pos, self.x, self.y, self.z, itemQty ? itemQty : 1);
										CModuleUtil::waitForCapsChange(selftmp.cap);
										Sleep(CModuleUtil::randomFormula(300, 100));
									}
								}
								totalQty -= itemQty;
							}
						}
					}
					deleteAndNull(cont);
				}
			}
			if (config->depotTrigger[i].when<config->depotTrigger[i].remain&& !config->depotDropInsteadOfDeposit && config->depotTrigger[i].remain > totalQty)
			{
				// pickup from depot
				int qtyToPickup = config->depotTrigger[i].remain - totalQty;

				CTibiaContainer *depotChest = reader.readContainer(depotContNr);
				int itemNr;

				if (!depotChest->flagOnOff)
				{
					// shouldn't happen...
					deleteAndNull(depotChest);
					goto DPfinish;
				}
				/**
				 * If main depot container is open then we can try to take our item from it (to avoid
				 * massive close/open of containers)
				 */
				for (itemNr = depotChest->itemsInside - 1; itemNr >= 0 && qtyToPickup > 0; itemNr--)
				{
					CTibiaItem *item = ((CTibiaItem *)depotChest->items.GetAt(itemNr));
					if (item->objectId == objectToMove)
					{
						int qty = min(qtyToPickup, item->quantity ? item->quantity : 1);
						qtyToPickup -= depotDepositTakeFromChest(objectToMove, depotContNr, itemNr, qty, depotContNr, depotContNr2);
					}
				}
				deleteAndNull(depotChest);

				//Try the same for currently open sub container
				CTibiaContainer *subCont = reader.readContainer(depotContNr2);
				if (subCont->flagOnOff)
				{
					for (itemNr = subCont->itemsInside - 1; itemNr >= 0 && qtyToPickup > 0; itemNr--)
					{
						CTibiaItem *item = ((CTibiaItem *)subCont->items.GetAt(itemNr));
						if (item->objectId == objectToMove)
						{
							int qty = min(qtyToPickup, item->quantity ? item->quantity : 1);
							qtyToPickup -= depotDepositTakeFromChest(objectToMove, depotContNr2, itemNr, qty, depotContNr, depotContNr2);
						}
					}
				}
				deleteAndNull(subCont);

				depotChest = reader.readContainer(depotContNr);
				/**
				 * Scan through all containers in the depot chest to look for some matching one
				 */
				int count = depotChest->itemsInside;
				for (int itemNrChest = 0; itemNrChest < count && qtyToPickup > 0; itemNrChest++)
				{
					CTibiaItem *item = (CTibiaItem*)depotChest->items.GetAt(itemNrChest);
					if (CTileReader::getTileReader().getTile(item->objectId)->isContainer)
					{
						// that object is a container, so open it
						subCont = reader.readContainer(depotContNr2);
						if (subCont->flagOnOff)
						{
							CPackSender::closeContainer(depotContNr2);
							CModuleUtil::waitForOpenContainer(depotContNr2, false);
						}
						deleteAndNull(subCont);
						CPackSender::openContainerFromContainer(item->objectId, 0x40 + depotContNr, itemNrChest, depotContNr2);
						CModuleUtil::waitForOpenContainer(depotContNr2, true);
						subCont = reader.readContainer(depotContNr2);
						if (subCont->flagOnOff)
						{
							for (itemNr = subCont->itemsInside - 1; itemNr >= 0 && qtyToPickup > 0; itemNr--)
							{
								CTibiaItem *item = ((CTibiaItem *)subCont->items.GetAt(itemNr));
								if (item->objectId == objectToMove)
								{
									int qty = min(qtyToPickup, item->quantity ? item->quantity : 1);
									qtyToPickup -= depotDepositTakeFromChest(objectToMove, depotContNr2, itemNr, qty, depotContNr, depotContNr2);
								}
							}
						}
						deleteAndNull(subCont);
					}
				}

				deleteAndNull(depotChest);
			} // if pickup from depot
		}
	}
DPfinish:
	if (config->depositLootedItemList)
	{
		//deposit all of every looted item to save from having to add them individually to the list
		CUIntArray* lootedItems = CTibiaItem::getLootItemIdArrayPtr();
		CUIntArray depositList;
		for (int i = 0; i < lootedItems->GetSize(); i++)
		{
			int itemId = lootedItems->GetAt(i);
			if (findInIntArray(itemId, alreadyManagedList) == -1 && findInIntArray(itemId, depositList) == -1)
				depositList.Add(itemId);
		}
		sortArray(depositList);
		CUIntArray lootableItems;
		listAllItemsInContainers(lootableItems, lootableItems, depotContNr, depotContNr2);
		listAllItemsInContainers(depositList, lootableItems, depotContNr, depotContNr2);
		size_t lootableItemCount = lootableItems.GetSize();
		for (unsigned int i = 0; i < lootableItemCount; i++)
		{
			int objectToMove = lootableItems.GetAt(i);
			int contNr;
			int totalQty = countAllItemsOfType(objectToMove, depotContNr, depotContNr2);
			if (totalQty)
			{
				// deposit to depot
				int qtyToMove   = totalQty;
				int openContNr  = 0;
				int openContMax = reader.readOpenContainerCount();
				for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
				{
					CTibiaContainer *cont = reader.readContainer(contNr);
					if (contNr == depotContNr || contNr == depotContNr2)
					{
						if (cont->flagOnOff)
							openContNr++;
						deleteAndNull(cont);
						continue;
					}

					if (cont->flagOnOff)
					{
						openContNr++;
						int itemNr;
						for (itemNr = cont->itemsInside - 1; itemNr >= 0; itemNr--)
						{
							CTibiaContainer *contCheck = reader.readContainer(contNr);
							if (!contCheck->flagOnOff)
							{
								deleteAndNull(contCheck);
								deleteAndNull(cont);
								goto DPfinish2;
							}
							delete contCheck;
							CTibiaItem *item = (CTibiaItem *)cont->items.GetAt(itemNr);
							if (item->objectId == objectToMove)
							{
								int itemQty = item->quantity ? item->quantity : 1;
								if (itemQty > 0)
								{
									if (!config->depotDropInsteadOfDeposit)
									{
										// move to the depot chest
										depotDepositMoveToChest(objectToMove, contNr, item->pos, itemQty, depotContNr, depotContNr2);
									}
									else
									{
										// move onto the floor
										CTibiaCharacter selftmp;
										reader.readSelfCharacter(&selftmp);
										CPackSender::moveObjectFromContainerToFloor(objectToMove, 0x40 + contNr, item->pos, self.x, self.y, self.z, itemQty ? itemQty : 1);
										CModuleUtil::waitForCapsChange(selftmp.cap);
										Sleep(CModuleUtil::randomFormula(300, 100));
									}
								}
								totalQty -= itemQty;
							}
						}
					}
					deleteAndNull(cont);
				}
			}
		}
	}
DPfinish2:

	CVariableStore::setVariable("walking_control", "depotwalker");
	CVariableStore::setVariable("walking_priority", config->depotModPriorityStr);
	// all is finished :) - we can go back to the hunting area
	if (!config->depotDropInsteadOfDeposit)
	{
		CTibiaContainer *cont = reader.readContainer(depotContNr2);
		if (cont->flagOnOff)
		{
			CPackSender::closeContainer(depotContNr2);
			CModuleUtil::waitForOpenContainer(depotContNr2, false);
			Sleep(CModuleUtil::randomFormula(300, 100));
		}
		delete cont;
		cont = reader.readContainer(depotContNr);
		if (cont->flagOnOff)
		{
			CPackSender::closeContainer(depotContNr);
			CModuleUtil::waitForOpenContainer(depotContNr, false);
		}
		delete cont;
	}
	globalAutoAttackStateDepot = CToolAutoAttackStateDepot_notRunning;
	depotX                     = depotY = depotZ = 0;
	waypointTargetX            = waypointTargetY = waypointTargetZ = 0;
	persistentShouldGo         = 0;
}

/**
 * Make sure that some item of type objectId is in the location (e.g. hand).
 */
int ensureItemInPlace(int outputDebug, int location, int locationAddress, int objectId)
{
	CMemReader& reader = CMemReader::getMemReader();

	
	CTibiaItem *itemSlot       = reader.readItem(locationAddress);

	int hasNeededItem = 0;
	if (itemSlot->objectId == objectId)
	{
		// everything's ok - no changes
		deleteAndNull(itemSlot);
		return 1;
	}

	globalAutoAttackStateTraining = CToolAutoAttackStateTraining_switchingWeapon;
	for (int i = 0; i < 2 && itemSlot->objectId != objectId; i++)
	{
		if (outputDebug)
			registerDebug("Next moving item in ensureItemInPlace");

		// Search for the item that should occupy the slot
		CUIntArray itemsAccepted;
		int contNr;
		itemsAccepted.Add(objectId);
		int openContNr  = 0;
		int openContMax = reader.readOpenContainerCount();
		for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
		{
			CTibiaItem *itemWear = CModuleUtil::lookupItem(contNr, &itemsAccepted);
			if (itemWear->objectId)
			{
				CTibiaContainer *cont = reader.readContainer(contNr);
				if (cont->flagOnOff)
				{
					openContNr++;
					if (cont->itemsInside >= cont->size && itemSlot->objectId != 0 || time(NULL) - containerTimes[contNr] < CONTAINER_TIME_CUTOFF)
					{
						// container with desired item is full or not a carried container and we need a place to put the item occupying the slot
						int hasSpace    = 0;
						int openContNr  = 0;
						int openContMax = reader.readOpenContainerCount();
						for (int contNrSpace = 0; contNrSpace < reader.m_memMaxContainers && openContNr < openContMax; contNrSpace++)
						{
							if  (time(NULL) - containerTimes[contNrSpace] < CONTAINER_TIME_CUTOFF)
								continue;                                                            //skip if not carried

							CTibiaContainer *contSpace = reader.readContainer(contNrSpace);
							if (contSpace->flagOnOff)
							{
								openContNr++;
								if (contSpace->itemsInside < contSpace->size)
								{
									// container has some free places
									CPackSender::moveObjectBetweenContainers(itemSlot->objectId, location, 0, 0x40 + contNrSpace, contSpace->size - 1, itemSlot->quantity ? itemSlot->quantity : 1);
									if (!CModuleUtil::waitForItemChange(locationAddress, itemSlot->objectId))
										//Failed to move object out of hand
										return 0;
									Sleep(CModuleUtil::randomFormula(300, 100));
									hasSpace    = 1;
									contNrSpace = 10000; // stop the loop
								}
							}
							deleteAndNull(contSpace);
						}
						if (!hasSpace)
						{
							// means: we have no place in any container
							if (time(NULL) - lastTAMessageTm > taMessageDelay)
							{
								lastTAMessageTm = time(NULL);
								CPackSender::sendTAMessage("I've an invalid weapon in hand, and I can't move it anywhere!");
							}
							deleteAndNull(itemSlot);
							deleteAndNull(itemWear);
							deleteAndNull(cont);
							return 0;
						}
						CTibiaItem *item = reader.readItem(locationAddress);
						if (item->objectId != 0)
						{
							//if slot is not empty then we failed at moving the item
							delete item;
							return 0;
						}
						delete item;
					}
					//Assert: we can simply switch the item to wear and the item in the slot OR the slot is empty
					CPackSender::moveObjectBetweenContainers(itemWear->objectId, 0x40 + contNr, itemWear->pos, location, 0, itemWear->quantity ? itemWear->quantity : 1);
					CModuleUtil::waitForItemChange(locationAddress, itemSlot->objectId);
					Sleep(CModuleUtil::randomFormula(300, 100));
					contNr        = 10000; // stop the loop
					hasNeededItem = 1;
				}
				deleteAndNull(cont);
			}
			deleteAndNull(itemWear);
		}
		if (!hasNeededItem)
		{
			//means: we cannot find the needed item
			if (time(NULL) - lastTAMessageTm > taMessageDelay)
			{
				lastTAMessageTm = time(NULL);
				CPackSender::sendTAMessage("I can't find the training/fight weapon in any container!");
			}
			deleteAndNull(itemSlot);
			return 0;
		}
		CModuleUtil::waitForItemChange(locationAddress, itemSlot->objectId);
		deleteAndNull(itemSlot);
		itemSlot = reader.readItem(locationAddress);
	}
	deleteAndNull(itemSlot);
	return 1;
}

/**
 * Check the training related things, like:
 * 1. Check whether we should be in fight or training mode (and switch weapons).
 * 2. Check whether we should be full attack/def/dont touch (if blood control is active).
 * 3. Switch weapon if needed.
 */
void trainingCheck(CConfigData *config, int currentlyAttackedCreatureNr, int alienFound, int attackingCreatures, time_t lastAttackedCreatureBloodHit, int *attackMode)
{
	CMemReader& reader = CMemReader::getMemReader();

	
	int weaponHandAddress      = reader.m_memAddressLeftHand;
	int weaponHand             = 6;

	if (config->trainingActivate)
	{
		char buf[256];
		sprintf(buf, "Training check: attackedCr=%d, alien=%d, #attacking=%d, lastHpDrop=%d, attackMode=%d", currentlyAttackedCreatureNr, alienFound, attackingCreatures, time(NULL) - lastAttackedCreatureBloodHit, *attackMode);
		if (config->debug)
			registerDebug(buf);

		if (config->fightWhenAlien && !alienFound && !attackingCreatures && currentlyAttackedCreatureNr == -1)
		{
			//if (config->debug) registerDebug("Training: non-training mode.");
			if (!ensureItemInPlace(config->debug, weaponHand, weaponHandAddress, config->weaponFight))
			{
				if (config->debug)
					registerDebug("Training: Failed to equip fight weapon for non-training mode.");
				ensureItemInPlace(config->debug, weaponHand, weaponHandAddress, config->weaponTrain);
			}
			globalAutoAttackStateTraining = CToolAutoAttackStateTraining_notRunning;
			return;
		}

		int canTrain = 1;
		if (config->fightWhenAlien && alienFound)
			canTrain = 0;
		if (config->fightWhenSurrounded && attackingCreatures > 2)
			canTrain = 0;

		static time_t lastFightModeTm = 0;

		if (canTrain && time(NULL) - lastFightModeTm > 5)//and has been more than 5 seconds since fight mode
		{
			lastFightModeTm = 0;
			//if (config->debug) registerDebug("Training: Training mode.");
			if (!ensureItemInPlace(config->debug, weaponHand, weaponHandAddress, config->weaponTrain))
			{
				registerDebug("Training: Failed to switch to training weapon.");
				ensureItemInPlace(config->debug, weaponHand, weaponHandAddress, config->weaponFight);
			}
			if (config->trainingMode)
				*attackMode = config->trainingMode;                //0-blank, 1-full atk, 3-full def

			globalAutoAttackStateTraining = CToolAutoAttackStateTraining_training;
			if (config->bloodHit)
			{
				sprintf(buf, "Training: inside blood hit control (%d-%d=%d)", time(NULL), lastAttackedCreatureBloodHit, time(NULL) - lastAttackedCreatureBloodHit);
				if (config->debug)
					registerDebug(buf);
				if (time(NULL) - lastAttackedCreatureBloodHit > 20)
				{
					// if 20s has passed since last blood hit, then we do harder attack mode
					// otherwise we do selected attack mode
					*attackMode                   = max(*attackMode - 1, 1);
					globalAutoAttackStateTraining = CToolAutoAttackStateTraining_trainingLessDef;
				}
			}
		}
		else
		{
			if (!lastFightModeTm)
				lastFightModeTm = time(NULL);
			//if (config->debug) registerDebug("Training: Fight mode.");
			if (!ensureItemInPlace(config->debug, weaponHand, weaponHandAddress, config->weaponFight))
			{
				if (config->debug)
					registerDebug("Training: Failed to equip fight for fight mode.");
				ensureItemInPlace(config->debug, weaponHand, weaponHandAddress, config->weaponTrain);
			}
			globalAutoAttackStateTraining = CToolAutoAttackStateTraining_fighting;
		}
	}
}

void dropItemsFromContainer(int contNr, int x, int y, int z, CUIntArray* dropItems = NULL)
{
	CMemReader& reader = CMemReader::getMemReader();

	CTibiaContainer *dropCont = reader.readContainer(contNr);
	if (dropCont->flagOnOff)
	{
		int itemNr;
		for (itemNr = dropCont->itemsInside - 1; itemNr >= 0; itemNr--)
		{
			CTibiaItem *lootItem = (CTibiaItem *)dropCont->items.GetAt(itemNr);
			for (int listNr = 0; dropItems == NULL || listNr < dropItems->GetSize(); listNr++)
			{
				if (dropItems == NULL || dropItems->GetAt(listNr) == lootItem->objectId)
				{
					int passx = x, passy = y;
					CModuleUtil::findFreeSpace(passx, passy, z);//changes x and y
					CPackSender::moveObjectFromContainerToFloor(lootItem->objectId, 0x40 + contNr, lootItem->pos, passx, passy, z, lootItem->quantity ? lootItem->quantity : 1);
					CModuleUtil::waitForItemsInsideChange(contNr, itemNr + 1);
					Sleep(CModuleUtil::randomFormula(500, 200));
					break;
				}
			}
		}
	}
	deleteAndNull(dropCont);
}

/////////////////////////////////////////////////////////////////////////////
int droppedLootCheck(CConfigData *config, CUIntArray& lootedArr, int radius = 7)
{
	char buf[256];
	CMemReader& reader = CMemReader::getMemReader();	
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);

	if (config->lootFromFloor && self.cap > config->capacityLimit)
	{
		if (config->debug)
			registerDebug("Entering 'loot from floor' area");

		int freeContNr  = -1;
		int freeContPos = -1;

		// lookup a free place in any container
		int contNr;
		int openContNr  = 0;
		int openContMax = reader.readOpenContainerCount();
		for (contNr = 0; contNr < reader.m_memMaxContainers && openContNr < openContMax; contNr++)
		{
			CTibiaContainer *cont = reader.readContainer(contNr);
			if (cont->flagOnOff)
			{
				openContNr++;
				if (cont->itemsInside < cont->size)
				{
					freeContNr  = contNr;
					freeContPos = cont->size - 1;
					deleteAndNull(cont);
					break;
				}
			}
			deleteAndNull(cont);
		}

		if (freeContNr == -1)
		{
			if (config->debug)
				registerDebug("Loot from floor: no free container found");
			// no free place in container found - exit
			return 0;
		}
		else
		{
			sprintf(buf, "Container Found: Cont %d Slot%d)", freeContNr, freeContPos);
			if (config->debug)
				registerDebug(buf);
		}


		int foundLootedObjectId = 0;
		int isTopItem           = 0;
		int x                   = 0, y = 0;
		int xSwitch             = 0;
		int ySwitch             = 0;
		int count               = 0;
		while (x != radius + 1 && y != radius + 1)
		{
			//manage x and y coords for spiraling
			if (xSwitch == 0 && ySwitch == 0)
			{
				xSwitch = 1;
			}
			else if (x == y && x >= 0 && xSwitch == 1 && ySwitch == 0)
			{
				x++;
				xSwitch = 0;
				ySwitch = -1;
			}
			else if (!xSwitch && !(x == y && x >= 0) && abs(x) == abs(y))
			{
				xSwitch = ySwitch;
				ySwitch = 0;
				x      += xSwitch;
				y      += ySwitch;
			}
			else if (!ySwitch && abs(x) == abs(y))
			{
				ySwitch = -xSwitch;
				xSwitch = 0;
				x      += xSwitch;
				y      += ySwitch;
			}
			else
			{
				x += xSwitch;
				y += ySwitch;
			}
			//char buf[1111];
			//sprintf(buf,"looter spiral %d %d",x,y);
			//CPackSender::sendTAMessage(buf);

			if (abs(x) > 7 || abs(y) > 5)
				continue;

			int stackCount = reader.mapGetPointItemsCount(point(x, y, 0));
			if (stackCount == 0)
				continue;

			foundLootedObjectId = 0;
			isTopItem           = 0;

			//horribly inefficient for 165 tiles, is performed later //int topPos=reader.itemOnTopIndex(x,y);
			//top of Tibia's stack starts somewhere in the middle of the array
			for (int pos = 0; pos < stackCount; pos++)
			{
				count++;
				int tileId = reader.mapGetPointItemId(point(x, y, 0), pos);
				int ind    = binarySearch(tileId, lootedArr);
				if (ind != -1)
				{
					foundLootedObjectId = (lootedArr)[ind];
					isTopItem           = pos == reader.itemOnTopIndex(x, y);
					break;
				}
			}

			if (!foundLootedObjectId)
				continue;
			//sprintf(buf,"test:%d,%d,%d,%d,(%d,%d)",f1,f2,reader.getItemIndex(x,y,f1),ModuleUtil::getItemIndex(x,y,f2),x,y);
			//AfxMessageBox(buf);

			int shouldStandOn = 0;//reader.mapGetPointItemsCount(point(x,y,0))>=10;

			//Walk To Square
			if (abs(x) > 1 || abs(y) > 1 || shouldStandOn && !(x == 0 && y == 0))
			{
				uint8_t path[15];
				int pathSize = 0;
				memset(path, 0x00, sizeof(path[0]) * 15);
				if (config->debug)
				{
					sprintf(buf, "findPathOnMap: loot from floor (%d, %d) %x", x, y, foundLootedObjectId);
					registerDebug(buf);
				}
				CTibiaCharacter self2;
				reader.readSelfCharacter(&self2);
				struct point destPoint = CModuleUtil::findPathOnMap(self2.x, self2.y, self2.z, self.x + x, self.y + y, self.z, 0, path, shouldStandOn ? 0 : 1);
				for (pathSize = 0; pathSize < 15 && path[pathSize]; pathSize++)
				{
				}

				//continue if farther than 10 spaces or path does not include enough info to get there(meaning floor change)
				if (pathSize >= 10 || pathSize < x + y - 3)
				{
					continue;
				}

				sprintf(buf, "Loot from floor: item (%d,%d,%d)", x, y, pathSize);
				if (config->debug)
					registerDebug(buf);
				sprintf(buf, "Path[0]: %d\tpathSize: %d", path[0], pathSize);
				if (config->debug)
					registerDebug(buf);

				if (pathSize)
				{
					//CPackSender::stopAll();
					//sprintf(buf, "Walking attempt: %d\nPath Size: %d", walkItem, pathSize);
					//AfxMessageBox(buf);
					CModuleUtil::executeWalk(self2.x, self2.y, self2.z, path);
					// wait for reaching final point
					CModuleUtil::waitToStandOnSquare(destPoint.x, destPoint.y);
				}
				else
				{
					sprintf(buf, "Loot from floor: aiks, no path found (%d,%d,%d)->(%d,%d,%d)!", self2.x, self2.y, self2.z, self.x + x, self.y + y, self.z);
					if (config->debug)
						registerDebug(buf);
					continue;
				}
			}
			CTibiaCharacter self2;
			reader.readSelfCharacter(&self2);
			int itemX              = self.x + x - self2.x;
			int itemY              = self.y + y - self2.y;

			sprintf(buf, "Loot from floor: found looted object id=0x%x at %d,%d", foundLootedObjectId, itemX, itemY);
			if (config->debug)
				registerDebug(buf);

			//If failed to get within 1 square, let cavebot take over again
			if (!(abs(itemX) <= 1 && abs(itemY) <= 1 && self2.z == self.z))
				break;

			//Uncover Item
			if (!isTopItem)
			{
				int offsetX, offsetY;
				int foundSpace = 0;
				// Need to find free square to dump items
				for (offsetX = -1; offsetX <= 1; offsetX++)
				{
					for (offsetY = -1; offsetY <= 1; offsetY++)
					{
						// double loop break!
						if (self2.x + offsetX != self.x + x && self2.y + offsetY != self.y + y && CTibiaMap::getTibiaMap().isPointAvailable(self2.x + offsetX, self2.y + offsetY, self2.z) && reader.mapGetPointItemsCount(point(offsetX, offsetY, 0)) < 9)
						{
							//force loop break;
							foundSpace = 1;
							goto exitLoop;
						}
					}
				}
				if (!(self2.x == self.x + x && self2.y == self.y + y))
				{
					offsetX    = 0;
					offsetY    = 0;
					foundSpace = 1;
				}
				exitLoop :
				sprintf(buf, "Loot from floor: using (%d,%d) as dropout offset/item=%d", offsetX, offsetY, foundLootedObjectId);
				if (config->debug)
					registerDebug(buf);
				// do it only when drop place is found
				if (foundSpace)
				{
					int itemInd = reader.getItemIndex(itemX, itemY, foundLootedObjectId);
					int topInd  = reader.itemOnTopIndex(itemX, itemY);
					int count   = reader.mapGetPointItemsCount(point(itemX, itemY, 0));
					sprintf(buf, "Loot from floor vars: itemInd %d,topInd %d,count %d,(%d,%d)=%d", itemInd, topInd, count, itemX, itemY, foundLootedObjectId);
					if (config->debug)
						registerDebug(buf);

					int itemIds[10];
					int itemQtys[10];
					int numItems = 0;
					for (int ind = topInd; ind < itemInd; ind++)
					{
						itemIds[numItems]  = reader.mapGetPointItemId(point(itemX, itemY, 0), ind);
						itemQtys[numItems] = reader.mapGetPointItemExtraInfo(point(itemX, itemY, 0), ind, 1);
						numItems++;
					}
					for (int i = 0; i < numItems; i++)
					{
						CPackSender::moveObjectFromFloorToFloor(itemIds[i], self.x + x, self.y + y, self.z, self2.x + offsetX, self2.y + offsetY, self2.z, itemQtys[i] ? itemQtys[i] : 1);
						Sleep(CModuleUtil::randomFormula(400, 200));
					}
				}
				else
				{
					if (config->debug)
						registerDebug("Loot from floor: bad luck - no place to throw junk.");
					continue;
				}
			}

			//Take item
			if (reader.isItemOnTop(itemX, itemY, foundLootedObjectId))
			{
				if (config->debug)
					registerDebug("Loot from floor: picking up item");
				int qty = reader.itemOnTopQty(itemX, itemY);
				CPackSender::moveObjectFromFloorToContainer(foundLootedObjectId, self.x + x, self.y + y, self.z, 0x40 + freeContNr, freeContPos, qty ? qty : 1);
				Sleep(CModuleUtil::randomFormula(400, 200));
				if (CModuleUtil::waitForCapsChange(self.cap))
					return 1;
				else
					return 0; //Item could not be picked up for some reason.
			}
			else
			{
				if (config->debug)
					registerDebug("Loot from floor: tough luck - item is not on top. Please wait and try again.");
			}
			//If gotten this far, let cavebot takeover for a bit
			break;
		}
	} // if (config->lootFromFloor&&self.cap>config->capacityLimit) {
	return 0;
}

void fireRunesAgainstCreature(CConfigData *config, int creatureId)
{
	CMemReader& reader = CMemReader::getMemReader();
	int contNr, realContNr;
	CUIntArray acceptedItems;
	CTibiaItem *rune = new CTibiaItem();

	if (config->debug)
		registerDebug("Firing runes at alien enemy");

	acceptedItems.RemoveAll();
	acceptedItems.Add(CTibiaItem::getValueForConst("runeSD"));
	for (contNr = 0; contNr < reader.m_memMaxContainers && !rune->objectId; contNr++)
	{
		delete rune;
		rune = CModuleUtil::lookupItem(contNr, &acceptedItems), realContNr = contNr;
	}


	acceptedItems.RemoveAll();
	acceptedItems.Add(CTibiaItem::getValueForConst("runeExplo"));
	for (contNr = 0; contNr < reader.m_memMaxContainers && !rune->objectId; contNr++)
	{
		delete rune;
		rune = CModuleUtil::lookupItem(contNr, &acceptedItems), realContNr = contNr;
	}

	acceptedItems.RemoveAll();
	acceptedItems.Add(CTibiaItem::getValueForConst("runeHMM"));
	for (contNr = 0; contNr < reader.m_memMaxContainers && !rune->objectId; contNr++)
	{
		delete rune;
		rune = CModuleUtil::lookupItem(contNr, &acceptedItems), realContNr = contNr;
	}

	acceptedItems.RemoveAll();
	acceptedItems.Add(CTibiaItem::getValueForConst("runeGFB"));
	for (contNr = 0; contNr < reader.m_memMaxContainers && !rune->objectId; contNr++)
	{
		delete rune;
		rune = CModuleUtil::lookupItem(contNr, &acceptedItems), realContNr = contNr;
	}

	if (rune->objectId)
	{
		if (config->debug)
			registerDebug("Rune to fire found!");
		CPackSender::castRuneAgainstCreature(0x40 + realContNr, rune->pos, rune->objectId, creatureId, 2);
	}
	delete rune;
} // fireRunesAgainstCreature()

/////////////////////////////////////////////////////////////////////////////
// shared memory initalisation
void shMemInit(CSharedMemory *pMem)
{
	int pid = GetCurrentProcessId();
	char varName[128];
	sprintf(varName, "mod_cavebot_%d", pid);
	// this means shmem initialisation failed
	if (!pMem || !pMem->IsCreated())
		return;
	pMem->AddDwordValue(varName, 0);
}

// return monster priority (order on the monster list)
// monsters out of the list have priority (0)
// highest priority have higher numbers
int getAttackPriority(CConfigData *config, char *monsterName)
{
	char buf[64];
	int monstListNr;
	int i, len;
	strcpy(buf, monsterName);
	// first filter out [] created by creature info
	for (i = 0, len = strlen(buf); i < len; i++)
	{
		if (buf[i] == '[')
		{
			buf[i] = '\0';
			break;
		}
	}
	for (monstListNr = 0; monstListNr < config->monsterCount && monstListNr < MAX_MONSTERLISTCOUNT; monstListNr++)
	{
		if (!_strcmpi(config->monsterList[monstListNr], buf))
			return config->monsterCount - monstListNr;
	}

	return 0;
}

int isInHalfSleep()
{
	CMemReader& reader = CMemReader::getMemReader();
	const char *var = CVariableStore::getVariable("walking_control");
	return strcmp(var, "cavebot") != 0 && strcmp(var, "depotwalker") != 0;
}

int isInFullSleep()
{
	CMemReader& reader = CMemReader::getMemReader();
	const char *var = CVariableStore::getVariable("cavebot_fullsleep");
	if (strcmp(var, "true"))
		return 0;
	else
		return 1;
}

int isLooterDone(CConfigData *config)
{
	if (!config->lootWhileKill)
		return 1;
	CMemReader& reader = CMemReader::getMemReader();
	const char *var = CVariableStore::getVariable("autolooterTm");
	//check if looter is done or time given to looter is up
	if (!strcmp(var, "") || time(NULL) > autolooterTm || abs(time(NULL) - autolooterTm) > 3600 * 24)
		return 1;
	return 0;
}

int shouldLoot()
{
	CMemReader& reader = CMemReader::getMemReader();
	const char *var = CVariableStore::getVariable("autolooterTm");
	int tmp;
	if (sscanf(var, "%d %d", &tmp, &tmp) == 2)
		return 1;
	return 0;
}

//returns 1 if needed to resend attack info
//returns 0 if same creature already being attacked
int AttackCreature(CConfigData *config, int id)
{
	CMemReader& reader = CMemReader::getMemReader();

	CTibiaCharacter attackedCh;
	int oldId                   = reader.getAttackedCreature();
	if (reader.getCharacterByTibiaId(&attackedCh, id))
	{
		if (oldId != id)
		{
			reader.cancelAttackCoords();
			Sleep(200);
			CPackSender::attack(id);
			if (id)
				lastSendAttackTm = time(NULL);
			currentPosTM = time(NULL);
			if (config->debug && id)
			{
				char buf[128];
				sprintf(buf, "used to be=%d, set to=%d, now attacking=%d,hp=%d", oldId, id, reader.getAttackedCreature(), attackedCh.hpPercLeft);
				registerDebug(buf);
			}
			return 1;
		}
	}
	return 0;
}

//returns the smallest radius of a square centred at x1,y1 and covers x2,y2
//same equation as finding distance for rods or wands
int maxDist(int x1, int y1, int x2, int y2)
{
	return max(abs(x2 - x1), abs(y2 - y1));
}

//returns the smallest number of steps needed to stand beside x2,y2 starting from x1,y1
//same distance ideally travelled by "Auto Follow"
int taxiDist(int x1, int y1, int x2, int y2)
{
	return abs(x2 - x1) + abs(y2 - y1) - 2 + (x1 == x2) + (y1 == y2);
}

// will eventually use the minimap to determine if path is ok
int canGetToPoint(int X, int Y, int Z)
{
	/*
	CMemReader& reader = CMemReader::getMemReader();
	uint8_t path[15];
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	CModuleUtil::findPathOnMap(self.x,self.y,self.z,actualTargetX,actualTargetY,actualTargetZ,0,path);
	CPackSenderProxy sender;
	char buf[111];
	sprintf(buf,"%d,%d,%d,%d",path&&path[0]&&!path[8],path,path[0],path[8]);
	CPackSender::sendTAMessage(buf);
	return path&&path[0]&&!path[14];*/
	return 1;
}

void SendAttackMode(int attack, int follow, int attLock, int PVPMode, int refreshCur = 0)
{
	//Ensures that only changes in the three values are sent, and only one at a time

	CMemReader& reader = CMemReader::getMemReader();
	static int curAttack  = reader.getPlayerModeAttackType();
	static int curFollow  = reader.getPlayerModeFollow();
	static int curAttLock = reader.getPlayerModeAttackPlayers();
	static int curPVPMode = reader.getPlayerModePVP();

	static int buttonAttack  = reader.getPlayerModeAttackType();
	static int buttonFollow  = reader.getPlayerModeFollow();
	static int buttonAttLock = reader.getPlayerModeAttackPlayers();
	static int buttonPVPMode = reader.getPlayerModePVP();

	if (refreshCur)
	{
		curAttack  = reader.getPlayerModeAttackType();
		curFollow  = reader.getPlayerModeFollow();
		curAttLock = reader.getPlayerModeAttackPlayers();
		curPVPMode = reader.getPlayerModePVP();
	}
	if (buttonAttack != reader.getPlayerModeAttackType())
	{
		curAttack    = reader.getPlayerModeAttackType();
		buttonAttack = reader.getPlayerModeAttackType();
	}
	if (buttonFollow != reader.getPlayerModeFollow())
	{
		curFollow    = reader.getPlayerModeFollow();
		buttonFollow = reader.getPlayerModeFollow();
	}
	if (buttonAttLock != reader.getPlayerModeAttackPlayers())
	{
		curAttLock    = reader.getPlayerModeAttackPlayers();
		buttonAttLock = reader.getPlayerModeAttackPlayers();
	}
	if (buttonPVPMode != reader.getPlayerModePVP())
	{
		curPVPMode    = reader.getPlayerModePVP();
		buttonPVPMode = reader.getPlayerModePVP();
	}

	if (attack == -1)
		attack = curAttack;
	if (follow == -1)
		follow = curFollow;
	if (attLock == -1)
		attLock = curAttLock;
	if (PVPMode == -1)
		PVPMode = curPVPMode;

	if (attack != curAttack)
	{
		CPackSender::attackMode(attack, curFollow, curAttLock, curPVPMode);
		//if going to send another attack mode change right away then sleep
		if (attLock != curAttLock || follow != curFollow || PVPMode != curPVPMode)
			Sleep(CModuleUtil::randomFormula(700, 500));
	}
	if (follow != curFollow)
	{
		CPackSender::attackMode(curAttack, follow, curAttLock, curPVPMode);
		//if going to send another attack mode change right away then sleep
		if (attLock != curAttLock || PVPMode != curPVPMode)
			Sleep(CModuleUtil::randomFormula(700, 500));
	}
	if (attLock != curAttLock)
	{
		CPackSender::attackMode(curAttack, curFollow, attLock, curPVPMode);
		//if going to send another attack mode change right away then sleep
		if (PVPMode != curPVPMode)
			Sleep(CModuleUtil::randomFormula(700, 500));
	}
	if (PVPMode != curPVPMode)
		CPackSender::attackMode(curAttack, curFollow, curAttLock, PVPMode);
	//char buf[111];
	//sprintf(buf,"Attack Set to: %d,%d",attack,follow);
	//CPackSender::sendTAMessage(buf);
	curAttack  = attack;
	curFollow  = follow;
	curAttLock = attLock;
	curPVPMode = PVPMode;
}

int messageThereIsNoWay(int creatureNr)
{
	if (creatureNr == -1)
		return 0;
	CMemReader& reader = CMemReader::getMemReader();
	CTibiaCharacter self;
	reader.readSelfCharacter(&self);
	int x = self.x;
	int y = self.y;

	for (int t = 0; t < 30; t++)
	{
		CIpcMessage mess;
		CTibiaCharacter creature;
		reader.readVisibleCreature(&creature, creatureNr);
		reader.readSelfCharacter(&self);
		if (CIPCBackPipe::readFromPipe(&mess, 1103))
		{
			return 1;
		}
		else if ((self.x != x || self.y != y) || taxiDist(self.x, self.y, creature.x, creature.y) == 0)
		{
			return 0;
		}
		Sleep(50);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Tool thread function

int toolThreadShouldStop = 0;
HANDLE toolThreadHandle;
HANDLE lootThreadHandle;
HANDLE queueThreadHandle;

//helper thread since adding to queue will still go through getGlobalVariable so the spellcaster can add to it.
//Looting corpses takes a long time and lootThreadProc may miss changes in autolooterTm, so this thread collects them
DWORD WINAPI queueThreadProc(LPVOID lpParam)
{
	CMemReader& reader = CMemReader::getMemReader();

	int a    = 0;
	int myID = queueThreadId;
	while (!toolThreadShouldStop && myID == queueThreadId)  //If cavebot starts new queueThreadProc then quit
	{
		Sleep(200);
		if (shouldLoot())
		{
			const char* var = CVariableStore::getVariable("autolooterTm");
			unsigned int endTime, tibiaId;
			sscanf(var, "%d %d", &tibiaId, &endTime);
			CVariableStore::setVariable("autolooterTm", "wait");

			CTibiaCharacter attackedCh;
			if (reader.getCharacterByTibiaId(&attackedCh, tibiaId))
			{
				CModuleUtil::waitForCreatureDisappear(attackedCh.nr);
				corpseQueue.Add(Corpse(attackedCh.x, attackedCh.y, attackedCh.z, GetTickCount()));
			}
		}
	}
	if (toolThreadShouldStop || !queueThreadId)  // only remove corpses from queue when stopping
	{
		while (corpseQueue.GetCount())
		{
			corpseQueue.Remove();
		}
	}
	queueThreadId = 0;
	return 0;
}

DWORD WINAPI lootThreadProc(LPVOID lpParam)
{
	CMemReader& reader = CMemReader::getMemReader();

	
	
	CConfigData *config        = (CConfigData *)lpParam;
	int myID                   = lootThreadId;
	//start helper thread since adding to queue will still go through getGlobalVariable so the spellcaster can add to it.

	while (!toolThreadShouldStop && myID == lootThreadId)
	{
		Sleep(100);
		while (corpseQueue.GetCount() && globalAutoAttackStateAttack != CToolAutoAttackStateAttack_macroPause)
		{
			Corpse corpseCh = corpseQueue.Remove();
			if (GetTickCount() - corpseCh.tod > 1000 * 60)
				continue;
			int lootContNr[3];
			lootContNr[0] = reader.findNextClosedContainer();
			lootContNr[1] = reader.findNextClosedContainer(lootContNr[0]);

			globalAutoAttackStateLoot = CToolAutoAttackStateLoot_opening;
			CTibiaCharacter self;
			CMemReader::getMemReader().readSelfCharacter(&self);
			int corpseId          = reader.itemOnTopCode(corpseCh.x - self.x, corpseCh.y - self.y);
			CTibiaTile *tile      = CTileReader::getTileReader().getTile(corpseId);
			if (corpseId && tile && tile->isContainer) //If there is no corpse ID, TA has "lost" the body. No sense in trying to open something that won't be there.
			{       // Open corpse to container, wait to get to corpse on ground and wait for open
				Sleep(CModuleUtil::randomFormula(200, 100));

				CPackSender::openContainerFromFloor(corpseId, corpseCh.x, corpseCh.y, corpseCh.z, lootContNr[0]);

				if (!CModuleUtil::waitToApproachSquare(corpseCh.x, corpseCh.y))
				{
					//(waitToApproachSquare returns false) => (corpse >1 sqm away and not reached yet), so try again
					CPackSender::openContainerFromFloor(corpseId, corpseCh.x, corpseCh.y, corpseCh.z, lootContNr[0]);
					CModuleUtil::waitToApproachSquare(corpseCh.x, corpseCh.y);
				}
				if (CModuleUtil::waitForOpenContainer(lootContNr[0], true))
				{
					if (config->lootInBags)
					{
						if (config->debug)
							registerDebug("Looter: Opening bag (second container)");
						CTibiaContainer *cont = reader.readContainer(lootContNr[0]);
						int itemNr;
						for (itemNr = 0; itemNr < cont->itemsInside; itemNr++)
						{
							CTibiaItem *insideItem = (CTibiaItem *)cont->items.GetAt(itemNr);
							CTibiaTile* tile       = CTileReader::getTileReader().getTile(insideItem->objectId);
							if (insideItem->objectId && tile && tile->isContainer)
							{
								globalAutoAttackStateLoot = CToolAutoAttackStateLoot_openingBag;
								Sleep(CModuleUtil::randomFormula(200, 100));
								CPackSender::openContainerFromContainer(insideItem->objectId, 0x40 + lootContNr[0], insideItem->pos, lootContNr[1]);
								if (!CModuleUtil::waitForOpenContainer(lootContNr[1], true))
									if (config->debug)
										registerDebug("Failed opening bag");
								break;
							}
						}
						deleteAndNull(cont);
					}
					CUIntArray acceptedItems;

					if (config->lootGp)
					{
						acceptedItems.Add(CTibiaItem::getValueForConst("GP"));
						acceptedItems.Add(CTibiaItem::getValueForConst("PlatinumCoin"));
						acceptedItems.Add(CTibiaItem::getValueForConst("CrystalCoin"));
					}
					if (config->lootWorms)
						acceptedItems.Add(CTibiaItem::getValueForConst("worms"));
					if (config->lootCustom)
						acceptedItems.Append(*CTibiaItem::getLootItemIdArrayPtr());
					if (config->lootFood)
						acceptedItems.Append(*CTibiaItem::getFoodIdArrayPtr());
					int lootTakeItem;
					for (int contNrInd = 0; contNrInd < 2; contNrInd++)// loot from first, then last if lootInBags
					{
						int contNr = lootContNr[contNrInd];

						CTibiaContainer *lootCont = reader.readContainer(contNr);
						if (lootCont->flagOnOff)
						{
							for (lootTakeItem = 0; lootTakeItem < 1; lootTakeItem++)
							{
								globalAutoAttackStateLoot = CToolAutoAttackStateLoot_moveing;
								if (self.cap > config->capacityLimit)
									CModuleUtil::lootItemFromContainer(contNr, &acceptedItems, lootContNr[0], lootContNr[1]);
								if (config->eatFromCorpse)
								{
									CModuleUtil::eatItemFromContainer(contNr);
									CModuleUtil::eatItemFromContainer(contNr);
								}
							}
							if (config->dropNotLooted)
							{
								CTibiaCharacter self;
								reader.readSelfCharacter(&self);
								if (config->dropWhenCapacityLimitReached
								    && self.cap < config->capacityLimit
								    && !config->dropOnlyLooted
								    || !config->dropWhenCapacityLimitReached
								    && config->dropListCount != 0)
								{
									CUIntArray droppedItems;
									droppedItems.SetSize(config->dropListCount);
									for (int i = 0; i < config->dropListCount; i++)
									{
										int itemid = CTibiaItem::getItemId(config->dropList[i]);
										if (itemid)
											droppedItems.Add(itemid);
									}
									dropItemsFromContainer(contNr, corpseCh.x, corpseCh.y, corpseCh.z, &droppedItems);
								}
								else if (self.cap < config->capacityLimit && config->dropOnlyLooted)
								{
									dropItemsFromContainer(contNr, corpseCh.x, corpseCh.y, corpseCh.z, &acceptedItems);
								}
								else if (!config->dropWhenCapacityLimitReached && config->dropListCount == 0)
								{
									dropItemsFromContainer(contNr, corpseCh.x, corpseCh.y, corpseCh.z);
								}
							}
							globalAutoAttackStateLoot = CToolAutoAttackStateLoot_closing;
							Sleep(CModuleUtil::randomFormula(250, 50)); //Add short delay for autostacker to take items
							CPackSender::closeContainer(contNr);
							CModuleUtil::waitForOpenContainer(contNr, false);
						}
						delete lootCont;
					}
				} // if waitForContainer(9)
				else
				{
					char buf[128];
					sprintf(buf, "Failed to open Item ID:%d", corpseId);
					//CPackSender::sendTAMessage(buf);
				}
			}
			if (!corpseQueue.GetCount())
				CVariableStore::setVariable("autolooterTm", "");
		}
		if (globalAutoAttackStateAttack == CToolAutoAttackStateAttack_macroPause)
			globalAutoAttackStateLoot = CToolAutoAttackStateLoot_macroPause;
		else
			globalAutoAttackStateLoot = CToolAutoAttackStateLoot_notRunning;
	}
	lootThreadId = 0;
	return 0;
}

DWORD WINAPI toolThreadProc(LPVOID lpParam)
{
	CMemReader& reader = CMemReader::getMemReader();

	CConfigData *config        = (CConfigData *)lpParam;
	int pid                    = GetCurrentProcessId();
	CSharedMemory sh_mem("TibiaAuto", 1024, shMemInit);
	char varName[128];
	sprintf(varName, "mod_cavebot_%d", pid);
	int iter = 0;

	int attackConsiderPeriod        = 20000;
	int currentlyAttackedCreatureNr = -1;

	int lastStandingX = 0, lastStandingY = 0, lastStandingZ = 0;

	time_t lastAttackedCreatureBloodHit = 0;
	int reachedAttackedCreature         = 0;//true if 1 sqm away or hp dropped while attacking
	int shareAlienBackattack            = 0;
	int alienCreatureForTrainerFound    = 0;

	int pauseInvoked = 0;

	int modRuns = -1;//for performing actions once every 10 iterations
	// reset globals
	waypointTargetX         = waypointTargetY = waypointTargetZ = 0;
	actualTargetX           = actualTargetY = actualTargetZ = 0;
	depotX                  = depotY = depotZ = 0;
	lastDepotPathNotFoundTm = 0;
	persistentShouldGo      = 0;

	creatureAttackDist    = 0;
	walkerStandingEndTm   = 0;
	firstCreatureAttackTM = 0;
	currentPosTM          = time(NULL);
	lastSendAttackTm      = 0;

	int loggedOut            = 0;
	int wasLoggedOut         = 0;
	int attackTimeoutOnLogin = 10; //used for disabling attack for first 10 seconds after logging in
	time_t loginTm           = 0;

	CUIntArray lootFromFloorArr;

	if (config->lootFood)
		lootFromFloorArr.Append(*CTibiaItem::getFoodIdArrayPtr());
	if (config->lootGp)
		lootFromFloorArr.Add(CTibiaItem::getValueForConst("GP"));
	if (config->lootWorms)
		lootFromFloorArr.Add(CTibiaItem::getValueForConst("worms"));
	if (config->lootCustom)
		lootFromFloorArr.Append(*CTibiaItem::getLootItemIdArrayPtr());
	sortArray(lootFromFloorArr);

	int i;
	int openContNr  = 0;
	int openContMax = reader.readOpenContainerCount();
	for (i = 0; i < reader.m_memMaxContainers; i++)
	{
		if (openContNr < openContMax)
		{
			CTibiaContainer* cont = reader.readContainer(i);
			if (cont->flagOnOff)
			{
				openContNr++;
				containerTimes[i] = time(NULL) - CONTAINER_TIME_CUTOFF;
			}
			else
			{
				containerTimes[i] = 0;
			}
			delete cont;
		}
		else
		{
			containerTimes[i] = 0;
		}
	}

	int waypointsCount = 0;
	for (i = 0; i < MAX_WAYPOINTCOUNT; i++)
	{
		if (config->waypointList[i].x || config->waypointList[i].y || config->waypointList[i].z)  //y==z==-1 if a delay
			waypointsCount++;
		else
			break;
	}
	if (config->selectedWaypoint != -1)
		currentWaypointNr = config->selectedWaypoint;
	else
		currentWaypointNr = min(0, waypointsCount - 1);

	// clean the debug file
	if (config->debug)
		CModuleUtil::masterDebug("tibiaauto-debug-cavebot.txt");

	shareAlienBackattack = config->shareAlienBackattack;
	if (!sh_mem.IsCreated())
	{
		if (config->debug)
			registerDebug("ERROR: shmem initialisation failed");
		shareAlienBackattack = 0;
	}

	Creature creatureList[MAX_LOOT_ARRAY];

	CIpcMessage mess;


	bool DISPLAY_TIMING = false;
	int TIMING_MAX      = 10;
	int TIMING_COUNTS[100];
	memset(TIMING_COUNTS, 0, 100 * sizeof(int));
	int TIMING_TOTALS[100];
	memset(TIMING_TOTALS, 0, 100 * sizeof(int));
	while (!toolThreadShouldStop)
	{
		Sleep(250);

		int beginningS = GetTickCount();
		loggedOut = !reader.isLoggedIn();
		if (loggedOut)
		{
			currentlyAttackedCreatureNr = -1;
			wasLoggedOut                = 1;
		}

		//on first run after being logged out perform these
		if (!loggedOut && wasLoggedOut)
		{
			//refresh attackmode since Tibia's buttons would display server's settings
			SendAttackMode(-1, -1, -1, -1, 1);
			loginTm      = time(NULL);
			wasLoggedOut = 0;
		}

		if (loggedOut || !config->pausingEnable)
		{
			// flush IPC communication if not logged in or we do not pay attention to pauses
			while (CIPCBackPipe::readFromPipe(&mess, 1009))
			{
			};
			while (CIPCBackPipe::readFromPipe(&mess, 2002))
			{
			};
			while (CIPCBackPipe::readFromPipe(&mess, 1103))
			{
			};
		}

		if (loggedOut)
			continue;        // do not proceed if not connected

		if (time(NULL) - loginTm < attackTimeoutOnLogin)  // do not walk or attack until attack timeout is finished
			continue;

		int pausingS = GetTickCount();
		if (config->pausingEnable)
		{
			if (CIPCBackPipe::readFromPipe(&mess, 1009) || CIPCBackPipe::readFromPipe(&mess, 2002))
			{
				int msgLen;
				char msgBuf[512];
				memset(msgBuf, 0, 512);
				memcpy(&msgLen, mess.payload, sizeof(int));
				memcpy(msgBuf, mess.payload + 4, msgLen);

				if (!strncmp("%ta pause", msgBuf, 9))
				{
					pauseInvoked = !pauseInvoked;
					if (pauseInvoked)
						CPackSender::sendTAMessage("Paused Cavebot");
					else
						CPackSender::sendTAMessage("Cavebot Unpaused");
					if (pauseInvoked)
					{
						globalAutoAttackStateAttack = CToolAutoAttackStateAttack_macroPause;
						globalAutoAttackStateWalker = CToolAutoAttackStateWalker_macroPause;
						globalAutoAttackStateDepot  = CToolAutoAttackStateDepot_macroPause;
						AttackCreature(config, 0);
						reader.setAttackedCreature(0);
						reader.cancelAttackCoords();
						//restore attack mode
						SendAttackMode(reader.getPlayerModeAttackType(), reader.getPlayerModeFollow(), -1, -1);

						if (config->debug)
							registerDebug("Paused cavebot");
						CVariableStore::setVariable("walking_control", "userpause");
						CVariableStore::setVariable("walking_priority", "10");
					}
					else
					{
						CVariableStore::setVariable("walking_control", "");
						CVariableStore::setVariable("walking_priority", "0");
						//Depot walker relies on states to decide what to do
						globalAutoAttackStateDepot  = CToolAutoAttackStateDepot_notRunning;
						globalAutoAttackStateAttack = CToolAutoAttackStateAttack_notRunning;
						globalAutoAttackStateWalker = CToolAutoAttackStateWalker_notRunning;
					}
				}
			}
			if (pauseInvoked)
				continue;
		}
		int pausingE = GetTickCount();
		if (DISPLAY_TIMING)
		{
			TIMING_COUNTS[3] += 1;
			TIMING_TOTALS[3] += pausingE - pausingS;
			if (TIMING_COUNTS[3] == TIMING_MAX)
			{
				char bufDisp[400];
				sprintf(bufDisp, "pausing %d", TIMING_TOTALS[3]);
				CPackSender::sendTAMessage(bufDisp);
				TIMING_COUNTS[3] = 0;
				TIMING_TOTALS[3] = 0;
			}
		}

		bool control    = strcmp(CVariableStore::getVariable("walking_control"), "cavebot") == 0;
		int modpriority = atoi(CVariableStore::getVariable("walking_priority"));
		if (!control && atoi(config->modPriorityStr) > modpriority)
		{
			CVariableStore::setVariable("walking_control", "cavebot");
			CVariableStore::setVariable("walking_priority", config->modPriorityStr);
		}


		if (config->debug)
		{
			char buf[256];
			sprintf(buf, "Next loop. States: %d, %d, %d, %d, paused=%d, sleep=%d/%d", globalAutoAttackStateAttack, globalAutoAttackStateLoot, globalAutoAttackStateWalker, globalAutoAttackStateDepot, pauseInvoked, isInHalfSleep(), isInFullSleep());
			registerDebug(buf);
		}

		if (isInHalfSleep())
			globalAutoAttackStateWalker = CToolAutoAttackStateWalker_halfSleep;
		else if (globalAutoAttackStateWalker == CToolAutoAttackStateWalker_halfSleep)
			globalAutoAttackStateWalker = CToolAutoAttackStateWalker_notRunning;
		if (isInFullSleep())
			globalAutoAttackStateWalker = CToolAutoAttackStateWalker_fullSleep;
		else if (globalAutoAttackStateWalker == CToolAutoAttackStateWalker_fullSleep)
			globalAutoAttackStateWalker = CToolAutoAttackStateWalker_notRunning;

		// if in a full sleep mode then just do nothing
		if (isInFullSleep())
			continue;

		int openContNr  = 0;
		int openContMax = reader.readOpenContainerCount();
		for (i = 0; i < reader.m_memMaxContainers; i++)
		{
			if (openContNr < openContMax)
			{
				CTibiaContainer* cont = reader.readContainer(i);
				if (cont->flagOnOff)
				{
					openContNr++;
					if (containerTimes[i] == 0)
						containerTimes[i] = time(NULL);
				}
				else
				{
					containerTimes[i] = 0;
				}
				delete cont;
			}
			else
			{
				containerTimes[i] = 0;
			}
		}

		modRuns++;//for performing actions once every 10 iterations
		if (modRuns == 1000000000)
			modRuns = 0;
		CTibiaCharacter self;
		reader.readSelfCharacter(&self);

		int depotS = GetTickCount();
		/**
		 * Check whether we should go to a depot
		 */
		if (modRuns % 10 == 0 || depotX != 0)
			depotCheck(config);
		if (modRuns % (30 * 4) == 10 && config->lootFromFloor)
		{
			lootFromFloorArr.RemoveAll();
			if (config->lootFood)
				lootFromFloorArr.Append(*CTibiaItem::getFoodIdArrayPtr());
			if (config->lootGp)
				lootFromFloorArr.Add(CTibiaItem::getValueForConst("GP"));
			if (config->lootWorms)
				lootFromFloorArr.Add(CTibiaItem::getValueForConst("worms"));
			if (config->lootCustom)
				lootFromFloorArr.Append(*CTibiaItem::getLootItemIdArrayPtr());
			sortArray(lootFromFloorArr);
		}
		if (globalAutoAttackStateDepot == CToolAutoAttackStateDepot_notRunning)
			depotX = depotY = depotZ = 0;

		int depotE = GetTickCount();
		if (DISPLAY_TIMING)
		{
			TIMING_COUNTS[4] += 1;
			TIMING_TOTALS[4] += depotE - depotS;
			if (TIMING_COUNTS[4] == TIMING_MAX)
			{
				char bufDisp[400];
				sprintf(bufDisp, "depot %d", TIMING_TOTALS[4]);
				CPackSender::sendTAMessage(bufDisp);
				TIMING_COUNTS[4] = 0;
				TIMING_TOTALS[4] = 0;
			}
		}


		/**
		 * Check whether we should collected dropped loot.
		 * Conditions:
		 * 1. we are not attacking anything
		 * 2. we are not in a half sleep (no walking) mode
		 * 3. there is something to loot
		 */
		int moving = CMemUtil::getMemUtil().GetMemIntValue(reader.m_memAddressTilesToGo);
		if (config->lootFromFloor && lootFromFloorArr.GetSize() && !isInHalfSleep() && isLooterDone(config))
		{
			static time_t lastFullDroppedLootCheckTm  = 0;
			static time_t lastSmallDroppedLootCheckTm = 0;
			if (currentlyAttackedCreatureNr == -1 && moving <= 2 && time(NULL) - lastFullDroppedLootCheckTm > 6)
			{
				if (!droppedLootCheck(config, lootFromFloorArr)) // if no dropped loot found, wait 6 seconds before trying again.
					lastFullDroppedLootCheckTm = time(NULL);
			}
			else if (time(NULL) - lastSmallDroppedLootCheckTm > 1)
			{
				if (!droppedLootCheck(config, lootFromFloorArr, 1))// wait 1 second between unsuccessful loot from floors
					lastSmallDroppedLootCheckTm = time(NULL);
			}
		}


		/*wis notes:
		   going to depot ignore creatures that aren't attacking
		 */

		int beginningE = GetTickCount();
		if (DISPLAY_TIMING)
		{
			TIMING_COUNTS[0] += 1;
			TIMING_TOTALS[0] += beginningE - beginningS;
			if (TIMING_COUNTS[0] == TIMING_MAX)
			{
				char bufDisp[400];
				sprintf(bufDisp, "beginning %d", TIMING_TOTALS[0]);
				CPackSender::sendTAMessage(bufDisp);
				TIMING_COUNTS[0] = 0;
				TIMING_TOTALS[0] = 0;
			}
		}

		int attackS = GetTickCount();

		/*****
		   Start attack process
		 ******/
		//if (!reader.getAttackedCreature()) currentlyAttackedCreatureNr = -1;//this is not needed yet(but may be)
		CTibiaCharacter attackedCh;
		reader.readVisibleCreature(&attackedCh, currentlyAttackedCreatureNr);

		// keep track of how long standing in same place for info to user
		if (lastStandingX != self.x || lastStandingY != self.y || lastStandingZ != self.z)
		{
			lastStandingX = self.x;
			lastStandingY = self.y;
			lastStandingZ = self.z;
			currentPosTM  = time(NULL);
		}
		/**
		 * Check if currently attacked creature is alive still
		 * If it is dead inform the looter it can loot
		 */
		if (pauseInvoked)
		{
			globalAutoAttackStateLoot = CToolAutoAttackStateLoot_macroPause;
		}
		else if (currentlyAttackedCreatureNr != -1 && (self.cap > config->capacityLimit || config->eatFromCorpse || config->dropNotLooted) && (config->lootFood || config->lootGp || config->lootWorms || config->lootCustom || config->eatFromCorpse || config->dropNotLooted))
		{
			// now 's see whether creature is still alive or not
			if (!attackedCh.hpPercLeft &&
			    abs(attackedCh.x - self.x) + abs(attackedCh.y - self.y) <= 10 &&
			    attackedCh.z == self.z)
			{
				if (config->debug)
					registerDebug("Looter: Creature is dead.");

//				// disabled since CIpcMessage is slow
//				//check loot message for creature we killed
//				CIpcMessage mess;
//				unsigned int lootMessageTm;
//				CString lootCreatureName;
//				CString lootString;
//				CString attackChreatureName=attackedCh.name;
//				attackChreatureName.MakeLower();
//				while(CIPCBackPipe::readFromPipe(&mess,1104)){
//					unsigned int _lootMessageTm = *(unsigned int*)(mess.payload);
//					CString _lootCreatureName = (char*)(mess.payload+4);
//					CString _lootString = (char*)(mess.payload+4+400);
//					if(_lootCreatureName.Find(attackChreatureName)!=-1){
//						lootMessageTm = _lootMessageTm;
//						lootCreatureName = _lootCreatureName[0];
//						lootString = _lootString[0];
//					}
//				}
//
//				int msSinceKill = reader.getCurrentTm()-lootMessageTm;
//				if(msSinceKill < 1500 && lootString.Compare("nothing")==0){ // if recent creature contains nothing then don't loot
//					char buf[1024];
//					sprintf(buf,"Looter: Loot message says creature contains nothing. For %s piped info:%d, %s, %s",attackedCh.name,msSinceKill,lootCreatureName[0],lootString[0]);
//					if (config->debug) registerDebug(buf);
//				}else{
				if (1)
				{
					if (config->lootWhileKill)
					{
						if (!lootThreadId)
						{
							if (config->debug)
								registerDebug("Running lootThreadProc");
							lootThreadHandle = ::CreateThread(NULL, 0, lootThreadProc, config, 0, &lootThreadId);
						}
						if (!queueThreadId)
						{
							if (config->debug)
								registerDebug("Running queueThreadProc");
							queueThreadHandle = ::CreateThread(NULL, 0, queueThreadProc, 0, 0, &queueThreadId);
						}

						char buf[30];
						memset(buf, 0, 30);
						autolooterTm = time(NULL) + 10;
						sprintf(buf, "%d %d", attackedCh.tibiaId, autolooterTm);
						CVariableStore::setVariable("autolooterTm", buf);

						if (config->debug)
							registerDebug("Tmp:Setting currentlyAttackedCreatureNr = -1 & changing attackedCh");
						currentlyAttackedCreatureNr = -1;
						reader.setAttackedCreature(0);
						reader.readVisibleCreature(&attackedCh, currentlyAttackedCreatureNr);
					}
					else
					{
						FILE *lootStatsFile = NULL;
						// time,rand,creature,name,pos,objectId,count,bagopen,checksum
						int killNr = rand();

						int lootContNr[2];
						lootContNr[0] = reader.findNextClosedContainer();
						if (config->lootInBags)
							lootContNr[1] = reader.findNextClosedContainer(lootContNr[0]);

						if (config->gatherLootStats)
						{
							char installPath[1024];
							CModuleUtil::getInstallPath(installPath);
							char pathBuf[2048];
							sprintf(pathBuf, "%s\\tibiaauto-stats-loot.txt", installPath);
							lootStatsFile = fopen(pathBuf, "a+");
						}

						// first make sure all needed containers are closed

						// attacked creature dead, near me, same floor
						globalAutoAttackStateLoot = CToolAutoAttackStateLoot_opening;

						CModuleUtil::waitForCreatureDisappear(attackedCh.nr);
						reader.readSelfCharacter(&self);

						int corpseId = reader.itemOnTopCode(attackedCh.x - self.x, attackedCh.y - self.y);
						if (corpseId && CTileReader::getTileReader().getTile(corpseId)->isContainer) //If there is no corpse ID, TA has "lost" the body. No sense in trying to open something that won't be there.
						{       // Open corpse to container, wait to get to corpse on ground and wait for open
							CPackSender::openContainerFromFloor(corpseId, attackedCh.x, attackedCh.y, attackedCh.z, lootContNr[0]);

							if (!CModuleUtil::waitToApproachSquare(attackedCh.x, attackedCh.y))
							{
								//(waitToApproachSquare returns false) => (corpse >1 sqm away and not reached yet), so try again
								CPackSender::openContainerFromFloor(corpseId, attackedCh.x, attackedCh.y, attackedCh.z, lootContNr[0]);
								CModuleUtil::waitToApproachSquare(attackedCh.x, attackedCh.y);
							}

							//Normal looting if autolooter not enabled
							if (CModuleUtil::waitForOpenContainer(lootContNr[0], true))
							{
								//CPackSender::sendTAMessage("[debug] opened container");
								if (config->debug)
									registerDebug("Looter: Opening dead creature corpse (first container)");
								if (config->lootInBags)
								{
									if (config->debug)
										registerDebug("Looter: Opening bag (second container)");
									CTibiaContainer *cont = reader.readContainer(lootContNr[0]);
									int itemNr;
									for (itemNr = 0; itemNr < cont->itemsInside; itemNr++)
									{
										CTibiaItem *insideItem = (CTibiaItem *)cont->items.GetAt(itemNr);
										CTibiaTile* tile       = CTileReader::getTileReader().getTile(insideItem->objectId);
										if (insideItem->objectId && tile && tile->isContainer)
										{
											globalAutoAttackStateLoot = CToolAutoAttackStateLoot_openingBag;
											CPackSender::openContainerFromContainer(insideItem->objectId, 0x40 + lootContNr[0], insideItem->pos, lootContNr[1]);
											if (!CModuleUtil::waitForOpenContainer(lootContNr[1], true))
												if (config->debug)
													registerDebug("Failed opening bag");
											break;
										}
									}
									deleteAndNull(cont);
								}
								if (lootStatsFile)
								{
									int checksum;
									time_t tm = time(NULL);
									for (int contNrInd = 0; contNrInd < 1 + config->lootInBags; contNrInd++)// get stats from first, then last if lootInBags
									{
										int contNr = lootContNr[contNrInd];
										//CPackSender::sendTAMessage("[debug] stats from conts");
										CTibiaContainer *lootCont = reader.readContainer(contNr);
										if (lootCont->flagOnOff)
										{
											if (config->debug)
												registerDebug("Recording Stats for Container Number", intstr(contNr).c_str());
											int itemNr;
											for (itemNr = 0; itemNr < lootCont->itemsInside; itemNr++)
											{
												int i, len;
												char statChName[128];
												for (i = 0, strcpy(statChName, attackedCh.name), len = strlen(statChName); i < len; i++)
												{
													if (statChName[i] == '[')
														statChName[i] = '\0';
												}

												CTibiaItem *lootItem = (CTibiaItem *)lootCont->items.GetAt(itemNr);
												checksum = CModuleUtil::calcLootChecksum(tm, killNr, strlen(statChName), 100 + itemNr, lootItem->objectId, (lootItem->quantity ? lootItem->quantity : 1), config->lootInBags && contNr == lootContNr[1], attackedCh.x, attackedCh.y, attackedCh.z);
												if (checksum < 0)
													checksum *= -1;
												fprintf(lootStatsFile, "%d,%d,'%s',%d,%d,%d,%d,%d,%d,%d,%d\n", tm, killNr, statChName, 100 + itemNr, lootItem->objectId, lootItem->quantity ? lootItem->quantity : 1, config->lootInBags && contNr == lootContNr[1], attackedCh.x, attackedCh.y, attackedCh.z, checksum);
											}
										}
										deleteAndNull(lootCont);
									}
									//free(lootCont);
									if (config->debug)
										registerDebug("Container data registered.");
								}

								CUIntArray acceptedItems;

								if (config->lootGp)
								{
									acceptedItems.Add(CTibiaItem::getValueForConst("GP"));
									acceptedItems.Add(CTibiaItem::getValueForConst("PlatinumCoin"));
									acceptedItems.Add(CTibiaItem::getValueForConst("CrystalCoin"));
								}
								if (config->lootWorms)
									acceptedItems.Add(CTibiaItem::getValueForConst("worms"));
								if (config->lootCustom)
									acceptedItems.Append(*CTibiaItem::getLootItemIdArrayPtr());
								if (config->lootFood)
									acceptedItems.Append(*CTibiaItem::getFoodIdArrayPtr());
								int lootTakeItem;
								for (int contNrInd = 0; contNrInd < 1 + config->lootInBags; contNrInd++)// loot from first, then last if lootInBags
								{
									int contNr = lootContNr[contNrInd];

									globalAutoAttackStateLoot = CToolAutoAttackStateLoot_moveing;
									CTibiaContainer *lootCont = reader.readContainer(contNr);
									//CPackSender::sendTAMessage("[debug] looting");
									if (lootCont->flagOnOff)
									{
										if (config->debug)
											registerDebug("Looting from container number", intstr(contNrInd).c_str());
										for (lootTakeItem = 0; lootTakeItem < 1; lootTakeItem++)
										{
											if (self.cap > config->capacityLimit)
												CModuleUtil::lootItemFromContainer(contNr, &acceptedItems, lootContNr[0], lootContNr[1]);
											if (config->eatFromCorpse)
											{
												CModuleUtil::eatItemFromContainer(contNr);
												CModuleUtil::eatItemFromContainer(contNr);
											}
										}
										if (config->dropNotLooted)
										{
											CTibiaCharacter self;
											reader.readSelfCharacter(&self);
											if (attackedCh.z != self.z)
											{
												dumpCreatureInfo("XXXYYYZZZ Creature replaced??", attackedCh.tibiaId);
												char buf[1111];
												sprintf(buf, "Drop looted?? %x,%d (%d,%d,%d)", attackedCh.tibiaId, attackedCh.nr, attackedCh.x, attackedCh.y, attackedCh.z);
												//AfxMessageBox(buf);
											}
											if (config->dropWhenCapacityLimitReached
											    && self.cap < config->capacityLimit
											    && !config->dropOnlyLooted
											    || !config->dropWhenCapacityLimitReached
											    && config->dropListCount != 0)
											{
												CUIntArray droppedItems;
												droppedItems.SetSize(config->dropListCount);
												for (int i = 0; i < config->dropListCount; i++)
												{
													int itemid = CTibiaItem::getItemId(config->dropList[i]);
													if (itemid)
														droppedItems.Add(itemid);
												}
												dropItemsFromContainer(contNr, attackedCh.x, attackedCh.y, attackedCh.z, &droppedItems);
												if (config->debug)
													registerDebug("Dropping custom items from container:", intstr(contNrInd).c_str());
											}
											else if (self.cap < config->capacityLimit && config->dropOnlyLooted)
											{
												dropItemsFromContainer(contNr, attackedCh.x, attackedCh.y, attackedCh.z, &acceptedItems);
												if (config->debug)
													registerDebug("Dropping loot items from container:", intstr(contNrInd).c_str());
											}
											else if (!config->dropWhenCapacityLimitReached && config->dropListCount == 0)
											{
												dropItemsFromContainer(contNr, attackedCh.x, attackedCh.y, attackedCh.z);
												if (config->debug)
													registerDebug("Dropping all items from container:", intstr(contNrInd).c_str());
											}
										}

										//CPackSender::sendTAMessage("[debug] closing bag");
										Sleep(CModuleUtil::randomFormula(250, 50)); //Add short delay for autostacker to take items
										CPackSender::closeContainer(contNr);
										CModuleUtil::waitForOpenContainer(contNr, false);
									}
									deleteAndNull(lootCont);
								}
								globalAutoAttackStateLoot = CToolAutoAttackStateLoot_notRunning;
								if (config->debug)
									registerDebug("End of looting");
							} // if waitForContainer(9)
							else
							{
								char buf[128];
								sprintf(buf, "Failed to open Item ID:%d", corpseId);
								if (config->debug)
									registerDebug(buf);
							}
							if (lootStatsFile)
								fclose(lootStatsFile);
						}
						globalAutoAttackStateLoot = CToolAutoAttackStateLoot_notRunning;
						if (config->debug)
							registerDebug("Tmp:Setting currentlyAttackedCreatureNr = -1 & changing attackedCh");
						currentlyAttackedCreatureNr = -1;
						reader.setAttackedCreature(0);
					reader.readVisibleCreature(&attackedCh, currentlyAttackedCreatureNr);
					}//if lootWhileKilling enabled
				}//if loot msg is "nothing"
			}
			else  // if creatureIsNotDead
			{
				char debugBuf[256];
				sprintf(debugBuf, "Attacked creature info: x dist=%d y dist=%d z dist=%d id=%d visible=%d hp=%d", abs(self.x - attackedCh.x), abs(self.y - attackedCh.y), abs(self.z - attackedCh.z), attackedCh.tibiaId, attackedCh.visible, attackedCh.hpPercLeft);
				if (config->debug)
					registerDebug(debugBuf);
			}
		}

		//only time currentlyAttackedCreatureNr can = -1 is if looter has run(fixes skipping monster)
		if (currentlyAttackedCreatureNr != -1 && !creatureList[currentlyAttackedCreatureNr].isOnscreen)
		{
			currentlyAttackedCreatureNr = -1;
			reader.setAttackedCreature(0);
		}

		if (config->debug)
		{
			char buf[128];
			sprintf(buf, "Tmp:currentlyAttackedCreatureNr=%d", currentlyAttackedCreatureNr);
			registerDebug(buf);
		}
		// if client thinks we are not attacking anything, then
		// send "attack new target" to it and increment failed attack for creatureNr by 1
		if (reader.getAttackedCreature() == 0 && currentlyAttackedCreatureNr != -1 && (attackedCh.initialized && attackedCh.hpPercLeft))
		{
			char buf[256];
			sprintf(buf, "Resetting attacked creature to %d,%d,%d [1]", currentlyAttackedCreatureNr, reader.getAttackedCreature(), attackedCh.hpPercLeft);
			if (config->debug)
				registerDebug(buf);

			creatureList[currentlyAttackedCreatureNr].failedAttacks++;
			AttackCreature(config, attackedCh.tibiaId);
			char debugBuf[256];
			sprintf(debugBuf, "Attacked creature info: x dist=%d y dist=%d z dist=%d id=%d visible=%d hp=%d", abs(self.x - attackedCh.x), abs(self.y - attackedCh.y), abs(self.z - attackedCh.z), attackedCh.tibiaId, attackedCh.visible, attackedCh.hpPercLeft);
			//AfxMessageBox(debugBuf);
			if (config->debug)
				registerDebug("Incrementing failed attempts since something cancelled attacking creature.");
		}
		else if (reader.getAttackedCreature() && attackedCh.initialized && reader.getAttackedCreature() != attackedCh.tibiaId)
		{
			//wis: keep player-chosen creature as attackee
			CTibiaCharacter attCh;
			if (reader.getCharacterByTibiaId(&attCh, reader.getAttackedCreature()))
			{
				currentlyAttackedCreatureNr = attCh.nr;
				char buf[128];
				sprintf(buf, "Setting attacked creature to %d [1]", currentlyAttackedCreatureNr);
				if (config->debug)
					registerDebug(buf);
			}
			//attCh is NULL
		}
		//if autofollow on, still not <=1 from creature and standing for > 2s refresh attack and add fail attack
		//wait .5 secs between failed attacks
		else if (currentlyAttackedCreatureNr != -1 && config->autoFollow && creatureList[currentlyAttackedCreatureNr].distance(self.x, self.y) > 1 && time(NULL) - currentPosTM > (2.5 + .5 * creatureList[currentlyAttackedCreatureNr].failedAttacks))
		{
			if (creatureList[currentlyAttackedCreatureNr].tibiaId)
			{
				creatureList[currentlyAttackedCreatureNr].failedAttacks++;
				AttackCreature(config, creatureList[currentlyAttackedCreatureNr].tibiaId);
				char buf[256];
				sprintf(buf, "Incrementing failed attempts for not reaching target in time. time:%d lastPosChange:%d, %d", time(NULL), currentPosTM, time(NULL) - currentPosTM > (2.5 + .5 * creatureList[currentlyAttackedCreatureNr].failedAttacks));
				if (config->debug)
					registerDebug(buf);
			}
		}
		// refresh information
		int currentTm           = reader.getCurrentTm();
		int playersOnScreen     = 0;
		int monstersSurrounding = 0;
		alienCreatureForTrainerFound = 0;

		int crNr;
		for (crNr = 0; crNr < reader.m_memMaxCreatures; crNr++)
		{
			CTibiaCharacter ch;
			reader.readVisibleCreature(&ch, crNr);
			if (ch.tibiaId == 0)
			{
				creatureList[crNr].tibiaId = 0; //Needed since list is now cleaned on logout so that creatures later in this list don't need to be overwritten with 0's
				break;
			}
			/**
			 * creature ID for creature number has changed,
			 * so new creature is occupying the slot already
			 */
			if (creatureList[crNr].tibiaId != ch.tibiaId && currentlyAttackedCreatureNr != crNr)
			{
				time_t keepAttackTm = creatureList[crNr].lastAttackTm;
				creatureList[crNr]              = Creature();
				creatureList[crNr].lastAttackTm = keepAttackTm;
				creatureList[crNr].tibiaId      = ch.tibiaId;

				int monstListNr;
				//Get name and take care of monsters numbered by TA's creature info
				int i, len;
				char statChName[128];
				for (i = 0, strcpy(statChName, ch.name), len = strlen(statChName); i < len; i++)
				{
					if (statChName[i] == '[')
						statChName[i] = '\0';
				}
				strcpy(creatureList[crNr].name, statChName);

				// if attackAllMonsters set default priority to lowest and overwrite with actual priority
				if (config->attackAllMonsters && ch.tibiaId >= 0x40001000)
					creatureList[crNr].listPriority = 1;
				// scan creature list to find its priority
				for (monstListNr = 0; monstListNr < config->monsterCount && monstListNr < MAX_MONSTERLISTCOUNT; monstListNr++)
				{
					if (config->monsterList[monstListNr][0] == 0)
					{
						break;
					}
					else if (!_strcmpi(config->monsterList[monstListNr], creatureList[crNr].name))
					{
						creatureList[crNr].listPriority = config->monsterCount - monstListNr + 1;//2 is last item in list, 1 is for 'attackAllMonster' monsters, 0 means not in attack list
						break;
					}
				}

				// scan ignore list
				for (monstListNr = 0; monstListNr < min(MAX_IGNORECOUNT, config->ignoreCount); monstListNr++)
				{
					if (!_strcmpi(config->ignoreList[monstListNr], creatureList[crNr].name))
					{
						if (config->debug && creatureList[crNr].isIgnoredUntil - time(NULL) < 9999)
							registerDebug("Creature found on ignore list");
						creatureList[crNr].isIgnoredUntil = 1555555555;//ignore forever
					}
				}

				//ignore self forever
				if (ch.tibiaId == self.tibiaId)
					creatureList[crNr].isIgnoredUntil = 1555555555;                      //ignore forever
			}
			if (ch.visible == 0 || abs(self.x - ch.x) > 7 || abs(self.y - ch.y) > 5 || ch.z != self.z)
			{
				creatureList[crNr].isOnscreen      = 0;
				creatureList[crNr].isWithinMargins = ch.visible && ch.z == self.z;
				if (creatureList[crNr].tibiaId < 0x40000000 && creatureList[crNr].isWithinMargins)
					playersOnScreen++;
			}
			else
			{
				if (crNr == currentlyAttackedCreatureNr)
				{
					//if creature we are attacking has lost health, record time of bloodhit
					if (ch.hpPercLeft < creatureList[crNr].hpPercLeft)
						lastAttackedCreatureBloodHit = time(NULL);
					//determine if creature reached to ignore creature if not reached in time
					if (!reachedAttackedCreature && (time(NULL) - lastAttackedCreatureBloodHit < 1 || creatureList[crNr].distance(self.x, self.y) <= 1))
						reachedAttackedCreature = 1;
					//if attacked creature has not been reached after given time, ignore creature
					if (!reachedAttackedCreature && time(NULL) - firstCreatureAttackTM > config->unreachableAfter)
						creatureList[crNr].isIgnoredUntil = time(NULL) + config->suspendAfterUnreachable;
				}
				// Attack time has changed. We interpret this by saying that ch.tibiaId is attacking us
				// even if ID changed, since 'lastAttackTm' is NOT updated when Tibia reuses slot.
				if (creatureList[crNr].lastAttackTm != ch.lastAttackTm && currentTm - ch.lastAttackTm < attackConsiderPeriod)
				{
					creatureList[crNr].isAttacking = 1;
					char buf[256];
					sprintf(buf, "lastAttackTm change for %d: was %d/%d is %d/%d", creatureList[crNr].tibiaId, creatureList[crNr].lastAttackTm, ch.tibiaId, ch.lastAttackTm);
					if (config->debug)
						registerDebug(buf);
				}
				else if (currentTm - ch.lastAttackTm >= attackConsiderPeriod)
				{
					creatureList[crNr].isAttacking = 0;
				}
				creatureList[crNr].lastAttackTm    = ch.lastAttackTm;
				creatureList[crNr].x               = ch.x;
				creatureList[crNr].y               = ch.y;
				creatureList[crNr].z               = ch.z;
				creatureList[crNr].isOnscreen      = 1;
				creatureList[crNr].isWithinMargins = 1;
				creatureList[crNr].isDead          = (ch.hpPercLeft == 0);
				creatureList[crNr].hpPercLeft      = ch.hpPercLeft;
				creatureList[crNr].isIgnoredUntil  = (creatureList[crNr].isIgnoredUntil < time(NULL)) ? 0 : creatureList[crNr].isIgnoredUntil;
				//outfit type&& head color==0 <=> creature is invisible
				creatureList[crNr].isInvisible = (ch.monsterType == 0 && ch.colorHead == 0);

				//ignore creatures with >=3 failed attacks for 30 secs
				if (creatureList[crNr].failedAttacks >= 3 && !creatureList[crNr].isInvisible && strcmp(creatureList[crNr].name, "Warlock") && strcmp(creatureList[crNr].name, "Infernalist"))
				{
					creatureList[crNr].isIgnoredUntil = time(NULL) + 30;
					creatureList[crNr].failedAttacks  = 0;
					if (config->debug)
						registerDebug("Ignore creatures with >=3 failed attacks");
				} //else if creature attacking us from 1 away but ignored for <5 mins, unignore
				else if (creatureList[crNr].isIgnoredUntil - time(NULL) < 9999 && !creatureList[crNr].isInvisible && (creatureList[crNr].distance(self.x, self.y) <= 1 && currentlyAttackedCreatureNr != -1))
				{
					creatureList[crNr].isIgnoredUntil = 0;
				}

				//update environmental variables

				//changed to exclude the criteria of attacking, do not count oneself, count players only if they are attacking you
				//if (creatureList[crNr].isAttacking && taxiDist(self.x,self.y,creatureList[crNr].x,creatureList[crNr].y)<=1) monstersSurrounding++;
				if ((creatureList[crNr].tibiaId > 0x40000000 || creatureList[crNr].isAttacking) && creatureList[crNr].tibiaId != self.tibiaId && self.z == creatureList[crNr].z && taxiDist(self.x, self.y, creatureList[crNr].x, creatureList[crNr].y) <= 1)
					monstersSurrounding++;

				if (crNr != self.nr && creatureList[crNr].tibiaId < 0x40000000 && creatureList[crNr].isWithinMargins)
					playersOnScreen++;
				//Edit: Alien creature if monster or attacking player to avoid switching weapons when interrupted by player
				if (crNr != self.nr && !creatureList[crNr].listPriority && creatureList[crNr].isOnscreen && (creatureList[crNr].tibiaId > 0x40000000 || creatureList[crNr].isAttacking))
					alienCreatureForTrainerFound = 1;

				// if sharing alien backattack is active, then we may attack
				// creatures which are not directly attacking us
				if (creatureList[crNr].isAttacking && shareAlienBackattack)
				{
					int count = sh_mem.GetVariablesCount();
					int pos;
					for (pos = 0; pos < count; pos++)
					{
						ValueHeader valueHeader;
						sh_mem.GetValueInfo(pos, &valueHeader);
						if (strcmp(varName, (char *)valueHeader.wszValueName))
						{
							int attackTibiaId     = 0;
							unsigned long int len = 4;
							// avoid looping with out own attack target
							sh_mem.GetValue((char *)valueHeader.wszValueName, &attackTibiaId, &len);

							if (attackTibiaId == ch.tibiaId)
							{
								if (config->debug)
									registerDebug("Setting isAttacking because of shm info");
								creatureList[crNr].isAttacking = 1;
							}
						}
					}
				}
			}// if visible
			if (config->debug && modRuns % 10 == 0 && creatureList[crNr].isOnscreen)
			{
				char buf[256];
				sprintf(buf, "%s, nr=%d, isatta=%d, isonscreen=%d, iswithinmargins=%d, atktm=%d ID=%d ignore=%d,x=%d,y=%d,z=%d", crNr != self.nr ? creatureList[crNr].name : "****", crNr, creatureList[crNr].isAttacking, creatureList[crNr].isOnscreen, creatureList[crNr].isWithinMargins, creatureList[crNr].lastAttackTm, creatureList[crNr].tibiaId, creatureList[crNr].isIgnoredUntil ? creatureList[crNr].isIgnoredUntil - time(NULL) : 0, creatureList[crNr].x, creatureList[crNr].y, creatureList[crNr].z);
				registerDebug(buf);
			}
		}//end for
		 //Determine best creature to attack on-screen
		int bestCreatureNr = -1;
		for (crNr = 0; crNr < reader.m_memMaxCreatures; crNr++)
		{
			if (creatureList[crNr].tibiaId == 0)
				break;
			//If cannot attack, don't
			if (!creatureList[crNr].isOnscreen || creatureList[crNr].isInvisible || creatureList[crNr].isDead || crNr == self.nr || (reader.getSelfEventFlags() & 16384 /*PZ zone*/) != 0)
				continue;
			//If shouldn't attack, don't
			if (creatureList[crNr].hpPercLeft < config->attackHpAbove)
			{
				if (config->debug && modRuns % 10 == 0)
					registerDebug("Quit Case:hp above value");
				continue;
			}
			if (config->dontAttackPlayers && creatureList[crNr].tibiaId < 0x40000000)
			{
				if (config->debug && modRuns % 10 == 0)
					registerDebug("Quit Case:No attack player");
				continue;
			}
			if (config->attackOnlyAttacking && !creatureList[crNr].isAttacking)
			{
				if (config->debug && modRuns % 10 == 0)
					registerDebug("Quit Case:only attack attacking");
				continue;
			}
			if ((!creatureList[crNr].isAttacking || creatureList[crNr].hpPercLeft == 100) && maxDist(self.x, self.y, creatureList[crNr].x, creatureList[crNr].y) > config->attackRange)
			{
				if (config->debug && modRuns % 10 == 0)
					registerDebug("Quit Case:out of range");
				continue;
			}
			if (creatureList[crNr].isIgnoredUntil)
			{
				if (config->debug && modRuns % 10 == 0)
					registerDebug("Quit Case:is ignored");
				continue;
			}
			if (config->suspendOnEnemy && playersOnScreen && !creatureList[crNr].isAttacking)
			{
				if (config->debug)
					registerDebug("Quit Case:Suspend on enemy");
				continue;
			}                 //only attack creatures attacking me if suspendOnEnemy and player on screen
			if (!(creatureList[crNr].listPriority || config->forceAttackAfterAttack && creatureList[crNr].isAttacking))
			{
				if (config->debug && modRuns % 10 == 0)
					registerDebug("Quit Case:Options do not allow attack");
				continue;
			}

			//if shareBackattackAlien, add possible attack option to list if not on priority list
			if (!creatureList[crNr].listPriority && shareAlienBackattack)
			{
				// if we are backattacking and shareing backattack
				// with other instances is active - then register it
				// in the shm
				sh_mem.SetValue(varName, &creatureList[crNr].tibiaId, 4);
			}

			//if bestCreatureNr is not picked yet switch to first reasonable option
			if (bestCreatureNr == -1)
				bestCreatureNr = crNr;
			int isCloser  = taxiDist(self.x, self.y, creatureList[crNr].x, creatureList[crNr].y) + 1 < taxiDist(self.x, self.y, creatureList[bestCreatureNr].x, creatureList[bestCreatureNr].y);
			int isFarther = taxiDist(self.x, self.y, creatureList[crNr].x, creatureList[crNr].y) - 1 > taxiDist(self.x, self.y, creatureList[bestCreatureNr].x, creatureList[bestCreatureNr].y);
			if (isCloser)
			{
				if (creatureList[bestCreatureNr].isAttacking &&
				    (creatureList[bestCreatureNr].listPriority > creatureList[crNr].listPriority && creatureList[bestCreatureNr].listPriority ||
				     creatureList[bestCreatureNr].listPriority == 0 && creatureList[crNr].listPriority != 0) &&
				    canGetToPoint(creatureList[bestCreatureNr].x, creatureList[bestCreatureNr].y, creatureList[bestCreatureNr].z))
				{
					//distance creature has higher priority and can be reached
				}
				else
				{
					char buf[256];
					sprintf(buf, "Attacker: CrNr %d Better because closer.", crNr);
					if (config->debug)
						registerDebug(buf);
					bestCreatureNr = crNr;
				}
			}
			else if (!isFarther)
			{
				//If force attack prioritize creature not on list with lowest health
				if ((config->forceAttackAfterAttack && creatureList[crNr].listPriority == 0 && !isFarther && creatureList[crNr].isAttacking) &&
				    (creatureList[crNr].hpPercLeft < creatureList[bestCreatureNr].hpPercLeft || creatureList[bestCreatureNr].listPriority != 0))
				{
					char buf[256];
					sprintf(buf, "Attacker: CrNr %d Better because alien creature.", crNr);
					if (config->debug)
						registerDebug(buf);
					bestCreatureNr = crNr;
				}
				//If attack only if attacked prioritize creature not on list or creatures attacking us with highest priority
				if ((!config->attackOnlyAttacking || creatureList[crNr].isAttacking) && creatureList[crNr].listPriority != 0 &&
				    (creatureList[crNr].listPriority > creatureList[bestCreatureNr].listPriority && creatureList[bestCreatureNr].listPriority != 0))
				{
					char buf[256];
					sprintf(buf, "Attacker: CrNr %d Better because higher priority.", crNr);
					if (config->debug)
						registerDebug(buf);
					bestCreatureNr = crNr;
				}
			}
			else if (isFarther)
			{
				if (creatureList[crNr].isAttacking &&
				    (creatureList[crNr].listPriority > creatureList[bestCreatureNr].listPriority && creatureList[bestCreatureNr].listPriority ||
				     creatureList[crNr].listPriority == 0 && creatureList[bestCreatureNr].listPriority != 0) &&
				    canGetToPoint(creatureList[crNr].x, creatureList[crNr].y, creatureList[crNr].z))
				{
					char buf[256];
					sprintf(buf, "Attacker: CrNr %d Better because attacking and higher priority.", crNr);
					if (config->debug)
						registerDebug(buf);
					bestCreatureNr = crNr;
				}
			}

/*
                        if (monstersSurrounding<=2){
                                if (!isFarther&&creatureList[crNr].listPriority>=creatureList[bestCreatureNr].listPriority){
                                        if (config->debug) registerDebug("Better:Closer");
                                        bestCreatureNr=crNr;
                                }
                        }else{
                                //If force attack prioritize creature not on list with lowest health
                                if ((config->forceAttackAfterAttack && creatureList[crNr].listPriority==0 && !isFarther && creatureList[crNr].isAttacking) &&
                                        (creatureList[crNr].hpPercLeft<creatureList[bestCreatureNr].hpPercLeft || creatureList[bestCreatureNr].listPriority!=0)){
                                        if (config->debug) registerDebug("Better:alien creature");
                                        bestCreatureNr=crNr;}
                                //If attack only if attacked prioritize creature not on list or creatures attacking us with highest priority
                                if ((!config->attackOnlyAttacking || creatureList[crNr].isAttacking) && creatureList[crNr].listPriority!=0  &&
                                        (creatureList[crNr].listPriority>creatureList[bestCreatureNr].listPriority && creatureList[bestCreatureNr].listPriority!=0)){
                                        if (config->debug) registerDebug("Better:Higher Priority");
                                        bestCreatureNr=crNr;}
                        }
 */
			char buf[256];
			sprintf(buf, "Nr=%d, name=%s\t isAttacking=%d, Prio=%d, fail#=%d, hp%%=%d, Id=%d, Onscreen=%d, Invis=%d, Dead=%d, lastAttackTm=%d, IgnoredUntil=%d, shouldAttack=%d, dist=%d", crNr, creatureList[crNr].name, creatureList[crNr].isAttacking, creatureList[crNr].listPriority, creatureList[crNr].failedAttacks, creatureList[crNr].hpPercLeft, creatureList[crNr].tibiaId, creatureList[crNr].isOnscreen, creatureList[crNr].isInvisible, creatureList[crNr].isDead, creatureList[crNr].lastAttackTm, creatureList[crNr].isIgnoredUntil, crNr == bestCreatureNr, taxiDist(self.x, self.y, creatureList[crNr].x, creatureList[crNr].y));
			if (config->debug)
				registerDebug(buf);
		}

		//Determine whether currently attacked creature is better than bestCreature
		if (bestCreatureNr != -1)
		{
			//If cannot attack currentlyAttackedCreature, switch to bestCreature
			if ((currentlyAttackedCreatureNr == -1
			     || !creatureList[currentlyAttackedCreatureNr].isOnscreen
			     || creatureList[currentlyAttackedCreatureNr].isInvisible
			     //|| creatureList[currentlyAttackedCreatureNr].isDead // if current creature is dead we still need to loot it
			     || creatureList[currentlyAttackedCreatureNr].hpPercLeft < config->attackHpAbove
			     || config->dontAttackPlayers && creatureList[currentlyAttackedCreatureNr].tibiaId<0x40000000
			                                                                                       || config->attackOnlyAttacking && !creatureList[currentlyAttackedCreatureNr].isAttacking
			                                                                                       || maxDist(self.x, self.y, creatureList[currentlyAttackedCreatureNr].x, creatureList[currentlyAttackedCreatureNr].y)>config->attackRange && creatureList[currentlyAttackedCreatureNr].hpPercLeft > 15 // only switch when the monstr is out of range if they don't seem to be inconveniently 'running in low health'
			     || creatureList[currentlyAttackedCreatureNr].isIgnoredUntil
			     || config->suspendOnEnemy && playersOnScreen && creatureList[currentlyAttackedCreatureNr].isAttacking == 0
			     || !(creatureList[currentlyAttackedCreatureNr].listPriority || config->forceAttackAfterAttack && creatureList[currentlyAttackedCreatureNr].isAttacking))
			    && (isLooterDone(config) || maxDist(self.x, self.y, creatureList[bestCreatureNr].x, creatureList[bestCreatureNr].y) <= 1))
			{
				currentlyAttackedCreatureNr = bestCreatureNr;
			}
			//if no harm in switching and monster is not already in low health and running, switch
			else if ((!reachedAttackedCreature || !config->stickToMonster)
			         && (!config->autoFollow || creatureList[currentlyAttackedCreatureNr].hpPercLeft > 15)
			         && creatureList[bestCreatureNr].listPriority != creatureList[currentlyAttackedCreatureNr].listPriority)
			{
				currentlyAttackedCreatureNr = bestCreatureNr;
				if (config->debug)
				{
					char buf[256];
					sprintf(buf, "Switching to Nr=%d name=%s point=%d,%d,%d id=%d", bestCreatureNr, creatureList[bestCreatureNr].name, creatureList[bestCreatureNr].x, creatureList[bestCreatureNr].y, creatureList[bestCreatureNr].z, creatureList[bestCreatureNr].tibiaId);
					registerDebug(buf);
				}
			}
		}
		else if (creatureList[currentlyAttackedCreatureNr].hpPercLeft != 0)
		{
			//if no best creature, then something probably ran away.quickly
			currentlyAttackedCreatureNr = -1;
		}
		// If attack packet never reached destination, then the red attack box is meaningless and we need to attack again
		if (currentlyAttackedCreatureNr != -1 && reader.getAttackedCreature() && reader.getAttackedCreature() == creatureList[currentlyAttackedCreatureNr].tibiaId)
		{
			int tmTmp = RandomVariableTime((int&)lastSendAttackTm, 10, 200, GET);
			if ((lastAttackedCreatureBloodHit == 0 || time(NULL) - lastAttackedCreatureBloodHit > tmTmp) && time(NULL) - lastSendAttackTm > tmTmp)
			{
				reader.setAttackedCreature(0);
				AttackCreature(config, creatureList[currentlyAttackedCreatureNr].tibiaId);
				RandomVariableTime((int&)lastSendAttackTm, 10, 200, MAKE);
				registerDebug("Resetting attack box as creature has not lost health in a while.");
			}
		}
		if (config->debug)
			registerDebug("Entering attack execution area");

		/**
		 * Do the training control things.
		 */
		int attackMode = config->mode + 1;
		trainingCheck(config, currentlyAttackedCreatureNr, alienCreatureForTrainerFound, monstersSurrounding, lastAttackedCreatureBloodHit, &attackMode);
		SendAttackMode(attackMode, config->autoFollow, -1, -1);

		CTibiaCharacter attackCh;
		reader.readVisibleCreature(&attackCh, currentlyAttackedCreatureNr);
		//perform server visible tasks if we have something to attack
		if (currentlyAttackedCreatureNr != -1 && attackCh.hpPercLeft)//uses most recent hpPercLeft to prevent crash??
		{
			CIpcMessage mess;
			while (CIPCBackPipe::readFromPipe(&mess, 1103))
			{
			};
			if (AttackCreature(config, creatureList[currentlyAttackedCreatureNr].tibiaId))
			{
				firstCreatureAttackTM   = time(NULL);
				reachedAttackedCreature = 0;
				if (config->autoFollow && messageThereIsNoWay(currentlyAttackedCreatureNr))
					creatureList[currentlyAttackedCreatureNr].isIgnoredUntil = time(NULL) + config->suspendAfterUnreachable;
			}

			// if we are backattacking alien then maybe fire runes at them
			// note: fire runes only if my own hp is over 50%
			// to avoid uh-exhaust
			if (!creatureList[currentlyAttackedCreatureNr].listPriority)
			{
				globalAutoAttackStateAttack = CToolAutoAttackStateAttack_attackingAlienFound;
				if (config->backattackRunes && self.hp > self.maxHp / 2)
				{
					if (config->debug)
						registerDebug("Firing SD at enemy");
					fireRunesAgainstCreature(config, creatureList[currentlyAttackedCreatureNr].tibiaId);
				}
			}
			else
			{
				globalAutoAttackStateAttack = CToolAutoAttackStateAttack_attackingCreature;
			}
			if (config->debug)
			{
				char buf[256];
				sprintf(buf, "Attacking Nr=%d name=%s point=%d,%d,%d id=%d ignore=%d hp%%=%d", bestCreatureNr, creatureList[bestCreatureNr].name, creatureList[bestCreatureNr].x, creatureList[bestCreatureNr].y, creatureList[bestCreatureNr].z, creatureList[bestCreatureNr].tibiaId, creatureList[bestCreatureNr].isIgnoredUntil, attackCh.hpPercLeft);
				registerDebug(buf);
			}
		}
		else
		{
			AttackCreature(config, 0);
			globalAutoAttackStateAttack = CToolAutoAttackStateAttack_notRunning;
			if (config->debug)
				registerDebug("No attack target found");
		}
		/*****
		   End attack process
		 ******/

		int attackE = GetTickCount();
		if (DISPLAY_TIMING)
		{
			TIMING_COUNTS[1] += 1;
			TIMING_TOTALS[1] += attackE - attackS;
			if (TIMING_COUNTS[1] == TIMING_MAX)
			{
				char bufDisp[400];
				sprintf(bufDisp, "attack %d", TIMING_TOTALS[1]);
				CPackSender::sendTAMessage(bufDisp);
				TIMING_COUNTS[1] = 0;
				TIMING_TOTALS[1] = 0;
			}
		}


		int walkS = GetTickCount();

		/*****
		   Start Walking Process
		 ******/
		if (currentlyAttackedCreatureNr == -1 && isLooterDone(config))//wis:make sure doesn't start while looting last monster
		{       //Waypoint selection algorithim	starts here
			if (self.x == depotX && self.y == depotY && self.z == depotZ)
			{
				if (config->debug)
					registerDebug("Depot reached");
				// depot waypoint reached!
				// then do the depot stuff
				depotDeposit(config);
			}
			else if (actualTargetX == self.x && actualTargetY == self.y && actualTargetZ == self.z && depotX == 0)
			{
				if (config->debug)
					registerDebug("Waypoint reached");
				// normal waypoint reached
				waypointTargetX             = waypointTargetY = waypointTargetZ = 0;
				walkerStandingEndTm         = time(NULL) + config->standStill;
				globalAutoAttackStateWalker = CToolAutoAttackStateWalker_standing;
				actualTargetX               = 0;
				actualTargetY               = 0;
				actualTargetZ               = 0;
			}

			//if Standing time not ended yet
			if (time(NULL) < walkerStandingEndTm)
			{
				globalAutoAttackStateWalker = CToolAutoAttackStateWalker_standing;
				if (config->debug)
					registerDebug("Standing");
				continue;
			}
			// no target found - go somewhere
			if (!waypointTargetX && !waypointTargetY && !waypointTargetZ)
			{
				if (config->debug)
					registerDebug("Getting location for walker.");

				if (depotX && depotY && depotZ)// || globalAutoAttackStateDepot==CToolAutoAttackStateDepot_walking) {
				{       // if we are walking to a depot - then just go there
					waypointTargetX = depotX;
					waypointTargetY = depotY;
					waypointTargetZ = depotZ;
					if (config->debug)
						registerDebug("Continuing to a depot");
				}
				//if waypoint recently reached OR no path found for 4 secs OR stuck for 10 secs find new waypoint
				else if (walkerStandingEndTm || globalAutoAttackStateWalker == CToolAutoAttackStateWalker_noPathFound && time(NULL) - currentPosTM > 4 || time(NULL) - currentPosTM > 10)
				{
					walkerStandingEndTm = 0;
					// find a new goto target from waypoint list
					if (!waypointsCount)
					{
						globalAutoAttackStateWalker = CToolAutoAttackStateWalker_noWaypoints;
						continue;
					}
					int nextWaypoint = 0;
					switch (config->waypointSelectMode)
					{
					case 0: // random
						//also exludes previous waypoint from being selected again
						if (waypointsCount != 1)
						{
							nextWaypoint  = waypointsCount ? (rand() % (waypointsCount - 1)) : 0;
							nextWaypoint += (int)(nextWaypoint >= currentWaypointNr);
						}
						currentWaypointNr = nextWaypoint;
						break;
					case 1: // round-robin
						currentWaypointNr++;
						if (currentWaypointNr == waypointsCount)
							currentWaypointNr = 0;
						break;
					case 2: // forward and back
						if (currentWaypointNr == waypointsCount - 1)
							forwardBackDir = -1;
						if (currentWaypointNr == 0)
							forwardBackDir = 1;
						if (waypointsCount > 1)
							currentWaypointNr += forwardBackDir;
						break;
					case 3: // reverse
						currentWaypointNr--;
						if (currentWaypointNr == -1)
							currentWaypointNr = waypointsCount - 1;
						break;
					}
				}
				//else continue to previous waypoint
				// y and z ==-1 means it is a delay
				if (config->waypointList[currentWaypointNr].y == -1 && config->waypointList[currentWaypointNr].z == -1)
				{
					int delay = config->waypointList[currentWaypointNr].x;
					walkerStandingEndTm = time(NULL) + delay;//replaces default standStill delay
					char buf[128];
					sprintf(buf, "Standing at delaypoint (%d) for %d sec", currentWaypointNr, delay);
					if (config->debug)
						registerDebug(buf);
				}
				else
				{
					waypointTargetX = config->waypointList[currentWaypointNr].x;
					waypointTargetY = config->waypointList[currentWaypointNr].y;
					waypointTargetZ = config->waypointList[currentWaypointNr].z;
					char buf[128];
					sprintf(buf, "Walking to waypoint (%d,%d,%d)", waypointTargetX, waypointTargetY, waypointTargetZ);
					if (config->debug)
						registerDebug(buf);
				}
			}                                               //End waypoint selection algorithim
			                                                //Waypoint walking algrithim starts here:
			if (waypointTargetX && waypointTargetY && !isInHalfSleep())
			{
				int mapUsed = config->mapUsed;
				switch (mapUsed)
				{
				// Tibia Auto map chosen
				case 1:
				{
					char buf[256];
					uint8_t path[15];
					reader.readSelfCharacter(&self);
					// proceed with path searching
					sprintf(buf, "findPathOnMap: standard walk (%d,%d,%d)->(%d,%d,%d)", self.x, self.y, self.z, waypointTargetX, waypointTargetY, waypointTargetZ);
					if (config->debug)
						registerDebug(buf);

					int ticksStart  = GetTickCount();
					point destPoint = CModuleUtil::findPathOnMap(self.x, self.y, self.z, waypointTargetX, waypointTargetY, waypointTargetZ, (depotX ? MAP_POINT_TYPE_DEPOT : 0), path, (rand() % max(1, config->radius)) + 1);
					actualTargetX = destPoint.x;
					actualTargetY = destPoint.y;
					actualTargetZ = destPoint.z;
					int ticksEnd = GetTickCount();
					if (DISPLAY_TIMING)
					{
						TIMING_COUNTS[5] += 1;
						TIMING_TOTALS[5] += ticksEnd - ticksStart;
						if (TIMING_COUNTS[5] == TIMING_MAX)
						{
							char bufDisp[400];
							sprintf(bufDisp, "path %d", TIMING_TOTALS[5]);
							CPackSender::sendTAMessage(bufDisp);
							TIMING_COUNTS[5] = 0;
							TIMING_TOTALS[5] = 0;
						}
					}
					if (DISPLAY_TIMING && 0)
					{
						char bufDisp[400];
						sprintf(bufDisp, "path %d", ticksEnd - ticksStart);
						CPackSender::sendTAMessage(bufDisp);
					}

					sprintf(buf, "timing: findPathOnMap() = %dms", ticksEnd - ticksStart);
					if (config->debug)
						registerDebug(buf);

					int executeS = GetTickCount();
					int pathSize;
					for (pathSize = 0; pathSize < 15 && path[pathSize]; pathSize++)
					{
					}
					if (actualTargetX && (pathSize || self.x == actualTargetX && actualTargetY == self.y && self.z == actualTargetZ))
					{
						CModuleUtil::executeWalk(self.x, self.y, self.z, path);
						globalAutoAttackStateWalker = CToolAutoAttackStateWalker_ok;
						if (config->debug)
							registerDebug("Walking: execute walk");
					}
					else
					{
						char buf[128];
						sprintf(buf, "Walking: no path found to (%d,%d,%d)", waypointTargetX, waypointTargetY, waypointTargetZ);
						if (config->debug)
							registerDebug(buf);

						// if no path found - then we forget current target
						waypointTargetX             = waypointTargetY = waypointTargetZ = 0;
						globalAutoAttackStateWalker = CToolAutoAttackStateWalker_noPathFound;
					}
					int executeE = GetTickCount();
					if (DISPLAY_TIMING)
					{
						TIMING_COUNTS[6] += 1;
						TIMING_TOTALS[6] += executeE - executeS;
						if (TIMING_COUNTS[6] == TIMING_MAX)
						{
							char bufDisp[400];
							sprintf(bufDisp, "execute %d", TIMING_TOTALS[6]);
							CPackSender::sendTAMessage(bufDisp);
							TIMING_COUNTS[6] = 0;
							TIMING_TOTALS[6] = 0;
						}
					}
					break;
				}                 // case 1
				// Tibia client map chosen
				default:
				{
					//Let's try something really new!
					//First: Let's map our path
					char buf[256];
					uint8_t path[15];
					reader.readSelfCharacter(&self);
					bool walking = false;
					if (self.z == waypointTargetZ)
					{
						//Let's try "clicking" there (Attempt 1)
						point startPoint;
						startPoint.x  = self.x;
						startPoint.y  = self.y;
						startPoint.z  = self.z;
						actualTargetX = waypointTargetX;
						actualTargetY = waypointTargetY;
						actualTargetZ = waypointTargetZ;
						reader.writeGotoCoords(actualTargetX, actualTargetY, actualTargetZ);
						walking = true;
						//Wait long enough to get a proper response from the white text
						int timeIn  = GetTickCount();
						int timeOut = GetTickCount();
						while (startPoint.x != self.x || startPoint.y != self.y || timeOut - timeIn < 1000)
						{
							timeOut = GetTickCount();
							Sleep(50);
						}
						char whiteText[15];                             //Stores white text.
						for (int j = 0; j < 15; j++)      //Long enough to store "There is no way"
						{
							whiteText[j] = (char)CMemUtil::getMemUtil().GetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + j);
						}
						//If we are still not moving kick TA in the butt to get her going
						if (strstr(whiteText, "There is no way") != 0 || strstr(whiteText, "Destination") != 0)
						{
							//Since Tibia merely times out the white text not override it, we need to change the text for subsequent iterations.
							//Let's have fun and give the users hope.  :P
							CMemUtil::getMemUtil().SetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + 9, 'T');
							CMemUtil::getMemUtil().SetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + 10, 'A');
							//Path blocked (parcels, crates, etc.)
							globalAutoAttackStateWalker = CToolAutoAttackStateWalker_noPathFound;
							walking                     = false;
						}
						else
						{
							globalAutoAttackStateWalker = CToolAutoAttackStateWalker_ok;
						}
					}
					if (!walking)
					{
						// proceed with path searching
						sprintf(buf, "findPathOnMap: standard walk (%d,%d,%d)->(%d,%d,%d)", self.x, self.y, self.z, waypointTargetX, waypointTargetY, waypointTargetZ);
						if (config->debug)
							registerDebug(buf);
						int ticksStart  = GetTickCount();
						point destPoint = CModuleUtil::findPathOnMap(self.x, self.y, self.z, waypointTargetX, waypointTargetY, waypointTargetZ, 0, path, (rand() % max(1, config->radius)) + 1);
						actualTargetX = destPoint.x;
						actualTargetY = destPoint.y;
						actualTargetZ = destPoint.z;
						int ticksEnd = GetTickCount();
						//Now let's try to "click" the mini	map. Let Tibia do the walking for us
						//Check the full path to our target
						int pathPointCount = CModuleUtil::GetPathTabCount();
						int pathPointIndex;
						//Limit searching to the first 200 relevant points or 0 for "all"
						if (pathPointCount >= 200)
							pathPointIndex = pathPointCount - 200;
						else
							pathPointIndex = 0;
						//Set a intermediate waypoint to walk to
						point pathPoint;
						pathPoint.x = 0;
						pathPoint.y = 0;
						pathPoint.z = 0;
						//We con only "click" a path to points on our level
						pathPoint = CModuleUtil::GetPathTab(pathPointIndex);
						while (pathPoint.z != self.z && pathPointIndex < pathPointCount)
						{
							pathPoint = CModuleUtil::GetPathTab(++pathPointIndex);
						}
						sprintf(buf, "self: (%d, %d, %d)\npathPoint: (%d, %d, %d),Index: %d", self.x, self.y, self.z, pathPoint.x, pathPoint.y, pathPoint.z, pathPointIndex);
						if (config->debug)
							registerDebug(buf);
						reader.readSelfCharacter(&self);
						//Are we close to needing to change levels?
						if (abs(self.x - pathPoint.x) <= 2 && abs(self.y - pathPoint.y) <= 2)
						{
							//For short paths TA map method proves best especially near rope spots and teleporters
							int pathSize;
							for (pathSize = 0; pathSize < 15 && path[pathSize]; pathSize++)
							{
							}
							if (pathSize || self.x == actualTargetX && actualTargetY == self.y && self.z == actualTargetZ)
							{
								CModuleUtil::executeWalk(self.x, self.y, self.z, path);
								globalAutoAttackStateWalker = CToolAutoAttackStateWalker_ok;
								if (config->debug)
									registerDebug("Walking: execute walk");
							}
							else
							{
								char buf[128];
								sprintf(buf, "Walking: no path found to (%d,%d,%d)", waypointTargetX, waypointTargetY, waypointTargetZ);
								if (config->debug)
									registerDebug(buf);
								// if no path found - then we forget current target
								waypointTargetX             = waypointTargetY = waypointTargetZ = 0;
								globalAutoAttackStateWalker = CToolAutoAttackStateWalker_noPathFound;
							}
						}
						//Did we find a pathPoint on our level?
						else if (pathPoint.z == self.z)
						{
							//Found it! Let's try "clicking" there (Attempt 2)
							reader.readSelfCharacter(&self);
							point startPoint;
							startPoint.x = self.x;
							startPoint.y = self.y;
							startPoint.z = self.z;
							reader.writeGotoCoords(pathPoint.x, pathPoint.y, pathPoint.z);
							//Wait long enough to get a proper responce from the white text
							int timeIn  = GetTickCount();
							int timeOut = GetTickCount();
							while (startPoint.x != self.x || startPoint.y != self.y || timeOut - timeIn < 500)
							{
								timeOut = GetTickCount();
							}
							char whiteText[15];                             //Stores white text.
							for (int j = 0; j < 15; j++)      //Long enough to store "There is no way"
							{
								whiteText[j] = (char)CMemUtil::getMemUtil().GetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + j);
							}
							//If we are still not moving kick TA in the butt to get her going
							if (strstr(whiteText, "There is no way") != 0 || strstr(whiteText, "Destination") != 0)
							{
								//Since Tibia merely times out the white text not override it, we need to change the text for subsequent iterations.
								//Let's have fun and give the users hope.  :P
								CMemUtil::getMemUtil().SetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + 9, 'T');
								CMemUtil::getMemUtil().SetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + 10, 'A');

								//Path blocked (parcels, crates, etc.)
								//One more time through path selection
								//Limit searching to the half the first selection
								pathPointIndex += (int)((pathPointCount - pathPointIndex) / 2);
								pathPoint.x     = 0;
								pathPoint.y     = 0;
								pathPoint.z     = 0;
								//We con only "click" a path to points on our level
								pathPoint = CModuleUtil::GetPathTab(pathPointIndex);
								while (pathPoint.z != self.z && pathPointIndex < pathPointCount)
								{
									pathPoint = CModuleUtil::GetPathTab(++pathPointIndex);
								}
								sprintf(buf, "self: (%d, %d, %d)\nHalf-Range pathPoint: (%d, %d, %d)<Index: %d", self.x, self.y, self.z, pathPoint.x, pathPoint.y, pathPoint.z, pathPointIndex);
								if (config->debug)
									registerDebug(buf);
								//Did we find a pathPoint on our level?
								if (pathPoint.z == self.z)
								{
									//Found it! Let's try "clicking" there (Attempt 3)
									reader.readSelfCharacter(&self);
									startPoint.x = self.x;
									startPoint.y = self.y;
									startPoint.z = self.z;
									reader.writeGotoCoords(pathPoint.x, pathPoint.y, pathPoint.z);
									//Wait longer this time to get a proper responce from the white text
									int timeIn  = GetTickCount();
									int timeOut = GetTickCount();
									while (startPoint.x != self.x || startPoint.y != self.y || timeOut - timeIn < 1000)
									{
										timeOut = GetTickCount();
									}
									char whiteText[15];                             //Stores white text.
									for (int j = 0; j < 15; j++)      //Long enough to store "There is no way"
									{
										whiteText[j] = (char)CMemUtil::getMemUtil().GetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + j);
									}
									if (strstr(whiteText, "There is no way") != 0 || strstr(whiteText, "Destination") != 0 || (startPoint.x == self.x && startPoint.y == self.y))
									{
										//Since Tibia merely times out the white text not delete it, we need to change the text for subsequent iterations.
										//Let's have fun and give the users hope.  :P
										CMemUtil::getMemUtil().SetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + 9, 'T');
										CMemUtil::getMemUtil().SetMemIntValue(CTibiaItem::getValueForConst("addrWhiteMessage") + 10, 'A');
										//Path blocked (parcels, crates, etc.)
										//Oh well, we tried. Let's let TA's executeWalk method take over for awhile.
										//Proceed with path searching
										sprintf(buf, "findPathOnMap: standard walk (%d,%d,%d)->(%d,%d,%d)", self.x, self.y, self.z, waypointTargetX, waypointTargetY, waypointTargetZ);
										if (config->debug)
											registerDebug(buf);
										int ticksStart  = GetTickCount();
										point destPoint = CModuleUtil::findPathOnMap(self.x, self.y, self.z, waypointTargetX, waypointTargetY, waypointTargetZ, 0, path, (rand() % max(1, config->radius)) + 1);
										int ticksEnd    = GetTickCount();
										int pathSize;
										for (pathSize = 0; pathSize < 15 && path[pathSize]; pathSize++)
										{
										}
										if (pathSize || self.x == destPoint.x && destPoint.y == self.y && self.z == destPoint.z)
										{
											CModuleUtil::executeWalk(self.x, self.y, self.z, path);
											globalAutoAttackStateWalker = CToolAutoAttackStateWalker_ok;
											if (config->debug)
												registerDebug("Walking: execute walk");
										}
										else
										{
											char buf[128];
											sprintf(buf, "Walking: no path found to (%d,%d,%d)", waypointTargetX, waypointTargetY, waypointTargetZ);
											if (config->debug)
												registerDebug(buf);

											// if no path found - then we forget current target
											waypointTargetX             = waypointTargetY = waypointTargetZ = 0;
											globalAutoAttackStateWalker = CToolAutoAttackStateWalker_noPathFound;
										}
									}
								}
							}
							else
							{
								globalAutoAttackStateWalker = CToolAutoAttackStateWalker_ok;
							}
						}
						else
						{
							//No point on our level we must use TA's executeWalk method
							sprintf(buf, "timing: findPathOnMap() = %dms", ticksEnd - ticksStart);
							if (config->debug)
								registerDebug(buf);
							int pathSize;
							for (pathSize = 0; pathSize < 15 && path[pathSize]; pathSize++)
							{
							}
							if (pathSize || self.x == actualTargetX && actualTargetY == self.y && self.z == actualTargetZ)
							{
								CModuleUtil::executeWalk(self.x, self.y, self.z, path);
								globalAutoAttackStateWalker = CToolAutoAttackStateWalker_ok;
								if (config->debug)
									registerDebug("Walking: execute walk");
							}
							else
							{
								char buf[128];
								sprintf(buf, "Walking: no path found to (%d,%d,%d)", waypointTargetX, waypointTargetY, waypointTargetZ);
								if (config->debug)
									registerDebug(buf);

								// if no path found - then we forget current target
								waypointTargetX             = waypointTargetY = waypointTargetZ = 0;
								globalAutoAttackStateWalker = CToolAutoAttackStateWalker_noPathFound;
							}
						}
					}
					break;
				}        // default
				}
			}                                               //End waypoint walking algorithim
		}                                                       // if (no currentlyAttackedCreatureNr)
		int walkE = GetTickCount();
		if (DISPLAY_TIMING)
		{
			TIMING_COUNTS[2] += 1;
			TIMING_TOTALS[2] += walkE - walkS;
			if (TIMING_COUNTS[2] == TIMING_MAX)
			{
				char bufDisp[400];
				sprintf(bufDisp, "walk %d", TIMING_TOTALS[2]);
				CPackSender::sendTAMessage(bufDisp);
				TIMING_COUNTS[2] = 0;
				TIMING_TOTALS[2] = 0;
			}
		}
	} // while

	if (shareAlienBackattack)
	{
		int zero = 0;
		sh_mem.SetValue(varName, &zero, 4);
	}
	// cancel attacks
	//CPackSender::stopAll();
	CPackSender::attack(0);

	reader.cancelAttackCoords();
	// restore attack mode
	SendAttackMode(reader.getPlayerModeAttackType(), reader.getPlayerModeFollow(), -1, -1);

	if (config->debug)
		registerDebug("Exiting cavebot");
	globalAutoAttackStateAttack   = CToolAutoAttackStateAttack_notRunning;
	globalAutoAttackStateLoot     = CToolAutoAttackStateLoot_notRunning;
	globalAutoAttackStateWalker   = CToolAutoAttackStateWalker_notRunning;
	globalAutoAttackStateDepot    = CToolAutoAttackStateDepot_notRunning;
	globalAutoAttackStateTraining = CToolAutoAttackStateTraining_notRunning;
	toolThreadShouldStop          = 0;
	lootThreadId                  = 0;
	queueThreadId                 = 0;

	if (strcmp(CVariableStore::getVariable("walking_control"), "cavebot") == 0)
	{
		CVariableStore::setVariable("walking_control", "");
		CVariableStore::setVariable("walking_priority", "0");
	}
	if (strcmp(CVariableStore::getVariable("walking_control"), "userpause") == 0)
	{
		CVariableStore::setVariable("walking_control", "");
		CVariableStore::setVariable("walking_priority", "0");
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CMod_cavebotApp construction

CMod_cavebotApp::CMod_cavebotApp()
{
	m_currentWaypointNr = 0;
	m_currentMonsterNr = 0;
	m_currentIgnoreNr = 0;
	m_currentDepotEntryNr = 0;
	m_currentDroplistEntryNr = 0;
	m_configDialog = NULL;
	m_started      = 0;
	m_configData   = new CConfigData();
}

CMod_cavebotApp::~CMod_cavebotApp()
{
	if (m_configDialog)
		deleteAndNull(m_configDialog);
	deleteAndNull(m_configData);
}

char * CMod_cavebotApp::getName()
{
	return "Cave bot";
}

int CMod_cavebotApp::isStarted()
{
	return m_started;
}

void CMod_cavebotApp::start()
{
	superStart();
	if (m_configDialog)
	{
		m_configDialog->disableControls();
		m_configDialog->activateEnableButton(true);
	}

	if (m_configData->gatherLootStats)
	{
		char installPath[1024];
		CModuleUtil::getInstallPath(installPath);
		char pathBuf[2048];
		sprintf(pathBuf, "%s\\tibiaauto-stats-loot.txt", installPath);
		FILE *f = fopen(pathBuf, "r");

		if (f)
		{
			fseek(f, 0, SEEK_END);

			int flen = ftell(f);
			fclose(f);
			if (flen > 1024 * 800 && m_configDialog)
			{
				CSendStats info;
				info.DoModal();
			}
		}
	}

	DWORD threadId;

	toolThreadShouldStop = 0;
	toolThreadHandle     = ::CreateThread(NULL, 0, toolThreadProc, m_configData, 0, &threadId);
	m_started            = 1;
}

void CMod_cavebotApp::stop()
{
	toolThreadShouldStop = 1;
	while (toolThreadShouldStop)
	{
		Sleep(50);
	}
	m_started = 0;

	if (m_configDialog)
	{
		m_configDialog->enableControls();
		m_configDialog->activateEnableButton(false);
	}
}

void CMod_cavebotApp::showConfigDialog()
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

void CMod_cavebotApp::configToControls()
{
	if (m_configDialog)
		m_configDialog->configToControls(m_configData);
}

void CMod_cavebotApp::controlsToConfig()
{
	if (m_configDialog)
	{
		deleteAndNull(m_configData);
		m_configData = m_configDialog->controlsToConfig();
	}
}

void CMod_cavebotApp::disableControls()
{
	if (m_configDialog)
		m_configDialog->disableControls();
}

void CMod_cavebotApp::enableControls()
{
	if (m_configDialog)
		m_configDialog->enableControls();
}

char *CMod_cavebotApp::getVersion()
{
	return "3.1";
}

int CMod_cavebotApp::validateConfig(int showAlerts)
{
	if (m_configData->capacityLimit < 0)
	{
		if (showAlerts)
			AfxMessageBox("Capacity limit must be >=0!");
		return 0;
	}
	if (m_configData->attackRange < 0 || m_configData->attackRange > 8)
	{
		if (showAlerts)
			AfxMessageBox("Attack range must be between 0 and 8!");
		return 0;
	}
	if (m_configData->suspendAfterUnreachable < 0)
	{
		if (showAlerts)
			AfxMessageBox("'suspend after unreachable' must be >=0!");
		return 0;
	}
	if (m_configData->unreachableAfter < 0)
	{
		if (showAlerts)
			AfxMessageBox("'unreachable after' must be >= 0!");
		return 0;
	}
	if (m_configData->standStill < 0)
	{
		if (showAlerts)
			AfxMessageBox("Stand still must be >=0!");
		return 0;
	}
	if (m_configData->attackHpAbove < 0 || m_configData->attackHpAbove > 100)
	{
		if (showAlerts)
			AfxMessageBox("Attack hp above % must be between 0 and 100!");
		return 0;
	}
	if (m_configData->depotCap < 0)
	{
		if (showAlerts)
			AfxMessageBox("Depot capacity must be >= 0!");
		return 0;
	}
	if (m_configData->radius < 0)
	{
		if (showAlerts)
			AfxMessageBox("Radius must be >= 0!");
		return 0;
	}

	return 1;
}

void CMod_cavebotApp::resetConfig()
{
	if (m_configData)
	{
		delete m_configData;
		m_configData = NULL;
	}
	m_configData = new CConfigData();
}

void CMod_cavebotApp::loadConfigParam(const char *paramName, char *paramValue)
{
	if (!strcmp(paramName, "attack/follow"))
		m_configData->autoFollow = (atoi(paramValue) ? 1 : 0);
	if (!strcmp(paramName, "attack/mode"))
		m_configData->mode = atoi(paramValue);
	if (!strcmp(paramName, "attack/attackAllMonsters"))
		m_configData->attackAllMonsters = (atoi(paramValue) ? 1 : 0);
	if (!strcmp(paramName, "attack/suspendOnEnemey"))
		m_configData->suspendOnEnemy = atoi(paramValue);
	if (!strcmp(paramName, "attack/suspendOnNoMove"))
		m_configData->suspendOnNoMove = atoi(paramValue);
	if (!strcmp(paramName, "attack/range"))
		m_configData->attackRange = atoi(paramValue);
	if (!strcmp(paramName, "attack/stick"))
		m_configData->stickToMonster = atoi(paramValue);
	if (!strcmp(paramName, "attack/unreachableAfter"))
		m_configData->unreachableAfter = atoi(paramValue);
	if (!strcmp(paramName, "attack/suspendAfterUnreachable"))
		m_configData->suspendAfterUnreachable = atoi(paramValue);
	if (!strcmp(paramName, "attack/attackOnlyAttacking"))
		m_configData->attackOnlyAttacking = atoi(paramValue);
	if (!strcmp(paramName, "attack/forceAttackAfterAttack"))
		m_configData->forceAttackAfterAttack = atoi(paramValue);
	if (!strcmp(paramName, "attack/hpAbove"))
		m_configData->attackHpAbove = atoi(paramValue);
	if (!strcmp(paramName, "attack/backattackRunes"))
		m_configData->backattackRunes = atoi(paramValue);
	if (!strcmp(paramName, "attack/shareAlienBackattack"))
		m_configData->shareAlienBackattack = atoi(paramValue);

	if (!strcmp(paramName, "loot/item/gp"))
		m_configData->lootGp = atoi(paramValue);
	if (!strcmp(paramName, "loot/item/custom"))
		m_configData->lootCustom = atoi(paramValue);
	if (!strcmp(paramName, "loot/item/worm"))
		m_configData->lootWorms = atoi(paramValue);
	if (!strcmp(paramName, "loot/item/food"))
		m_configData->lootFood = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/inbag"))
		m_configData->lootInBags = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/capacityLimit"))
		m_configData->capacityLimit = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/eatFromCorpse"))
		m_configData->eatFromCorpse = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/dropNotLooted"))
		m_configData->dropNotLooted = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/lootFromFloor"))
		m_configData->lootFromFloor = atoi(paramValue);

	if (!strcmp(paramName, "training/weaponTrain"))
		m_configData->weaponTrain = atoi(paramValue);
	if (!strcmp(paramName, "training/weaponFight"))
		m_configData->weaponFight = atoi(paramValue);
	if (!strcmp(paramName, "training/trainingMode"))
		m_configData->trainingMode = atoi(paramValue);
	if (!strcmp(paramName, "training/fightWhenSurrounded"))
		m_configData->fightWhenSurrounded = atoi(paramValue);
	if (!strcmp(paramName, "training/fightWhenAlien"))
		m_configData->fightWhenAlien = atoi(paramValue);
	if (!strcmp(paramName, "training/bloodHit"))
		m_configData->bloodHit = atoi(paramValue);
	if (!strcmp(paramName, "training/activate"))
		m_configData->trainingActivate = atoi(paramValue);
	if (!strcmp(paramName, "depot/depotDropInsteadOfDepositon"))
		m_configData->depotDropInsteadOfDeposit = atoi(paramValue);
	if (!strcmp(paramName, "depot/depotCap"))
		m_configData->depotCap = atoi(paramValue);
	if (!strcmp(paramName, "walker/radius"))
		m_configData->radius = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/whilekilling"))
		m_configData->lootWhileKill = atoi(paramValue);
	if (!strcmp(paramName, "attack/dontAttackPlayers"))
		m_configData->dontAttackPlayers = atoi(paramValue);

	if (!strcmp(paramName, "general/debug"))
		m_configData->debug = atoi(paramValue);

	// stats should be always gathered
	//if (!strcmp(paramName,"loot/stats/gather")) m_configData->gatherLootStats=atoi(paramValue);

	if (!strcmp(paramName, "walker/other/selectMode"))
		m_configData->waypointSelectMode = atoi(paramValue);
	if (!strcmp(paramName, "walker/other/mapUsed"))
		m_configData->mapUsed = atoi(paramValue);
	if (!strcmp(paramName, "walker/other/standAfterWaypointReached"))
		m_configData->standStill = atoi(paramValue);

	if (!strcmp(paramName, "walker/waypoint"))
	{
		if (m_currentWaypointNr == 0)
		{
			int i;
			for (i = 0; i < MAX_WAYPOINTCOUNT; i++)
			{
				m_configData->waypointList[i].x = 0;
				m_configData->waypointList[i].y = 0;
				m_configData->waypointList[i].z = 0;
			}
		}
		if (m_currentWaypointNr < MAX_WAYPOINTCOUNT){
			// y and z == -1 means delay instead of waypoint
			sscanf(paramValue, "%d,%d,%d",
				&m_configData->waypointList[m_currentWaypointNr].x,
				&m_configData->waypointList[m_currentWaypointNr].y,
				&m_configData->waypointList[m_currentWaypointNr].z);
		}
		else {
			if (m_currentWaypointNr == MAX_WAYPOINTCOUNT){
				char buf[256];
				sprintf(buf, "You cannot add more than %d waypoints to the cavebot waypoint list.", MAX_WAYPOINTCOUNT);
				AfxMessageBox(buf);
			}
		}
		m_currentWaypointNr++;
	}
	if (!strcmp(paramName, "attack/monster"))
	{
		if (m_currentMonsterNr == 0)
			m_configData->monsterCount = 0;
		if (m_currentMonsterNr < MAX_MONSTERLISTCOUNT)
		{
			strcpy(m_configData->monsterList[m_configData->monsterCount], paramValue);
			m_configData->monsterCount++;
		}
		else
		{
			if (m_currentMonsterNr == MAX_MONSTERLISTCOUNT)
			{
				char buf[256];
				sprintf(buf, "You cannot add more than %d monsters to the cavebot attack list.", MAX_MONSTERLISTCOUNT);
				AfxMessageBox(buf);
			}
		}
		m_currentMonsterNr++;
	}
	if (!strcmp(paramName, "attack/ignore"))
	{
		if (m_currentIgnoreNr == 0)
			m_configData->ignoreCount = 0;
		if (m_currentIgnoreNr < MAX_IGNORECOUNT){
			strcpy(m_configData->ignoreList[m_configData->ignoreCount], paramValue);
			m_configData->ignoreCount++;
		}
		else {
			if (m_currentIgnoreNr == MAX_IGNORECOUNT)
			{
				char buf[256];
				sprintf(buf, "You cannot add more than %d monsters to the cavebot ignore list.", MAX_IGNORECOUNT);
				AfxMessageBox(buf);
			}
		}
		m_currentIgnoreNr++;
	}
	if (!strcmp(paramName, "depot/entry"))
	{
		if (m_currentDepotEntryNr == 0)
		{
			int i;
			for (i = 0; i < MAX_DEPOTTRIGGERCOUNT; i++)
				m_configData->depotTrigger[i].itemName[0] = '\0';
		}
		if (m_currentDepotEntryNr < MAX_DEPOTTRIGGERCOUNT){
			sscanf(paramValue, "%d,%d,%63[^\t\r\n]", &m_configData->depotTrigger[m_currentDepotEntryNr].when, &m_configData->depotTrigger[m_currentDepotEntryNr].remain, m_configData->depotTrigger[m_currentDepotEntryNr].itemName);
			if (m_configData->depotTrigger[m_currentDepotEntryNr].itemName[0] == 0){
				m_currentDepotEntryNr--;
			}
		}
		else
		{
			if (m_currentDepotEntryNr == MAX_DEPOTTRIGGERCOUNT)
			{
				char buf[256];
				sprintf(buf, "You cannot add more than %d depot triggers to the depot walker trigger list.", MAX_DEPOTTRIGGERCOUNT);
				AfxMessageBox(buf);
			}
		}
		m_currentDepotEntryNr++;
	}
	if (!strcmp(paramName, "walker/other/standAfterWaypointReached"))
		m_configData->dropListCount = atoi(paramValue);
	if (!strcmp(paramName, "walker/other/standAfterWaypointReached"))
		m_configData->dropWhenCapacityLimitReached = atoi(paramValue);
	if (!strcmp(paramName, "walker/other/standAfterWaypointReached"))
		m_configData->dropOnlyLooted = atoi(paramValue);
	if (!strcmp(paramName, "walker/other/standAfterWaypointReached"))
		m_configData->standStill = atoi(paramValue);

	if (!strcmp(paramName, "loot/other/dropList"))
	{
		if (m_currentDroplistEntryNr == 0)
			m_configData->dropListCount = 0;
		if (m_currentDroplistEntryNr < MAX_DROPLISTCOUNT)
		{
			strcpy(m_configData->dropList[m_currentDroplistEntryNr], paramValue);
			m_configData->dropListCount++;
		}
		else
		{
			if (m_currentDroplistEntryNr == MAX_DROPLISTCOUNT)
			{
				char buf[256];
				sprintf(buf, "You cannot add more than %d drop items to the loot drop list.", MAX_DROPLISTCOUNT);
				AfxMessageBox(buf);
			}
		}
		m_currentDroplistEntryNr++;
	}

	if (!strcmp(paramName, "loot/other/dropWhenCapacityLimitReached"))
		m_configData->dropWhenCapacityLimitReached = atoi(paramValue);
	if (!strcmp(paramName, "loot/other/dropOnlyLooted"))
		m_configData->dropOnlyLooted = atoi(paramValue);
	if (!strcmp(paramName, "depot/depotModPriority"))
		sprintf(m_configData->depotModPriorityStr, "%s", paramValue);
	if (!strcmp(paramName, "depot/stopByDepot"))
		m_configData->stopByDepot = atoi(paramValue);
	if (!strcmp(paramName, "depot/depositLootedItemList"))
		m_configData->depositLootedItemList = atoi(paramValue);
}

char *CMod_cavebotApp::saveConfigParam(const char *paramName)
{
	static char buf[1024];
	buf[0] = 0;

	if (!strcmp(paramName, "attack/follow"))
		sprintf(buf, "%d", m_configData->autoFollow);
	if (!strcmp(paramName, "attack/mode"))
		sprintf(buf, "%d", m_configData->mode);
	if (!strcmp(paramName, "attack/attackAllMonsters"))
		sprintf(buf, "%d", m_configData->attackAllMonsters);
	if (!strcmp(paramName, "attack/suspendOnEnemey"))
		sprintf(buf, "%d", m_configData->suspendOnEnemy);
	if (!strcmp(paramName, "attack/suspendOnNoMove"))
		sprintf(buf, "%d", m_configData->suspendOnNoMove);
	if (!strcmp(paramName, "attack/range"))
		sprintf(buf, "%d", m_configData->attackRange);
	if (!strcmp(paramName, "attack/stick"))
		sprintf(buf, "%d", m_configData->stickToMonster);
	if (!strcmp(paramName, "attack/unreachableAfter"))
		sprintf(buf, "%d", m_configData->unreachableAfter);
	if (!strcmp(paramName, "attack/suspendAfterUnreachable"))
		sprintf(buf, "%d", m_configData->suspendAfterUnreachable);
	if (!strcmp(paramName, "attack/attackOnlyAttacking"))
		sprintf(buf, "%d", m_configData->attackOnlyAttacking);
	if (!strcmp(paramName, "attack/forceAttackAfterAttack"))
		sprintf(buf, "%d", m_configData->forceAttackAfterAttack);
	if (!strcmp(paramName, "attack/hpAbove"))
		sprintf(buf, "%d", m_configData->attackHpAbove);
	if (!strcmp(paramName, "attack/backattackRunes"))
		sprintf(buf, "%d", m_configData->backattackRunes);
	if (!strcmp(paramName, "attack/shareAlienBackattack"))
		sprintf(buf, "%d", m_configData->shareAlienBackattack);

	if (!strcmp(paramName, "loot/item/gp"))
		sprintf(buf, "%d", m_configData->lootGp);
	if (!strcmp(paramName, "loot/item/custom"))
		sprintf(buf, "%d", m_configData->lootCustom);
	if (!strcmp(paramName, "loot/item/worm"))
		sprintf(buf, "%d", m_configData->lootWorms);
	if (!strcmp(paramName, "loot/item/food"))
		sprintf(buf, "%d", m_configData->lootFood);
	if (!strcmp(paramName, "loot/other/inbag"))
		sprintf(buf, "%d", m_configData->lootInBags);
	if (!strcmp(paramName, "loot/other/capacityLimit"))
		sprintf(buf, "%d", m_configData->capacityLimit);
	if (!strcmp(paramName, "loot/other/eatFromCorpse"))
		sprintf(buf, "%d", m_configData->eatFromCorpse);
	if (!strcmp(paramName, "loot/stats/gather"))
		sprintf(buf, "%d", m_configData->gatherLootStats);
	if (!strcmp(paramName, "loot/other/dropNotLooted"))
		sprintf(buf, "%d", m_configData->dropNotLooted);
	if (!strcmp(paramName, "loot/other/lootFromFloor"))
		sprintf(buf, "%d", m_configData->lootFromFloor);

	if (!strcmp(paramName, "walker/other/selectMode"))
		sprintf(buf, "%d", m_configData->waypointSelectMode);
	if (!strcmp(paramName, "walker/other/mapUsed"))
		sprintf(buf, "%d", m_configData->mapUsed);
	if (!strcmp(paramName, "walker/other/standAfterWaypointReached"))
		sprintf(buf, "%d", m_configData->standStill);

	if (!strcmp(paramName, "general/debug"))
		sprintf(buf, "%d", m_configData->debug);

	if (!strcmp(paramName, "attack/monster") && m_currentMonsterNr < m_configData->monsterCount && m_currentMonsterNr < MAX_MONSTERLISTCOUNT)
	{
		strcpy(buf, m_configData->monsterList[m_currentMonsterNr]);
		m_currentMonsterNr++;
	}
	if (!strcmp(paramName, "attack/ignore") && m_currentIgnoreNr < m_configData->ignoreCount && m_currentIgnoreNr < MAX_IGNORECOUNT)
	{
		strcpy(buf, m_configData->ignoreList[m_currentIgnoreNr]);
		m_currentIgnoreNr++;
	}
	if (!strcmp(paramName, "walker/waypoint") && (m_configData->waypointList[m_currentWaypointNr].x || m_configData->waypointList[m_currentWaypointNr].y || m_configData->waypointList[m_currentWaypointNr].z) && m_currentWaypointNr < MAX_WAYPOINTCOUNT)
	{
		// y and z == -1 means delay
		sprintf(buf, "%d,%d,%d", m_configData->waypointList[m_currentWaypointNr].x, m_configData->waypointList[m_currentWaypointNr].y, m_configData->waypointList[m_currentWaypointNr].z);
		m_currentWaypointNr++;
	}
	if (!strcmp(paramName, "depot/entry") && strlen(m_configData->depotTrigger[m_currentDepotEntryNr].itemName) && m_currentDepotEntryNr < MAX_DEPOTTRIGGERCOUNT)
	{
		sprintf(buf, "%d,%d,%s", m_configData->depotTrigger[m_currentDepotEntryNr].when, m_configData->depotTrigger[m_currentDepotEntryNr].remain, m_configData->depotTrigger[m_currentDepotEntryNr].itemName);
		m_currentDepotEntryNr++;
	}

	if (!strcmp(paramName, "training/weaponTrain"))
		sprintf(buf, "%d", m_configData->weaponTrain);
	if (!strcmp(paramName, "training/weaponFight"))
		sprintf(buf, "%d", m_configData->weaponFight);
	if (!strcmp(paramName, "training/trainingMode"))
		sprintf(buf, "%d", m_configData->trainingMode);
	if (!strcmp(paramName, "training/fightWhenSurrounded"))
		sprintf(buf, "%d", m_configData->fightWhenSurrounded);
	if (!strcmp(paramName, "training/fightWhenAlien"))
		sprintf(buf, "%d", m_configData->fightWhenAlien);
	if (!strcmp(paramName, "training/bloodHit"))
		sprintf(buf, "%d", m_configData->bloodHit);
	if (!strcmp(paramName, "training/activate"))
		sprintf(buf, "%d", m_configData->trainingActivate);
	if (!strcmp(paramName, "depot/depotDropInsteadOfDepositon"))
		sprintf(buf, "%d", m_configData->depotDropInsteadOfDeposit);
	if (!strcmp(paramName, "depot/depotCap"))
		sprintf(buf, "%d", m_configData->depotCap);
	if (!strcmp(paramName, "walker/radius"))
		sprintf(buf, "%d", m_configData->radius);
	if (!strcmp(paramName, "loot/other/whilekilling"))
		sprintf(buf, "%d", m_configData->lootWhileKill);
	if (!strcmp(paramName, "attack/dontAttackPlayers"))
		sprintf(buf, "%d", m_configData->dontAttackPlayers);
	if (!strcmp(paramName, "loot/other/dropList") && m_currentDroplistEntryNr < m_configData->dropListCount && m_currentDroplistEntryNr < MAX_DROPLISTCOUNT)
	{
		strcpy(buf, m_configData->dropList[m_currentDroplistEntryNr]);
		m_currentDroplistEntryNr++;
	}

	if (!strcmp(paramName, "loot/other/dropWhenCapacityLimitReached"))
		sprintf(buf, "%d", m_configData->dropWhenCapacityLimitReached);
	if (!strcmp(paramName, "loot/other/dropOnlyLooted"))
		sprintf(buf, "%d", m_configData->dropOnlyLooted);
	if (!strcmp(paramName, "depot/depotModPriority"))
		sprintf(buf, "%s", m_configData->depotModPriorityStr);
	if (!strcmp(paramName, "depot/stopByDepot"))
		sprintf(buf, "%d", m_configData->stopByDepot);
	if (!strcmp(paramName, "depot/depositLootedItemList"))
		sprintf(buf, "%d", m_configData->depositLootedItemList);
	return buf;
}

static const char *configParamNames[] =
{

	"attack/follow",
	"attack/mode",
	"attack/suspendOnEnemey",
	"attack/monster",
	"loot/item/gp",
	"loot/item/custom",
	"loot/item/worm",
	"loot/item/food",
	"loot/other/inbag",
	"loot/other/capacityLimit",
	"walker/waypoint",
	"walker/other/selectMode",
	"walker/other/mapUsed",
	"loot/stats/gather",
	"attack/suspendOnNoMove",
	"attack/range",
	"loot/other/eatFromCorpse",
	"attack/stick",
	"attack/unreachableAfter",
	"attack/suspendAfterUnreachable",
	"attack/attackOnlyAttacking",
	"attack/forceAttackAfterAttack",
	"walker/other/standAfterWaypointReached",
	"depot/entry",
	"general/debug",
	"training/weaponTrain",
	"training/weaponFight",
	"training/fightWhenSurrounded",
	"training/fightWhenAlien",
	"training/bloodHit",
	"training/activate",
	"depot/depotDropInsteadOfDepositon",
	"loot/other/dropNotLooted",
	"loot/other/lootFromFloor",
	"attack/hpAbove",
	"attack/ignore",
	"attack/backattackRunes",
	"attack/shareAlienBackattack",
	"depot/depotCap",
	"training/trainingMode",
	"walker/radius",
	"loot/other/whilekilling",
	"attack/dontAttackPlayers",
	"loot/other/dropList",
	"loot/other/dropWhenCapacityLimitReached",
	"loot/other/dropOnlyLooted",
	"depot/depotModPriority",
	"depot/stopByDepot",
	"depot/depositLootedItemList",
	"attack/attackAllMonsters",
	NULL,
};

const char **CMod_cavebotApp::getConfigParamNames()
{
	return configParamNames;
}

int CMod_cavebotApp::isMultiParam(const char *paramName)
{
	if (!strcmp(paramName, "walker/waypoint"))
		return 1;
	if (!strcmp(paramName, "attack/monster"))
		return 1;
	if (!strcmp(paramName, "depot/entry"))
		return 1;
	if (!strcmp(paramName, "attack/ignore"))
		return 1;
	if (!strcmp(paramName, "loot/other/dropList"))
		return 1;
	return 0;
}

void CMod_cavebotApp::resetMultiParamAccess(const char *paramName)
{
	if (!strcmp(paramName, "walker/waypoint"))
		m_currentWaypointNr = 0;
	if (!strcmp(paramName, "attack/monster"))
		m_currentMonsterNr = 0;
	if (!strcmp(paramName, "depot/entry"))
		m_currentDepotEntryNr = 0;
	if (!strcmp(paramName, "attack/ignore"))
		m_currentIgnoreNr = 0;
	if (!strcmp(paramName, "loot/other/dropList"))
		m_currentDroplistEntryNr = 0;
}

void CMod_cavebotApp::getNewSkin(CSkin newSkin)
{
	skin = newSkin;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (m_configDialog)
	{
		m_configDialog->DoSetButtonSkin();
		m_configDialog->Invalidate();
	}
}
