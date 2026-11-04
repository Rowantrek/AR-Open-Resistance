#ifdef WORKBENCH
//! Pure planning regressions. No engine spawning or scheduler requests.
class AROR_QRFSelectionSmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		array<ref AROR_ForceProfile> profiles = {};
		profiles.Insert(Profile("ural", AROR_EForceType.MOTORIZED, 8));
		profiles.Insert(Profile("btr", AROR_EForceType.MECHANIZED, 10));
		profiles.Insert(Profile("brdm", AROR_EForceType.ARMOUR, 4));
		array<ref AROR_GroundQRFRecipe> recipes = {};
		array<string> tierOne = {"ural", "btr"};
		array<string> tierTwo = {"ural", "btr", "btr"};
		array<string> tierThree = {"ural", "ural", "btr", "btr", "brdm"};
		recipes.Insert(Recipe("l1", 1, tierOne));
		recipes.Insert(Recipe("l2", 2, tierTwo));
		recipes.Insert(Recipe("l3", 3, tierThree));
		AROR_QRFAvailabilitySnapshot ready = new AROR_QRFAvailabilitySnapshot();
		ready.m_eRole = AROR_EFactionRole.INVADERS;
		ready.m_sFactionKey = "AROR_INVADERS";
		ready.m_sOriginLocationId = "test_outpost";
		ready.m_iCapturedAt = 100;
		ready.m_bGroundRouteReady = true;
		ready.m_iArrivalSeconds = 60;
		ready.m_iFreeGroups = 20;
		ready.m_iFreePersonnel = 128;
		ready.m_iFreeVehicles = 10;
		ready.m_aStock.Insert(Stock("ural", 2));
		ready.m_aStock.Insert(Stock("btr", 2));
		ready.m_aStock.Insert(Stock("brdm", 1));
		AROR_QRFSelectionContext context = new AROR_QRFSelectionContext();
		context.m_iDesiredTier = 3;
		AROR_QRFSelectionDecision decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 100);
		bool passed = Check(decision.m_Recipe == recipes[2], "Highest affordable tier");
		passed = Check(decision.m_iRequiredGroups == 9 && decision.m_iRequiredPersonnel == 40 && decision.m_iRequiredVehicles == 5, "Whole recipe budget") && passed;
		AROR_QRFReadyStock btr = ready.m_aStock[1];
		btr.m_iReadyPackages = 1;
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 100);
		passed = Check(decision.m_Recipe == recipes[0] && decision.m_bDowngraded, "Repeated profile stock shortage downgrades") && passed;
		context.m_bAllowDowngrade = false;
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 100);
		passed = Check(!decision.m_Recipe, "No unauthorized downgrade") && passed;
		context.m_bAllowDowngrade = true;
		btr.m_iReadyPackages = 2;
		context.m_iLocationTierCeiling = 1;
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 100);
		passed = Check(decision.m_Recipe == recipes[0], "Location ceiling") && passed;
		context.m_iLocationTierCeiling = 5;
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 131);
		passed = Check(!decision.m_Recipe, "Stale availability rejected") && passed;
		ready.m_bGroundRouteReady = false;
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 100);
		passed = Check(!decision.m_Recipe, "No route rejected") && passed;
		ready.m_bGroundRouteReady = true;
		ready.m_iArrivalSeconds = 181;
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, ready, 100);
		passed = Check(!decision.m_Recipe, "Late arrival rejected") && passed;
		ready.m_iArrivalSeconds = 60;
		AROR_QRFAvailabilitySnapshot limited = ready.WithRuntimeCapacity(3, 128, 10);
		decision = AROR_QRFSelectionPolicy.Select(recipes, profiles, ready.m_eRole, ready.m_sFactionKey, context, limited, 100);
		passed = Check(!decision.m_Recipe && ready.m_iFreeGroups == 20, "Runtime limits without mutating provider snapshot") && passed;
		passed = Check(limited.m_aStock.Count() == ready.m_aStock.Count(), "Runtime snapshot retains all stock entries") && passed;
		AROR_QRFReadyStock copiedStock = limited.m_aStock[0];
		AROR_QRFReadyStock originalStock = ready.m_aStock[0];
		int originalQuantity = originalStock.m_iReadyPackages;
		copiedStock.m_iReadyPackages = 0;
		passed = Check(copiedStock != originalStock && originalStock.m_iReadyPackages == originalQuantity, "Runtime stock copy is independent of provider") && passed;
		ready.m_aStock.Insert(Stock("btr", 10));
		passed = Check(!ready.IsValidAt(100), "Duplicate stock entry rejected") && passed;
		if (passed)
			Print("[AROR TEST] QRF selection planning regressions passed", LogLevel.NORMAL);
		return passed;
	}

	protected static AROR_QRFReadyStock Stock(string id, int count)
	{
		AROR_QRFReadyStock stock = new AROR_QRFReadyStock();
		stock.m_sProfileId = id;
		stock.m_iReadyPackages = count;
		return stock;
	}
	protected static AROR_GroundQRFRecipe Recipe(string id, int tier, array<string> elements)
	{
		AROR_GroundQRFRecipe recipe = AROR_GroundQRFRecipe.CreateForScript();
		recipe.m_sId = id;
		recipe.m_iLevel = tier;
		recipe.m_eRole = AROR_EFactionRole.INVADERS;
		recipe.m_sFactionKey = "AROR_INVADERS";
		recipe.m_aProfileIds = {};
		recipe.m_aProfileIds.Copy(elements);
		return recipe;
	}
	protected static AROR_ForceProfile Profile(string id, AROR_EForceType type, int budget)
	{
		AROR_ForceProfile profile = AROR_ForceProfile.CreateForScript();
		profile.m_sId = id;
		profile.m_eType = type;
		profile.m_eRole = AROR_EFactionRole.INVADERS;
		profile.m_sFactionKey = "AROR_INVADERS";
		profile.m_bEnabled = true;
		profile.m_iQRFWeight = 1;
		profile.m_iPersonnelBudget = budget;
		profile.m_iCrewCount = 1;
		profile.m_sVehicle = "test_vehicle.et";
		profile.m_fDismountDistance = 100;
		return profile;
	}
	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] QRF selection FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}
#endif
