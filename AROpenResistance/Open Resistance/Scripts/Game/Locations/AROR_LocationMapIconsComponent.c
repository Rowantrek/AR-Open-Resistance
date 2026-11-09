[ComponentEditorProps(category: "OpenResistance/Locations", description: "Draws location icons on any open map (client only). Attach to the game mode entity.")]
class AROR_LocationMapIconsComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Location markers (map marker layer 1): our own icon widgets drawn on whichever map is open
//! (player map, deploy screen, setup screen), positioned from world coordinates every frame while open.
//! Follows the location registry, so spawned locations (the HQ) get icons too. Never runs on a dedicated server.
class AROR_LocationMapIconsComponent : SCR_BaseGameModeComponent
{
	[Attribute("{6E2BA15FAEED49D7}UI/Layouts/Map/AROR_MapIcon.layout", UIWidgets.ResourceNamePicker, desc: "Shared icon layout: ImageWidget 'Icon', CanvasWidget 'Frame' for spotting silhouettes, RichTextWidget 'QRFLabel' for contact labels", params: "layout")]
	protected ResourceName m_sIconLayout;

	[Attribute("40", UIWidgets.Slider, desc: "Icon size of key sites (bases, outposts, airfields, ports, HQ, strongholds) in reference pixels; other locations scale from it", params: "8 128 1")]
	protected float m_fIconSize;

	[Attribute("0.8", desc: "Factories and resource sites, as a share of the key-site size", params: "0.3 1 0.05", category: "Marker tiers")]
	protected float m_fEconomicScale;

	[Attribute("0.72", desc: "Cities, as a share of the key-site size", params: "0.3 1 0.05", category: "Marker tiers")]
	protected float m_fCityScale;

	[Attribute("0.58", desc: "Towns, as a share of the key-site size (the map already prints their names)", params: "0.3 1 0.05", category: "Marker tiers")]
	protected float m_fTownScale;

	[Attribute("1", desc: "Print key sites' names in bold on a plate beside their marker", category: "Marker tiers")]
	protected bool m_bKeySiteNames;

	[Attribute("15", desc: "Key-site name size in reference pixels", params: "10 28 1", category: "Marker tiers")]
	protected int m_iNameFontSize;

	[Attribute("1", desc: "Mark each town's garrison post with a flag in the holding faction's colour (outline once taken)", category: "Marker tiers")]
	protected bool m_bPostMarkers;

	[Attribute("22", desc: "Garrison post flag size in reference pixels", params: "8 48 1", category: "Marker tiers")]
	protected float m_fPostMarkerSize;

	[Attribute("1", desc: "Draw locations as military-frame symbols (rectangle ours, diamond enemy) instead of the icon textures")]
	protected bool m_bFrameIcons;

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
	protected ref AROR_QRFMapIntel m_QRFIntel = new AROR_QRFMapIntel();
	protected ref AROR_MissionMapMarkers m_MissionMarkers = new AROR_MissionMapMarkers();
	protected ref AROR_OperationMapMarkers m_OperationMarkers = new AROR_OperationMapMarkers();
	protected ref AROR_PlayerMapMarkers m_PlayerMarkers = new AROR_PlayerMapMarkers();
	protected ref array<AROR_LocationComponent> m_aIconLocations = {};
	protected ref array<Widget> m_aIconWidgets = {};
	//! Parallel to m_aIconLocations; null where a location has no name plate or post flag
	protected ref array<Widget> m_aNamePlates = {};
	protected ref array<Widget> m_aNameLabels = {};
	protected ref array<Widget> m_aPostWidgets = {};
	protected ref array<int> m_aPostStates = {};	//!< Last drawn post state (role * 2 + taken), -1 for none yet
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
		// A phone/HICOMM map still obscures observation even when it draws its own location layer.
		SetQRFObserverMapOpen(true);
		if (s_bSuppressed)
			return;

		m_bHandlingMap = true;
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
		SetQRFObserverMapOpen(false);
		if (!m_bHandlingMap)
			return;

		m_bHandlingMap = false;
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
		m_Popup = null;
		m_Selector = null;
		ClearIcons();
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

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		AROR_LocationComponent nearest;
		float nearestDistance = float.MAX;

