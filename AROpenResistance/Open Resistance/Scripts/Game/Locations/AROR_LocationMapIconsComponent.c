[ComponentEditorProps(category: "OpenResistance/Locations", description: "Draws location icons on any open map (client only). Attach to the game mode entity.")]
class AROR_LocationMapIconsComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Location markers (map marker layer 1): our own icon widgets drawn on whichever map is open
//! (player map, deploy screen, setup screen), positioned from world coordinates every frame while open.
//! Follows the location registry, so spawned locations (the HQ) get icons too. Never runs on a dedicated server.
class AROR_LocationMapIconsComponent : SCR_BaseGameModeComponent
{
	[Attribute("{6E2BA15FAEED49D7}UI/Layouts/Map/AROR_MapIcon.layout", UIWidgets.ResourceNamePicker, desc: "Layout for one icon; must contain an ImageWidget named 'Icon'", params: "layout")]
	protected ResourceName m_sIconLayout;

	[Attribute("40", UIWidgets.Slider, desc: "Icon size in reference pixels", params: "8 128 1")]
	protected float m_fIconSize;

	[Attribute("1", desc: "Draw locations as military-frame symbols (rectangle ours, diamond enemy) instead of the icon textures")]
	protected bool m_bFrameIcons;

	[Attribute("1", desc: "Show the local player's own position and facing on the map", category: "Navigation")]
	protected bool m_bShowOwnPosition;

	[Attribute("1", desc: "Shift-click on the map sets a personal waypoint (shown on the map and in the world); shift-click it again to remove it", category: "Navigation")]
	protected bool m_bShiftClickWaypoint;

	[Attribute("{EC2170E3AEEDEC1E}UI/Layouts/Map/AROR_MapInfoPopup.layout", UIWidgets.ResourceNamePicker, desc: "Layout of the info popup shown when an icon is clicked (see AROR_LocationInfoPopup for the widget names)", params: "layout", category: "Info popup")]
	protected ResourceName m_sPopupLayout;

	[Attribute("360", desc: "Popup width in reference pixels (height fits the text)", category: "Info popup")]
	protected float m_fPopupWidth;

	[Attribute("0.031 0.043 0.035 0.94", UIWidgets.ColorPicker, desc: "Popup background colour (design system strong plate)", category: "Info popup")]
	protected ref Color m_PopupBackgroundColor;

	[Attribute("{9B65C8ACFFEE4DD6}UI/Layouts/Map/AROR_MapIconSelector.layout", UIWidgets.ResourceNamePicker, desc: "Corner-bracket selector shown on hover (and steady on the selected icon)", params: "layout", category: "Selector")]
	protected ResourceName m_sSelectorLayout;

	[Attribute("1 1 1 1", UIWidgets.ColorPicker, desc: "Selector colour", category: "Selector")]
	protected ref Color m_SelectorColor;

	[Attribute("12", desc: "Selector size beyond the icon size, in reference pixels", category: "Selector")]
	protected float m_fSelectorPadding;

	[Attribute("1.6", desc: "Seconds for one fade in and out while hovering", category: "Selector")]
	protected float m_fSelectorPulseSeconds;

	protected static const string ICON_WIDGET_NAME = "Icon";
	protected static const string LABEL_WIDGET_NAME = "QRFLabel";	//!< Shared with QRF intel markers, which caption it themselves
	protected static const string FRAME_WIDGET_NAME = "Frame";
	protected static const int POPUP_Z_ORDER = 1000;	//!< Above every marker, including QRF and mission markers created later
	protected static const int SELECTOR_Z_ORDER = 900;
	protected static const float POPUP_EDGE_MARGIN = 8;	//!< Keeps the popup this far inside the map, in reference pixels

	//! Extra clickable margin around an icon, in reference pixels
	protected static const float CLICK_MARGIN = 4;

	//! Set while another feature uses map clicks (HQ pick, teleport) so clicks do not open the popup
	protected static bool s_bInfoPopupSuppressed;
	protected static bool s_bSuppressed;
	protected bool m_bHandlingMap;	//!< The open map is one we attached to (so its close is ours to handle)

