enum AROR_ERadioControl
{
	POWER,
	NEXT_STATION,
	VOLUME_UP,
	VOLUME_DOWN
}

//! A control on a vehicle's dashboard, added to every vehicle by the Vehicle_Base override. Runs on the player's own
//! machine and asks the server through the player controller, which checks the player sits in the vehicle.
//! Compiles after AROR_RadioPlayerController (file name order), so it can call the method that adds.
class AROR_RadioVehicleAction : SCR_VehicleActionBase
{
	[Attribute("0", UIWidgets.ComboBox, desc: "What this control does", enums: ParamEnumArray.FromEnum(AROR_ERadioControl))]
	protected AROR_ERadioControl m_eControl;

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		AROR_RadioManagerComponent manager = AROR_RadioManagerComponent.GetInstance();
		if (!manager || manager.GetStations().IsEmpty())
			return false;

		if (m_eControl == AROR_ERadioControl.POWER)
			return true;

		if (manager.GetVehicleStation(SeatedVehicle(user)) < 0)
			return false;

		return m_eControl != AROR_ERadioControl.NEXT_STATION || manager.GetStations().Count() > 1;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return SeatedVehicle(user).IsValid();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		AROR_RadioManagerComponent manager = AROR_RadioManagerComponent.GetInstance();
		if (!manager)
			return false;

		RplId vehicleId = SCR_EntityHelper.EntityToRplId(GetOwner().GetRootParent());
		int station = manager.GetVehicleStation(vehicleId);
		switch (m_eControl)
		{
			case AROR_ERadioControl.POWER:
				outName = "Turn radio on";
				if (station >= 0)
					outName = "Turn radio off";
				break;

			case AROR_ERadioControl.NEXT_STATION:
				outName = "Radio: next station";
				break;

			case AROR_ERadioControl.VOLUME_UP:
				outName = string.Format("Radio: volume up (%1)", manager.GetVehicleVolume(vehicleId));
				break;

			case AROR_ERadioControl.VOLUME_DOWN:
				outName = string.Format("Radio: volume down (%1)", manager.GetVehicleVolume(vehicleId));
				break;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		AROR_RadioManagerComponent manager = AROR_RadioManagerComponent.GetInstance();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		RplId vehicleId = SeatedVehicle(pUserEntity);
		if (!manager || !controller || !vehicleId.IsValid())
			return;

		int station = manager.GetVehicleStation(vehicleId);
		switch (m_eControl)
		{
			case AROR_ERadioControl.POWER:
				if (station >= 0)
				{
					controller.AROR_RequestRadio(vehicleId, -1, -1);
					break;
				}

				controller.AROR_RequestRadio(vehicleId, 0, -1);
				AnnounceStation(manager, 0);
				break;

			case AROR_ERadioControl.NEXT_STATION:
				if (station < 0)
					break;

				station = (station + 1) % manager.GetStations().Count();
				controller.AROR_RequestRadio(vehicleId, station, -1);
				AnnounceStation(manager, station);
				break;

			case AROR_ERadioControl.VOLUME_UP:
				if (station >= 0)
					controller.AROR_RequestRadio(vehicleId, station, manager.GetVehicleVolume(vehicleId) + 1);
				break;

			case AROR_ERadioControl.VOLUME_DOWN:
				if (station >= 0)
					controller.AROR_RequestRadio(vehicleId, station, manager.GetVehicleVolume(vehicleId) - 1);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The station name and what it is playing, as a hint to the player who tuned it
	protected void AnnounceStation(AROR_RadioManagerComponent manager, int station)
	{
		array<ref AROR_RadioStation> stations = manager.GetStations();
		if (!stations.IsIndexValid(station))
			return;

		string text = stations[station].m_sName;
		AROR_RadioTrack current, upcoming;
		int offsetMs;
		if (manager.GetNowPlaying(station, current, offsetMs, upcoming))
			text = string.Format("%1: %2", text, current.m_sTitle);

		AROR_NotificationManagerComponent.ShowHint(text);
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle the character sits in, when it is this action's vehicle
	protected RplId SeatedVehicle(IEntity user)
	{
		if (!user)
			return RplId.Invalid();

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(user.FindComponent(CompartmentAccessComponent));
		if (!access || !access.GetCompartment())
			return RplId.Invalid();

		IEntity vehicle = access.GetCompartment().GetVehicle();
		if (!vehicle || vehicle != GetOwner().GetRootParent())
			return RplId.Invalid();

		return SCR_EntityHelper.EntityToRplId(vehicle);
	}
}
