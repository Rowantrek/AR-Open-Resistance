//! FIELD: personal recruits and player annotations. All mutations travel through the owning controller.
class AROR_FieldApp : AROR_PhonePooledApp
{
	protected static const ResourceName LAYOUT = "{7D0A31C4B95E6201}UI/Layouts/Phone/AROR_Field.layout";
	protected Widget m_wResource;
	protected EditBoxWidget m_wCaption;
	protected Widget m_wTile;
	protected RichTextWidget m_wTileGlyph;
	protected RichTextWidget m_wTileLabel;
	protected ref AROR_PhoneButtonHandler m_TileHandler;
	protected int m_iTab;
	protected int m_iPage;
	protected int m_iMarkerId;
	protected int m_iRecruitId;
	protected int m_iScope;
	protected int m_iKind;
	protected int m_iExpiry;
	protected int m_iTransferPlayer;
	protected int m_iConfirmId;
	protected int m_iConfirmUntil;
	protected static const int PAGE_SIZE = 8;

	bool Init(Widget root, AROR_PhoneMenu menu)
	{
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT, root);
		if (!m_wResource || !InitPools(m_wResource, "Field", 40, 64, 32))
			return false;
		m_wCaption = EditBoxWidget.Cast(m_wResource.FindAnyWidget("FieldCaption"));
		m_wTile = m_wResource.FindAnyWidget("FieldTileButton");
		m_wTileGlyph = RichTextWidget.Cast(m_wResource.FindAnyWidget("FieldTileGlyph"));
		m_wTileLabel = RichTextWidget.Cast(m_wResource.FindAnyWidget("FieldTileLabel"));
		if (!m_wCaption || !m_wTile || !m_wTileGlyph || !m_wTileLabel)
			return false;
		m_wCaption.SetText("Note");
		m_wCaption.SetTextColor(AROR_UIColors.TEXT);
		m_TileHandler = new AROR_PhoneButtonHandler(menu);
		m_wTile.AddHandler(m_TileHandler);
		m_wTile.SetColor(Color.FromRGBA(0, 0, 0, 0));
		m_wTileGlyph.SetText("F");
		m_wTileGlyph.SetColor(C(AROR_UIColors.REBEL));
		m_wTileLabel.SetText("FIELD");
		m_wTileLabel.SetColor(C(AROR_UIColors.TEXT));
		m_wBackground.SetColor(C(AROR_UIColors.SURFACE));
		SetVisible(false, 1);
		SetTileVisible(false, 1);
		return true;
	}

	void Shutdown()
	{
		if (m_wTile && m_TileHandler)
			m_wTile.RemoveHandler(m_TileHandler);
		m_TileHandler = null;
		if (m_wResource) m_wResource.RemoveFromHierarchy();
		m_wResource = null;
	}

	void SetTileVisible(bool visible, float opacity)
	{
		array<Widget> widgets = {m_wTile, m_wTileGlyph, m_wTileLabel};
		foreach (Widget widget : widgets)
		{
			if (!widget) continue;
			widget.SetVisible(visible);
			widget.SetOpacity(opacity);
		}
	}

	void LayoutTile(float left, float top, float scale)
	{
		PlaceRaw(m_wTile, left, top, 180 * scale, 240 * scale);
		PlaceRaw(m_wTileGlyph, left + 62 * scale, top + 28 * scale, 100 * scale, 130 * scale);
		m_wTileGlyph.SetExactFontSize(90 * scale);
		PlaceRaw(m_wTileLabel, left + 40 * scale, top + 195 * scale, 170 * scale, 45 * scale);
		m_wTileLabel.SetExactFontSize(32 * scale);
	}

	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		Text(40, 24, 1100, 55, 40, "FIELD  //  YOUR RESISTANCE", C(AROR_UIColors.TEXT), true);
		Button(40, 100, 350, 60, "MAP MARKERS", "markers", C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.TEXT), 24);
		Button(410, 100, 350, 60, "PERSONAL SQUAD", "squad", C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.TEXT), 24);
		m_wCaption.SetVisible(m_iTab == 0);
		if (m_iTab == 0)
			DrawMarkers();
		else
			DrawSquad();
		EndFrame();
	}

	protected void DrawMarkers()
	{
		int count = AROR_FieldClientState.s_aMarkers.Count();
		ClampPage(count);
		Text(40, 185, 870, 45, 23, "Select a marker to edit. Shared markers from others are read-only.", C(AROR_UIColors.TEXT_MUTED));
		for (int row = 0; row < PAGE_SIZE; row++)
		{
			int index = m_iPage * PAGE_SIZE + row;
			if (index >= count) break;
			AROR_FieldMarkerInfo marker = AROR_FieldClientState.s_aMarkers[index];
			string caption = marker.m_sCaption + "  [" + AROR_FieldClientState.ScopeName(marker.m_iScope) + "]";
			Color fill = C(AROR_UIColors.SURFACE_RAISED);
			if (marker.m_iId == m_iMarkerId) fill = C(AROR_UIColors.REBEL_DIM);
			Button(40, 245 + row * 62, 870, 54, caption, "marker:" + marker.m_iId, fill, C(AROR_UIColors.TEXT), 24, false);
		}
		Pager(count);
		Text(970, 185, 660, 42, 24, "CAPTION", C(AROR_UIColors.TEXT), true);
		Panel(970, 230, 650, 64, C(AROR_UIColors.SURFACE_RAISED));
		PlaceRaw(m_wCaption, 970 * m_fScale + m_fInsetX, 230 * m_fScale + m_fInsetY, 650 * m_fScale, 64 * m_fScale);
		Button(970, 320, 650, 56, "SHARE: " + AROR_FieldClientState.ScopeName(m_iScope), "scope", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(970, 390, 650, 56, "TYPE: " + AROR_FieldClientState.KindName(m_iKind), "kind", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		string expiry = "PERSISTENT";
		if (m_iExpiry == 1) expiry = "10 MINUTES";
		if (m_iExpiry == 2) expiry = "30 MINUTES";
		if (m_iExpiry == 3) expiry = "60 MINUTES";
		Button(970, 460, 650, 56, "EXPIRY: " + expiry, "expiry", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(970, 540, 310, 58, "NEW HERE", "here", C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.TEXT), 24);
		Button(1300, 540, 320, 58, "NEW ON MAP", "pick", C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.TEXT), 24);
		AROR_FieldMarkerInfo selected = SelectedMarker();
		string save, move, remove;
		if (selected && selected.m_bEditable)
		{
			save = "save";
			move = "move";
			remove = "delete";
		}
		Button(970, 620, 310, 58, "UPDATE SELECTED", save, C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 22);
		Button(1300, 620, 320, 58, "MOVE ON MAP", move, C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 22);
		string deleteLabel = "DELETE SELECTED";
		if (m_iConfirmId == m_iMarkerId && GetGame().GetWorld().GetWorldTime() < m_iConfirmUntil) deleteLabel = "CONFIRM DELETE";
		Button(970, 700, 650, 58, deleteLabel, remove, C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.DANGER), 24);
		Text(970, 782, 650, 85, 22, "Private and Resistance markers are saved. Squad markers last this session. New route points are numbered by creation order.", C(AROR_UIColors.TEXT_MUTED));
	}

	protected void DrawSquad()
	{
		AROR_RecruitmentComponent recruitment = AROR_RecruitmentComponent.GetInstance();
		if (!recruitment)
		{
			Text(40, 230, 1400, 80, 28, "Recruitment is unavailable in this scenario.", C(AROR_UIColors.WARN));
			return;
		}
		int count = AROR_FieldClientState.s_aRecruits.Count();
		ClampPage(count);
		Text(40, 185, 900, 45, 23, string.Format("%1 / %2 RECRUITS  //  %3", count, recruitment.GetPlayerLimit(), SelectionLabel()), C(AROR_UIColors.TEXT_MUTED));
		for (int row = 0; row < PAGE_SIZE; row++)
		{
			int index = m_iPage * PAGE_SIZE + row;
			if (index >= count) break;
			AROR_FieldRecruitInfo recruit = AROR_FieldClientState.s_aRecruits[index];
			string status = AROR_FieldClientState.OrderName(recruit.m_iOrder);
			if (!recruit.m_bFormed) status = "FORMING";
			string label = string.Format("RECRUIT %1  //  %2  //  GRID %3", recruit.m_iId, status, AROR_HighCommandComponent.GridReference(recruit.m_vPosition));
			Color fill = C(AROR_UIColors.SURFACE_RAISED);
			if (m_iRecruitId == 0 || m_iRecruitId == recruit.m_iId) fill = C(AROR_UIColors.REBEL_DIM);
			Button(40, 245 + row * 62, 870, 54, label, "recruit:" + recruit.m_iId, fill, C(AROR_UIColors.TEXT), 22, false);
		}
		Pager(count);
		Button(970, 185, 650, 58, string.Format("RECRUIT AT HQ  //  $%1 + %2 MP", recruitment.GetFundsCost(), recruitment.GetManpowerCost()), "raise", C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.TEXT), 24);
		Button(970, 265, 650, 56, "SELECT ALL", "all", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(970, 340, 200, 58, "FOLLOW", "order:1", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(1190, 340, 200, 58, "HOLD", "order:2", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(1410, 340, 210, 58, "REGROUP", "order:3", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(970, 420, 200, 58, "PASSENGER", "order:4", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 21);
		Button(1190, 420, 200, 58, "DRIVER", "order:5", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(1410, 420, 210, 58, "GUNNER", "order:6", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		Button(970, 500, 650, 58, "DISMOUNT  //  STOP VEHICLE FIRST", "order:7", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 24);
		string target = "CHOOSE PLAYER";
		if (m_iTransferPlayer > 0) target = GetGame().GetPlayerManager().GetPlayerName(m_iTransferPlayer);
		Button(970, 580, 420, 58, target, "target", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 23);
		Button(1410, 580, 210, 58, "TRANSFER", "order:8", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 22);
		string label = "DISMISS AT HQ  //  REFUND";
		if (m_iConfirmId == -1 && GetGame().GetWorld().GetWorldTime() < m_iConfirmUntil) label = "CONFIRM DISMISS";
		Button(970, 660, 650, 58, label, "dismiss", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.WARN), 24);
		Text(970, 750, 650, 130, 22, "Board your current vehicle or look at one before opening the phone. Assign driver/gunner individually. Transfers need players within 30 m and recruits within 50 m. Living recruits return their value on save/load or disconnect.", C(AROR_UIColors.TEXT_MUTED));
	}

	protected void ClampPage(int count)
	{
		m_iPage = Math.Clamp(m_iPage, 0, Math.Max(0, (count - 1) / PAGE_SIZE));
	}

	protected void Pager(int count)
	{
		Button(40, 780, 160, 58, "PREVIOUS", "prev", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 22);
		Button(220, 780, 160, 58, "NEXT", "next", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 22);
		Text(420, 792, 450, 42, 24, string.Format("PAGE %1  //  %2 TOTAL", m_iPage + 1, count), C(AROR_UIColors.TEXT_MUTED));
	}

	protected string SelectionLabel()
	{
		if (m_iRecruitId == 0) return "ALL SELECTED";
		return "SELECTED: " + m_iRecruitId;
	}

	protected AROR_FieldMarkerInfo SelectedMarker()
	{
		foreach (AROR_FieldMarkerInfo marker : AROR_FieldClientState.s_aMarkers)
		{
			if (marker.m_iId == m_iMarkerId) return marker;
		}
		return null;
	}

	protected int Lifetime()
	{
		if (m_iExpiry == 1) return 600;
		if (m_iExpiry == 2) return 1800;
		if (m_iExpiry == 3) return 3600;
		return 0;
	}

	protected void CycleTarget()
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		int current = ids.Find(m_iTransferPlayer);
		int self = GetGame().GetPlayerController().GetPlayerId();
		for (int i = 1; i <= ids.Count(); i++)
		{
			int candidate = ids[(current + i) % ids.Count()];
			if (candidate != self)
			{
				m_iTransferPlayer = candidate;
				return;
			}
		}
		m_iTransferPlayer = 0;
	}

	protected RplId VehicleTarget()
	{
		PlayerController controller = GetGame().GetPlayerController();
		IEntity player = controller.GetControlledEntity();
		if (!player) return RplId.Invalid();
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(player.FindComponent(CompartmentAccessComponent));
		if (access && access.GetCompartment())
			return SCR_EntityHelper.EntityToRplId(access.GetCompartment().GetVehicle());
		CameraBase camera = GetGame().GetCameraManager().CurrentCamera();
		if (!camera) return RplId.Invalid();
		vector hitPosition;
		IEntity entity = camera.GetCursorTargetWithPosition(hitPosition);
		for (int depth = 0; entity && depth < 16; depth++)
		{
			if (AROR_GarageSnapshotHelper.IsVehicle(entity))
				return SCR_EntityHelper.EntityToRplId(entity);
			entity = entity.GetParent();
		}
		return RplId.Invalid();
	}

	override protected void DoAction(string action)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller) return;
		if (action == "markers" || action == "squad")
		{
			m_iTab = 0;
			if (action == "squad") m_iTab = 1;
			m_iPage = 0;
			m_iConfirmId = 0;
		}
		else if (action == "prev") m_iPage = Math.Max(0, m_iPage - 1);
		else if (action == "next") m_iPage++;
		else if (action == "scope") m_iScope = (m_iScope + 1) % 3;
		else if (action == "kind") m_iKind = (m_iKind + 1) % 5;
		else if (action == "expiry") m_iExpiry = (m_iExpiry + 1) % 4;
		else if (action.IndexOf("marker:") == 0)
		{
			m_iMarkerId = action.Substring(7, action.Length() - 7).ToInt();
			m_iConfirmId = 0;
			AROR_FieldMarkerInfo selected = SelectedMarker();
			if (selected)
			{
				m_wCaption.SetText(selected.m_sCaption);
				m_iScope = selected.m_iScope;
				m_iKind = selected.m_iKind;
			}
		}
		else if (action == "pick" || action == "move")
		{
			int id;
			if (action == "move") id = m_iMarkerId;
			AROR_FieldClientState.PickMarker(id, m_wCaption.GetText(), m_iScope, m_iKind, Lifetime());
		}
		else if (action == "here" || action == "save")
		{
			IEntity player = controller.GetControlledEntity();
			if (!player) return;
			vector position = player.GetOrigin();
			int markerId;
			if (action == "save")
			{
				AROR_FieldMarkerInfo marker = SelectedMarker();
				if (!marker || !marker.m_bEditable) return;
				markerId = marker.m_iId;
				position = marker.m_vPosition;
			}
			controller.AROR_RequestFieldMarker(markerId, position, m_wCaption.GetText(), m_iScope, m_iKind, Lifetime());
		}
		else if (action == "delete")
		{
			if (Confirm(m_iMarkerId))
			{
				controller.AROR_RequestFieldMarker(-m_iMarkerId, vector.Zero, "", 0, 0, 0);
				m_iMarkerId = 0;
			}
		}
		else if (action == "all") m_iRecruitId = 0;
		else if (action.IndexOf("recruit:") == 0) m_iRecruitId = action.Substring(8, action.Length() - 8).ToInt();
		else if (action == "target") CycleTarget();
		else if (action == "raise") controller.AROR_RequestRecruit(AROR_ERecruitAction.RECRUIT, 0, 0, RplId.Invalid());
		else if (action == "dismiss")
		{
			if (Confirm(-1)) controller.AROR_RequestRecruit(AROR_ERecruitAction.DISMISS, m_iRecruitId, 0, RplId.Invalid());
		}
		else if (action.IndexOf("order:") == 0)
		{
			int order = action.Substring(6, action.Length() - 6).ToInt();
			RplId vehicle = RplId.Invalid();
			if (order >= AROR_ERecruitAction.BOARD_PASSENGER && order <= AROR_ERecruitAction.BOARD_GUNNER)
				vehicle = VehicleTarget();
			controller.AROR_RequestRecruit(order, m_iRecruitId, m_iTransferPlayer, vehicle);
		}
	}

	protected bool Confirm(int id)
	{
		int now = GetGame().GetWorld().GetWorldTime();
		if (m_iConfirmId == id && now < m_iConfirmUntil)
		{
			m_iConfirmId = 0;
			return true;
		}
		m_iConfirmId = id;
		m_iConfirmUntil = now + 5000;
		return false;
	}

	protected static Color C(int token) { return AROR_UIColors.Get(token); }
}
