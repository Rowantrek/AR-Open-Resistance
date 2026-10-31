//! MARS owns its layout and preview. The authoritative server resolves the same two-axis policy again.
class AROR_MarsApp : AROR_PhonePooledApp
{
	protected static const ResourceName LAYOUT = "{F35D38C65B724D61}UI/Layouts/Phone/AROR_Mars.layout";
	protected Widget m_wResource;
	protected EditBoxWidget m_wCredential;
	protected SliderWidget m_wReadiness;
	protected SliderWidget m_wCrackdown;
	protected CanvasWidget m_wBackdrop;
	protected ref AROR_MarsBackdrop m_Backdrop = new AROR_MarsBackdrop();
	protected ref AROR_ResponseSelection m_Preview;
	protected ref array<ref AROR_ResponseSelection> m_aTypePreviews = {};
	protected ref array<int> m_aTypes = {};
	protected string m_sCredential;
	protected string m_sMessage;
	protected bool m_bUnlocked;
	protected bool m_bPreviewDirty = true;
	protected AROR_EFactionRole m_eRole = AROR_EFactionRole.OCCUPIERS;
	protected int m_iType = AROR_EResponseType.COMBINED_ARMS;
	protected int m_iReadiness = 1;
	protected int m_iCrackdown = 1;
	protected int m_iTypePage;

