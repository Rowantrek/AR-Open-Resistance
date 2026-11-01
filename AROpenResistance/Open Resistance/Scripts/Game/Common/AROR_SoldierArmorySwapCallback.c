//! Held by the armory until the native inventory transaction completes; the owner pointer is weak.
class AROR_SoldierArmorySwapCallback : ScriptedInventoryOperationCallback
{
	AROR_SoldierArmory m_Armory;
	IEntity m_Character;
	SCR_InventoryStorageManagerComponent m_Inventory;
	IEntity m_OldWeapon;
	IEntity m_NewWeapon;
	ref BaseMagazineWell m_OldWell;
	ResourceName m_sNewMagazine;
	bool m_bSameMagazines;

	override protected void OnComplete()
	{
		if (m_Armory)
			m_Armory.FinishSwap_S(this, true);
	}

	override protected void OnFailed()
	{
		if (m_Armory)
			m_Armory.FinishSwap_S(this, false);
	}
}
