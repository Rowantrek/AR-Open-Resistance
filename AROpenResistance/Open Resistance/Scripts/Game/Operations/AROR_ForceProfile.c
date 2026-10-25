[BaseContainerProps()]
class AROR_ForceProfile
{
	[Attribute("", desc: "Unique stable profile ID")]
	string m_sId;
	[Attribute("1", desc: "Enable this profile for selection")]
	bool m_bEnabled;
	[Attribute("1", UIWidgets.ComboBox, desc: "Campaign role", enums: ParamEnumArray.FromEnum(AROR_EFactionRole))]
	AROR_EFactionRole m_eRole;
	[Attribute("AROR_OCCUPIERS", desc: "Required campaign faction key from AROR_FactionRolesComponent")]
	string m_sFactionKey;
	[Attribute("", desc: "Optional native asset faction allowed to become the campaign faction on first spawn only, e.g. US for AROR_OCCUPIERS. Empty accepts only unassigned or already-matching assets.")]
	string m_sAssetFactionKey;
	[Attribute("0", UIWidgets.ComboBox, desc: "Force execution policy", enums: ParamEnumArray.FromEnum(AROR_EForceType))]
	AROR_EForceType m_eType;
	[Attribute("1", desc: "Scheduled attack selection weight; zero disables attacks")]
	int m_iAttackWeight;
	[Attribute("1", desc: "QRF selection weight; zero disables QRFs")]
	int m_iQRFWeight;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Infantry group prefab; unused by armour", params: "et")]
	ResourceName m_sInfantryGroup;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "One crew character prefab repeated for required vehicle crew", params: "et")]
	ResourceName m_sCrewCharacter;
	[Attribute("1", desc: "Required crew count, including driver and gunner(s)")]
	int m_iCrewCount;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Ground vehicle or transport helicopter prefab", params: "et")]
	ResourceName m_sVehicle;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Airborne escort gunship prefab", params: "et")]
	ResourceName m_sEscortVehicle;
	[Attribute("1", desc: "Required gunship escort count for airborne profiles")]
	int m_iEscortCount;
	[Attribute("12", desc: "Personnel reservation, including infantry and crew; must cover actual group slots")]
	int m_iPersonnelBudget;
	[Attribute("150", desc: "Ground dismount distance from objective centre, metres")]
	float m_fDismountDistance;
	[Attribute("25", desc: "Replacement value per confirmed lost soldier/crew member", params: "0 100000 1")]
	int m_iPersonnelLossValue = 25;
	[Attribute("-1", desc: "Replacement value of the main vehicle; -1 uses the shared type price", params: "-1 100000 1")]
	int m_iVehicleLossValue = -1;
	[Attribute("2000", desc: "Replacement value of each airborne gunship", params: "0 100000 1")]
	int m_iEscortLossValue = 2000;

	int VehicleLossValue()
	{
		if (m_eType == AROR_EForceType.INFANTRY)
			return 0;
		if (m_iVehicleLossValue >= 0)
			return m_iVehicleLossValue;
		return AROR_QRFStrategicPolicy.VehicleValue(m_eType);
	}
	int ReplacementQuote()
	{
		int value = m_iPersonnelBudget * m_iPersonnelLossValue + VehicleLossValue();
		if (m_eType == AROR_EForceType.AIRBORNE)
			value += m_iEscortCount * m_iEscortLossValue;
		return value;
	}

	int GetWeight(AROR_EOperationType type)
	{
		if (type == AROR_EOperationType.QRF)
			return m_iQRFWeight;
		return m_iAttackWeight;
	}

	int GetGroupBudget()
	{
		if (m_eType == AROR_EForceType.INFANTRY || m_eType == AROR_EForceType.ARMOUR)
			return 1;
		return 2;
	}

	bool HasNativeExecutor()
	{
		// Flight/landing/escort needs a dedicated adapter; never dispatch it as a ground force.
		return m_eType == AROR_EForceType.MOTORIZED || m_eType == AROR_EForceType.MECHANIZED || m_eType == AROR_EForceType.ARMOUR || m_eType == AROR_EForceType.INFANTRY;
	}

	bool IsValid()
	{
		if (m_iPersonnelLossValue < 0 || m_iPersonnelLossValue > 100000)
			return false;
		if (m_iVehicleLossValue < -1 || m_iVehicleLossValue > 100000 || m_iEscortLossValue < 0 || m_iEscortLossValue > 100000)
			return false;
		if (m_sId.IsEmpty() || m_sFactionKey.IsEmpty() || m_iPersonnelBudget < 1 || m_iPersonnelBudget > 128 || m_iAttackWeight < 0 || m_iAttackWeight > 1000 || m_iQRFWeight < 0 || m_iQRFWeight > 1000)
			return false;
		if (m_eRole != AROR_EFactionRole.OCCUPIERS && m_eRole != AROR_EFactionRole.INVADERS)
			return false;
		if (m_eType == AROR_EForceType.INFANTRY)
			return !m_sInfantryGroup.IsEmpty();
		if (m_sVehicle.IsEmpty() || m_sCrewCharacter.IsEmpty() || m_iCrewCount < 1 || m_iCrewCount > 8 || m_iCrewCount > m_iPersonnelBudget)
			return false;
		if (m_eType == AROR_EForceType.ARMOUR)
			return true;
		if (m_sInfantryGroup.IsEmpty() || m_fDismountDistance < 20)
			return false;
		if (m_eType == AROR_EForceType.AIRBORNE)
			return !m_sEscortVehicle.IsEmpty() && m_iEscortCount > 0 && m_iEscortCount <= 4;
		return m_eType == AROR_EForceType.MOTORIZED || m_eType == AROR_EForceType.MECHANIZED;
	}
}
