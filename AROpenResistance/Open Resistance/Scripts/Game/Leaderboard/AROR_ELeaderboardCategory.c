//! Kill categories on the leaderboard, in display order (infantry first). Fixed core list: saves store one count per
//! value, so new values may only be appended.
enum AROR_ELeaderboardCategory
{
	INFANTRY,		//!< Enemy soldiers
	LIGHT_VEHICLE,	//!< Cars, trucks, boats and armed light vehicles such as an M2 Humvee or a DShK UAZ
	APC_IFV,		//!< APCs, IFVs, MRAPs and armoured cars: BTR, BMP, Bradley, JLTV, BRDM; also tracked AA
	ARMOUR,			//!< Main battle tanks
	AIRCRAFT		//!< Every helicopter and plane
}
