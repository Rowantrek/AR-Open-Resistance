[ComponentEditorProps(category: "OpenResistance/Capture", description: "AI recapture of military locations (server only). Attach to the game mode entity.")]
class AROR_RecaptureComponentClass : SCR_BaseGameModeComponentClass
{
}

//! AI recapture of military locations (see ARCHITECTURE.md -> Location Capture).
//! Periodically weighs forces inside each military location's radius: defenders (the controlling role's players
//! and AI) count double, attacking AI count single. An attacking AI role with more weight than the defenders makes
//! the location contested; it must keep that advantage for the contest time before it takes the location (subject
//! to the flip cooldown). Losing the advantage ends the contest. When a Resistance site becomes contested, the
//! Resistance gets a critical "territory under attack" alert. The Resistance itself only captures through the flag.
class AROR_RecaptureComponent : SCR_BaseGameModeComponent
{
	protected static const int CHECK_INTERVAL_MS = 5000;
	protected static const float DEFENDER_WEIGHT = 2;
	protected static const float ATTACKER_WEIGHT = 1;

	[Attribute("60", desc: "Seconds an attacking AI role must keep the force advantage inside a location before it takes it", params: "0 600 5")]
	protected int m_iContestSeconds;

	protected static AROR_RecaptureComponent s_Instance;

	//! Contested locations: ID -> world time (ms) the current attacker's advantage began
	protected ref map<string, int> m_mContestedSince = new map<string, int>();
	protected ref map<string, int> m_mContestAttacker = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! Server: true while an attacking AI role holds the force advantage at this location (a fight for it is on)
	static bool IsContested(string locationId)
	{
		return s_Instance && s_Instance.m_mContestedSince.Contains(locationId);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLocations()
	{
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
			return;

		array<AROR_LocationComponent> locations = {};
		locationManager.GetLocations(locations);

		foreach (AROR_LocationComponent location : locations)
		{
			if (location && !location.IsControlFixed() && AROR_CaptureLocationAction.IsCapturableLocation(location))
				CheckLocation(location);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLocation(notnull AROR_LocationComponent location)
	{
		AROR_LocationForces forces = AROR_LocationForces.Count(location);
		string id = location.GetId();

		AROR_EFactionRole defender = location.GetControllingRole();
		float defenderWeight = DEFENDER_WEIGHT * forces.GetTotal(defender);

		// Strongest attacking AI role (only AI roles recapture; the Resistance uses the flag)
		array<AROR_EFactionRole> aiRoles = {};
		AROR_FactionRoles.GetAIMilitary(aiRoles);
		AROR_EFactionRole attacker;
		float attackerWeight;
		foreach (AROR_EFactionRole role : aiRoles)
		{
			if (role == defender)
				continue;

			float weight = ATTACKER_WEIGHT * forces.GetAI(role);
			if (weight > attackerWeight)
			{
				attackerWeight = weight;
				attacker = role;
			}
		}

		if (attackerWeight <= defenderWeight || !location.CanBeTakenBy(attacker))
		{
			EndContest(id);
			return;
		}

		// A new attacker, or the first check with the advantage, starts the contest clock
		int now = GetGame().GetWorld().GetWorldTime();
		if (!m_mContestedSince.Contains(id) || m_mContestAttacker.Get(id) != attacker)
		{
			m_mContestedSince.Set(id, now);
			m_mContestAttacker.Set(id, attacker);
			AnnounceContest(location, attacker);
		}
		if (now - m_mContestedSince.Get(id) < m_iContestSeconds * 1000)
			return;

		if (AROR_CaptureLocationAction.CaptureLocation_S(location, attacker))
		{
			EndContest(id);
			Print(string.Format("[AROR] '%1' recaptured by %2 (attackers %3 vs defenders %4, held %5 s)",
				location.GetName(), typename.EnumToString(AROR_EFactionRole, attacker), attackerWeight, defenderWeight, m_iContestSeconds), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EndContest(string id)
	{
		m_mContestedSince.Remove(id);
		m_mContestAttacker.Remove(id);
	}

	//------------------------------------------------------------------------------------------------
	//! P1 for the Resistance: one of its sites is being taken and they have the contest time to react
	protected void AnnounceContest(AROR_LocationComponent location, AROR_EFactionRole attacker)
	{
		Print(string.Format("[AROR] '%1' contested by %2; %3 s to hold it", location.GetName(), typename.EnumToString(AROR_EFactionRole, attacker), m_iContestSeconds), LogLevel.NORMAL);
		if (location.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
			return;

		AROR_Notification alert = new AROR_Notification(AROR_ENotificationType.HEADLINE, "TERRITORY UNDER ATTACK / " + location.GetName(), string.Empty, AROR_UIColors.Get(AROR_UIColors.DANGER), "HQ");
		AROR_NotificationManagerComponent.Send_S(alert);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		s_Instance = this;
		GetGame().GetCallqueue().CallLater(CheckLocations, CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckLocations);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