		foreach (AROR_LocationComponent location : m_aIconLocations)
		{
			if (!location)
				continue;

			vector center = location.GetCenter();
			int iconX, iconY;
			mapEntity.WorldToScreen(center[0], center[2], iconX, iconY, true);

			float hitRadius = workspace.DPIScale(IconSize(location) * 0.5 + CLICK_MARGIN);
			float distance = Math.Sqrt((iconX - cursorX) * (iconX - cursorX) + (iconY - cursorY) * (iconY - cursorY));
			if (distance <= hitRadius && distance <= nearestDistance)
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
		m_aNamePlates.Insert(null);
		m_aNameLabels.Insert(null);
		m_aPostWidgets.Insert(null);
		m_aPostStates.Insert(-1);
		int index = m_aIconLocations.Count() - 1;

		image.LoadImageTexture(0, icon);
		float size = IconSize(location);
		FrameSlot.SetSize(root, size, size);
		FrameSlot.SetAlignment(root, 0.5, 0.5);
		Widget ownLabel = root.FindAnyWidget(LABEL_WIDGET_NAME);
		if (ownLabel)
			ownLabel.SetVisible(false);
		ApplyControlColor(image, location);

		if (m_bKeySiteNames && IsKeySite(location))
			CreateNamePlate(index, location);
		if (m_bPostMarkers && AROR_CaptureLocationAction.GetSettlementPost(location))
			m_aPostWidgets[index] = CreateBareWidget(true);
	}

	//------------------------------------------------------------------------------------------------
	// Marker tiers
	//------------------------------------------------------------------------------------------------
	//! Bases, outposts, airfields, heliports, ports, the HQ and strongholds: the places a campaign is fought over
	protected static bool IsKeySite(notnull AROR_LocationComponent location)
	{
		if (location.GetOwner().FindComponent(AROR_StrongholdComponent))
			return true;
		switch (location.GetLocationType())
		{
			case AROR_ELocationType.MILITARY_BASE:
			case AROR_ELocationType.OUTPOST:
			case AROR_ELocationType.AIRPORT:
			case AROR_ELocationType.HELIPORT:
			case AROR_ELocationType.PORT:
			case AROR_ELocationType.HQ:
				return true;
		}
		return false;
	}

	//! Marker size in reference pixels: key sites largest, then industry, cities and towns
	protected float IconSize(AROR_LocationComponent location)
	{
		if (!location || IsKeySite(location))
			return m_fIconSize;
		switch (location.GetLocationType())
		{
			case AROR_ELocationType.FACTORY:
			case AROR_ELocationType.RESOURCES:
				return m_fIconSize * m_fEconomicScale;
			case AROR_ELocationType.CITY:
				return m_fIconSize * m_fCityScale;
			case AROR_ELocationType.TOWN:
				return m_fIconSize * m_fTownScale;
		}
		return m_fIconSize * m_fEconomicScale;
	}

