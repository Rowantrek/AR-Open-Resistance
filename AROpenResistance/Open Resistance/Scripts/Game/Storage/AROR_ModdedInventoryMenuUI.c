//! Resistance arsenals open as their own container panel (with a category column) instead of being traversed inside the
//! Vicinity panel, and the Vicinity panel is hidden while one is open, like the vanilla arsenal.
modded class SCR_InventoryMenuUI
{
	//! Horizontal layout: a "Tabs" column and the vanilla arsenal panel
	protected static const ResourceName AROR_ARSENAL_LAYOUT = "{8C8B4B981D4E6A11}UI/Layouts/Arsenal/AROR_ArsenalContainer.layout";

	protected static const int AROR_ARSENAL_COLUMNS = 6;
	protected static const int AROR_ARSENAL_ROWS = 10;

	//------------------------------------------------------------------------------------------------
	override bool SetOpenStorage()
	{
		IEntity storageToOpen = GetInventoryStorageManager().GetStorageToOpen();
		if (!storageToOpen || !AROR_ResistanceArsenalComponent.IsResistanceArsenal(storageToOpen))
			return super.SetOpenStorage();

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(storageToOpen.FindComponent(BaseInventoryStorageComponent));
		if (!storage)
			return super.SetOpenStorage();

		SetStorageSwitchMode(false);
		OpenStorageAsContainer(storage);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Same as vanilla, but a Resistance arsenal uses our layout and does not bring the Vicinity panel back
	override void OpenStorageAsContainer(BaseInventoryStorageComponent storage, bool showVicinity = true, bool hideCloseButton = false)
	{
		if (!storage || !AROR_ResistanceArsenalComponent.IsResistanceArsenal(storage.GetOwner()))
		{
			super.OpenStorageAsContainer(storage, showVicinity, hideCloseButton);
			return;
		}

		foreach (SCR_InventoryOpenedStorageUI openedStorage : m_aOpenedStoragesUI)
		{
			if (openedStorage.GetStorage() == storage)
			{
				RemoveOpenStorage(openedStorage);
				return;
			}
		}

		// Taller than the 3-row default, as there is no Vicinity panel beside it
		SCR_InventoryOpenedStorageUI handler = new SCR_InventoryOpenedStorageArsenalUI(storage, null, this, 0, {storage}, AROR_ARSENAL_COLUMNS, AROR_ARSENAL_ROWS);

		Widget parent = m_widget.FindAnyWidget("OpenedStorages");
		Widget newStorage = GetGame().GetWorkspace().CreateWidgets(AROR_ARSENAL_LAYOUT, parent);
		if (!newStorage)
		{
			Print("[AROR] Arsenal container layout missing: " + AROR_ARSENAL_LAYOUT, LogLevel.WARNING);
			return;
		}

		newStorage.AddHandler(handler);
		handler.ActivateCloseStorageButton();

		ScrollLayoutWidget scroll = ScrollLayoutWidget.Cast(m_widget.FindAnyWidget("LeftAreaScroll"));
		if (scroll)
			scroll.SetSliderPos(0, 0);

		m_aOpenedStoragesUI.Insert(handler);
		AROR_HideVicinityForArsenal();
		FocusOnSlotInStorage(handler);
	}

	//------------------------------------------------------------------------------------------------
	override void ShowVicinity(bool compact = false)
	{
		super.ShowVicinity(compact);
		AROR_HideVicinityForArsenal();
	}

	//------------------------------------------------------------------------------------------------
	protected void AROR_HideVicinityForArsenal()
	{
		if (!m_wLootStorage)
			return;

		foreach (SCR_InventoryOpenedStorageUI openedStorage : m_aOpenedStoragesUI)
		{
			BaseInventoryStorageComponent storage = openedStorage.GetStorage();
			if (storage && AROR_ResistanceArsenalComponent.IsResistanceArsenal(storage.GetOwner()))
			{
				m_wLootStorage.SetVisible(false);
				return;
			}
		}
	}
}
