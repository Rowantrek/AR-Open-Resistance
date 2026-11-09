//! Client HUD: the town lockdown indicator. While the local player is near a town whose garrison response is
//! running (AROR_TownWavesComponent), a plate at the top centre shows who outnumbers whom and how close the army is
//! to retaking the town:
//!   "MEAUX  ·  ENEMY 14 vs 6  ·  TOWN LOST IN 4:10"  (red, the bar filling)
//!   "MEAUX  ·  ENEMY 5 vs 6  ·  RECOVERING 2:30"     (amber, the bar draining)
//!   "MEAUX  ·  ENEMY 3 vs 6  ·  HOLDING"             (green)
//! Built from the map icon layout (frame canvas for the plate and bar, rich text for the line), updated by the
//! notification manager's HUD loop. Never created on a dedicated server.
class AROR_TownContestHud
{
	protected static const ResourceName LAYOUT = "{6E2BA15FAEED49D7}UI/Layouts/Map/AROR_MapIcon.layout";
	protected static const float WIDTH = 460;
	protected static const float HEIGHT = 34;
	protected static const float BAR_HEIGHT = 5;
	protected static const float TOP = 128;		//!< Below the compass and the headline banner
	protected static const int FONT = 16;
	protected static const float EDGE = 4;
	protected static const float INSET = 12;
	protected static const int BAR_TRACK = 0x33FFFFFF;	//!< Faint track under the lockdown bar

	protected Widget m_wRoot;
	protected Widget m_wPlate;
	protected Widget m_wCaption;
	protected RichTextWidget m_wLabel;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	protected string m_sText;
	protected int m_iColor;
	protected float m_fFill = -1;

	//------------------------------------------------------------------------------------------------
	void AROR_TownContestHud(notnull Widget root)
	{
		m_wRoot = root;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wPlate = workspace.CreateWidgets(LAYOUT, root);
		m_wCaption = workspace.CreateWidgets(LAYOUT, root);
		if (!m_wPlate || !m_wCaption)
			return;
		Hide(m_wPlate, "Icon");
		Hide(m_wPlate, "QRFLabel");
		Hide(m_wCaption, "Icon");
		Hide(m_wCaption, "Frame");
		m_wLabel = RichTextWidget.Cast(m_wCaption.FindAnyWidget("QRFLabel"));
		if (m_wLabel)
		{
			m_wLabel.SetVisible(true);
			m_wLabel.SetExactFontSize(FONT);
			m_wLabel.SetBold(true);
			FrameSlot.SetPos(m_wLabel, EDGE + INSET, (HEIGHT - BAR_HEIGHT - FONT) * 0.5 - 1);
			FrameSlot.SetSize(m_wLabel, WIDTH - EDGE - INSET * 2, HEIGHT);
		}
		Prepare(m_wPlate);
		Prepare(m_wCaption);
	}

	protected static void Prepare(notnull Widget widget)
	{
		FrameSlot.SetSize(widget, WIDTH, HEIGHT);
		FrameSlot.SetAlignment(widget, 0, 0);
		widget.SetVisible(false);
	}

	protected static void Hide(notnull Widget root, string name)
	{
		Widget child = root.FindAnyWidget(name);
		if (child)
			child.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! A few times per second
	void Update()
	{
		if (!m_wPlate || !m_wCaption || !m_wLabel)
			return;
		AROR_TownWavesComponent town;
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (player)
			town = AROR_TownWavesComponent.FindContest(player.GetOrigin());
		if (!town || !town.GetLocation())
		{
			m_wPlate.SetVisible(false);
			m_wCaption.SetVisible(false);
			return;
		}

		int seconds = town.GetContestSeconds();
		int limit = Math.Max(town.GetLockdownSeconds(), 1);
		int enemies = town.GetContestEnemies();
		int friendlies = town.GetContestFriendlies();
		string name = town.GetLocation().GetName();
		name.ToUpper();
		string text = name + "  ·  ENEMY " + enemies.ToString() + " vs " + friendlies.ToString() + "  ·  ";
		int color = AROR_UIColors.REBEL;
		if (town.IsContestLosing())
		{
			color = AROR_UIColors.DANGER;
			text += "TOWN LOST IN " + Clock(limit - seconds);
		}
		else if (seconds > 0)
		{
			color = AROR_UIColors.WARN;
			text += "RECOVERING " + Clock(seconds);
		}
		else
			text += "HOLDING";

		float fill = seconds / (float)limit;
		if (text != m_sText || color != m_iColor)
		{
			m_sText = text;
			m_wLabel.SetText(text);
			m_wLabel.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT));
		}
		if (color != m_iColor || Math.AbsFloat(fill - m_fFill) > 0.002)
		{
			m_iColor = color;
			m_fFill = fill;
			Draw(fill, color);
		}

		// Top centre of the HUD
		float rootWidth, rootHeight;
		m_wRoot.GetScreenSize(rootWidth, rootHeight);
		float x = GetGame().GetWorkspace().DPIUnscale(rootWidth) * 0.5 - WIDTH * 0.5;
		FrameSlot.SetPos(m_wPlate, x, TOP);
		FrameSlot.SetPos(m_wCaption, x, TOP);
		m_wPlate.SetVisible(true);
		m_wCaption.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! The plate, its state-coloured edge, and the lockdown bar along the bottom (filled share in the state colour)
	protected void Draw(float fill, int color)
	{
		CanvasWidget canvas = CanvasWidget.Cast(m_wPlate.FindAnyWidget("Frame"));
		if (!canvas)
			return;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float w = workspace.DPIScale(WIDTH);
		float h = workspace.DPIScale(HEIGHT);
		float edge = workspace.DPIScale(EDGE);
		float bar = workspace.DPIScale(BAR_HEIGHT);
		m_aCommands.Clear();
		m_aCommands.Insert(Rectangle(0, 0, w, h, AROR_MapLabelPlate.PLATE_COLOR));
		m_aCommands.Insert(Rectangle(0, 0, edge, h, color));
		m_aCommands.Insert(Rectangle(edge, h - bar, w, h, BAR_TRACK));
		if (fill > 0)
			m_aCommands.Insert(Rectangle(edge, h - bar, edge + (w - edge) * Math.Clamp(fill, 0, 1), h, color));
		canvas.SetDrawCommands(m_aCommands);
	}

	protected static string Clock(int seconds)
	{
		seconds = Math.Max(seconds, 0);
		int minutes = seconds / 60;
		int rest = seconds - minutes * 60;
		string padded = rest.ToString();
		if (rest < 10)
			padded = "0" + padded;
		return minutes.ToString() + ":" + padded;
	}

	protected static PolygonDrawCommand Rectangle(float left, float top, float right, float bottom, int color)
	{
		array<float> points = {};
		points.Insert(left);
		points.Insert(top);
		points.Insert(right);
		points.Insert(top);
		points.Insert(right);
		points.Insert(bottom);
		points.Insert(left);
		points.Insert(bottom);
		PolygonDrawCommand polygon = new PolygonDrawCommand();
		polygon.m_iColor = color;
		polygon.m_Vertices = points;
		return polygon;
	}
}
