//! Rough arrival estimates for dispatched QRFs and attacks: straight-line distance, a road allowance and average
//! speeds. Only for player-facing alerts and map markers; nothing in the operation logic depends on it.
class AROR_OperationEta
{
	static const float GROUND_SPEED = 11;		//!< m/s, about 40 km/h averaged over Everon's roads
	static const float ROAD_FACTOR = 1.35;		//!< Roads wind: driven distance per straight-line metre
	static const int GROUND_DISMOUNT = 30;		//!< Seconds to stop and unload at the objective
	static const float AIR_SPEED = 40;			//!< m/s cruise for transport helicopters
	static const int AIR_LANDING = 60;			//!< Seconds to find the landing site, descend and unload

	//------------------------------------------------------------------------------------------------
	static int Estimate(float distance, bool airborne)
	{
		if (distance < 0)
			return -1;
		if (airborne)
			return Math.Round(distance / AIR_SPEED) + AIR_LANDING;
		return Math.Round(distance * ROAD_FACTOR / GROUND_SPEED) + GROUND_DISMOUNT;
	}

	//------------------------------------------------------------------------------------------------
	//! Headline wording, rounded up to whole minutes: "ETA ~4 MIN", "ETA UNDER 1 MIN", or empty when unknown
	static string Describe(int seconds)
	{
		if (seconds < 0)
			return string.Empty;
		if (seconds < 60)
			return "ETA UNDER 1 MIN";
		int minutes = Math.Ceil(seconds / 60.0);
		return string.Format("ETA ~%1 MIN", minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Map countdown: "ETA 3:05"
	static string Countdown(int seconds)
	{
		seconds = Math.Max(0, seconds);
		int minutes = seconds / 60;
		int rest = seconds % 60;
		if (rest < 10)
			return string.Format("ETA %1:0%2", minutes, rest);
		return string.Format("ETA %1:%2", minutes, rest);
	}
}
