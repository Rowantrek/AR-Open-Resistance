//! Session-local strategic request. Holds no entity references or ownership/resource state.
class AROR_Operation
{
	protected string m_sId;
	protected AROR_EFactionRole m_eRole;
	protected AROR_EOperationType m_eType;
	protected string m_sTargetLocationId;
	protected AROR_EOperationState m_eState;
	protected int m_iDueTime;
	protected int m_iStateSince;
	protected string m_sResult;
	protected string m_sForceProfileId;
	protected int m_iResponseLevel;
	protected string m_sRecipeId;
	protected int m_iRecipePackages;
	ref AROR_QRFTacticalCommander m_TacticalCommander;
	ref AROR_QRFFunding m_Funding;
	int m_iRequestedTier;
	//! Ordinary campaign selection only; debug/scripted requests retain their explicit family.
	int m_iResponseMixBucket = -1;
	bool m_bResponseMixTracked;
	bool m_bResponseMixAirborne;
	bool m_bResponseMixCounted;
	//! Equipment availability is frozen alongside requested intensity before recipe selection.
	int m_iPoolReadinessLevel;
	int m_iThreatPriority;
	int m_iRecoveredPackages;
	bool m_bCaptureThreatRecorded;
	int m_iCaptureReportedAt;
	int m_iPlanningRetries;
	string m_sRejectedOrigins;
	//! Explicit debug selections never mutate campaign Readiness/Crackdown or fixture settings.
	bool m_bDebugRequest;
	bool m_bUseResponseMetrics;
	string m_sRequestedProfileId;
	string m_sRequestedRecipeId;
	//! Bounded preparation allowance for incremental air landing-site planning plus boarding.
	int m_iSpawnPreparationTimeout;
	//! Scripted scenario responses (e.g. a stronghold's defence) are paid by the scenario, not the faction bank.
	bool m_bFundingWaived;
	//! Fixed launch point used instead of a friendly military origin (e.g. a beach landing).
	bool m_bHasOriginOverride;
	vector m_vOriginOverride;
	//! Always hold half the packages back as a second wave, whatever the scenario's wave settings.
	bool m_bForceTwoWaves;
	//! Estimated seconds from dispatch until the first package reaches the objective (-1 unknown), and the
	//! scheduler clock when it was estimated. Drives the dispatch headline and map marker.
	int m_iEtaSeconds = -1;
	int m_iEtaSetAt;

	//! Seconds of the arrival estimate still to run at the given scheduler clock, or -1 when there is none
	int GetEtaRemaining(int now)
	{
		if (m_iEtaSeconds < 0)
			return -1;
		return Math.Max(0, m_iEtaSeconds - (now - m_iEtaSetAt));
	}

	bool UsesTacticalResponse()
	{
		return m_eType == AROR_EOperationType.QRF || m_eType == AROR_EOperationType.ATTACK;
	}

	string GetRecipeId() { return m_sRecipeId; }
	int GetPackageCount()
	{
		if (!m_sRecipeId.IsEmpty())
			return m_iRecipePackages;
		return m_iResponseLevel;
	}

	bool BindGroundRecipe_S(string recipeId, int level, int packages)
	{
		if (!Replication.IsServer() || m_eState != AROR_EOperationState.SPAWNING)
			return false;
		if (recipeId.IsEmpty() || !m_sRecipeId.IsEmpty() || !m_sForceProfileId.IsEmpty())
			return false;
		if (level < 1 || level > 5 || packages < 1 || packages > 8)
			return false;
		m_sRecipeId = recipeId;
		m_iResponseLevel = level;
		m_iRecipePackages = packages;
		return true;
	}
	protected ref AROR_QRFSearchPlan m_QRFSearchPlan;

	AROR_QRFSearchPlan GetQRFSearchPlan()
	{
		return m_QRFSearchPlan;
	}

	int GetResponseLevel()
	{
		return m_iResponseLevel;
	}

	string GetForceProfileId()
	{
		return m_sForceProfileId;
	}

	//! A request selects once; retries must retain its original force profile.
	bool BindForceProfile_S(string profileId, int responseLevel = 1)
	{
		if (!m_sRecipeId.IsEmpty())
			return false;
		if (!Replication.IsServer() || m_eState != AROR_EOperationState.SPAWNING || profileId.IsEmpty() || !m_sForceProfileId.IsEmpty() || responseLevel < 1 || responseLevel > 5)
			return false;
		m_sForceProfileId = profileId;
		m_iResponseLevel = responseLevel;
		return true;
	}

	void AROR_Operation(string id, AROR_EFactionRole role, AROR_EOperationType type, string targetLocationId, int now, int delaySeconds)
	{
		m_sId = id;
		m_eRole = role;
		m_eType = type;
		m_sTargetLocationId = targetLocationId;
		m_eState = AROR_EOperationState.PLANNED;
		m_iDueTime = now + delaySeconds;
		m_iStateSince = now;
		if (UsesTacticalResponse())
			m_QRFSearchPlan = new AROR_QRFSearchPlan();
	}

	string GetId()
	{
		return m_sId;
	}
	AROR_EFactionRole GetRole()
	{
		return m_eRole;
	}
	AROR_EOperationType GetType()
	{
		return m_eType;
	}
	string GetTargetLocationId()
	{
		return m_sTargetLocationId;
	}
	AROR_EOperationState GetState()
	{
		return m_eState;
	}
	int GetDueTime()
	{
		return m_iDueTime;
	}
	int GetStateSince()
	{
		return m_iStateSince;
	}
	string GetResult()
	{
		return m_sResult;
	}

	bool IsTerminal()
	{
		return m_eState == AROR_EOperationState.SUCCEEDED || m_eState == AROR_EOperationState.FAILED || m_eState == AROR_EOperationState.CANCELLED;
	}

	//! Internal manager mutation. Consumers use manager acknowledgement/result APIs so events fire.
	bool Transition_S(AROR_EOperationState nextState, int now, string result)
	{
		if (!Replication.IsServer() || IsTerminal())
			return false;

		bool valid = nextState == AROR_EOperationState.FAILED || nextState == AROR_EOperationState.CANCELLED;
		if (m_eState == AROR_EOperationState.PLANNED && nextState == AROR_EOperationState.SPAWNING)
			valid = true;
		if (m_eState == AROR_EOperationState.SPAWNING && nextState == AROR_EOperationState.ACTIVE)
			valid = true;
		if (m_eState == AROR_EOperationState.ACTIVE && nextState == AROR_EOperationState.SUCCEEDED)
			valid = true;
		if (!valid)
			return false;

		m_eState = nextState;
		m_iStateSince = now;
		m_sResult = result;
		return true;
	}
}
