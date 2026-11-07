enum ORRadio_EControl
{
	POWER,
	NEXT_STATION,
	VOLUME_UP,
	VOLUME_DOWN
}

//! A control on a vehicle's dashboard, added to every vehicle by the Vehicle_Base override. Runs on the player's own
//! machine and asks the server through the player controller, which checks the player sits in the vehicle.
//! Compiles after ORRadio_PlayerController (file name order), so it can call the method that adds.
class ORRadio_VehicleAction : SCR_VehicleActionBase
{
	[Attribute("0", UIWidgets.ComboBox, desc: "What this control does", enums: ParamEnumArray.FromEnum(ORRadio_EControl))]
	protected ORRadio_EControl m_eControl;

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

		ORRadio_ManagerComponent manager = ORRadio_ManagerComponent.GetInstance();
		if (!manager || manager.GetStations().IsEmpty())
			return false;

		if (m_eControl == ORRadio_EControl.POWER)
			return true;

		if (manager.GetVehicleStation(SeatedVehicle(user)) < 0)
			return false;

		return m_eControl != ORRadio_EControl.NEXT_STATION || manager.GetStations().Count() > 1;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return SeatedVehicle(user).IsValid();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		ORRadio_ManagerComponent manager = ORRadio_ManagerComponent.GetInstance();
		if (!manager)
			return false;

		RplId vehicleId = SCR_EntityHelper.EntityToRplId(GetOwner().GetRootParent());
		int station = manager.GetVehicleStation(vehicleId);
		switch (m_eControl)
		{
			case ORRadio_EControl.POWER:
				outName = "Turn radio on";
				if (station >= 0)
					outName = "Turn radio off";
				break;

			case ORRadio_EControl.NEXT_STATION:
				outName = "Radio: next station";
				break;

			case ORRadio_EControl.VOLUME_UP:
				outName = string.Format("Radio: volume up (%1)", manager.GetVehicleVolume(vehicleId));
				break;

			case ORRadio_EControl.VOLUME_DOWN:
				outName = string.Format("Radio: volume down (%1)", manager.GetVehicleVolume(vehicleId));
				break;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		ORRadio_ManagerComponent manager = ORRadio_ManagerComponent.GetInstance();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		RplId vehicleId = SeatedVehicle(pUserEntity);
		if (!manager || !controller || !vehicleId.IsValid())
			return;

		int station = manager.GetVehicleStation(vehicleId);
		switch (m_eControl)
		{
			case ORRadio_EControl.POWER:
				if (station >= 0)
				{
					controller.ORRadio_Request(vehicleId, -1, -1);
					break;
				}

				controller.ORRadio_Request(vehicleId, 0, -1);
				AnnounceStation(manager, 0);
				break;

			case ORRadio_EControl.NEXT_STATION:
				if (station < 0)
					break;

				station = (station + 1) % manager.GetStations().Count();
				controller.ORRadio_Request(vehicleId, station, -1);
				AnnounceStation(manager, station);
				break;

			case ORRadio_EControl.VOLUME_UP:
				if (station >= 0)
					controller.ORRadio_Request(vehicleId, station, manager.GetVehicleVolume(vehicleId) + 1);
				break;

			case ORRadio_EControl.VOLUME_DOWN:
				if (station >= 0)
					controller.ORRadio_Request(vehicleId, station, manager.GetVehicleVolume(vehicleId) - 1);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The station name and what it is playing, as a hint to the player who tuned it
	protected void AnnounceStation(ORRadio_ManagerComponent manager, int station)
	{
		array<ref ORRadio_Station> stations = manager.GetStations();
		if (!stations.IsIndexValid(station))
			return;

		string text = stations[station].m_sName;
		ORRadio_Track current, upcoming;
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
