//! Session-only eligibility for one Resistance-held site. Captured (or loaded) sites become due after the capture
//! grace; a capture QRF on the site holds the attack while deployed and for a recovery time after it ends.
class AROR_TerritoryAttackState
{
	string m_sLocationId;
	int m_iCapturedAt;
	int m_iNextAttempt;
	int m_iQRFCompletedAt = -1;
	string m_sActiveQRF;

	bool IsDue(int now, int captureGrace, int afterQRF)
	{
		if (!m_sActiveQRF.IsEmpty() || now < m_iCapturedAt + captureGrace || now < m_iNextAttempt)
			return false;
		return m_iQRFCompletedAt < 0 || now >= m_iQRFCompletedAt + afterQRF;
	}
}
