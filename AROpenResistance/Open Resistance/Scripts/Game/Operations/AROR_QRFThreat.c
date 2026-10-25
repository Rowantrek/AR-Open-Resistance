//! Coalesced trusted report for one role/location. No entity or live target reference.
class AROR_QRFThreat
{
	AROR_EFactionRole m_eRole;
	string m_sLocationId;
	int m_iFirstSeen;
	int m_iLastSeen;
	int m_iNextEvidence;
	int m_iReports;
	int m_iNextAttempt;
	int m_iNextWaitLog;
	bool m_bCaptured;
	int m_iImportance;
	int m_iDesiredTier;
	int m_iTierRoll = -1;

	int Score(int now)
	{
		return AROR_QRFStrategicPolicy.Priority(m_iImportance, m_iReports, m_bCaptured, now - m_iFirstSeen);
	}
}
