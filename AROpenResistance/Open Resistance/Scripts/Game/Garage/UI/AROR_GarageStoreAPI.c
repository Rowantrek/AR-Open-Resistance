//! Client entry point to garage the vehicle under the local character's view (see ARCHITECTURE.md).
//! Triggered from the phone home quick dock (slot 0).
class AROR_GarageStoreAPI
{
	//------------------------------------------------------------------------------------------------
	//! Aim from the player camera center (max store-look distance), require near a Resistance HQ/capturable site.
	static void TryStoreLookedAtVehicle()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (!garage)
		{
			AROR_NotificationManagerComponent.ShowHint("Garage unavailable.");
			return;
		}

		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		IEntity controlled = playerController.GetControlledEntity();
		if (!controlled)
		{
			AROR_NotificationManagerComponent.ShowHint("No controlled character.");
			return;
		}

		if (!garage.IsNearFriendlyGarageSite(controlled.GetOrigin()))
		{
			AROR_NotificationManagerComponent.ShowHint("Must be near a Resistance HQ or military site.");
			return;
		}

		IEntity vehicle;
		string failReason;
		if (!FindLookedAtVehicle(controlled, garage.GetStoreLookDistance(), vehicle, failReason))
		{
			AROR_NotificationManagerComponent.ShowHint(failReason);
			return;
		}

		RplId vehicleId = SCR_EntityHelper.EntityToRplId(vehicle);
		if (!vehicleId.IsValid())
		{
			AROR_NotificationManagerComponent.ShowHint("Vehicle cannot be networked.");
			return;
		}

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		pc.AROR_RequestGarageStore(vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	static void OnStoreResultReceived(bool success, string reason)
	{
		if (!reason.IsEmpty())
			AROR_NotificationManagerComponent.ShowHint(reason);
		else if (success)
			AROR_NotificationManagerComponent.ShowHint("Vehicle stored.");
		else
			AROR_NotificationManagerComponent.ShowHint("Store failed.");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FindLookedAtVehicle(IEntity controlled, float maxDistance, out IEntity vehicle, out string failReason)
	{
		vehicle = null;
		failReason = "No vehicle in view.";

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return false;

		CameraBase camera = cameraManager.CurrentCamera();
		if (!camera)
			return false;

		vector hitPos;
		// Prefer an explicit short ray so range is strictly maxDistance (camera cursor target can be longer)
		IEntity hit = TraceLookRay(camera, maxDistance, controlled, hitPos);
		if (!hit)
		{
			hit = camera.GetCursorTargetWithPosition(hitPos);
			if (!hit)
				return false;

			if (vector.Distance(controlled.GetOrigin(), hitPos) > maxDistance
				&& vector.Distance(controlled.GetOrigin(), hit.GetOrigin()) > maxDistance)
			{
				failReason = "Look farther than store range.";
				return false;
			}
		}

		vehicle = FindVehicleInHierarchy(hit);
		if (!vehicle)
		{
			failReason = "Not looking at a vehicle.";
			return false;
		}

		if (vector.Distance(controlled.GetOrigin(), vehicle.GetOrigin()) > maxDistance + 1.5)
		{
			failReason = "Vehicle is too far away.";
			vehicle = null;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity TraceLookRay(CameraBase camera, float maxDistance, IEntity exclude, out vector hitPos)
	{
		hitPos = vector.Zero;

		vector mat[4];
		camera.GetWorldCameraTransform(mat);
		vector start = mat[3];
		vector forward = mat[2];
		forward.Normalize();

		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = start + forward * maxDistance;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		trace.Exclude = exclude;

		BaseWorld world = GetGame().GetWorld();
		float fraction = world.TraceMove(trace, null);
		if (fraction <= 0 || fraction >= 1 || !trace.TraceEnt)
			return null;

		hitPos = start + forward * (maxDistance * fraction);
		return trace.TraceEnt;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindVehicleInHierarchy(IEntity entity)
	{
		IEntity current = entity;
		for (int depth = 0; current && depth < 16; depth++)
		{
			if (AROR_GarageSnapshotHelper.IsVehicle(current))
				return current;

			current = current.GetParent();
		}

		return null;
	}
}
