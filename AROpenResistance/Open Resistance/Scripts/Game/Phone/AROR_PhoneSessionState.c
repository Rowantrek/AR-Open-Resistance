//! Client-only presentation state, owned by the game mode's Phone component until the world closes.
//! Contains no widgets/entities, is never replicated or saved, and grants no gameplay permissions.
class AROR_PhoneSessionState
{
	bool m_bCaptured;
	AROR_EPhoneApp m_eApp = AROR_EPhoneApp.HOME;
	AROR_EHiCommScreen m_eHiCommScreen = AROR_EHiCommScreen.LOGIN;
	int m_iHiCommLoginStart = -1;
	ref array<int> m_aHiCommSelected = {};
	AROR_EHighCommandOrder m_eHiCommOrder = AROR_EHighCommandOrder.MOVE;
	string m_sHiCommTarget;
	vector m_vHiCommMoveTarget;
	bool m_bHiCommMoveTarget;
	bool m_bHiCommGameMap = true;
	bool m_bHiCommOrderOfBattle = true;
	bool m_bHiCommHelp;
	bool m_bHiCommMapView;
	vector m_vHiCommMapCentre;
	float m_fHiCommMapZoom = 1;
	AROR_EOpsScreen m_eOpsScreen = AROR_EOpsScreen.LOGIN;
	int m_iOpsLoginStart = -1;
	string m_sOpsCategory;
	int m_iOpsSelected = -1;
	AROR_EFactionRole m_eRebIntelRole;
	int m_iSquadGroup = -1;
	int m_iSquadPage;
	string m_sSquadName;
	int m_iMarketTab;
	int m_iMarketScroll;
	int m_iMarketCartScroll;
	string m_sMarketQuery;
	bool m_bMarketPayFaction = true;
	ref array<string> m_aMarketCartPrefabs = {};
	ref array<int> m_aMarketCartQuantities = {};
	int m_iImportsTab;
	int m_iImportsScroll;
	AROR_EFactionRole m_eMarsRole = AROR_EFactionRole.OCCUPIERS;
	int m_iMarsType = AROR_EResponseType.COMBINED_ARMS;
	int m_iMarsReadiness = 1;
	int m_iMarsCrackdown = 1;
	int m_iMarsPage;
	bool m_bMarsUnlocked;
	string m_sMarsCredential;
	bool m_bTransferSelection;
	bool m_bTransferToResistance;
	int m_iTransferPlayer;
	string m_sTransferAmount;
}
