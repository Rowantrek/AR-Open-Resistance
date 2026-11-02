//! Client-only REBINTEL preferences, saved per player in their profile folder (never on the server).
//! Read by the HUD status widget and the level-change alerts; changed from the REBINTEL app.
class AROR_RebIntelSettings
{
	protected static const string FILE_PATH = "$profile:AROR_RebIntel.txt";
	protected static const int MAX_LINES = 32;

	protected static bool s_bLoaded;
	protected static bool s_bShowHud = true;
	protected static bool s_bAlerts = true;
	protected static bool s_bFull;
	protected static bool s_bLeft;
	protected static AROR_EFactionRole s_eTrackedRole = AROR_EFactionRole.OCCUPIERS;

	//------------------------------------------------------------------------------------------------
	static bool IsHudShown()
	{
		Load();
		return s_bShowHud;
	}

	//------------------------------------------------------------------------------------------------
	//! Level-change pop-ups (warnings and info); critical alerts cannot be turned off
	static bool AreAlertsShown()
	{
		Load();
		return s_bAlerts;
	}

	//------------------------------------------------------------------------------------------------
	//! Full adds a meter and the n/5 count under each value; compact is the default
	static bool IsFull()
	{
		Load();
		return s_bFull;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLeft()
	{
		Load();
		return s_bLeft;
	}

	//------------------------------------------------------------------------------------------------
	//! The REBINTEL tab last opened, so the app reopens on it
	static AROR_EFactionRole GetTrackedRole()
	{
		Load();
		return s_eTrackedRole;
	}

	//------------------------------------------------------------------------------------------------
	static void SetHudShown(bool shown)
	{
		Load();
		s_bShowHud = shown;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	static void SetAlertsShown(bool shown)
	{
		Load();
		s_bAlerts = shown;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	static void SetFull(bool full)
	{
		Load();
		s_bFull = full;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	static void SetLeft(bool left)
	{
		Load();
		s_bLeft = left;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	static void SetTrackedRole(AROR_EFactionRole role)
	{
		Load();
		if (s_eTrackedRole == role || (role != AROR_EFactionRole.OCCUPIERS && role != AROR_EFactionRole.INVADERS))
			return;
		s_eTrackedRole = role;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	//! "key=value" lines; unknown keys and bad values keep their defaults
	protected static void Load()
	{
		if (s_bLoaded)
			return;
		s_bLoaded = true;

		if (!FileIO.FileExists(FILE_PATH))
			return;
		FileHandle file = FileIO.OpenFile(FILE_PATH, FileMode.READ);
		if (!file)
			return;

		string line;
		for (int i = 0; i < MAX_LINES && file.ReadLine(line) >= 0; i++)
		{
			array<string> parts = {};
			line.Split("=", parts, false);
			if (parts.Count() != 2)
				continue;
			string key = parts[0].Trim();
			string value = parts[1].Trim();
			if (key == "hud")
				s_bShowHud = value == "1";
			else if (key == "alerts")
				s_bAlerts = value == "1";
			else if (key == "full")
				s_bFull = value == "1";
			else if (key == "left")
				s_bLeft = value == "1";
			else if (key == "role" && value == "invaders")
				s_eTrackedRole = AROR_EFactionRole.INVADERS;
		}
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Save()
	{
		FileHandle file = FileIO.OpenFile(FILE_PATH, FileMode.WRITE);
		if (!file)
		{
			Print("[AROR] RebIntel settings could not be saved to " + FILE_PATH, LogLevel.WARNING);
			return;
		}

		string role = "occupiers";
		if (s_eTrackedRole == AROR_EFactionRole.INVADERS)
			role = "invaders";

		file.WriteLine("hud=" + Flag(s_bShowHud));
		file.WriteLine("alerts=" + Flag(s_bAlerts));
		file.WriteLine("full=" + Flag(s_bFull));
		file.WriteLine("left=" + Flag(s_bLeft));
		file.WriteLine("role=" + role);
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static string Flag(bool value)
	{
		if (value)
			return "1";
		return "0";
	}
}
