//! Server-side presence of players and their commanded ground forces. Militia are deliberately excluded from
//! activation: an ambient garrison must not keep itself raised forever. Town defence checks militia separately.
class AROR_ResistancePresence
{
	static void GetPositions(notnull array<vector> positions)
	{
		positions.Clear();
		if (!Replication.IsServer())
			return;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			AddPosition(player, positions);
		}
		AROR_HighCommandComponent command = AROR_HighCommandComponent.GetInstance();
		if (command)
			command.AppendPresencePositions_S(positions);
		AROR_SquadManagerComponent squads = AROR_SquadManagerComponent.GetInstance();
		if (squads)
			squads.AppendPresencePositions_S(positions);
	}

	static void AddPosition(IEntity member, notnull array<vector> positions)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (AROR_QRFSpotting.IsResistance(character) && !AROR_LocationForces.IsInAircraft(character))
			positions.Insert(member.GetOrigin());
	}

	static float Nearest(vector center)
	{
		array<vector> positions = {};
		GetPositions(positions);
		float nearest = 1e10;
		foreach (vector position : positions)
			nearest = Math.Min(nearest, vector.DistanceXZ(position, center));
		return nearest;
	}
}
