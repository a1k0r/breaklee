#include "ClientProtocol.h"
#include "ClientProcedures.h"
#include "ClientSocket.h"
#include "Enumerations.h"
#include "IPCProcedures.h"
#include "Notification.h"
#include "Server.h"

CLIENT_PROCEDURE_BINDING(CREATE_CHARACTER) {
	S2C_DATA_CREATE_CHARACTER* Response = PacketBufferInit(Connection->PacketBuffer, S2C, CREATE_CHARACTER);

	if (!(Client->Flags & CLIENT_FLAGS_CHARACTER_INDEX_LOADED) || Client->Account.AccountID < 1) {
		SocketDisconnect(Socket, Connection);
		return;
	}

	if (Character) {
		Client->AccountInfo = Character->Data.AccountInfo;
	}

	if (!(Client->AccountInfo.CharacterSlotOpenMask & (1 << Packet->SlotIndex)) ||
		Client->Characters[Packet->SlotIndex].ID > 0) {
		Response->CharacterStatus = CREATE_CHARACTER_STATUS_NOT_ALLOWED;
		SocketSend(Socket, Connection, Response);
		return;
	}

	if (Packet->NameLength < MIN_CHARACTER_NAME_LENGTH ||
		Packet->NameLength > MAX_CHARACTER_NAME_LENGTH) {
		Response->CharacterStatus = CREATE_CHARACTER_STATUS_NAME_VALIDATION_FAILED;
		SocketSend(Socket, Connection, Response);
		return;
	}

	UInt32 RawStyle = SwapUInt32(Packet->Style);
	union _RTCharacterStyle Style = *((union _RTCharacterStyle*)&RawStyle);
	UInt32 BattleStyleIndex = Style.BattleStyle | (Style.ExtendedBattleStyle << 3);

	if (Style.Padding1 > 0 ||
		Style.Padding2 > 0 ||
		Style.Padding3 > 0 ||
		Style.Padding4 > 0 ||
		Style.Unknown1 < 1 ||
		Style.AuraCode > 0 ||
		Style.ShowHelmed < 1 ||
		Style.HairStyle >= MAX_CHARACTER_NORMAL_HAIR_COUNT ||
		BattleStyleIndex < RUNTIME_DATA_CHARACTER_BATTLE_STYLE_INDEX_MIN ||
		BattleStyleIndex > RUNTIME_DATA_CHARACTER_BATTLE_STYLE_INDEX_MAX ||
		Style.Face >= MAX_CHARACTER_NORMAL_FACE_COUNT ||
		Style.HairColor >= MAX_CHARACTER_NORMAL_HAIR_COLOR_COUNT ||
		Style.BattleRank > 1) {
		Response->CharacterStatus = CREATE_CHARACTER_STATUS_NOT_ALLOWED;
		SocketSend(Socket, Connection, Response);
		return;
	}

	if (Packet->CreateSpecialCharacter && Context->Config.WorldSvr.DebugCharacter) {
		Style.BattleRank = 10;
	}

	IPC_W2D_DATA_CREATE_CHARACTER* Request = IPCPacketBufferInit(Server->IPCSocket->PacketBuffer, W2D, CREATE_CHARACTER);
	memset(&Request->CharacterData, 0, sizeof(struct _RTCharacterData));
	Request->Header.SourceConnectionID = Connection->ID;
	Request->Header.Source = Server->IPCSocket->NodeID;
	Request->Header.Target.Group = Context->Config.WorldSvr.GroupIndex;
	Request->Header.Target.Type = IPC_TYPE_MASTERDB;
	Request->AccountID = Client->Account.AccountID;
	Request->CharacterSlotIndex = Packet->SlotIndex;
	Request->CharacterNameLength = Packet->NameLength;
	memcpy(Request->CharacterName, Packet->Name, Packet->NameLength);

	struct _RuntimeDataCharacterTemplate* CharacterTemplate = &Context->RuntimeData->CharacterTemplate[BattleStyleIndex - 1];
	if (CharacterTemplate->BattleStyleIndex != BattleStyleIndex) {
		Response->CharacterStatus = CREATE_CHARACTER_STATUS_DBERROR;
		SocketSend(Socket, Connection, Response);
		return;
	}

	Request->CharacterData.Info.Resource.HP = INT32_MAX;
	Request->CharacterData.Info.Resource.MP = INT32_MAX;
	Request->CharacterData.Info.Resource.SP = INT32_MAX;
	Request->CharacterData.Info.Resource.BP = INT32_MAX;
	Request->CharacterData.Info.Resource.DP = 0;
	Request->CharacterData.Info.Basic.Level = 1;
	Request->CharacterData.Info.Skill.Rank = 1;
	Request->CharacterData.Info.Skill.Level = 0;
	Request->CharacterData.Info.Style = Style;

	RTDataCharacterInitRef CharacterInit = RTRuntimeDataCharacterInitGet(Context->Runtime->Context, BattleStyleIndex);
	Request->CharacterData.Info.Position.WorldID = CharacterInit->WorldID;
	Request->CharacterData.Info.Position.X = CharacterInit->X;
	Request->CharacterData.Info.Position.Y = CharacterInit->Y;

	if (CharacterInit->Suit > 0) {
		RTItemSlotRef ItemSlot = &Request->CharacterData.EquipmentInfo.EquipmentSlots[Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount];
		ItemSlot->Item.ID = CharacterInit->Suit;
		ItemSlot->SlotIndex = RUNTIME_EQUIPMENT_SLOT_INDEX_SUIT;
		Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount += 1;
	}

	if (CharacterInit->Glove > 0) {
		RTItemSlotRef ItemSlot = &Request->CharacterData.EquipmentInfo.EquipmentSlots[Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount];
		ItemSlot->Item.ID = CharacterInit->Glove;
		ItemSlot->SlotIndex = RUNTIME_EQUIPMENT_SLOT_INDEX_GLOVES;
		Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount += 1;
	}

	if (CharacterInit->Boot > 0) {
		RTItemSlotRef ItemSlot = &Request->CharacterData.EquipmentInfo.EquipmentSlots[Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount];
		ItemSlot->Item.ID = CharacterInit->Boot;
		ItemSlot->SlotIndex = RUNTIME_EQUIPMENT_SLOT_INDEX_BOOTS;
		Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount += 1;
	}

	if (CharacterInit->RightHand > 0) {
		RTItemSlotRef ItemSlot = &Request->CharacterData.EquipmentInfo.EquipmentSlots[Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount];
		ItemSlot->Item.ID = CharacterInit->RightHand;
		ItemSlot->SlotIndex = RUNTIME_EQUIPMENT_SLOT_INDEX_WEAPON_RIGHT;
		Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount += 1;
	}

	if (CharacterInit->LeftHand > 0) {
		RTItemSlotRef ItemSlot = &Request->CharacterData.EquipmentInfo.EquipmentSlots[Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount];
		ItemSlot->Item.ID = CharacterInit->LeftHand;
		ItemSlot->SlotIndex = RUNTIME_EQUIPMENT_SLOT_INDEX_WEAPON_LEFT;
		Request->CharacterData.EquipmentInfo.Info.EquipmentSlotCount += 1;
	}

	RTDataCharacterInitStatRef CharacterInitStat = RTRuntimeDataCharacterInitStatGet(Context->Runtime->Context, BattleStyleIndex);
	Request->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_STR] = CharacterInitStat->Str;
	Request->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_DEX] = CharacterInitStat->Dex;
	Request->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_INT] = CharacterInitStat->Int;

	memcpy(&Request->CharacterData.InventoryInfo, &CharacterTemplate->Inventory, sizeof(struct _RTCharacterInventoryInfo));
	memcpy(&Request->CharacterData.SkillSlotInfo, &CharacterTemplate->SkillSlots, sizeof(struct _RTCharacterSkillSlotInfo));
	memcpy(&Request->CharacterData.QuickSlotInfo, &CharacterTemplate->QuickSlots, sizeof(struct _RTCharacterQuickSlotInfo));

	Request->CharacterData.Info.Profile.MapsMask = 0xFFFFFFFF;
	Request->CharacterData.Info.Profile.WarpMask = 0xFFFFFFFF;

	Request->CharacterData.AnimaMasteryInfo.Info.PresetCount = 3;
	for (Int32 PresetIndex = 0; PresetIndex < RUNTIME_MAX_ANIMA_MASTERY_PRESET_COUNT; PresetIndex += 1) {
		for (Int32 CategoryIndex = 0; CategoryIndex < RUNTIME_MAX_ANIMA_MASTERY_CATEGORY_COUNT; CategoryIndex += 1) {
			Request->CharacterData.AnimaMasteryInfo.PresetData[PresetIndex].CategoryOrder[CategoryIndex].StorageIndex = 0;
			Request->CharacterData.AnimaMasteryInfo.PresetData[PresetIndex].CategoryOrder[CategoryIndex].CategoryIndex = CategoryIndex;
		}
	}

	if (Packet->CreateSpecialCharacter && Context->Config.WorldSvr.DebugCharacter) {
		memset(Request->CharacterData.QuestInfo.Info.FinishedQuests, 0xFF, RUNTIME_CHARACTER_MAX_QUEST_FLAG_COUNT);
		Request->CharacterData.Info.Style.BattleRank = 20;
		Request->CharacterData.Info.Basic.Level = Runtime->Context->LevelList[Runtime->Context->LevelCount - 1].Level;
		Request->CharacterData.Info.Basic.Exp = Runtime->Context->LevelList[Runtime->Context->LevelCount - 1].AccumulatedExp;
		Request->CharacterData.OverlordMasteryInfo.Info.Level = Runtime->Context->OverlordMasteryExpList[Runtime->Context->OverlordMasteryExpCount - 1].Level;
		Request->CharacterData.OverlordMasteryInfo.Info.Exp = Runtime->Context->OverlordMasteryExpList[Runtime->Context->OverlordMasteryExpCount - 1].AccumulatedExp;
		Request->CharacterData.Info.Skill.Rank = 10;
		Request->CharacterData.Info.Skill.Point = 1000;
		Request->CharacterData.Info.Skill.Level = 540;
		Request->CharacterData.ForceWingInfo.Info.Grade = 1;
		Request->CharacterData.ForceWingInfo.Info.Level = 1;
		Request->CharacterData.MythMasteryInfo.Info.Level = 94;
		Request->CharacterData.MythMasteryInfo.Info.Exp = 0;
		Request->CharacterData.MythMasteryInfo.Info.Rebirth = 1000;
		Request->CharacterData.MythMasteryInfo.Info.HolyPower = 27500;
		Request->CharacterData.MythMasteryInfo.Info.UnlockedPageCount = 4;

		Request->CharacterData.Info.Profile.MapsMask = 0xFFFFFFFF;
		Request->CharacterData.Info.Profile.WarpMask = 0xFFFFFFFF;
		Request->CharacterData.Info.Profile.Nation = 3;
		Request->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_PNT] = 2000;

		Request->CharacterData.AbilityInfo.Info.EssenceAbilityCount = MIN(Runtime->Context->PassiveAbilityCostCount, RUNTIME_CHARACTER_MAX_ESSENCE_ABILITY_SLOT_COUNT);
		Request->CharacterData.AbilityInfo.Info.ExtendedEssenceAbilityCount = MAX(0, Request->CharacterData.AbilityInfo.Info.EssenceAbilityCount - RUNTIME_CHARACTER_ESSENCE_ABILITY_SLOT_COUNT);
		for (Int32 Index = 0; Index < Request->CharacterData.AbilityInfo.Info.EssenceAbilityCount; Index += 1) {
			Request->CharacterData.AbilityInfo.EssenceAbilitySlots[Index].AbilityID = Runtime->Context->PassiveAbilityCostList[Index].Index;
			Request->CharacterData.AbilityInfo.EssenceAbilitySlots[Index].Level = Runtime->Context->PassiveAbilityCostList[Index].PassiveAbilityCostLevelList[Runtime->Context->PassiveAbilityCostList[Index].PassiveAbilityCostLevelCount - 1].Level;
		}

		Request->CharacterData.AbilityInfo.Info.BlendedAbilityCount = MIN(Runtime->Context->BlendedAbilityCostCount, RUNTIME_CHARACTER_MAX_BLENDED_ABILITY_SLOT_COUNT);
		Request->CharacterData.AbilityInfo.Info.ExtendedBlendedAbilityCount = MAX(0, Request->CharacterData.AbilityInfo.Info.BlendedAbilityCount - RUNTIME_CHARACTER_BLENDED_ABILITY_SLOT_COUNT);
		for (Int32 Index = 0; Index < Request->CharacterData.AbilityInfo.Info.BlendedAbilityCount; Index += 1) {
			Request->CharacterData.AbilityInfo.BlendedAbilitySlots[Index].AbilityID = Runtime->Context->BlendedAbilityCostList[Index].Index;
		}

		Request->CharacterData.AbilityInfo.Info.KarmaAbilityCount = MIN(Runtime->Context->KarmaAbilityCostCount, RUNTIME_CHARACTER_MAX_KARMA_ABILITY_SLOT_COUNT);
		Request->CharacterData.AbilityInfo.Info.ExtendedKarmaAbilityCount = MAX(0, Request->CharacterData.AbilityInfo.Info.KarmaAbilityCount - RUNTIME_CHARACTER_KARMA_ABILITY_SLOT_COUNT);
		for (Int32 Index = 0; Index < Request->CharacterData.AbilityInfo.Info.KarmaAbilityCount; Index += 1) {
			Request->CharacterData.AbilityInfo.KarmaAbilitySlots[Index].AbilityID = Runtime->Context->KarmaAbilityCostList[Index].Index;
			Request->CharacterData.AbilityInfo.KarmaAbilitySlots[Index].Level = Runtime->Context->KarmaAbilityCostList[Index].KarmaAbilityCostLevelList[Runtime->Context->KarmaAbilityCostList[Index].KarmaAbilityCostLevelCount - 1].Level;
		}

		Request->CharacterData.Info.Profile.Nation = 2;
		Request->CharacterData.Info.Honor.Rank = Runtime->Context->HonorLevelFormulaList[Runtime->Context->HonorLevelFormulaCount - 1].Rank;
		Request->CharacterData.Info.Honor.Point = Runtime->Context->HonorLevelFormulaList[Runtime->Context->HonorLevelFormulaCount - 1].MaxPoint - 1;
		Request->CharacterData.HonorMedalInfo.Info.Score = Runtime->Context->HonorMedalScoreCategoryList[0].HonorMedalScoreMedalList[Runtime->Context->HonorMedalScoreCategoryList[0].HonorMedalScoreMedalCount - 1].AccumulatedRequiredScore - 1;
		Request->CharacterData.HonorMedalInfo.Info.SlotCount = 0;
		for (Int32 Index = 0; Index < 4; Index += 1) {
			RTHonorMedalSlotRef Slot = &Request->CharacterData.HonorMedalInfo.Slots[Request->CharacterData.HonorMedalInfo.Info.SlotCount];
			Slot->CategoryIndex = 0;
			Slot->GroupIndex = 1;
			Slot->SlotIndex = Request->CharacterData.HonorMedalInfo.Info.SlotCount;
			Slot->ForceEffectIndex = 45;
			Slot->IsUnlocked = 1;
			Request->CharacterData.HonorMedalInfo.Info.SlotCount += 1;
		}

		Int32 SlotIndex = Request->CharacterData.SkillSlotInfo.Info.SlotCount;
		for (Index Index = 144; Index < 148; Index++) {
			RTSkillSlotRef GmSkill = &Request->CharacterData.SkillSlotInfo.Skills[Request->CharacterData.SkillSlotInfo.Info.SlotCount];
			GmSkill->ID = Index;
			GmSkill->Level = 1;
			GmSkill->Index = SlotIndex++;
			Request->CharacterData.SkillSlotInfo.Info.SlotCount += 1;
		}
		/*
		AchievementIndex = Request->CharacterData.AchievementInfo.AchievementExtendedRewardCount;
		Request->CharacterData.AchievementInfo.AchievementRewardSlots[AchievementIndex].TitleIndex = AchievementReward->AchievementID;
		Request->CharacterData.AchievementInfo.AchievementRewardSlots[AchievementIndex].Unknown1 = 0;
		Request->CharacterData.AchievementInfo.AchievementRewardCount += 1;


		Request->CharacterData.Info.Alz= 999999999;
		Request->CharacterData.Info.Currency[RUNTIME_CHARACTER_CURRENCY_GEM] = 999999;
		Request->CharacterData.Info.Basic.Level = 200;
		Request->CharacterData.Info.Overlord.Level = 1;
		Request->CharacterData.Info.Overlord.Point = 1;
		Request->CharacterData.Info.Basic.Exp = RTRuntimeGetExpByLevel(Runtime, 200);
		Request->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_PNT] = 200 * 5;
		Request->CharacterData.Info.Style.BattleRank = 20;
		Request->CharacterData.Info.Skill.Rank = 10;
		Request->CharacterData.Info.Skill.Level = 500;
		Request->CharacterData.Info.Profile.Nation = 2;

		RTDataHonorLevelFormulaRef HonorLevelFormula = RTRuntimeDataHonorLevelFormulaGet(Runtime->Context, 20);
		Request->CharacterData.Info.Honor.Rank = HonorLevelFormula->Rank;
		Request->CharacterData.Info.Honor.Point = HonorLevelFormula->MaxPoint;
		Request->CharacterData.Info.Honor.Exp = 0;
		*/
	}

	IPCSocketUnicast(Server->IPCSocket, Request);
}

IPC_PROCEDURE_BINDING(D2W, CREATE_CHARACTER) {
	if (!ClientConnection || !Client) return;

	if (Packet->Status == CREATE_CHARACTER_STATUS_SUCCESS) {
		Client->Characters[Packet->CharacterSlotIndex] = Packet->Character;
	}
	
	S2C_DATA_CREATE_CHARACTER* Response = PacketBufferInit(ClientConnection->PacketBuffer, S2C, CREATE_CHARACTER);
	Response->CharacterIndex = Packet->Character.ID * MAX_CHARACTER_COUNT + Packet->CharacterSlotIndex;
	Response->CharacterStatus = Packet->Status;
	SocketSend(Context->ClientSocket, ClientConnection, Response);
}