	//------------------------------------------------------------------------------------------------
	//! An icon-layout widget with only its frame canvas showing (plates and post flags draw on it)
	protected Widget CreateBareWidget(bool showFrame)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(m_sIconLayout, m_wIconParent);
		if (!root)
			return null;
		Widget icon = root.FindAnyWidget(ICON_WIDGET_NAME);
		if (icon)
			icon.SetVisible(false);
		Widget frame = root.FindAnyWidget(FRAME_WIDGET_NAME);
		if (frame)
			frame.SetVisible(showFrame);
		Widget label = root.FindAnyWidget(LABEL_WIDGET_NAME);
		if (label)
			label.SetVisible(!showFrame);
		return root;
	}

	//! The key site's name in bold capitals on a dark plate to the right of its marker. The plate is its own widget
	//! below the text (a layout's canvas draws over its label).
	protected void CreateNamePlate(int index, notnull AROR_LocationComponent location)
	{
		string text = location.GetName();
		text.ToUpper();
		if (text.IsEmpty())
			return;
		float height = m_iNameFontSize + 9;
		float width = AROR_MapLabelPlate.Width(text, m_iNameFontSize);

		Widget plate = CreateBareWidget(true);
		Widget caption = CreateBareWidget(false);
		if (!plate || !caption)
			return;
		FrameSlot.SetAlignment(plate, 0, 0.5);
		FrameSlot.SetSize(plate, width, height);
		FrameSlot.SetAlignment(caption, 0, 0.5);
		FrameSlot.SetSize(caption, width, height);
		RichTextWidget label = RichTextWidget.Cast(caption.FindAnyWidget(LABEL_WIDGET_NAME));
		if (label)
		{
			label.SetExactFontSize(m_iNameFontSize);
			label.SetBold(true);
			label.SetColor(Color.FromRGBA(238, 242, 238, 255));
			label.SetText(text);
			FrameSlot.SetPos(label, AROR_MapLabelPlate.TextInset(), m_iNameFontSize * 0.2);
			FrameSlot.SetSize(label, width, height);
		}
		m_aNamePlates[index] = plate;
		m_aNameLabels[index] = caption;
		DrawNamePlate(index, location);
	}

	protected void DrawNamePlate(int index, notnull AROR_LocationComponent location)
	{
		Widget plate = m_aNamePlates[index];
		if (!plate)
			return;
		CanvasWidget canvas = CanvasWidget.Cast(plate.FindAnyWidget(FRAME_WIDGET_NAME));
		if (!canvas)
			return;
		array<ref CanvasWidgetCommand> commands = m_mFrameCommands.Get(canvas);
		if (!commands)
		{
			commands = new array<ref CanvasWidgetCommand>();
			m_mFrameCommands.Set(canvas, commands);
		}
		AROR_MapLabelPlate.Draw(canvas, commands, FrameSlot.GetSizeX(plate), FrameSlot.GetSizeY(plate), FactionColor(location).PackToInt());
	}

	//! The holding faction's colour, or the neutral grey
	protected Color FactionColor(notnull AROR_LocationComponent location)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		Faction faction;
		if (roles)
			faction = roles.GetFaction(location.GetControllingRole());
		if (faction)
			return faction.GetFactionColor();
		return AROR_MapFrameIcon.StrokeColor(null);
	}

	//------------------------------------------------------------------------------------------------
	//! The garrison post's flag, on the flag itself, shown once zoomed in far enough to tell it from the town marker.
	//! Redrawn when the holder or taken state changes.
	protected void UpdatePostMarker(int index, notnull SCR_MapEntity mapEntity, notnull WorkspaceWidget workspace, float iconX, float iconY)
	{
		Widget widget = m_aPostWidgets[index];
		AROR_LocationComponent location = m_aIconLocations[index];
		if (!widget || !location)
			return;
		AROR_GarrisonComponent post = AROR_CaptureLocationAction.GetSettlementPost(location);
		if (!post)
		{
			widget.SetVisible(false);
			return;
		}
		int state = location.GetControllingRole() * 2;
		if (post.IsPostBroken())
			state++;
		if (state != m_aPostStates[index])
		{
			m_aPostStates[index] = state;
			FrameSlot.SetSize(widget, m_fPostMarkerSize, m_fPostMarkerSize);
			FrameSlot.SetAlignment(widget, 0.3, 0.92);
			CanvasWidget canvas = CanvasWidget.Cast(widget.FindAnyWidget(FRAME_WIDGET_NAME));
			if (canvas)
			{
				array<ref CanvasWidgetCommand> commands = m_mFrameCommands.Get(canvas);
				if (!commands)
				{
					commands = new array<ref CanvasWidgetCommand>();
					m_mFrameCommands.Set(canvas, commands);
				}
				AROR_MapFrameIcon.DrawPennant(canvas, commands, workspace.DPIScale(m_fPostMarkerSize), workspace.DPIScale(1), FactionColor(location), post.IsPostBroken());
			}
		}

		// Only once zoomed in far enough that the flag stands clear of the town marker: zoomed out it is clutter
		float size = IconSize(location);
		vector flag = post.GetDefenceCenter();
		int flagX, flagY;
		mapEntity.WorldToScreen(flag[0], flag[2], flagX, flagY, true);
		float screenFlagX = workspace.DPIUnscale(flagX);
		float screenFlagY = workspace.DPIUnscale(flagY);
		float apart = Math.Sqrt((screenFlagX - iconX) * (screenFlagX - iconX) + (screenFlagY - iconY) * (screenFlagY - iconY));
		// Always shown: on the flag once zoomed in enough to tell it apart, otherwise against the town marker's
		// top-right so it never hides under it
		if (apart <= size * 0.5 + m_fPostMarkerSize * 0.5)
		{
			screenFlagX = iconX + size * 0.4;
			screenFlagY = iconY - size * 0.1;
		}
		widget.SetVisible(true);
		widget.SetZOrder(3);
		FrameSlot.SetPos(widget, screenFlagX, screenFlagY);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveIcon(AROR_LocationComponent location)
	{
		int index = m_aIconLocations.Find(location);
		if (index < 0)
			return;

		if (m_Popup && m_Popup.GetLocation() == location)
			m_Popup.Hide();

		RemoveWidget(m_aIconWidgets[index]);
		RemoveWidget(m_aNamePlates[index]);
		RemoveWidget(m_aNameLabels[index]);
		RemoveWidget(m_aPostWidgets[index]);

		m_aIconLocations.Remove(index);
		m_aIconWidgets.Remove(index);
		m_aNamePlates.Remove(index);
		m_aNameLabels.Remove(index);
		m_aPostWidgets.Remove(index);
		m_aPostStates.Remove(index);
	}

	protected void RemoveWidget(Widget widget)
	{
		if (!widget)
			return;
		m_mFrameCommands.Remove(widget.FindAnyWidget(FRAME_WIDGET_NAME));
		widget.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearIcons()
	{
		m_QRFIntel.ClearWidgets();
		m_MissionMarkers.ClearWidgets();
		m_OperationMarkers.ClearWidgets();
		m_PlayerMarkers.ClearWidgets();
		foreach (Widget widget : m_aIconWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}
		foreach (Widget plate : m_aNamePlates)
		{
			if (plate)
				plate.RemoveFromHierarchy();
		}
		foreach (Widget label : m_aNameLabels)
		{
			if (label)
				label.RemoveFromHierarchy();
		}
		foreach (Widget post : m_aPostWidgets)
		{
			if (post)
				post.RemoveFromHierarchy();
		}

		m_aIconLocations.Clear();
		m_aIconWidgets.Clear();
		m_aNamePlates.Clear();
		m_aNameLabels.Clear();
		m_aPostWidgets.Clear();
		m_aPostStates.Clear();
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
		ResourceName iconSet;
		string iconName;
		bool stronghold = location.GetOwner().FindComponent(AROR_StrongholdComponent) != null;
		bool baseIcon = !stronghold && AROR_MapFrameIcon.IconFor(location.GetLocationType(), iconSet, iconName);
		if (stronghold)
			DrawStrongholdFlag(frame, commands, location, color);
		else
			AROR_MapFrameIcon.Draw(frame, commands, workspace.DPIScale(IconSize(location)), workspace.DPIScale(1), location.GetLocationType(), location.GetControllingRole(), color, !baseIcon);

		// Only once the frame has drawn: if drawing fails, the texture icon stays visible
		if (!baseIcon)
		{
			image.SetVisible(false);
			return;
		}
		// The base game's own map icon for the type, tinted for contrast and drawn over the badge
		image.LoadImageFromSet(0, iconSet, iconName);
		image.SetColor(AROR_MapFrameIcon.GlyphColor(AROR_MapFrameIcon.StrokeColor(color)));
		float inset = IconSize(location) * AROR_MapFrameIcon.ICON_INSET;
		FrameSlot.SetOffsets(image, inset, inset, inset, inset);
		frame.SetZOrder(1);
		image.SetZOrder(2);
		image.SetVisible(true);
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
		DrawNamePlate(index, location);
		m_aPostStates[index] = -1;

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

			// The name sits just right of the marker; QRF captions line up under it
			float nameX = x + IconSize(location) * 0.5 + 4;
			if (m_aNamePlates[i])
				FrameSlot.SetPos(m_aNamePlates[i], nameX, y);
			if (m_aNameLabels[i])
				FrameSlot.SetPos(m_aNameLabels[i], nameX, y);
			if (m_aPostWidgets[i])
				UpdatePostMarker(i, mapEntity, workspace, x, y);

			if (m_Popup && m_Popup.GetLocation() == location)
				PositionPopup(workspace, x, y);
		}

		UpdateSelector(mapEntity, workspace);
		m_QRFIntel.Update(m_wIconParent, m_sIconLayout, mapEntity);
		m_MissionMarkers.Update(m_wIconParent, m_sIconLayout, mapEntity);
		m_OperationMarkers.Update(m_wIconParent, m_sIconLayout, mapEntity);
		m_PlayerMarkers.Update(m_wIconParent, m_sIconLayout, mapEntity);
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
		m_Selector.Show(workspace.DPIUnscale(screenX), workspace.DPIUnscale(screenY), IconSize(target) + m_fSelectorPadding, opacity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;
		AROR_QRFMapIntel.Clear();

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
		m_Popup = null;
		m_Selector = null;
		ClearIcons();

		super.OnDelete(owner);
	}
}
