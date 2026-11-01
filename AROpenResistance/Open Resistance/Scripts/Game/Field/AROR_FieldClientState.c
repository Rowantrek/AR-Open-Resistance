class AROR_FieldMarkerInfo
{
	int m_iId;
	bool m_bEditable;
	int m_iScope;
	int m_iKind;
	vector m_vPosition;
	string m_sCaption;
}

class AROR_FieldRecruitInfo
{
	int m_iId;
	bool m_bFormed;
	int m_iOrder;
	vector m_vPosition;
}

//! Only the owning controller can deliver these snapshots. No authoritative gameplay state lives here.
class AROR_FieldClientState
{
	static ref array<ref AROR_FieldMarkerInfo> s_aMarkers = {};
	static ref array<ref AROR_FieldRecruitInfo> s_aRecruits = {};
	protected static string s_sMarkers;
	protected static string s_sRecruits;
	protected static ref AROR_FieldMapPicker s_Picker;

	static void Clear()
	{
		s_aMarkers.Clear();
		s_aRecruits.Clear();
		s_sMarkers = string.Empty;
		s_sRecruits = string.Empty;
		if (s_Picker)
			s_Picker.Cancel();
		s_Picker = null;
	}

	static void Receive(string markers, string recruits)
	{
		if (markers != s_sMarkers)
		{
			s_sMarkers = markers;
			s_aMarkers.Clear();
			array<string> rows = {};
			markers.Split(";", rows, true);
			foreach (string row : rows)
			{
				array<string> fields = {};
				row.Split("|", fields, false);
				if (fields.Count() != 7)
					continue;
				AROR_FieldMarkerInfo marker = new AROR_FieldMarkerInfo();
				marker.m_iId = fields[0].ToInt();
				marker.m_bEditable = fields[1] == "1";
				marker.m_iScope = fields[2].ToInt();
				marker.m_iKind = fields[3].ToInt();
				marker.m_vPosition = Vector(fields[4].ToFloat(), 0, fields[5].ToFloat());
				marker.m_sCaption = fields[6];
				s_aMarkers.Insert(marker);
			}
		}
		if (recruits != s_sRecruits)
		{
			s_sRecruits = recruits;
			s_aRecruits.Clear();
			array<string> recruitRows = {};
			recruits.Split(";", recruitRows, true);
			foreach (string recruitRow : recruitRows)
			{
				array<string> recruitFields = {};
				recruitRow.Split("|", recruitFields, false);
				if (recruitFields.Count() != 5)
					continue;
				AROR_FieldRecruitInfo recruit = new AROR_FieldRecruitInfo();
				recruit.m_iId = recruitFields[0].ToInt();
				recruit.m_bFormed = recruitFields[1] == "1";
				recruit.m_iOrder = recruitFields[2].ToInt();
				recruit.m_vPosition = Vector(recruitFields[3].ToFloat(), 0, recruitFields[4].ToFloat());
				s_aRecruits.Insert(recruit);
			}
		}
	}

	static void PickMarker(int id, string caption, int scope, int kind, int lifetime)
	{
		if (s_Picker)
			s_Picker.Cancel();
		s_Picker = new AROR_FieldMapPicker();
		s_Picker.Start(id, caption, scope, kind, lifetime);
	}

	static string ScopeName(int scope)
	{
		if (scope == 1) return "SQUAD";
		if (scope == 2) return "RESISTANCE";
		return "PRIVATE";
	}

	static string KindName(int kind)
	{
		switch (kind)
		{
			case 1: return "DANGER";
			case 2: return "RALLY";
			case 3: return "SUPPLIES";
			case 4: return "ROUTE";
		}
		return "NOTE";
	}

	static string OrderName(int order)
	{
		switch (order)
		{
			case AROR_ERecruitAction.HOLD: return "HOLD";
			case AROR_ERecruitAction.BOARD_DRIVER: return "DRIVER";
			case AROR_ERecruitAction.BOARD_GUNNER: return "GUNNER";
			case AROR_ERecruitAction.BOARD_PASSENGER: return "PASSENGER";
		}
		return "FOLLOW";
	}
}
