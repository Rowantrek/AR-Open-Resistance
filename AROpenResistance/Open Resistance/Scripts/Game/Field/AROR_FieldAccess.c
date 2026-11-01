//! Shared validation for field tools. RPC callers supply intent; the server supplies identity and faction.
class AROR_FieldAccess
{
	static IEntity Player(int playerId)
	{
		AROR_GameFlowComponent flow = AROR_GameFlowComponent.GetInstance();
		if (!flow || !flow.IsPlaythroughReady() || playerId <= 0)
			return null;
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!AROR_ForceSpawner.IsAlive(player))
			return null;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(player.FindComponent(FactionAffiliationComponent));
		if (!affiliation || affiliation.GetAffiliatedFactionKey() != AROR_ForceSpawner.FactionKey(AROR_EFactionRole.RESISTANCE))
			return null;
		return player;
	}

	static string PlayerKey(int playerId)
	{
		BackendApi backend = GetGame().GetBackendApi();
		if (backend)
		{
			string identity = backend.GetPlayerIdentityId(playerId);
			if (!identity.IsEmpty())
				return identity;
		}
		return "player_" + playerId;
	}

	//! Reject non-finite/out-of-map coordinates using positive comparisons (also reject NaN).
	static bool MapPosition(vector position)
	{
		AROR_HighCommandComponent command = AROR_HighCommandComponent.GetInstance();
		if (!command)
			return false;
		vector minimum, maximum;
		command.GetMapBounds(minimum, maximum);
		return position[0] >= minimum[0] && position[0] <= maximum[0]
			&& position[2] >= minimum[2] && position[2] <= maximum[2];
	}

	//! Snapshot delimiters and rich-text markup never enter user captions.
	static string Caption(string text)
	{
		if (text.Length() > 48)
			text = text.Substring(0, 48);
		text.Replace("|", " ");
		text.Replace(";", " ");
		text.Replace("\n", " ");
		text.Replace("\r", " ");
		text.Replace("<", " ");
		text.Replace(">", " ");
		return text.Trim();
	}
}
