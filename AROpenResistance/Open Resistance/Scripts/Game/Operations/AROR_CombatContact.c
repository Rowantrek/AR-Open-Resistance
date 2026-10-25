//! Session-local report; only trusted, validated server combat callers may publish it.
class AROR_CombatContact
{
	AROR_EFactionRole m_eRole;
	string m_sLocationId;
	vector m_vPosition;
	int m_iTime;
	int m_iSequence;
}
