#ifdef WORKBENCH
class AROR_ForcePoolSmokeTests
{
	protected static AROR_ForceProfile MakeProfile(string id, AROR_EForceType type, int weight)
	{
		AROR_ForceProfile profile = AROR_ForceProfile.CreateForScript();
		profile.m_sId = id;
		profile.m_bEnabled = true;
		profile.m_eRole = AROR_EFactionRole.OCCUPIERS;
		profile.m_sFactionKey = "US";
		profile.m_eType = type;
		profile.m_iAttackWeight = weight;
		profile.m_iQRFWeight = weight;
		profile.m_sInfantryGroup = "test_infantry.et";
		profile.m_sCrewCharacter = "test_crew.et";
		profile.m_sVehicle = "test_vehicle.et";
		profile.m_sEscortVehicle = "test_gunship.et";
		profile.m_iCrewCount = 1;
		profile.m_iEscortCount = 1;
		profile.m_iPersonnelBudget = 6;
		profile.m_fDismountDistance = 150;
		return profile;
	}

	static bool Run()
	{
		AROR_ForceProfile motorized = MakeProfile("test_motorized", AROR_EForceType.MOTORIZED, 40);
		AROR_ForceProfile mechanized = MakeProfile("test_mechanized", AROR_EForceType.MECHANIZED, 30);
		AROR_ForceProfile airborne = MakeProfile("test_airborne", AROR_EForceType.AIRBORNE, 100);
		array<ref AROR_ForceProfile> profiles = {motorized, mechanized, airborne};
		bool passed = true;
		// The campaign faction may use another faction's models, but not arbitrary assets.
		AROR_OperationForce factionPolicy = new AROR_OperationForce();
		factionPolicy.m_sFactionKey = "AROR_OCCUPIERS";
		passed = Check(factionPolicy.CanInitializeSpawnFaction(""), "Unassigned spawn can join campaign faction") && passed;
		passed = Check(factionPolicy.CanInitializeSpawnFaction("AROR_OCCUPIERS"), "Campaign faction preserved") && passed;
		passed = Check(!factionPolicy.CanInitializeSpawnFaction("US"), "Native faction requires explicit asset declaration") && passed;
		factionPolicy.m_sAssetFactionKey = "US";
		passed = Check(factionPolicy.CanInitializeSpawnFaction("US"), "Declared US assets can join Occupiers") && passed;
		passed = Check(!factionPolicy.CanInitializeSpawnFaction("USSR") && !factionPolicy.CanInitializeSpawnFaction("AROR_INVADERS"), "Wrong native and campaign factions rejected") && passed;
		passed = Check(AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 0) == motorized, "First weight boundary") && passed;
		passed = Check(AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 39) == motorized, "Last motorized ticket") && passed;
		passed = Check(AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 40) == mechanized, "First mechanized ticket") && passed;
		passed = Check(AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 69) == mechanized, "Last eligible ticket") && passed;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 70), "Unsupported airborne excluded even if enabled") && passed;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.INVADERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 0), "Role isolation") && passed;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "USSR", AROR_EOperationType.QRF, 4, 32, 4, 0), "Faction isolation") && passed;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 1, 32, 4, 0), "Group capacity") && passed;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 5, 4, 0), "Member capacity") && passed;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 0, 0), "Vehicle capacity") && passed;
		motorized.m_iQRFWeight = 0;
		passed = Check(AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 0) == mechanized, "Zero QRF weight") && passed;
		passed = Check(AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.ATTACK, 4, 32, 4, 0) == motorized, "Separate attack weights") && passed;
		mechanized.m_bEnabled = false;
		passed = Check(!AROR_ForcePool.Pick(profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 4, 32, 4, 0), "Disabled profile") && passed;
		Print(string.Format("[AROR TEST] Force-pool smoke tests passed=%1", passed), LogLevel.NORMAL);
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print(string.Format("[AROR TEST] FAILED force-pool check: %1", label), LogLevel.ERROR);
		return condition;
	}
}
#endif
