//! Leaderboard hooks: on the server every character and vehicle gets a kill credit that listens to its damage events
//! (see AROR_KillCredit), and settles a watched death when the entity is deleted.
modded class SCR_CharacterDamageManagerComponent
{
	protected ref AROR_KillCredit m_AROR_KillCredit;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_AROR_KillCredit = AROR_KillCredit.Attach(owner, this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_AROR_KillCredit)
			m_AROR_KillCredit.OnOwnerDeleted();

		super.OnDelete(owner);
	}
}

modded class SCR_VehicleDamageManagerComponent
{
	protected ref AROR_KillCredit m_AROR_KillCredit;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_AROR_KillCredit = AROR_KillCredit.Attach(owner, this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_AROR_KillCredit)
			m_AROR_KillCredit.OnOwnerDeleted();

		super.OnDelete(owner);
	}
}