	protected Widget m_wIconParent;
	protected ref AROR_PlayerMapMarkers m_PlayerMarkers = new AROR_PlayerMapMarkers();
	protected ref AROR_NavigationMapMarkers m_Navigation = new AROR_NavigationMapMarkers();
	protected ref AROR_PlayerWaypoint m_Waypoint;
	protected ref AROR_QRFMapIntel m_QRFIntel = new AROR_QRFMapIntel();
	protected ref AROR_MissionMapMarkers m_MissionMarkers = new AROR_MissionMapMarkers();
	protected ref AROR_OperationMapMarkers m_OperationMarkers = new AROR_OperationMapMarkers();
	protected ref array<AROR_LocationComponent> m_aIconLocations = {};
	protected ref array<Widget> m_aIconWidgets = {};
	//! Draw commands per frame canvas: the canvas does not keep its own copy, so they live as long as the icon
	protected ref map<Widget, ref array<ref CanvasWidgetCommand>> m_mFrameCommands = new map<Widget, ref array<ref CanvasWidgetCommand>>();
	protected ref AROR_LocationInfoPopup m_Popup;
	protected ref AROR_MapIconSelector m_Selector;

	//------------------------------------------------------------------------------------------------
	static void SetInfoPopupSuppressed(bool suppressed)
	{
		s_bInfoPopupSuppressed = suppressed;
	}

	//------------------------------------------------------------------------------------------------
	//! While set, maps that open are ignored entirely (HICOMM opens the game map inside the phone and draws its
	//! own location icons there)
	static void SetSuppressed(bool suppressed)
	{
		s_bSuppressed = suppressed;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapOpen(MapConfiguration config)
	{
		if (s_bSuppressed)
			return;

		m_bHandlingMap = true;
		SetQRFObserverMapOpen(true);
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.GetMapWidget())
			return;

		ClearIcons();
		m_wIconParent = mapEntity.GetMapWidget().GetParent();
		if (!m_wIconParent)
			return;

		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
			return;

		// Positioning starts first, and each icon is built in its own call: a script error while building one
		// icon then loses only that icon instead of aborting this handler (and with it every icon on the map)
		GetGame().GetCallqueue().CallLater(UpdateIconPositions, 0, true);
		array<AROR_LocationComponent> locations = {};
		locationManager.GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
		{
			GetGame().GetCallqueue().Call(CreateIcon, location);
		}
		GetGame().GetCallqueue().Call(ReportIcons, locations.Count());

		locationManager.GetOnLocationRegistered().Insert(CreateIcon);
		locationManager.GetOnLocationUnregistered().Insert(RemoveIcon);
		locationManager.GetOnLocationControlChanged().Insert(OnLocationControlChanged);
		locationManager.GetOnLocationSupportChanged().Insert(OnLocationSupportChanged);

		if (!m_sPopupLayout.IsEmpty())
		{
			m_Popup = new AROR_LocationInfoPopup(m_sPopupLayout, m_wIconParent, m_fPopupWidth, m_PopupBackgroundColor);
			m_Popup.SetZOrder(POPUP_Z_ORDER);
			SCR_MapEntity.GetOnSelection().Insert(OnMapClicked);
		}

		if (!m_sSelectorLayout.IsEmpty())
		{
			m_Selector = new AROR_MapIconSelector(m_sSelectorLayout, m_wIconParent, m_SelectorColor);
			m_Selector.SetZOrder(SELECTOR_Z_ORDER);
		}

		if (m_Waypoint)
			SCR_MapEntity.GetOnSelection().Insert(OnWaypointClick);

		// Per frame only while a map is open (scheduled above), so icons follow pan and zoom interpolation
	}

