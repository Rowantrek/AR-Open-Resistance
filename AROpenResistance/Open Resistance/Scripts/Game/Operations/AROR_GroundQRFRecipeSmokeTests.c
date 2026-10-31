#ifdef WORKBENCH
//! Policy tests only: dummy resource names are never spawned or loaded.
class AROR_GroundQRFRecipeSmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		AROR_ForceProfile truck = Profile("truck", AROR_EForceType.MOTORIZED);
		AROR_ForceProfile ifv = Profile("ifv", AROR_EForceType.MECHANIZED);
		array<ref AROR_ForceProfile> profiles = {truck, ifv};
		AROR_GroundQRFRecipe recipe = AROR_GroundQRFRecipe.CreateForScript();
		recipe.m_sId = "combined_arms_l1";
		recipe.m_iLevel = 1;
		recipe.m_eRole = AROR_EFactionRole.OCCUPIERS;
		recipe.m_sFactionKey = "AROR_OCCUPIERS";
		recipe.m_aProfileIds = {"truck", "ifv"};
		array<AROR_ForceProfile> resolved = {};
		bool passed = Check(recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved), "Truck and IFV resolve");
		passed = Check(resolved.Count() == 2 && recipe.m_iLevel == 1, "Tier one contains two elements") && passed;
		ifv.m_iMinimumReadinessLevel = 3;
		passed = Check(!recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved, 1), "Low readiness cannot unlock an APC") && passed;
		passed = Check(recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved, 3), "Readiness unlocks equipment independently of response tier") && passed;
		ifv.m_iMinimumReadinessLevel = 1;
		recipe.m_aProfileIds = {"truck", "missing"};
		passed = Check(!recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved), "Missing second element cannot reuse first") && passed;
		recipe.m_aProfileIds = {"truck", "truck"};
		passed = Check(recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved) && resolved.Count() == 2, "Repeated profile means two complete elements") && passed;
		recipe.m_aProfileIds = {"truck", "ifv"};
		ifv.m_bEnabled = false;
		passed = Check(!recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved), "Disabled required IFV rejected") && passed;
		ifv.m_bEnabled = true;
		passed = Check(!recipe.Resolve(profiles, AROR_EFactionRole.INVADERS, recipe.m_sFactionKey, resolved), "Role mismatch rejected") && passed;
		passed = Check(!recipe.Resolve(profiles, recipe.m_eRole, "OTHER", resolved), "Faction mismatch rejected") && passed;
		ifv.m_eType = AROR_EForceType.AIRBORNE;
		ifv.m_sEscortVehicle = "test_escort.et";
		ifv.m_iEscortCount = 1;
		passed = Check(!recipe.Resolve(profiles, recipe.m_eRole, recipe.m_sFactionKey, resolved), "Airborne excluded from ground recipe") && passed;

		AROR_Operation operation = new AROR_Operation("recipe_test", AROR_EFactionRole.OCCUPIERS, AROR_EOperationType.QRF, "test", 0, 0);
		operation.Transition_S(AROR_EOperationState.SPAWNING, 0, "");
		passed = Check(operation.BindGroundRecipe_S(recipe.m_sId, 1, 2), "Recipe binds once") && passed;
		passed = Check(operation.GetResponseLevel() == 1 && operation.GetPackageCount() == 2, "Authored tier separate from package count") && passed;
		passed = Check(!operation.BindGroundRecipe_S("other", 2, 3), "Recipe cannot reroll") && passed;
		passed = Check(!operation.BindForceProfile_S("truck", 1), "Homogeneous profile cannot overwrite recipe") && passed;
		if (passed)
			Print("[AROR TEST] Ground QRF recipe regressions passed", LogLevel.NORMAL);
		return passed;
	}

	protected static AROR_ForceProfile Profile(string id, AROR_EForceType type)
	{
		AROR_ForceProfile profile = AROR_ForceProfile.CreateForScript();
		profile.m_sId = id;
		profile.m_eType = type;
		profile.m_eRole = AROR_EFactionRole.OCCUPIERS;
		profile.m_sFactionKey = "AROR_OCCUPIERS";
		profile.m_bEnabled = true;
		profile.m_iQRFWeight = 1;
		profile.m_iPersonnelBudget = 10;
		profile.m_iCrewCount = 1;
		profile.m_sVehicle = "test_vehicle.et";
		profile.m_sCrewCharacter = "test_crew.et";
		profile.m_sInfantryGroup = "test_team.et";
		profile.m_fDismountDistance = 100;
		return profile;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] Ground recipe FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}
#endif
