[ComponentEditorProps(category: "OpenResistance/Storage", description: "Bridges this arsenal box to Resistance Storage. Attach on the same entity as SCR_ArsenalComponent.")]
class AROR_ResistanceArsenalComponentClass : ScriptComponentClass
{
}

//! Marks an arsenal as backed by AROR_ResistanceStorageComponent (see ARCHITECTURE.md -> Resistance Storage).
//! Stock list refreshes when the campaign ledger changes. Take/refund are handled by the modded
//! SCR_ResourcePlayerControllerInventoryComponent path.
class AROR_ResistanceArsenalComponent : ScriptComponent
{
	protected static const int BIND_RETRY_MS = 500;
	protected static const int BIND_RETRY_MAX = 20;

	protected SCR_ArsenalComponent m_Arsenal;
	protected bool m_bListening;
	protected int m_iBindAttempts;

	//------------------------------------------------------------------------------------------------
	static AROR_ResistanceArsenalComponent FindOn(IEntity entity)
	{
		if (!entity)
			return null;

		return AROR_ResistanceArsenalComponent.Cast(entity.FindComponent(AROR_ResistanceArsenalComponent));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsResistanceArsenal(IEntity entity)
	{
		return FindOn(entity) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode() || !GetGame().InPlayMode())
			return;

		m_Arsenal = SCR_ArsenalComponent.Cast(owner.FindComponent(SCR_ArsenalComponent));
		if (!m_Arsenal)
		{
			Print("[AROR] AROR_ResistanceArsenalComponent needs SCR_ArsenalComponent on the same entity", LogLevel.WARNING);
			return;
		}

		// Vanilla FactionInit disables arsenals when ArsenalManager is missing or the type flag is off.
		// Re-enable after that CallLater(FactionInit) has run.
		GetGame().GetCallqueue().CallLater(EnsureArsenalEnabled, 100);

		// Storage may init after world props; retry briefly
		TryBindStorage();
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureArsenalEnabled()
	{
		if (!m_Arsenal)
			return;

		if (!m_Arsenal.IsArsenalEnabled())
			m_Arsenal.SetArsenalEnabled(true, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryBindStorage()
	{
		if (m_bListening)
			return;

		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
		if (!storage)
		{
			m_iBindAttempts++;
			if (m_iBindAttempts < BIND_RETRY_MAX)
				GetGame().GetCallqueue().CallLater(TryBindStorage, BIND_RETRY_MS);
			else
				Print("[AROR] AROR_ResistanceArsenalComponent: Resistance Storage not found on game mode", LogLevel.WARNING);

			return;
		}

		storage.GetOnStorageChanged().Insert(OnStorageChanged);
		m_bListening = true;
		RefreshFromStorage();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStorageChanged()
	{
		RefreshFromStorage();
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshFromStorage()
	{
		if (m_Arsenal)
			m_Arsenal.RefreshArsenal();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(TryBindStorage);
		GetGame().GetCallqueue().Remove(EnsureArsenalEnabled);

		if (m_bListening)
		{
			AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
			if (storage)
				storage.GetOnStorageChanged().Remove(OnStorageChanged);

			m_bListening = false;
		}

		m_Arsenal = null;
		super.OnDelete(owner);
	}
}