	bool Init(Widget phoneRoot)
	{
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT, phoneRoot);
		if (!m_wResource || !InitPools(m_wResource, "Mars", 12, 40, 40))
			return false;
		m_wCredential = EditBoxWidget.Cast(m_wResource.FindAnyWidget("MarsCredential"));
		m_wReadiness = SliderWidget.Cast(m_wResource.FindAnyWidget("MarsReadinessSlider"));
		m_wCrackdown = SliderWidget.Cast(m_wResource.FindAnyWidget("MarsCrackdownSlider"));
		m_wBackdrop = CanvasWidget.Cast(m_wResource.FindAnyWidget("MarsBackdrop"));
		if (!m_wCredential || !m_wReadiness || !m_wCrackdown || !m_wBackdrop)
			return false;
		// Native UI API: EnfusionScriptAPIPublic/interfaceEditBoxWidget.html and interfaceSliderWidget.html.
		m_wCredential.SetObfuscationChar("*");
		m_wCredential.SetPlaceholderText("Access password");
		m_wCredential.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT));
		m_wBackground.SetColor(AROR_UIColors.Get(AROR_UIColors.MARS_SURFACE));
		m_wBackdrop.SetZOrder(1);
		m_wBackground.SetZOrder(0);
		foreach (ImageWidget panel : m_aPanels)
			panel.SetZOrder(2);
		foreach (RichTextWidget text : m_aTexts)
			text.SetZOrder(4);
		foreach (ImageWidget buttonBg : m_aButtonBackgrounds)
			buttonBg.SetZOrder(3);
		foreach (RichTextWidget buttonLabel : m_aButtonLabels)
			buttonLabel.SetZOrder(4);
		foreach (Widget button : m_aButtons)
			button.SetZOrder(5);
		m_wCredential.SetZOrder(6);
		array<SliderWidget> sliders = {m_wReadiness, m_wCrackdown};
		foreach (SliderWidget slider : sliders)
		{
			slider.SetDefaultStyle();
			slider.SetRange(1, 5);
			slider.SetStep(1);
			slider.SetCurrent(1);
			slider.SetColor(AROR_UIColors.Get(AROR_UIColors.MARS_ACCENT));
			slider.SetZOrder(6);
		}
		AROR_ResponsePolicy.GetTypes(m_aTypes);
		UseCampaignLevels();
		SetVisible(false, 1);
		return true;
	}

	void Shutdown()
	{
		m_sCredential = string.Empty;
		m_bUnlocked = false;
		if (m_wCredential)
			m_wCredential.SetText(string.Empty);
		if (m_wResource)
			m_wResource.RemoveFromHierarchy();
		m_wResource = null;
	}

	override void SetVisible(bool visible, float opacity)
	{
		super.SetVisible(visible, opacity);
		if (m_wResource)
			m_wResource.SetVisible(visible);
	}

	protected AROR_OperationExecutorComponent GetExecutor()
	{
		IEntity mode = GetGame().GetGameMode();
		if (!mode)
			return null;
		return AROR_OperationExecutorComponent.Cast(mode.FindComponent(AROR_OperationExecutorComponent));
	}

	protected AROR_OperationManagerComponent GetOperations()
	{
		IEntity mode = GetGame().GetGameMode();
		if (!mode)
			return null;
		return AROR_OperationManagerComponent.Cast(mode.FindComponent(AROR_OperationManagerComponent));
	}

	protected void UseCampaignLevels()
	{
		AROR_OperationManagerComponent operations = GetOperations();
		if (operations && operations.HasThreatState())
		{
			m_iReadiness = operations.GetReadinessLevel(m_eRole);
			m_iCrackdown = operations.GetCrackdownLevel(m_eRole);
		}
		m_wReadiness.SetCurrent(m_iReadiness);
		m_wCrackdown.SetCurrent(m_iCrackdown);
		m_bPreviewDirty = true;
	}

	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		Color text = AROR_UIColors.Get(AROR_UIColors.TEXT);
		Color muted = AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED);
		Color accent = AROR_UIColors.Get(AROR_UIColors.MARS_ACCENT);
		PlaceRaw(m_wBackdrop, 0, 0, WIDTH * scale, HEIGHT * scale);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_Backdrop.Draw(m_wBackdrop, workspace.DPIScale(WIDTH * scale), workspace.DPIScale(HEIGHT * scale), workspace.DPIScale(scale), m_bUnlocked);
		m_wCredential.SetVisible(!m_bUnlocked);
		m_wReadiness.SetVisible(m_bUnlocked);
		m_wCrackdown.SetVisible(m_bUnlocked);
		Text(32, 24, 800, 45, 32, "MARS", accent, true);
		Text(34, 74, 900, 28, 16, "MILITARY RESPONSE SIMULATOR / FIELD TEST CONSOLE", muted);
		if (!m_bUnlocked)
		{
			TextCentre(391, 426, 64, "MARS", accent, true);
			Text(760, 242, 800, 45, 26, "RESTRICTED UPLINK", text, true);
			Text(760, 292, 800, 32, 18, "Enter your access password", muted);
			Panel(758, 340, 804, 68, AROR_UIColors.Get(AROR_UIColors.MARS_LINE));
			PlaceRaw(m_wCredential, 770 * scale, 350 * scale, 780 * scale, 48 * scale);
			Button(760, 448, 800, 64, "ESTABLISH UPLINK", "unlock", accent, AROR_UIColors.Get(AROR_UIColors.MARS_INK), 22);
			Text(760, 540, 800, 70, 18, m_sMessage, accent);
			EndFrame();
			return;
		}
		int readiness = Math.Clamp(Math.Round(m_wReadiness.GetCurrent()), 1, 5);
		int crackdown = Math.Clamp(Math.Round(m_wCrackdown.GetCurrent()), 1, 5);
		if (readiness != m_iReadiness || crackdown != m_iCrackdown)
		{
			m_iReadiness = readiness;
			m_iCrackdown = crackdown;
			m_bPreviewDirty = true;
		}
		DrawControl(1028, 28, 220, 62, "US / OCCUPIERS", "us", m_eRole == AROR_EFactionRole.OCCUPIERS);
		DrawControl(1260, 28, 220, 62, "USSR / INVADERS", "ussr", m_eRole == AROR_EFactionRole.INVADERS);
		DrawControl(1492, 28, 168, 62, "LOCK", "lock", false);
		AROR_OperationExecutorComponent executor = GetExecutor();
		if (executor && m_bPreviewDirty)
		{
			m_aTypePreviews.Clear();
			m_Preview = null;
			foreach (int type : m_aTypes)
			{
				AROR_ResponseSelection choice = executor.GetResponsePreview(m_eRole, type, m_iReadiness, m_iCrackdown);
				m_aTypePreviews.Insert(choice);
				if (type == m_iType)
					m_Preview = choice;
			}
			m_bPreviewDirty = false;
		}
		AROR_LocationComponent target;
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller && controller.GetControlledEntity())
			target = AROR_MarsTargets.NearestRebelTerritory(AROR_LocationManagerComponent.GetInstance(), controller.GetControlledEntity().GetOrigin());
		string destination = "No Rebel-owned territory available";
		if (target)
			destination = "OBJECTIVE / " + target.GetName();
		Text(32, 130, 1600, 36, 22, destination, text, true);
		Text(32, 180, 950, 30, 18, "01 / SELECT RESPONSE TYPE", accent, true);
		for (int slot = 0; slot < 6; slot++)
		{
			int index = m_iTypePage * 6 + slot;
			if (!m_aTypes.IsIndexValid(index))
				break;
			int row = slot / 3;
			int column = slot - row * 3;
			DrawControl(32 + column * 328, 226 + row * 140, 312, 118, AROR_ResponsePolicy.TypeName(m_aTypes[index]), "type:" + m_aTypes[index], m_iType == m_aTypes[index], 21);
			string availability = "LOCKED / UNAVAILABLE";
			if (m_aTypePreviews.IsIndexValid(index) && m_aTypePreviews[index].IsAvailable())
				availability = string.Format("%1 ELEMENTS / %2 VEHICLES", m_aTypePreviews[index].m_aElements.Count(), m_aTypePreviews[index].m_iVehicles);
			Color caption = muted;
			if (m_iType == m_aTypes[index])
				caption = AROR_UIColors.Get(AROR_UIColors.MARS_INK);
			TextCentre(188 + column * 328, 312 + row * 140, 14, availability, caption);
		}
		if (m_aTypes.Count() > 6)
		{
			DrawControl(32, 490, 150, 32, "PREVIOUS", "previous", false, 15);
			DrawControl(850, 490, 150, 32, "NEXT", "next", false, 15);
		}
		DrawMetrics(scale);
		Panel(32, 536, 968, 268, AROR_UIColors.Get(AROR_UIColors.MARS_PANEL));
		Text(56, 557, 900, 30, 18, "03 / DISPATCH PREVIEW", accent, true);
		string launchAction;
		if (m_Preview && m_Preview.IsAvailable())
		{
			Text(56, 602, 900, 40, 25, AROR_ResponsePolicy.TypeName(m_iType), text, true);
			string summary = string.Format("%1 elements / %2 vehicles / %3 groups", m_Preview.m_aElements.Count(), m_Preview.m_iVehicles, m_Preview.m_iGroups);
			Text(56, 649, 900, 30, 21, summary, text);
			Text(56, 690, 900, 36, 16, m_Preview.GetComposition(), text);
			Text(56, 735, 900, 26, 15, string.Format("Personnel budget %1 / equipment R%2 / requested intensity C%3 / resolved tier %4", m_Preview.m_iPersonnel, m_iReadiness, m_iCrackdown, m_Preview.m_iTier), muted);
			string roster = m_Preview.m_sRecipeId;
			if (roster.IsEmpty())
				roster = m_Preview.m_sProfileId;
			Text(56, 770, 900, 24, 14, "CATALOGUE / " + roster, muted);
			if (target)
				launchAction = "launch";
		}
		else
		{
			string reason = "No configured executor available";
			if (m_Preview)
				reason = m_Preview.m_sReason;
			Text(56, 611, 900, 100, 23, reason, text);
			Text(56, 728, 900, 50, 17, "Increase Readiness to unlock equipment, or select another response type.", muted);
		}
