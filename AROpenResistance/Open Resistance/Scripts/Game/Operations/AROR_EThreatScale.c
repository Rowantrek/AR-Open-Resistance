//! The two enemy status scales, each an integer level 1-5 (see AROR_ThreatLabels for display names)
enum AROR_EThreatScale
{
	READINESS,	//!< How much the enemy can bring (was "war level"); gates QRF equipment
	CRACKDOWN	//!< How hard the enemy reacts (was "aggression"); sets QRF strength
}
