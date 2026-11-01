//! Mirrors the project's map-picking lifecycle, with cancellation on map close and world teardown.
class AROR_FieldMapPicker
{
	protected int m_iId;
	protected int m_iScope;
	protected int m_iKind;
	protected int m_iLifetime;
	protected string m_sCaption;
	protected bool m_bActive;

	void Start(int id, string caption, int scope, int kind, int lifetime)
	{
		m_iId = id;
		m_sCaption = caption;
		m_iScope = scope;
		m_iKind = kind;
		m_iLifetime = lifetime;
		AROR_PhoneMenu phone = AROR_PhoneMenu.GetInstance();
		if (phone)
			phone.RequestClose();
		GetGame().GetCallqueue().CallLater(OpenMap, 500);
	}

	protected void OpenMap()
	{
		if (RplSession.Mode() == RplMode.Dedicated || !GetGame().GetPlayerController())
			return;
		m_bActive = true;
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(true);
		SCR_MapEntity.GetOnSelection().Insert(OnSelection);
		SCR_MapEntity.GetOnMapClose().Insert(OnClosed);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
		AROR_NotificationManagerComponent.ShowHint("Click the map to place your marker. Close the map to cancel.");
	}

	protected void OnSelection(vector screenPosition)
	{
		if (!m_bActive)
			return;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!mapEntity || !controller)
			return;
		float x, z;
		mapEntity.GetMapCursorWorldPosition(x, z);
		vector position = Vector(x, 0, z);
		if (!AROR_FieldAccess.MapPosition(position))
			return;
		controller.AROR_RequestFieldMarker(m_iId, position, m_sCaption, m_iScope, m_iKind, m_iLifetime);
		Cancel();
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
	}

	protected void OnClosed(MapConfiguration config) { Cancel(); }

	void Cancel()
	{
		GetGame().GetCallqueue().Remove(OpenMap);
		if (!m_bActive)
			return;
		m_bActive = false;
		SCR_MapEntity.GetOnSelection().Remove(OnSelection);
		SCR_MapEntity.GetOnMapClose().Remove(OnClosed);
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(false);
	}
}
