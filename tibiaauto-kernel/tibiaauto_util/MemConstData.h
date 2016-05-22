#pragma once

#include "tibiaauto_util.h"

class TIBIAAUTOUTIL_API CMemConstData
{
public:
	CMemConstData();

	int m_memAddressXor;
	int m_memAddressPacketCount;
	int m_memAddressVIP;
	int m_memAddressFirstContainer;
	int m_memAddressFirstCreature;
	int m_memAddressHP;
	int m_memAddressMana;
	int m_memAddressHPMax;
	int m_memAddressManaMax;
	int m_memAddressCap;
	int m_memAddressStamina;
	int m_memAddressExp;
	int m_memAddressLvl;
	int m_memAddressMlvl;
	int m_memAddressMlvlPercLeft;
	int m_memAddressSoulPoints;
	int m_memAddressLeftHand;
	int m_memAddressBackpack;
	int m_memAddressRightHand;
	int m_memAddressSlotArrow;
	int m_memAddressSelfPosX;
	int m_memAddressSelfPosY;
	int m_memAddressSelfPosZ;
	int m_memAddressBattleMin;
	int m_memAddressBattleMax;
	int m_memAddressAttackedCreature;
	int m_memAddressFollowedCreature;
	int m_memAddressSkillSword;
	int m_memAddressSkillAxe;
	int m_memAddressSkillShield;
	int m_memAddressSkillClub;
	int m_memAddressSkillDist;
	int m_memAddressSkillFish;
	int m_memAddressSkillSwordPercLeft;
	int m_memAddressSkillAxePercLeft;
	int m_memAddressSkillShieldPercLeft;
	int m_memAddressSkillClubPercLeft;
	int m_memAddressSkillDistPercLeft;
	int m_memAddressSkillFishPercLeft;
	int m_memAddressTradeCountSelf;
	int m_memAddressTradeCountPartner;
	int m_memAddressTradeFirstItemSelf;
	int m_memAddressTradeFirstItemPartner;
	int m_memAddressSelfFlags;
	int m_memAddressGoX;
	int m_memAddressGoY;
	int m_memAddressGoZ;
	int m_memAddressVocation;
	int m_memAddressRevealCName1;
	int m_memAddressRevealCName2;
	int m_memAddressRevealCName3;
	int m_memAddressRevealCName4;
	int m_memAddressSelfId;
	int m_memAddressMapStart;
	int m_memAddressMiniMapStart;
	int m_memAddressCurrentTm;
	int m_memAddressCurrentTileToGo;
	int m_memAddressTilesToGo;
	int m_memAddressPathToGo;
	int m_memAddressSkillFist;
	int m_memAddressSkillFistPercLeft;

	int m_memAddressFunTibiaPrintText;
	int m_memAddressFunTibiaPlayerNameText;
	int m_memAddressFunTibiaInfoMiddleScreen;
	int m_memAddressFunTibiaIsCreatureVisible;
	int m_memAddressFunTibiaEncrypt;
	int m_memAddressFunTibiaDecrypt;
	int m_memAddressFunTibiaShouldParseRecv;
	int m_memAddressFunTibiaInfoMessageBox;
	int m_memAddressArrayPtrRecvStream;
	int m_memAddressCallDrawRect;
	int m_memAddressCallDrawBlackRect;
	int m_memAddressCallPrintText01;
	int m_memAddressCallPrintText02;
	int m_memAddressCallPrintText03;
	int m_memAddressCallPrintText04;
	int m_memAddressCallPrintText05;
	int m_memAddressCallPlayerNameText01;
	int m_memAddressCallPlayerNameText02;
	int m_memAddressCallPlayerNameText03;
	int m_memAddressCallPlayerNameText04;
	int m_memAddressCallPlayerNameText05;
	int m_memAddressCallPlayerNameText06;
	int m_memAddressCallPlayerNameText07;
	int m_memAddressCallPlayerNameText08;
	int m_memAddressCallPlayerNameText09;
	int m_memAddressCallPlayerNameText10;
	int m_memAddressCallInfoMiddleScreen01;
	int m_memAddressCallInfoMiddleScreen02;
	int m_memAddressCallInfoMiddleScreen03;
	int m_memAddressCallInfoMiddleScreen04;
	int m_memAddressCallInfoMessageBox01;
	int m_memAddressCallInfoMessageBox02;
	int m_memAddressCallInfoMessageBox03;
	int m_memAddressCallInfoMessageBox04;
	int m_memAddressCallInfoMessageBox05;
	int m_memAddressCallInfoMessageBox06;
	int m_memAddressCallInfoMessageBox07;
	int m_memAddressCallInfoMessageBox08;
	int m_memAddressCallInfoMessageBox09;
	int m_memAddressCallInfoMessageBox10;
	int m_memAddressCallInfoMessageBox11;
	int m_memAddressCallInfoMessageBox12;
	int m_memAddressCallInfoMessageBox13;
	int m_memAddressCallEncrypt01;
	int m_memAddressCallDecrypt01;
	int m_memAddressCallShouldParseRecv01;

	int m_memLengthContainer;
	int m_memLengthItem;
	int m_memLengthCreature;
	int m_memLengthMapTile;
	int m_memLengthVIP;


	int m_memMaxContainers;
	int m_memMaxCreatures;
	int m_memMaxMapTiles;

	int m_offsetCreatureVisible;
};
