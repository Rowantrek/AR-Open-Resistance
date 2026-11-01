[BaseContainerProps()]
class AROR_ForceProfile
{
	//! Attribute defaults belong to the resource container, not declaration initializers (which overwrite authored values).
	[Attribute("", desc: "Unique stable profile ID")]
	string m_sId;
	[Attribute("1", desc: "Enable this profile for selection")]
	bool m_bEnabled;
	[Attribute("1", desc: "Minimum campaign readiness permitting this profile in a QRF recipe", params: "1 5 1")]
	int m_iMinimumReadinessLevel;
	[Attribute("0", desc: "Classify a mounted support vehicle as light armed transport in map intelligence. Passenger delivery can be enabled independently.")]
	bool m_bLightVehicle;
	[Attribute("0", desc: "Fill usable ground or transport-helicopter passenger seats with infantry, within the reserved personnel budget. Vehicle crew remain a separate group.")]
	bool m_bFillPassengerSeats;
	[Attribute("1", UIWidgets.ComboBox, desc: "Campaign role", enums: AROR_FactionRoles.MilitaryRoleParams())]
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
	[Attribute("", UIWidgets.ComboBox, desc: "Infantry unit types, in order (the first leads); each spawns as the role's base character dressed from its faction preset loadout. Empty uses a fire team (team leader, automatic rifleman, grenadier, rifleman). Unused by armour.", enums: ParamEnumArray.FromEnum(AROR_EUnitType))]
	ref array<AROR_EUnitType> m_aInfantryUnits;
	[Attribute("12", UIWidgets.ComboBox, desc: "Unit type repeated for the required ground vehicle crew (airborne pilots are REAPER's own)", enums: ParamEnumArray.FromEnum(AROR_EUnitType))]
	AROR_EUnitType m_eCrewType;
	[Attribute("1", desc: "Required crew count, including driver and gunner(s)")]
	int m_iCrewCount;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Ground vehicle or transport helicopter prefab", params: "et")]
	ResourceName m_sVehicle;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Optional airborne gunship prefab; required when escort count is one", params: "et")]
	ResourceName m_sEscortVehicle;
	[Attribute("1", desc: "Airborne gunship escorts: zero for light transport-only packages, one for escorted packages", params: "0 1 1")]
	int m_iEscortCount;
	[Attribute("12", desc: "Personnel reservation, including infantry and crew; must cover actual group slots")]
	int m_iPersonnelBudget;
	[Attribute("150", desc: "Ground dismount distance from objective centre, metres")]
	float m_fDismountDistance;
	[Attribute("25", desc: "Replacement value per confirmed lost soldier/crew member", params: "0 100000 1")]
	int m_iPersonnelLossValue;
	[Attribute("-1", desc: "Replacement value of the main vehicle; -1 uses the shared type price", params: "-1 100000 1")]
	int m_iVehicleLossValue;
	[Attribute("2000", desc: "Replacement value of each airborne gunship", params: "0 100000 1")]
	int m_iEscortLossValue;

	//! The infantry unit types to field, in order: the authored list, else a default fire team
	void GetInfantryUnits(notnull array<AROR_EUnitType> outUnits)
	{
		outUnits.Clear();
		if (m_aInfantryUnits && !m_aInfantryUnits.IsEmpty())
		{
			outUnits.Copy(m_aInfantryUnits);
			return;
		}
		GetDefaultFireTeam(outUnits);
	}

	//! Team leader, automatic rifleman, grenadier and rifleman
	static void GetDefaultFireTeam(notnull array<AROR_EUnitType> outUnits)
	{
		outUnits.Clear();
		outUnits.Insert(AROR_EUnitType.TEAMLEADER);
		outUnits.Insert(AROR_EUnitType.AUTORIFLEMAN);
		outUnits.Insert(AROR_EUnitType.GRENADIER);
		outUnits.Insert(AROR_EUnitType.RIFLEMAN);
	}

	//! Preserve explicit Readiness/pricing defaults for script-created fixtures; caller fills required identity/assets.
	static AROR_ForceProfile CreateForScript()
	{
		AROR_ForceProfile profile = new AROR_ForceProfile();
		profile.m_eCrewType = AROR_EUnitType.CREW;
		profile.m_iMinimumReadinessLevel = 1;
		profile.m_iPersonnelLossValue = 25;
		profile.m_iVehicleLossValue = -1;
		profile.m_iEscortLossValue = 2000;
		return profile;
	}

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
	int GetSpottingKind()
	{
		if (m_bLightVehicle)
			return 5;
		return m_eType;
	}

	int GetGroupBudget()
	{
		if (m_eType == AROR_EForceType.AIRBORNE)
			return 2 + m_iEscortCount;
		if (m_eType == AROR_EForceType.INFANTRY || (m_eType == AROR_EForceType.ARMOUR && !m_bFillPassengerSeats))
			return 1;
		return 2;
	}

	int GetVehicleBudget()
	{
		if (m_eType == AROR_EForceType.AIRBORNE)
			return 1 + m_iEscortCount;
		if (m_eType == AROR_EForceType.INFANTRY)
			return 0;
		return 1;
	}

	bool HasNativeExecutor()
	{
		// Flight/landing/escort needs a dedicated adapter; never dispatch it as a ground force.
		return m_eType == AROR_EForceType.MOTORIZED || m_eType == AROR_EForceType.MECHANIZED || m_eType == AROR_EForceType.ARMOUR || m_eType == AROR_EForceType.INFANTRY;
	}

	bool IsValid()
	{
		if (m_bFillPassengerSeats && m_eType == AROR_EForceType.INFANTRY)
			return false;
		if (m_bLightVehicle && m_eType != AROR_EForceType.ARMOUR)
			return false;
		if (m_iMinimumReadinessLevel < 1 || m_iMinimumReadinessLevel > 5)
			return false;
		if (m_iPersonnelLossValue < 0 || m_iPersonnelLossValue > 100000)
			return false;
		if (m_iVehicleLossValue < -1 || m_iVehicleLossValue > 100000 || m_iEscortLossValue < 0 || m_iEscortLossValue > 100000)
			return false;
		if (m_sId.IsEmpty() || m_sFactionKey.IsEmpty() || m_iPersonnelBudget < 1 || m_iPersonnelBudget > 128 || m_iAttackWeight < 0 || m_iAttackWeight > 1000 || m_iQRFWeight < 0 || m_iQRFWeight > 1000)
			return false;
		if (m_eRole != AROR_EFactionRole.OCCUPIERS && m_eRole != AROR_EFactionRole.INVADERS)
			return false;
		if (m_eType == AROR_EForceType.INFANTRY)
			return true;
		if (m_sVehicle.IsEmpty() || m_iCrewCount < 1 || m_iCrewCount > 8 || m_iCrewCount > m_iPersonnelBudget)
			return false;
		if (m_eType == AROR_EForceType.ARMOUR && !m_bFillPassengerSeats)
			return true;
		if (m_fDismountDistance < 20)
			return false;
		if (m_eType == AROR_EForceType.AIRBORNE)
			return m_iEscortCount >= 0 && m_iEscortCount <= 1 && (m_iEscortCount == 0 || !m_sEscortVehicle.IsEmpty());
		return m_eType == AROR_EForceType.MOTORIZED || m_eType == AROR_EForceType.MECHANIZED || (m_eType == AROR_EForceType.ARMOUR && m_bFillPassengerSeats);
	}
}
