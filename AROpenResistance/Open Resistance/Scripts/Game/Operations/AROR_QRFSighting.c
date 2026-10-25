//! Server report: coordinates stop changing when nobody observes the asset.
class AROR_QRFSighting
{
	IEntity m_Entity;
	string m_sId;
	int m_iKind;
	int m_iSide;
	vector m_vPosition;
	int m_iLastSeen;
	int m_iObserver;
	int m_iFirstLook;
	int m_iLastLook;
	bool m_bConfirmed;
}