	//------------------------------------------------------------------------------------------------
	//! One log line per map opening, so missing icons show up in the console
	protected void ReportIcons(int locationCount)
	{
		Print(string.Format("[AROR] Map icons: %1 of %2 locations drawn", m_aIconLocations.Count(), locationCount), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapClose(MapConfiguration config)
	{
		if (!m_bHandlingMap)
			return;

		m_bHandlingMap = false;
		SetQRFObserverMapOpen(false);
		GetGame().GetCallqueue().Remove(UpdateIconPositions);

		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (locationManager)
		{
			locationManager.GetOnLocationRegistered().Remove(CreateIcon);
			locationManager.GetOnLocationUnregistered().Remove(RemoveIcon);
			locationManager.GetOnLocationControlChanged().Remove(OnLocationControlChanged);
			locationManager.GetOnLocationSupportChanged().Remove(OnLocationSupportChanged);
		}

		SCR_MapEntity.GetOnSelection().Remove(OnMapClicked);
		SCR_MapEntity.GetOnSelection().Remove(OnWaypointClick);
		m_Popup = null;
		m_Selector = null;
		ClearIcons();
	}

	//------------------------------------------------------------------------------------------------
	// Personal waypoint
	//------------------------------------------------------------------------------------------------
	protected static bool IsShiftHeld()
	{
		return Debug.KeyState(KeyCode.KC_LSHIFT) || Debug.KeyState(KeyCode.KC_RSHIFT);
	}

	//------------------------------------------------------------------------------------------------
	//! Shift-click: sets or moves the waypoint, or removes it when the click is on it. Ignored while another
	//! feature uses map clicks (marker placement, HQ pick, teleport).
	protected void OnWaypointClick(vector screenPosition)
	{
		if (!m_Waypoint || s_bInfoPopupSuppressed || !IsShiftHeld())
			return;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		float worldX, worldZ;
		mapEntity.GetMapCursorWorldPosition(worldX, worldZ);
		int cursorX, cursorY;
		mapEntity.WorldToScreen(worldX, worldZ, cursorX, cursorY, true);
		if (AROR_NavigationMapMarkers.IsOnWaypoint(mapEntity, m_Waypoint, cursorX, cursorY))
		{
			m_Waypoint.Clear();
			AROR_NotificationManagerComponent.ShowHint("Personal marker removed.");
			return;
		}

		m_Waypoint.Set(Vector(worldX, 0, worldZ));
		Print(string.Format("[AROR] Personal marker set at %1", m_Waypoint.GetPosition()), LogLevel.NORMAL);
		AROR_NotificationManagerComponent.ShowHint("Personal marker set. Shift-click it on the map to remove it.");
	}

	//------------------------------------------------------------------------------------------------
	// Info popup
	//------------------------------------------------------------------------------------------------
	//! The click event's coordinates are not used: the map cursor's world position is converted to the screen
	//! the same way as the icons, so both are compared in the same space
	protected void OnMapClicked(vector screenPosition)
	{
		if (!m_Popup)
			return;

		// Shift-click is the waypoint's (OnWaypointClick)
		if (m_Waypoint && IsShiftHeld())
			return;

		if (s_bInfoPopupSuppressed)
		{
			m_Popup.Hide();
			return;
		}

		AROR_LocationComponent clicked = FindIconAtCursor();
		if (!clicked || clicked == m_Popup.GetLocation())
		{
			// Clicking empty map, or the same icon again, closes the popup
			m_Popup.Hide();
			return;
		}

		m_Popup.Show(clicked);
		UpdateIconPositions();
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_LocationComponent FindIconAtCursor()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return null;

		float cursorWorldX, cursorWorldZ;
		mapEntity.GetMapCursorWorldPosition(cursorWorldX, cursorWorldZ);

		int cursorX, cursorY;
		mapEntity.WorldToScreen(cursorWorldX, cursorWorldZ, cursorX, cursorY, true);

		float hitRadius = GetGame().GetWorkspace().DPIScale(m_fIconSize * 0.5 + CLICK_MARGIN);
		AROR_LocationComponent nearest;
		float nearestDistance = hitRadius;

		foreach (AROR_LocationComponent location : m_aIconLocations)
		{
			if (!location)
				continue;

			vector center = location.GetCenter();
			int iconX, iconY;
			mapEntity.WorldToScreen(center[0], center[2], iconX, iconY, true);

			float distance = Math.Sqrt((iconX - cursorX) * (iconX - cursorX) + (iconY - cursorY) * (iconY - cursorY));
			if (distance <= nearestDistance)
			{
				nearestDistance = distance;
				nearest = location;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLocationSupportChanged(AROR_LocationComponent location)
	{
		if (m_Popup && m_Popup.GetLocation() == location)
			m_Popup.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateIcon(AROR_LocationComponent location)
	{
		if (!location || !m_wIconParent || m_aIconLocations.Contains(location))
			return;

		ResourceName icon = location.GetMapIcon();
		if (icon.IsEmpty())
			return;

		Widget root = GetGame().GetWorkspace().CreateWidgets(m_sIconLayout, m_wIconParent);
		if (!root)
			return;

		ImageWidget image = ImageWidget.Cast(root.FindAnyWidget(ICON_WIDGET_NAME));
		if (!image)
		{
			Print(string.Format("[AROR] Map icon layout has no ImageWidget named '%1'", ICON_WIDGET_NAME), LogLevel.ERROR);
			root.RemoveFromHierarchy();
			return;
		}

		// Registered first, so the icon is always positioned even if its styling fails
		m_aIconLocations.Insert(location);
		m_aIconWidgets.Insert(root);

		image.LoadImageTexture(0, icon);
		FrameSlot.SetSize(root, m_fIconSize, m_fIconSize);
		FrameSlot.SetAlignment(root, 0.5, 0.5);
		Widget ownLabel = root.FindAnyWidget(LABEL_WIDGET_NAME);
		if (ownLabel)
			ownLabel.SetVisible(false);
		ApplyControlColor(image, location);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveIcon(AROR_LocationComponent location)
	{
		int index = m_aIconLocations.Find(location);
		if (index < 0)
			return;

		if (m_Popup && m_Popup.GetLocation() == location)
			m_Popup.Hide();

		if (m_aIconWidgets[index])
		{
			m_mFrameCommands.Remove(m_aIconWidgets[index].FindAnyWidget(FRAME_WIDGET_NAME));
			m_aIconWidgets[index].RemoveFromHierarchy();
		}

		m_aIconLocations.Remove(index);
		m_aIconWidgets.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearIcons()
	{
		m_PlayerMarkers.ClearWidgets();
		m_Navigation.ClearWidgets();
		m_QRFIntel.ClearWidgets();
		m_MissionMarkers.ClearWidgets();
		m_OperationMarkers.ClearWidgets();
		foreach (Widget widget : m_aIconWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}

		m_aIconLocations.Clear();
		m_aIconWidgets.Clear();
		m_mFrameCommands.Clear();
		m_wIconParent = null;
	}
	protected void SetQRFObserverMapOpen(bool open)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.AROR_SetQRFMapOpen(open);
	}

	//------------------------------------------------------------------------------------------------
	//! Tint the icon with the controlling role's faction colour (icons must be white to show the tint)
	protected void ApplyControlColor(notnull ImageWidget image, notnull AROR_LocationComponent location)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return;

		Faction faction = roles.GetFaction(location.GetControllingRole());
		if (faction)
			image.SetColor(faction.GetFactionColor());

		// Military-frame style: the frame canvas replaces the texture icon (see AROR_MapFrameIcon)
		Widget root = image.GetParent();
		CanvasWidget frame;
		if (root)
			frame = CanvasWidget.Cast(root.FindAnyWidget(FRAME_WIDGET_NAME));
		bool framed = m_bFrameIcons && frame != null;
		if (frame)
			frame.SetVisible(framed);
		if (!framed)
		{
			image.SetVisible(true);
			return;
		}

		Color color;
		if (faction)
			color = faction.GetFactionColor();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		array<ref CanvasWidgetCommand> commands = m_mFrameCommands.Get(frame);
		if (!commands)
		{
			commands = new array<ref CanvasWidgetCommand>();
			m_mFrameCommands.Set(frame, commands);
		}
		if (location.GetOwner().FindComponent(AROR_StrongholdComponent))
			DrawStrongholdFlag(frame, commands, location, color);
		else
			AROR_MapFrameIcon.Draw(frame, commands, workspace.DPIScale(m_fIconSize), workspace.DPIScale(1), location.GetLocationType(), location.GetControllingRole(), color);

		// Only once the frame has drawn: if drawing fails, the texture icon stays visible
		image.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Strongholds are the campaign's objectives: they show the national flag of the army holding them, centred in
	//! the icon square, instead of a military frame
	protected void DrawStrongholdFlag(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, notnull AROR_LocationComponent location, Color color)
	{
		string presetId;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			presetId = presets.GetPresetId(location.GetControllingRole());
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float width = workspace.DPIScale(m_fIconSize);
		float height = width * 2 / 3;
		AROR_ArmyFlag.DrawAt(canvas, commands, 0, (width - height) * 0.5, width, height, presetId, color, workspace.DPIScale(AROR_MapFrameIcon.KEYLINE_WIDTH));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLocationControlChanged(AROR_LocationComponent location)
	{
		int index = m_aIconLocations.Find(location);
		if (index < 0 || !m_aIconWidgets[index])
			return;

		ImageWidget image = ImageWidget.Cast(m_aIconWidgets[index].FindAnyWidget(ICON_WIDGET_NAME));
		if (image)
			ApplyControlColor(image, location);

		if (m_Popup && m_Popup.GetLocation() == location)
			m_Popup.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateIconPositions()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		int screenX, screenY;

		foreach (int i, AROR_LocationComponent location : m_aIconLocations)
		{
			Widget widget = m_aIconWidgets[i];
			if (!location || !widget)
				continue;

			vector center = location.GetCenter();
			mapEntity.WorldToScreen(center[0], center[2], screenX, screenY, true);
			float x = workspace.DPIUnscale(screenX);
			float y = workspace.DPIUnscale(screenY);
			FrameSlot.SetPos(widget, x, y);

			if (m_Popup && m_Popup.GetLocation() == location)
				PositionPopup(workspace, x, y);
		}

		UpdateSelector(mapEntity, workspace);
		m_QRFIntel.Update(m_wIconParent, m_sIconLayout, mapEntity);
		m_PlayerMarkers.Update(m_wIconParent, m_sIconLayout, mapEntity);
		m_MissionMarkers.Update(m_wIconParent, m_sIconLayout, mapEntity);
		m_OperationMarkers.Update(m_wIconParent, m_sIconLayout, mapEntity);
		// Last, so the player's own arrow and waypoint draw above the other markers
		m_Navigation.Update(m_wIconParent, m_sIconLayout, mapEntity, m_Waypoint, m_bShowOwnPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Popup sits to the right of its icon (clear of the selector), top-aligned with it. Near the right edge of the
	//! map it flips to the left of the icon; vertically it is kept inside the map.
	protected void PositionPopup(notnull WorkspaceWidget workspace, float iconX, float iconY)
	{
		float mapWidth, mapHeight;
		if (m_wIconParent)
			m_wIconParent.GetScreenSize(mapWidth, mapHeight);
		mapWidth = workspace.DPIUnscale(mapWidth);
		mapHeight = workspace.DPIUnscale(mapHeight);

		float offset = (m_fIconSize + m_fSelectorPadding) * 0.5 + CLICK_MARGIN;
		float width = m_Popup.GetWidth();
		float height = m_Popup.GetHeight();

		float x = iconX + offset;
		float y = iconY - m_fIconSize * 0.5;
		if (mapWidth > 0)
		{
			if (x + width > mapWidth - POPUP_EDGE_MARGIN)
				x = iconX - offset - width;
			x = Math.Clamp(x, POPUP_EDGE_MARGIN, Math.Max(POPUP_EDGE_MARGIN, mapWidth - width - POPUP_EDGE_MARGIN));
		}
		if (mapHeight > 0)
			y = Math.Clamp(y, POPUP_EDGE_MARGIN, Math.Max(POPUP_EDGE_MARGIN, mapHeight - height - POPUP_EDGE_MARGIN));

		m_Popup.SetPosition(x, y);
	}

	//------------------------------------------------------------------------------------------------
	//! Brackets pulse around the hovered icon and stay steady around the icon whose popup is open
	protected void UpdateSelector(notnull SCR_MapEntity mapEntity, notnull WorkspaceWidget workspace)
	{
		if (!m_Selector)
			return;

		AROR_LocationComponent selected;
		if (m_Popup)
			selected = m_Popup.GetLocation();

		AROR_LocationComponent hovered;
		if (!s_bInfoPopupSuppressed)
			hovered = FindIconAtCursor();

		AROR_LocationComponent target = hovered;
		if (!target)
			target = selected;

		if (!target)
		{
			m_Selector.Hide();
			return;
		}

		float opacity = 1;
		if (target != selected && m_fSelectorPulseSeconds > 0)
		{
			float seconds = GetGame().GetWorld().GetWorldTime() * 0.001;
			float wave = Math.Sin(seconds * Math.PI2 / m_fSelectorPulseSeconds);
			opacity = 0.25 + 0.75 * (0.5 + 0.5 * wave);
		}

		vector center = target.GetCenter();
		int screenX, screenY;
		mapEntity.WorldToScreen(center[0], center[2], screenX, screenY, true);
		m_Selector.Show(workspace.DPIUnscale(screenX), workspace.DPIUnscale(screenY), m_fIconSize + m_fSelectorPadding, opacity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;
		AROR_QRFMapIntel.Clear();
		if (m_bShiftClickWaypoint)
			m_Waypoint = new AROR_PlayerWaypoint(m_sIconLayout);

		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UpdateIconPositions);
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
		SCR_MapEntity.GetOnSelection().Remove(OnMapClicked);
		SCR_MapEntity.GetOnSelection().Remove(OnWaypointClick);
		if (m_Waypoint)
			m_Waypoint.Clear();
		m_Waypoint = null;
		m_Popup = null;
		m_Selector = null;
		ClearIcons();

		super.OnDelete(owner);
	}
}
