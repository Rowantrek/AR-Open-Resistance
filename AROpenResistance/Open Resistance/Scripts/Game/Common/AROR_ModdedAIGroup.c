//! Every AI group: soldiers are dressed from their preset unit's loadouts (see AROR_AIOutfitter) and soldiers of the
//! enemy roles are armed from their role's faction preset (see AROR_SoldierArmory) as they join. Modded rather than
//! subclassed because garrisons, QRFs, patrols and missions all use base-game group prefabs.
modded class SCR_AIGroup
{
	//------------------------------------------------------------------------------------------------
	override void OnAgentAdded(AIAgent child)
	{
		super.OnAgentAdded(child);
		AROR_AIOutfitter.OnAgentAdded_S(child);
		AROR_SoldierArmory.OnAgentAdded_S(child);
	}
}
