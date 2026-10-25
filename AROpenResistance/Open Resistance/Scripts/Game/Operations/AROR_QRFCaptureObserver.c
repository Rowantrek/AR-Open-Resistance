//! Session-only change detection on canonical location events; never owns or changes territory.
class AROR_QRFCaptureObserver
{
	protected AROR_OperationManagerComponent m_Operations;
	protected AROR_LocationManagerComponent m_Locations;
	protected ref array<ref AROR_QRFObservedControl> m_aObserved = {};

	bool Initialize_S(AROR_OperationManagerComponent operations)
	{
		if (!Replication.IsServer() || !operations)
			return false;
		m_Locations = AROR_LocationManagerComponent.GetInstance();
		if (!m_Locations)
			return false;
		m_Operations = operations;
		array<AROR_LocationComponent> locations = {};
		m_Locations.GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
			OnRegistered(location);
		m_Locations.GetOnLocationRegistered().Insert(OnRegistered);
		m_Locations.GetOnLocationUnregistered().Insert(OnUnregistered);
		m_Locations.GetOnLocationControlChanged().Insert(OnControlChanged);
		return true;
	}
	protected void OnRegistered(AROR_LocationComponent location)
	{
		if (!location)
			return;
		AROR_ELocationType type = location.GetLocationType();
		if (type != AROR_ELocationType.TOWN && type != AROR_ELocationType.CITY && !AROR_CaptureLocationAction.IsCapturableType(type))
			return;
		OnUnregistered(location);
		AROR_QRFObservedControl observed = new AROR_QRFObservedControl();
		observed.m_sId = location.GetId();
		observed.m_eRole = location.GetControllingRole();
		m_aObserved.Insert(observed);
	}
	protected void OnUnregistered(AROR_LocationComponent location)
	{
		if (!location)
			return;
		for (int index = m_aObserved.Count() - 1; index >= 0; index--)
		{
			if (m_aObserved[index].m_sId == location.GetId())
				m_aObserved.Remove(index);
		}
	}
	protected void OnControlChanged(AROR_LocationComponent location)
	{
		if (!Replication.IsServer() || !location || !m_Operations)
			return;
		foreach (AROR_QRFObservedControl observed : m_aObserved)
		{
			if (observed.m_sId != location.GetId())
				continue;
			AROR_EFactionRole previous = observed.m_eRole;
			observed.m_eRole = location.GetControllingRole();
			if (previous != observed.m_eRole)
				m_Operations.ReportLocationCapture_S(previous, location.GetId());
			return;
		}
		// A newly restored/registered location establishes a baseline, not a historical attack.
		OnRegistered(location);
	}
	void Detach_S()
	{
		if (m_Locations)
		{
			m_Locations.GetOnLocationRegistered().Remove(OnRegistered);
			m_Locations.GetOnLocationUnregistered().Remove(OnUnregistered);
			m_Locations.GetOnLocationControlChanged().Remove(OnControlChanged);
		}
		m_aObserved.Clear();
		m_Locations = null;
		m_Operations = null;
	}
}

class AROR_QRFObservedControl
{
	string m_sId;
	AROR_EFactionRole m_eRole;
}