#ifdef WORKBENCH
		// Workbench only: the same response as a territory attack (staging, then assault), without the campaign timers
		string attackAction;
		if (!launchAction.IsEmpty())
			attackAction = "attack";
		Button(1040, 650, 305, 78, "SUMMON RESPONSE", launchAction, accent, AROR_UIColors.Get(AROR_UIColors.MARS_INK), 21);
		Button(1355, 650, 305, 78, "LAUNCH AS ATTACK", attackAction, accent, AROR_UIColors.Get(AROR_UIColors.MARS_INK), 21);
#else
		Button(1040, 650, 620, 78, "SUMMON RESPONSE", launchAction, accent, AROR_UIColors.Get(AROR_UIColors.MARS_INK), 25);
#endif
		DrawControl(1040, 748, 620, 52, "USE CAMPAIGN LEVELS", "campaign", false);
		Text(32, 815, 1620, 24, 15, m_sMessage, text);
		AROR_OperationManagerComponent operations = GetOperations();
		if (operations)
			Text(32, 849, 1620, 44, 14, operations.GetMarsStatus(), muted);
		EndFrame();
	}

	protected void DrawMetrics(float scale)
	{
		Color text = AROR_UIColors.Get(AROR_UIColors.TEXT);
		Color muted = AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED);
		Color accent = AROR_UIColors.Get(AROR_UIColors.MARS_ACCENT);
		Panel(1040, 180, 620, 438, AROR_UIColors.Get(AROR_UIColors.MARS_PANEL));
		Text(1064, 200, 570, 30, 18, "02 / RESPONSE METRICS", accent, true);
		Text(1064, 252, 500, 35, 23, "READINESS / " + m_iReadiness, text, true);
		Text(1064, 292, 570, 30, 16, "Equipment available to this response", muted);
		PlaceRaw(m_wReadiness, 1064 * scale, 334 * scale, 572 * scale, 48 * scale);
		Text(1064, 413, 500, 35, 23, "CRACKDOWN / " + m_iCrackdown, text, true);
		Text(1064, 453, 570, 30, 16, "Force size and authored response tier", muted);
		PlaceRaw(m_wCrackdown, 1064 * scale, 495 * scale, 572 * scale, 48 * scale);
		for (int level = 1; level <= 5; level++)
		{
			float x = 1064 + (level - 1) * 139;
			Text(x, 382, 40, 25, 16, level.ToString(), muted);
			Text(x, 543, 40, 25, 16, level.ToString(), muted);
		}
		Text(1064, 580, 570, 26, 15, "TEST VALUES / campaign metrics stay unchanged", muted);
	}

	protected void DrawControl(float x, float y, float width, float height, string label, string action, bool selected, float size = 18)
	{
		int fill = AROR_UIColors.MARS_PANEL;
		int ink = AROR_UIColors.TEXT;
		if (selected)
		{
			fill = AROR_UIColors.MARS_ACCENT;
			ink = AROR_UIColors.MARS_INK;
		}
		Button(x, y, width, height, label, action, AROR_UIColors.Get(fill), AROR_UIColors.Get(ink), size);
	}

	override protected void DoAction(string action)
	{
		if (action == "unlock")
		{
			string credential = m_wCredential.GetText();
			m_bUnlocked = AROR_MarsAccess.Accepts(credential);
			m_wCredential.SetText(string.Empty);
			m_sMessage = "Access denied";
			if (m_bUnlocked)
			{
				m_sCredential = credential;
				m_sMessage = "Choose a response type and levels. The server validates the resulting force and launch territory.";
			}
			return;
		}
		if (!m_bUnlocked)
			return;
		if (action == "lock")
		{
			m_bUnlocked = false;
			m_sCredential = string.Empty;
			m_sMessage = string.Empty;
			return;
		}
		if (action == "us" || action == "ussr")
		{
			m_eRole = AROR_EFactionRole.OCCUPIERS;
			if (action == "ussr")
				m_eRole = AROR_EFactionRole.INVADERS;
			UseCampaignLevels();
		}
		else if (action == "campaign")
			UseCampaignLevels();
		else if (action == "previous")
			m_iTypePage = Math.Max(0, m_iTypePage - 1);
		else if (action == "next" && (m_iTypePage + 1) * 6 < m_aTypes.Count())
			m_iTypePage++;
		else if (action.IndexOf("type:") == 0)
		{
			m_iType = action.Substring(5, action.Length() - 5).ToInt();
			m_bPreviewDirty = true;
		}
		else if ((action == "launch" || action == "attack") && m_Preview && m_Preview.IsAvailable())
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (controller)
			{
				string selection = "type:";
				if (action == "attack")
					selection = "attack:";
				controller.AROR_RequestMars(m_sCredential, m_eRole, selection + m_iType, m_iCrackdown, m_iReadiness);
				m_sMessage = "Response requested. Server admission and operation state appear below.";
			}
		}
	}
}
