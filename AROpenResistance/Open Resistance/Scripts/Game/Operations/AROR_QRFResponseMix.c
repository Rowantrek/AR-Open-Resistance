//! Running dispatch mix, independent of force size, finance, and native execution.
class AROR_QRFResponseMix
{
	static bool IsCampaignQRF(AROR_Operation operation)
	{
		return operation && operation.GetType() == AROR_EOperationType.QRF
			&& !operation.m_bDebugRequest && !operation.m_bFundingWaived
			&& operation.m_sRequestedProfileId.IsEmpty() && operation.m_sRequestedRecipeId.IsEmpty();
	}

	static bool PreferAir(int dispatched, int airborne, int percent)
	{
		if (!CountsValid(dispatched, airborne) || percent < 0 || percent > 100)
			return false;
		// Nearest attainable cumulative share; at 30%, the first dispatch stays ground.
		return airborne < Math.Round((dispatched + 1) * (percent / 100.0));
	}

	static bool CountsValid(int dispatched, int airborne)
	{
		return dispatched >= 0 && dispatched <= 1000000000 && airborne >= 0 && airborne <= dispatched;
	}
}
