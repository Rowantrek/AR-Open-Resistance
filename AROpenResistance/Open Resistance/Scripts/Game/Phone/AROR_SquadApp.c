//! The Squad app uses its own copy of the existing widget pool; no new resource GUIDs are required.
class AROR_SquadApp : AROR_PhonePooledApp
{
	protected static const ResourceName LAYOUT = "{6AB5C0D7B1A4C3E2}UI/Layouts/Phone/AROR_BlackMarket.layout";
	protected static const int PAGE_SIZE = 5;
	protected Widget m_wResource;
	protected EditBoxWidget m_wName;
	protected int m_iGroupId = -1;
	protected int m_iPage;
	protected int m_iConfirmDismiss = -1;
	protected bool m_bShown;
	protected ref array<AROR_SquadMemberInfo> m_aMembers = {};

	protected static Color C(int token)
	{
		return AROR_UIColors.Get(token);
	}

	bool Init(Widget parent)
	{
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT, parent);
		if (!m_wResource || !InitPools(m_wResource, "BM", 96, 120, 64, 16, 16))
			return false;
		m_wName = EditBoxWidget.Cast(m_wResource.FindAnyWidget("BMSearch"));
		if (!m_wName)
			return false;
		m_wName.SetPlaceholderText("Squad name");
		m_wName.SetColor(C(AROR_UIColors.TEXT));
		m_wBackground.SetColor(C(AROR_UIColors.SURFACE));
		SetVisible(false, 1);
		return true;
	}

	Widget GetResource()
	{
		return m_wResource;
	}

	void Shutdown()
	{
		if (m_wResource)
			m_wResource.RemoveFromHierarchy();
		m_wResource = null;
	}

	override void SetVisible(bool visible, float opacity)
	{
		super.SetVisible(visible, opacity);
		if (visible && !m_bShown)
		{
			m_iGroupId = -1;
			m_iPage = 0;
			m_iConfirmDismiss = -1;
		}
		m_bShown = visible;
		m_wName.SetVisible(visible && m_iGroupId >= 0);
	}

	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		Text(40, 26, 1200, 60, 38, "SQUAD MANAGER", C(AROR_UIColors.TEXT), true);
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		AROR_SquadManagerComponent manager = AROR_SquadManagerComponent.GetInstance();
		SCR_AIGroup group;
		if (player && groups)
			group = groups.GetPlayerGroup(player.GetPlayerId());
		m_wName.SetVisible(group && manager);
		if (!group || !manager)
		{
			m_iGroupId = -1;
			Text(40, 180, 1500, 120, 28, "Recruit your first rifleman at a Resistance-owned flag to start your squad.", C(AROR_UIColors.TEXT));
			DrawInstructions();
			EndFrame();
			return;
		}
		if (m_iGroupId != group.GetGroupID())
		{
			m_iGroupId = group.GetGroupID();
			m_wName.SetText(group.GetCustomName());
			m_iConfirmDismiss = -1;
			m_iPage = 0;
		}
		bool leader = group.IsPlayerLeader(player.GetPlayerId());
		if (!leader && m_wName.GetText() != group.GetCustomName())
			m_wName.SetText(group.GetCustomName());
		m_wName.SetEnabled(leader);
		Panel(40, 116, 1050, 64, C(AROR_UIColors.SURFACE_RAISED));
		PlaceRaw(m_wName, 56 * m_fScale, 124 * m_fScale, 1018 * m_fScale, 48 * m_fScale);
		string reason;
		string renameAction;
		if (leader && AROR_ResistanceName.Validate(m_wName.GetText(), reason))
			renameAction = "rename";
		Button(1120, 116, 300, 64, "RENAME SQUAD", renameAction, C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.REBEL), 23);
		Text(40, 192, 1500, 34, 20, "Up to 32 letters, digits and single spaces. Only the squad leader can rename or dismiss.", C(AROR_UIColors.TEXT_MUTED));

		m_aMembers.Clear();
		foreach (AROR_SquadMemberInfo info : manager.GetRoster())
		{
			if (info.m_iGroupId == m_iGroupId)
				m_aMembers.Insert(info);
		}
		Text(40, 246, 1400, 38, 26, string.Format("FLAG RECRUITS  %1 / %2", m_aMembers.Count(), manager.GetLimit()), C(AROR_UIColors.REBEL), true);
		int maxPage = Math.Max(0, Math.Ceil(m_aMembers.Count() / (PAGE_SIZE * 1.0)) - 1);
		m_iPage = Math.Clamp(m_iPage, 0, maxPage);
		for (int row = 0; row < PAGE_SIZE; row++)
		{
			int index = m_iPage * PAGE_SIZE + row;
			if (index >= m_aMembers.Count())
				break;
			AROR_SquadMemberInfo member = m_aMembers[index];
			float y = 300 + row * 76;
			Panel(40, y, 1620, 66, C(AROR_UIColors.SURFACE_RAISED));
			Text(60, y + 17, 690, 40, 25, "Rifleman " + member.m_iId, C(AROR_UIColors.TEXT), true);
			Text(770, y + 20, 400, 32, 22, member.m_sState, C(AROR_UIColors.TEXT_MUTED));
			string action;
			if (leader)
				action = "dismiss:" + member.m_iId;
			string label = "DISMISS";
			if (m_iConfirmDismiss == member.m_iId)
				label = "CONFIRM DISMISS";
			Button(1260, y + 7, 380, 52, label, action, C(AROR_UIColors.PLATE_STRONG), C(AROR_UIColors.WARN), 22);
		}
		if (m_aMembers.IsEmpty())
			Text(60, 340, 1500, 60, 26, "No riflemen recruited. Walk up to an owned flag and choose Recruit rifleman.", C(AROR_UIColors.TEXT_MUTED));
		if (maxPage > 0)
		{
			Button(40, 700, 180, 48, "PREV", "prev", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 20);
			Text(250, 710, 250, 32, 20, string.Format("%1 / %2", m_iPage + 1, maxPage + 1), C(AROR_UIColors.TEXT_MUTED));
			Button(520, 700, 180, 48, "NEXT", "next", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 20);
		}
		DrawInstructions();
		EndFrame();
	}

	protected void DrawInstructions()
	{
		Panel(40, 770, 1620, 1, C(AROR_UIColors.LINE_STRONG));
		Text(40, 790, 1600, 76, 23, "ORDERS: Close the phone and use Reforger's squad command wheel. Aim at the ground for movement orders or a vehicle for Get in / Get out. Recruitment is free for now; riflemen last for this session.", C(AROR_UIColors.TEXT_MUTED));
	}

	override protected void DoAction(string action)
	{
		if (!m_bShown)
			return;
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player)
			return;
		if (action == "rename")
			player.AROR_RequestSquadRename(m_iGroupId, m_wName.GetText());
		else if (action == "prev")
			m_iPage = Math.Max(m_iPage - 1, 0);
		else if (action == "next")
			m_iPage++;
		else if (action.IndexOf("dismiss:") == 0)
		{
			int id = action.Substring(8, action.Length() - 8).ToInt();
			if (m_iConfirmDismiss == id)
			{
				player.AROR_RequestSquadDismiss(m_iGroupId, id);
				m_iConfirmDismiss = -1;
			}
			else
				m_iConfirmDismiss = id;
		}
	}
}
