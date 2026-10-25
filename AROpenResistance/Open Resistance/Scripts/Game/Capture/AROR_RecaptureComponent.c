[ComponentEditorProps(category: "OpenResistance/Capture", description: "AI recapture of military locations (server only). Attach to the game mode entity.")]
class AROR_RecaptureComponentClass : SCR_BaseGameModeComponentClass
{
}

//! AI recapture of military locations (see ARCHITECTURE.md -> Location Capture).
//! Periodically weighs forces inside each military location's radius: defenders (the controlling role's players
//! and AI) count double, attacking AI count single. An attacking AI role with more weight than the defenders
//! takes the location, subject to the flip cooldown. The Resistance only captures through the flag interaction.
class AROR_RecaptureComponent : SCR_BaseGameModeComponent
{
	protected static const int CHECK_INTERVAL_MS = 5000;
	protected static const float DEFENDER_WEIGHT = 2;
	protected static const float ATTACKER_WEIGHT = 1;

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

		AROR_EFactionRole defender = location.GetControllingRole();
		float defenderWeight = DEFENDER_WEIGHT * forces.GetTotal(defender);

		// Strongest attacking AI role (only AI roles recapture; the Resistance uses the flag)
		array<AROR_EFactionRole> aiRoles = {AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS};
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
			return;

		if (AROR_CaptureLocationAction.CaptureLocation_S(location, attacker))
		{
			Print(string.Format("[AROR] '%1' recaptured by %2 (attackers %3 vs defenders %4)",
				location.GetName(), typename.EnumToString(AROR_EFactionRole, attacker), attackerWeight, defenderWeight), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(CheckLocations, CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckLocations);
		super.OnDelete(owner);
	}
}
