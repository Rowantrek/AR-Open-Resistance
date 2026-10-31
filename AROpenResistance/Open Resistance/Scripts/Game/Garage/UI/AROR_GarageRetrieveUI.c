//! Client entry point for Resistance Garage retrieve (see ARCHITECTURE.md).
//! Call Open(anchor) from a future user action on a garage object. No temporary hotkey.
class AROR_GarageRetrieveUI
{
	protected static const string PRESET_BROWSE = "aror_garage_browse";
	protected static const string PRESET_INVENTORY = "aror_garage_inventory";
	protected static const ResourceName DIALOG_PRESETS = "{5BB4D0ED53225DDC}Configs/UI/AROR_SetupDialogs.conf";

	protected static ref AROR_GarageRetrieveUI s_Scheduler = new AROR_GarageRetrieveUI();

	protected static IEntity s_Anchor;
	protected static RplId s_AnchorRplId;
	protected static int s_iIndex;
	protected static ref array<int> s_aIds = {};
	protected static ref array<ResourceName> s_aPrefabs = {};
	protected static ref array<float> s_aHealths = {};
	protected static ref array<float> s_aFuels = {};
	protected static ref array<string> s_aPendingCargo;
	protected static int s_iPendingCargoEntryId = -1;
	protected static bool s_bWaitingReserve;
	protected static int s_iPendingReserveId = -1;

