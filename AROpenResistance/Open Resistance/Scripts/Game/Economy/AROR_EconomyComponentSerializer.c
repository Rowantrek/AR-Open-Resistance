//! Saves faction funds, Resistance manpower and player money (AROR_EconomyComponent on the game mode, which has a Persistence component).
//! Nothing is written while funds are at their starting values and no player has money yet.
class AROR_EconomyComponentSerializer : ScriptedComponentSerializer
{
	//! 2: adds Resistance manpower
	protected static const int VERSION = 2;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_EconomyComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_EconomyComponent economy = AROR_EconomyComponent.Cast(component);

		array<string> playerKeys = {};
		array<int> playerBalances = {};
		economy.GetPlayerMoneyEntries(playerKeys, playerBalances);

		// HICOMM squads are not saved: save funds and manpower as if every living squad had been refunded in full,
		// so whichever save is loaded, the Resistance keeps what its squads cost
		int refundFunds, refundManpower;
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.Cast(owner.FindComponent(AROR_HighCommandComponent));
		if (highCommand)
			highCommand.GetRefundValue_S(refundFunds, refundManpower);

		if (economy.HasStartingFactionFunds() && economy.HasStartingManpower() && playerKeys.IsEmpty() && refundFunds == 0 && refundManpower == 0)
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", VERSION);
		context.WriteValue("resistance", economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE) + refundFunds);
		context.WriteValue("occupiers", economy.GetFactionFunds(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("invaders", economy.GetFactionFunds(AROR_EFactionRole.INVADERS));
		context.WriteValue("manpower", economy.GetManpower() + refundManpower);
		context.WriteValue("playerKeys", playerKeys);
		context.WriteValue("playerBalances", playerBalances);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		int resistance, occupiers, invaders;
		if (context.ReadValue("resistance", resistance) && context.ReadValue("occupiers", occupiers) && context.ReadValue("invaders", invaders))
			economy.RestoreFactionFunds_S(resistance, occupiers, invaders);

		// Version 1 saves have no manpower; it then stays at the starting value
		int manpower;
		if (version >= 2 && context.ReadValue("manpower", manpower))
			economy.RestoreManpower_S(manpower);

		array<string> playerKeys = {};
		array<int> playerBalances = {};
		if (context.ReadValue("playerKeys", playerKeys) && context.ReadValue("playerBalances", playerBalances))
			economy.RestorePlayerMoney_S(playerKeys, playerBalances);

		return true;
	}
}
