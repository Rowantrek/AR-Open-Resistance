//! Resistance arsenals reuse the supply-cost badge to show stock quantity instead of supply cost.
modded class SCR_ArsenalInventorySlotUI
{
	//------------------------------------------------------------------------------------------------
	override void SetSlotVisible(bool bVisible)
	{
		super.SetSlotVisible(bVisible);
		TryShowResistanceStockQuantity();
	}

	//------------------------------------------------------------------------------------------------
	override void Refresh()
	{
		super.Refresh();
		TryShowResistanceStockQuantity();
	}

	//------------------------------------------------------------------------------------------------
	//! After vanilla hides the cost (supplies off), put qty back on that widget.
	protected void TryShowResistanceStockQuantity()
	{
		if (!m_widget || !m_pItem || !m_pItem.GetOwner())
			return;

		SCR_InventoryStorageBaseUI storageUI = GetStorageUI();
		if (!storageUI)
			return;

		BaseInventoryStorageComponent storage = storageUI.GetCurrentNavigationStorage();
		if (!storage || !storage.GetOwner())
			return;

		if (!AROR_ResistanceArsenalComponent.IsResistanceArsenal(storage.GetOwner()))
			return;

		if (!m_CostResourceHolder)
			m_CostResourceHolder = m_widget.FindAnyWidget(COST_RESOURCES_WIDGET_NAME);

		if (m_CostResourceHolder && !m_CostResourceHolderText)
			m_CostResourceHolderText = TextWidget.Cast(m_CostResourceHolder.FindAnyWidget(RESOURCES_TEXT_WIDGET_NAME));

		if (!m_CostResourceHolder || !m_CostResourceHolderText)
			return;

		ResourceName prefab;
		EntityPrefabData prefabData = m_pItem.GetOwner().GetPrefabData();
		if (prefabData)
			prefab = prefabData.GetPrefabName();

		int qty = 0;
		AROR_ResistanceStorageComponent resistanceStorage = AROR_ResistanceStorageComponent.GetInstance();
		if (resistanceStorage && !prefab.IsEmpty())
			qty = resistanceStorage.GetQuantity(prefab);

		m_CostResourceHolder.SetVisible(true);
		m_CostResourceHolderText.SetText(qty.ToString());
		SetItemAvailability(qty > 0);
	}
}
