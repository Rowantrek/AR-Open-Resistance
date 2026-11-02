#ifdef WORKBENCH
//! Memory-only native script checks. Sentinel resource names are never loaded or spawned.
class AROR_JointResponseSmokeTests
{
	protected static bool Check(bool passed, string label)
	{
		if (!passed)
			Print("[AROR JOINT TEST] FAILED: " + label, LogLevel.ERROR);
		return passed;
	}

	protected static AROR_ForceProfile Profile(string id, AROR_EForceType type)
	{
		AROR_ForceProfile profile = AROR_ForceProfile.CreateForScript();
		profile.m_sId = id;
		profile.m_bEnabled = true;
		profile.m_eRole = AROR_EFactionRole.OCCUPIERS;
		profile.m_sFactionKey = "joint_test_faction";
		profile.m_eType = type;
		profile.m_iMinimumReadinessLevel = 5;
		profile.m_iAttackWeight = 1;
		profile.m_iQRFWeight = 1;
		profile.m_sInfantryGroup = "joint_test_infantry";
		profile.m_sCrewCharacter = "joint_test_crew";
		profile.m_sVehicle = "joint_test_vehicle";
		profile.m_sEscortVehicle = "joint_test_escort";
		profile.m_iEscortCount = 1;
		profile.m_iCrewCount = 1;
		profile.m_iPersonnelBudget = 20;
		profile.m_fDismountDistance = 100;
		return profile;
	}

	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		bool passed = true;
		AROR_ForceProfile air = Profile("joint_test_air", AROR_EForceType.AIRBORNE);
		AROR_ForceProfile ground = Profile("joint_test_ground", AROR_EForceType.MOTORIZED);
		array<ref AROR_ForceProfile> profiles = {air, ground};
		AROR_GroundQRFRecipe recipe = AROR_GroundQRFRecipe.CreateForScript();
		recipe.m_sId = "joint_test_recipe";
		recipe.m_eResponseType = AROR_EResponseType.AIR_GROUND;
		recipe.m_iLevel = 2;
		recipe.m_iMinimumReadinessLevel = 5;
		recipe.m_iMaximumReadinessLevel = 5;
		recipe.m_eRole = AROR_EFactionRole.OCCUPIERS;
		recipe.m_sFactionKey = "joint_test_faction";
		recipe.m_aProfileIds = {air.m_sId, ground.m_sId};
		array<ref AROR_GroundQRFRecipe> recipes = {recipe};
		array<AROR_ForceProfile> resolved = {};
		passed = Check(recipe.Resolve(profiles, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", resolved, 5) && resolved.Count() == 2, "complete joint recipe resolves once") && passed;
		passed = Check(air.GetGroupBudget() == 3 && air.GetVehicleBudget() == 2, "transport, gunship and infantry reservations") && passed;
		AROR_ResponseSelection selection = AROR_ResponsePolicy.Select(profiles, recipes, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", AROR_EResponseType.AIR_GROUND, 5, 5, true);
		passed = Check(selection.IsAvailable() && selection.m_iTier == 2 && selection.m_aElements.Count() == 2 && selection.m_iGroups == 5 && selection.m_iVehicles == 3, "authored list is never multiplied by Crackdown") && passed;
		selection = AROR_ResponsePolicy.Select(profiles, recipes, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", AROR_EResponseType.AIR_GROUND, 4, 5, true);
		passed = Check(!selection.IsAvailable(), "equipment gate remains authoritative") && passed;
		selection = AROR_ResponsePolicy.Select(profiles, recipes, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", AROR_EResponseType.AIR_GROUND, 5, 5, false);
		passed = Check(!selection.IsAvailable(), "disabled adapter cannot select a joint force") && passed;
		recipe.m_aProfileIds = {ground.m_sId, air.m_sId};
		passed = Check(!recipe.Resolve(profiles, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", resolved, 5), "air package must lead the authored list") && passed;
		recipe.m_aProfileIds = {air.m_sId, air.m_sId, ground.m_sId};
		passed = Check(!recipe.Resolve(profiles, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", resolved, 5), "multiple air pairs require a separate accepted implementation") && passed;
		air.m_iMinimumReadinessLevel = 1;
		air.m_iEscortCount = 0;
		air.m_sEscortVehicle = "";
		ground.m_iMinimumReadinessLevel = 1;
		recipe.m_iMinimumReadinessLevel = 2;
		recipe.m_iMaximumReadinessLevel = 4;
		recipe.m_aProfileIds = {air.m_sId, ground.m_sId};
		passed = Check(air.IsValid() && air.GetGroupBudget() == 2 && air.GetVehicleBudget() == 1, "light air reserves no phantom gunship") && passed;
		passed = Check(!recipe.Resolve(profiles, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", resolved, 1) && recipe.Resolve(profiles, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", resolved, 2), "light joint equipment gate starts at Readiness two") && passed;
		selection = AROR_ResponsePolicy.Select(profiles, recipes, AROR_EFactionRole.OCCUPIERS, "joint_test_faction", AROR_EResponseType.AIRBORNE, 1, 1, true);
		passed = Check(selection.IsAvailable() && selection.m_iGroups == 2 && selection.m_iVehicles == 1, "Readiness-one standalone transport insertion") && passed;
		air.m_iEscortCount = 2;
		passed = Check(!air.IsValid(), "unsupported multiple-gunship profile is rejected") && passed;
		Print(string.Format("[AROR JOINT TEST] Joint response smoke tests passed=%1; no native actors or routes tested", passed), LogLevel.NORMAL);
		return passed;
	}
}
#endif