	//------------------------------------------------------------------------------------------------
	//! Open the retrieve browser anchored to a garage object (client).
	static void Open(IEntity anchor)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !anchor)
			return;

		if (SCR_ConfigurableDialogUi.GetCurrentDialog())
			return;

		if (AROR_GaragePlacementController.IsActive())
			return;

		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (!garage)
			return;

		RplId anchorId = SCR_EntityHelper.EntityToRplId(anchor);
		if (!anchorId.IsValid())
		{
			Print("[AROR] Garage retrieve: anchor needs an RplComponent", LogLevel.WARNING);
			return;
		}

		s_Anchor = anchor;
		s_AnchorRplId = anchorId;
		RefreshEntryCache();
		s_iIndex = 0;
		OpenBrowseDialog();
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetAnchor()
	{
		return s_Anchor;
	}

	//------------------------------------------------------------------------------------------------
	static RplId GetAnchorRplId()
	{
		return s_AnchorRplId;
	}

	//------------------------------------------------------------------------------------------------
	static void RefreshEntryCache()
	{
		s_aIds.Clear();
		s_aPrefabs.Clear();
		s_aHealths.Clear();
		s_aFuels.Clear();

		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (garage)
			garage.GetEntries(s_aIds, s_aPrefabs, s_aHealths, s_aFuels);

		if (s_iIndex >= s_aIds.Count())
			s_iIndex = Math.Max(s_aIds.Count() - 1, 0);
	}

	//------------------------------------------------------------------------------------------------
	static int GetSelectedEntryId()
	{
		if (s_iIndex < 0 || s_iIndex >= s_aIds.Count())
			return -1;

		return s_aIds[s_iIndex];
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName GetSelectedPrefab()
	{
		ResourceName empty;
		if (s_iIndex < 0 || s_iIndex >= s_aPrefabs.Count())
			return empty;

		return s_aPrefabs[s_iIndex];
	}

	//------------------------------------------------------------------------------------------------
	static string BuildBrowseMessage()
	{
		int count = s_aIds.Count();
		if (count == 0)
			return "Garage is empty.";

		ResourceName prefab = s_aPrefabs[s_iIndex];
		float health = s_aHealths[s_iIndex];
		float fuel = s_aFuels[s_iIndex];
		float radius = 50;
		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (garage)
			radius = garage.GetPlaceRadius();

		return string.Format(
			"Vehicle %1 of %2\nID %3\n%4\nHealth %5%%\nFuel %6%%\n\nSpawn: place a ghost within %7 m of this garage.",
			s_iIndex + 1,
			count,
			s_aIds[s_iIndex],
			AROR_GarageSnapshotHelper.PrefabDisplayName(prefab),
			Math.Round(health * 100),
			Math.Round(fuel * 100),
			Math.Round(radius)
		);
	}

	//------------------------------------------------------------------------------------------------
	static void OpenBrowseDialog()
	{
		RefreshEntryCache();
		AROR_GarageBrowseDialog dialog = new AROR_GarageBrowseDialog();
		SCR_ConfigurableDialogUi.CreateFromPreset(DIALOG_PRESETS, PRESET_BROWSE, dialog);
		dialog.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	static void StepIndex(int delta)
	{
		int count = s_aIds.Count();
		if (count == 0)
			return;

		s_iIndex = Math.Clamp(s_iIndex + delta, 0, count - 1);
	}

	//------------------------------------------------------------------------------------------------
	static bool CanStepPrevious()
	{
		return s_iIndex > 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanStepNext()
	{
		return s_iIndex < s_aIds.Count() - 1;
	}

	//------------------------------------------------------------------------------------------------
	static void RequestInventory()
	{
		int id = GetSelectedEntryId();
		if (id <= 0)
			return;

		s_iPendingCargoEntryId = id;
		s_aPendingCargo = null;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.AROR_RequestGarageEntryDetail(id);
	}

	//------------------------------------------------------------------------------------------------
	static void OnEntryDetailReceived(int entryId, notnull array<string> labels)
	{
		if (entryId != s_iPendingCargoEntryId)
			return;

		s_aPendingCargo = {};
		s_aPendingCargo.Copy(labels);
		GetGame().GetCallqueue().CallLater(s_Scheduler.OpenInventoryDialog_I, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenInventoryDialog_I()
	{
		OpenInventoryDialog();
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenInventoryDialog()
	{
		if (!s_aPendingCargo)
			return;

		string message;
		if (s_aPendingCargo.IsEmpty())
			message = "No cargo stored on this vehicle.";
		else
		{
			int shown = Math.Min(s_aPendingCargo.Count(), 20);
			for (int i = 0; i < shown; i++)
			{
				if (i > 0)
					message += "\n";
				message += s_aPendingCargo[i];
			}

			if (s_aPendingCargo.Count() > shown)
				message += string.Format("\n… +%1 more", s_aPendingCargo.Count() - shown);
		}

		AROR_GarageInventoryDialog dialog = new AROR_GarageInventoryDialog();
		SCR_ConfigurableDialogUi.CreateFromPreset(DIALOG_PRESETS, PRESET_INVENTORY, dialog);
		dialog.SetMessage(message);
	}

	//------------------------------------------------------------------------------------------------
	static void RequestSpawnPlacement()
	{
		int id = GetSelectedEntryId();
		if (id <= 0 || !s_Anchor)
			return;

		s_bWaitingReserve = true;
		s_iPendingReserveId = id;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.AROR_RequestGarageReserve(id);
	}

	//------------------------------------------------------------------------------------------------
	static void OnReserveResultReceived(int entryId, bool success)
	{
		if (!s_bWaitingReserve || entryId != s_iPendingReserveId)
			return;

		s_bWaitingReserve = false;
		if (!success)
		{
			AROR_NotificationManagerComponent.ShowHint("Could not reserve that vehicle (missing or in use).");
			GetGame().GetCallqueue().CallLater(s_Scheduler.OpenBrowseDialog_I, 0, false);
			return;
		}

		GetGame().GetCallqueue().CallLater(s_Scheduler.StartPlacementDeferred_I, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	void OpenBrowseDialog_I()
	{
		OpenBrowseDialog();
	}

	//------------------------------------------------------------------------------------------------
	void StartPlacementDeferred_I()
	{
		int entryId = s_iPendingReserveId;
		ResourceName prefab = GetSelectedPrefab();
		AROR_GaragePlacementController.Start(entryId, prefab, s_Anchor, s_AnchorRplId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestSpawnPlacement_I()
	{
		RequestSpawnPlacement();
	}

	//------------------------------------------------------------------------------------------------
	void RequestInventory_I()
	{
		RequestInventory();
	}

	//------------------------------------------------------------------------------------------------
	static void OnRetrieveResultReceived(bool success)
	{
		AROR_GaragePlacementController.Stop();
		if (success)
			AROR_NotificationManagerComponent.ShowHint("Vehicle retrieved.");
		else
		{
			AROR_NotificationManagerComponent.ShowHint("Retrieve failed (range, collision, or entry gone).");
			GetGame().GetCallqueue().CallLater(s_Scheduler.OpenBrowseDialog_I, 0, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void OnPlacementCancelled()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.AROR_RequestGarageRelease();

		GetGame().GetCallqueue().CallLater(s_Scheduler.OpenBrowseDialog_I, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	static AROR_GarageRetrieveUI GetScheduler()
	{
		return s_Scheduler;
	}

	//------------------------------------------------------------------------------------------------
	static void CloseAll()
	{
		s_Anchor = null;
		s_AnchorRplId = RplId.Invalid();
		s_bWaitingReserve = false;
		AROR_GaragePlacementController.Stop();
	}
}
