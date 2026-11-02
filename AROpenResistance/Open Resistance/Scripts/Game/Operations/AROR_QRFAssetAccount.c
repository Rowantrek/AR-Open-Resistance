//! One observed physical asset. Missing/virtualized entities alone are not confirmed deaths.
class AROR_QRFAssetAccount
{
	IEntity m_Entity;
	int m_iValue;
	int m_iPackageIndex = -1;
	bool m_bVehicle;
	bool m_bSettled;
	string m_sFaction;
	string m_sNativeFaction;

	bool IsLost()
	{
		if (!m_Entity || m_bSettled)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Entity);
		if (character && character.GetCharacterController() && character.GetCharacterController().IsDead())
			return true;
		DamageManagerComponent damage = DamageManagerComponent.Cast(m_Entity.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return true;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(m_Entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		string key = affiliation.GetAffiliatedFactionKey();
		if (key.IsEmpty() || key == m_sFaction)
			return false;
		// Native vehicle affiliation can revert after occupants leave; that is not capture.
		if (m_bVehicle && key == m_sNativeFaction)
			return false;
		return true;
	}
}
