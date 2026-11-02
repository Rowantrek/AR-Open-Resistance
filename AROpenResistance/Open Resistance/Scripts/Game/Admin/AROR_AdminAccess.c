//! Shared role authorization for campaign debug tools. A UI credential is not authority.
class AROR_AdminAccess
{
	static bool IsAdministrator(int playerId)
	{
		if (playerId <= 0)
			return false;
#ifdef WORKBENCH
		return true;
#else
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		if (players.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR) || players.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR))
			return true;

		// The player hosting a listen server always has admin rights over their own session. Only the host's own
		// machine reports RplMode.Listen, and there the local player is the host, so other clients never qualify.
		if (RplSession.Mode() != RplMode.Listen)
			return false;

		PlayerController localController = GetGame().GetPlayerController();
		return localController && localController.GetPlayerId() == playerId;
#endif
	}
}
