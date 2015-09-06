#pragma once
#include "PythonScript.h"

PyObject *tibiaauto_reader_setProcessId(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readSelfCharacter(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readOpenContainerCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readContainerItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeSelfLightPower(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeSelfLightColor(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readSelfLightPower(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readSelfLightColor(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_cancelAttackCoords(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeGotoCoords(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getLoggedCharNr(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getSelfEventFlags(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeVisibleCreatureName(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getTradeItemPartner(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getTradeItemSelf(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getTradeCountPartner(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getTradeCountSelf(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getAttackedCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setAttackedCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getFollowedCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setFollowedCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_GetLoggedChar(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readBattleListMax(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readBattleListMin(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readVisibleCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getCharacterByTibiaId(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapGetPointItemsCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapGetPointItemId(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapSetPointItemsCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapSetPointItemId(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapGetPointItemExtraInfo(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapGetPointStackIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getCurrentTm(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setRemainingTilesToGo(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setMemIntValue(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getMemIntValue(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeEnableRevealCName(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeDisableRevealCName(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getProcessId(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getBaseAddr(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getKernelMainVersion(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getKernelPatchVersion(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeCreatureLightPower(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeCreatureLightColor(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readCreatureLightPower(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readCreatureLightColor(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getTibiaTile(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setGlobalVariable(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getGlobalVariable(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useWithObjectFromFloorOnFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useWithObjectFromFloorInContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useWithObjectFromContainerInContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useWithObjectFromContainerOnFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useWithObjectOnFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useWithObjectInContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_moveObjectFromFloorToFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_moveObjectFromFloorToContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_moveObjectBetweenContainers(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_moveObjectFromContainerToFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_castRuneAgainstCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_castRuneAgainstHuman(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useItemOnCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useItemFromContainerOnCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useItemFromFloorOnCreature(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendTAMessage(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useItemOnFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_useItemInContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_openAutoContainerFromFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_openContainerFromFloor(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_openAutoContainerFromContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_openContainerFromContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_say(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sayWhisper(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sayYell(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sayNPC(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_tell(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sayOnChan(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_npcBuy(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_npcSell(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_logout(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_walkOnTAMap(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_closeContainer(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_attackMode(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_attack(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_follow(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_turnLeft(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_turnRight(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_turnUp(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_turnDown(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stopAll(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepMulti(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepLeft(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepRight(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepUp(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepDown(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepUpRight(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepDownRight(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepUpLeft(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_stepDownLeft(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_printText(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendMount(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendDismount(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendDirectPacket(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendCreatureInfo(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_look(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_ignoreLook(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendAutoAimConfig(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendClearCreatureInfo(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_enableCName(PyObject *self, PyObject *args);
PyObject *tibiaauto_sender_sendAttackedCreatureToAutoAim(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_removePointAvailable(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_prohPointClear(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_prohPointAdd(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_getPointType(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_getPointTypeNoProh(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_setPointType(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_getPrevPointZ(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_getPrevPointY(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_getPrevPointX(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_clearPrevPoint(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_setPrevPoint(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_clear(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_setPointAsAvailable(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_isPointAvailable(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_isPointAvailableNoProh(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_size(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_isPointInMiniMap(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_getMiniMapPoint(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_setPointLocked(PyObject *self, PyObject *args);
PyObject *tibiaauto_map_isPointLocked(PyObject *self, PyObject *args);
PyObject *tibiaauto_regexp_match(PyObject *self, PyObject *args);
PyObject *tibiaauto_alice_respond(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemName(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemId(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getFoodIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getLootItemIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemIdAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getFoodIdAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getLootItemIdAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemNameAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getFoodNameAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getLootItemNameAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getFoodTimeAtIndex(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_addItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_addFood(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_addLootItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_removeItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_removeFood(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_removeLootItem(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_clearFoodList(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getFoodCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getLootItemCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getValueForConst(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_refreshItemLists(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_saveItemLists(PyObject *self, PyObject *args);
/* Deprecated Section Start*/
PyObject *tibiaauto_item_getName(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getObjectId(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getCorpseIdByCreatureName(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsItems(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsItemsId(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsItemsCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsFood(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsFoodId(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsFoodCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsCorpses(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsCorpsesId(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsCorpsesCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsLooted(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsLootedId(PyObject *self, PyObject *args);
PyObject *tibiaauto_item_getItemsLootedCount(PyObject *self, PyObject *args);
/* Deprecated Section End*/
PyObject *tibiaauto_kernel_startModule(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_stopModule(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getModuleCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getModuleName(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getModuleDesc(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getModuleVersion(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_isModuleStarted(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_startPythonModule(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_stopPythonModule(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getPythonModuleCount(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getPythonModuleName(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getPythonModuleDesc(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_getPythonModuleVersion(PyObject *self, PyObject *args);
PyObject *tibiaauto_kernel_isPythonModuleStarted(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readMiniMap(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readMiniMapLabel(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readMiniMapPoint(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeMiniMapPoint(PyObject *self, PyObject *args);
PyObject *tibiaauto_crstat_count(PyObject *self, PyObject *args);
PyObject *tibiaauto_crstat_tibiaId(PyObject *self, PyObject *args);
PyObject *tibiaauto_crstat_name(PyObject *self, PyObject *args);
PyObject *tibiaauto_crstat_inarea(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setMainWindowText(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setMainTrayText(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getPlayerModeAttackPlayers(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getPlayerModeAttackType(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getPlayerModeFollow(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getPlayerModePVP(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getConnectionState(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_isLoggedIn(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getOpenWindowName(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_setXRayValues(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getXRayValue1(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getXRayValue2(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_writeCreatureDeltaXY(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getCreatureDeltaX(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_getCreatureDeltaY(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_readVIPEntry(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapGetPointTopPos(PyObject *self, PyObject *args);
PyObject *tibiaauto_reader_mapGetPointSeenOnTopPos(PyObject *self, PyObject *args);
PyObject *tibiaauto_packet_first(PyObject *self, PyObject *args);
