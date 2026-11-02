//! Shared entry check. The server rechecks every debug request; never log credentials.
class AROR_MarsAccess
{
	//! Packaged multiplayer sessions require a real server role; the shipped entry
	//! credential is a UI lock, not authorization to spawn free campaign forces.
	static bool CanRequest_S(int playerId)
	{
		return Replication.IsServer() && AROR_AdminAccess.IsAdministrator(playerId);
	}

	static bool Accepts(string credential)
	{
		return credential == "Vote4Chicken";
	}
}
