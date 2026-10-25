//! Shared location-type gate for campaign QRF dispatch/return origins.
//! Military outposts use the existing MILITARY_BASE type; ordinary ones use OUTPOST.
class AROR_QRFOriginPolicy
{
	static bool IsOriginType(AROR_ELocationType type)
	{
		return type == AROR_ELocationType.OUTPOST || type == AROR_ELocationType.MILITARY_BASE || type == AROR_ELocationType.AIRPORT;
	}
}
