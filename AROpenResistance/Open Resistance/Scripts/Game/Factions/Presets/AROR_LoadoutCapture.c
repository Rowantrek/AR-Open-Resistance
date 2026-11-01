#ifdef WORKBENCH
//! Workbench only: reads the kit an admin's own character currently wears and carries and writes it to a named text
//! file as an AROR_UnitLoadout block, ready to be pasted into a faction preset's unit (it carries no Readiness: that is
//! chosen where it is pasted). Started from the Admin menu's Tools tab. Uses the base unit loadout importer's reading
//! (clothing and contents, weapons with the attachments and magazine that differ from the weapon prefab's own, loose
//! items), so a kit captured here looks the same as an imported one. The character is only read, never changed or deleted.
class AROR_LoadoutCapture : AROR_UnitLoadoutImporter
{
	protected static const string CAPTURE_FOLDER = "$profile:AROR_Loadouts/";
	protected static const int MAX_NAME_LENGTH = 48;
	protected static const string NAME_CHARACTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";

	protected string m_sName;
	protected int m_iPlayerId;

	//------------------------------------------------------------------------------------------------
	//! Server: captures the character's kit under the name; the result is reported to the player as a hint
	static void Capture(int playerId, IEntity character, string rawName)
	{
		string name = CleanName(rawName);
		if (name.IsEmpty())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Capture loadout: type a name first (letters, numbers, _ and -).");
			return;
		}

		if (!character)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Capture loadout: you have no character to read.");
			return;
		}

		if (s_Running)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Capture loadout: another capture is still running.");
			return;
		}

		AROR_LoadoutCapture capture = new AROR_LoadoutCapture();
		capture.m_sName = name;
		capture.m_iPlayerId = playerId;
		s_Running = capture;
		capture.Begin(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Letters, numbers, _ and - only (spaces become _), limited in length; empty when nothing usable is left
	protected static string CleanName(string rawName)
	{
		string name = rawName;
		name.Replace(" ", "_");

		string clean;
		for (int i = 0; i < name.Length() && clean.Length() < MAX_NAME_LENGTH; i++)
		{
			string character = name.Substring(i, 1);
			if (NAME_CHARACTERS.IndexOf(character) >= 0)
				clean += character;
		}

		return clean;
	}

	//------------------------------------------------------------------------------------------------
	protected void Begin(IEntity character)
	{
		AROR_LoadoutImportUnit unit = new AROR_LoadoutImportUnit();
		unit.m_sPresetId = m_sName;
		unit.m_Entity = character;
		ReadCharacter(unit);

		// The character belongs to the player: forget it so CleanUp never deletes it
		unit.m_Entity = null;
		m_aUnits.Insert(unit);

		SpawnWeaponDefaults();
		GetGame().GetCallqueue().CallLater(Finish, SETTLE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	override protected void Finish()
	{
		SubtractWeaponDefaults();
		string path = WriteCapture();
		CleanUp();

		if (path.IsEmpty())
			AROR_NotificationManagerComponent.SendHint_S(m_iPlayerId, string.Format("Capture loadout: could not write '%1'; see the console.", m_sName));
		else
			AROR_NotificationManagerComponent.SendHint_S(m_iPlayerId, string.Format("Loadout captured to %1", path), AROR_UIColors.Get(AROR_UIColors.REBEL));
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the captured kit as a single AROR_UnitLoadout block; returns the file path, or empty on failure
	protected string WriteCapture()
	{
		if (m_aUnits.IsEmpty())
			return string.Empty;

		// AppendLoadout wraps the loadout in its unit's m_aLoadouts list: keep only the loadout block itself
		array<string> lines = {};
		AppendLoadout(m_aUnits[0], lines);
		if (lines.Count() < 3)
			return string.Empty;

		lines.Remove(lines.Count() - 1);
		lines.Remove(0);

		FileIO.MakeDirectory(CAPTURE_FOLDER);
		string path = CAPTURE_FOLDER + m_sName + ".txt";
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			Print(string.Format("[AROR] Capture loadout: cannot write %1", path), LogLevel.ERROR);
			return string.Empty;
		}

		foreach (string line : lines)
		{
			file.WriteLine(line);
		}

		file.Close();
		Print(string.Format("[AROR] Capture loadout: '%1' written to %2", m_sName, path), LogLevel.NORMAL);
		return path;
	}
}
#endif
