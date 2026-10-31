//! Browse dialog for Resistance Garage retrieve (preset aror_garage_browse).
class AROR_GarageBrowseDialog : SCR_ConfigurableDialogUi
{
	protected string m_sPressedTag;

	//------------------------------------------------------------------------------------------------
	void Refresh()
	{
		SetMessage(AROR_GarageRetrieveUI.BuildBrowseMessage());
		SetButtonEnabled("previous", AROR_GarageRetrieveUI.CanStepPrevious());
		SetButtonEnabled("next", AROR_GarageRetrieveUI.CanStepNext());
		SetButtonEnabled("spawn", AROR_GarageRetrieveUI.GetSelectedEntryId() > 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetButtonEnabled(string tag, bool enabled)
	{
		SCR_InputButtonComponent button = FindButton(tag);
		if (button)
			button.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnButtonPressed(SCR_InputButtonComponent button)
	{
		super.OnButtonPressed(button);

		if (!m_sPressedTag.IsEmpty())
			return;

		string tag = GetButtonTag(button);
		if (tag == "previous" || tag == "next")
		{
			if (tag == "previous")
				AROR_GarageRetrieveUI.StepIndex(-1);
			else
				AROR_GarageRetrieveUI.StepIndex(1);

			Refresh();
			return;
		}

		m_sPressedTag = tag;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		super.OnMenuClose();

		if (m_sPressedTag.IsEmpty())
			return;

		AROR_GarageRetrieveUI scheduler = AROR_GarageRetrieveUI.GetScheduler();
		if (m_sPressedTag == "spawn")
			GetGame().GetCallqueue().CallLater(scheduler.RequestSpawnPlacement_I, 0, false);
		else if (m_sPressedTag == "cancel")
			AROR_GarageRetrieveUI.CloseAll();
	}
}

//! Cargo list dialog (preset aror_garage_inventory). Back returns to browse.
class AROR_GarageInventoryDialog : SCR_ConfigurableDialogUi
{
	protected string m_sPressedTag;

	//------------------------------------------------------------------------------------------------
	override protected void OnButtonPressed(SCR_InputButtonComponent button)
	{
		super.OnButtonPressed(button);

		if (!m_sPressedTag.IsEmpty())
			return;

		m_sPressedTag = GetButtonTag(button);
		Close();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		super.OnMenuClose();

		if (m_sPressedTag == "back" || m_sPressedTag.IsEmpty())
			GetGame().GetCallqueue().CallLater(AROR_GarageRetrieveUI.GetScheduler().OpenBrowseDialog_I, 0, false);
	}
}
