//! Server: one player's campaign record, keyed by identity like personal money (see AROR_LeaderboardComponent)
class AROR_LeaderboardEntry
{
	string m_sKey;
	string m_sName;	//!< Last known name, so offline players stay on the board
	ref array<int> m_aKills = {0, 0, 0, 0, 0};		//!< Own kills per AROR_ELeaderboardCategory
	ref array<int> m_aAssists = {0, 0, 0, 0, 0};	//!< Kills by AI the player commanded, per category

	//------------------------------------------------------------------------------------------------
	int GetAssistTotal()
	{
		int total;
		foreach (int count : m_aAssists)
		{
			total += count;
		}

		return total;
	}
}

//! Client: one row of the replicated board, already sorted by score
class AROR_LeaderboardRow
{
	int m_iPlayerId;	//!< 0 while the player is offline
	string m_sName;
	int m_iScore;
	ref array<int> m_aKills = {0, 0, 0, 0, 0};
	int m_iAssists;		//!< General count over every category
	int m_iAssistPoints;
}

//! A vehicle prefab whose path contains m_sMatch goes in m_eCategory, whatever its preset type says. Lets armoured
//! cars that presets list as ARMED_CAR (JLTV, MRAPs) count as APC / IFV while an M2 Humvee stays a light vehicle.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sMatch")]
class AROR_LeaderboardVehicleRule
{
	[Attribute("", desc: "Text the vehicle prefab path contains (not case sensitive), e.g. JLTV")]
	string m_sMatch;

	[Attribute("2", UIWidgets.ComboBox, desc: "Leaderboard category for matching vehicles", enums: ParamEnumArray.FromEnum(AROR_ELeaderboardCategory))]
	AROR_ELeaderboardCategory m_eCategory;
}

//! Server: who has earned the death of one character or vehicle. Created by the modded damage managers
//! (AROR_ModdedDamageManagers.c) and fed by their damage events; AROR_LeaderboardComponent decides and counts.
class AROR_KillCredit
{
	protected IEntity m_Owner;
	protected SCR_DamageManagerComponent m_DamageManager;

	int m_iPlayerId;	//!< Player credited for the last hostile hit; 0 = nobody (e.g. the enemy hit it last)
	bool m_bAssist;		//!< The credited player commanded the AI that hit
	float m_fHitTime;	//!< World time (ms) of that hit
	bool m_bResolved;	//!< Its death has been judged; later hits on the body or wreck change nothing
	bool m_bPending;	//!< In the leaderboard's watch list

	//------------------------------------------------------------------------------------------------
	static AROR_KillCredit Attach(IEntity owner, SCR_DamageManagerComponent damageManager)
	{
		if (!owner || !damageManager || !GetGame().InPlayMode())
			return null;

		AROR_KillCredit credit = new AROR_KillCredit();
		credit.m_Owner = owner;
		credit.m_DamageManager = damageManager;
		damageManager.GetOnDamage().Insert(credit.OnDamage);
		return credit;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetOwner()
	{
		return m_Owner;
	}

	//------------------------------------------------------------------------------------------------
	//! Dead character or destroyed vehicle
	bool IsDead()
	{
		if (!m_Owner)
			return false;

		if (m_DamageManager && m_DamageManager.IsDestroyed())
			return true;

		ChimeraCharacter character = ChimeraCharacter.Cast(m_Owner);
		return character && character.GetCharacterController() && character.GetCharacterController().IsDead();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDamage(BaseDamageContext context)
	{
		if (m_bResolved || !Replication.IsServer())
			return;

		AROR_LeaderboardComponent leaderboard = AROR_LeaderboardComponent.GetInstance();
		if (leaderboard)
			leaderboard.OnHit_S(this, context);
	}
}
