//! Leaderboard hooks: every character and vehicle gets a kill credit that listens to its damage events (see
//! AROR_KillCredit). Only the server acts on them.
modded class SCR_CharacterDamageManagerComponent
{
	protected ref AROR_KillCredit m_AROR_KillCredit;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_AROR_KillCredit = AROR_KillCredit.Attach(owner, this);
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
}
