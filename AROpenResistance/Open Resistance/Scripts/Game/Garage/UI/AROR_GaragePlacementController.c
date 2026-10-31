//! Client-only ghost placement for garage retrieve (overhead view: move, rotate, place/cancel).
//! Selection already happened in the browse dialog — no build catalogue.
class AROR_GaragePlacementController
{
	protected static const ResourceName OVERHEAD_CAMERA_PREFAB = "{D6DE32D1C0FCC1C7}Prefabs/Editor/Camera/ManualCameraBase.et";
	protected static const float OVERHEAD_HEIGHT = 28;
	protected static const float OVERHEAD_PITCH = -85;
	protected static const float OVERHEAD_FOV = 40;
	//! Metres of ground travel per unscaled screen pixel of mouse delta (absolute raycast felt far too fast)
	protected static const float MOUSE_METERS_PER_PIXEL = 0.025;

	protected static bool s_bActive;
	protected static int s_iEntryId = -1;
	protected static ResourceName s_sPrefab;
	protected static IEntity s_Anchor;
	protected static RplId s_AnchorRplId;
	protected static IEntity s_Ghost;
	protected static float s_fYaw;
	protected static bool s_bValid;
	protected static vector s_vPlacePos;
	protected static bool s_bPlaceHeld;
	protected static bool s_bCancelHeld;
	protected static bool s_bWasValid;
	protected static bool s_bMouseSeeded;
	protected static float s_fLastMouseX;
	protected static float s_fLastMouseY;

	protected static SCR_ManualCamera s_OverheadCamera;
	protected static CameraBase s_PlayerCamera;

	//------------------------------------------------------------------------------------------------
	static bool IsActive()
	{
		return s_bActive;
	}

	//------------------------------------------------------------------------------------------------
	static void Start(int entryId, ResourceName prefab, IEntity anchor, RplId anchorRplId)
	{
		if (s_bActive)
			Stop();

		if (entryId <= 0 || prefab.IsEmpty() || !anchor)
			return;

		s_bActive = true;
		s_iEntryId = entryId;
		s_sPrefab = prefab;
		s_Anchor = anchor;
		s_AnchorRplId = anchorRplId;
		s_fYaw = 0;
		s_bValid = false;
		s_bWasValid = false;
		s_bPlaceHeld = false;
		s_bCancelHeld = false;
		s_bMouseSeeded = false;
		s_vPlacePos = anchor.GetOrigin();
		s_vPlacePos[1] = GetGame().GetWorld().GetSurfaceY(s_vPlacePos[0], s_vPlacePos[2]);

		SpawnGhost();
		StartOverheadCamera();
		AROR_NotificationManagerComponent.ShowHint("Space place · Esc/RMB cancel · Q/E or Shift+Wheel rotate");
		GetGame().GetCallqueue().CallLater(Tick, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		GetGame().GetCallqueue().Remove(Tick);
		StopOverheadCamera();
		DeleteGhost();
		s_bActive = false;
		s_iEntryId = -1;
		ResourceName emptyPrefab;
		s_sPrefab = emptyPrefab;
		s_Anchor = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SpawnGhost()
	{
		DeleteGhost();

		Resource resource = Resource.Load(s_sPrefab);
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = s_Anchor.GetOrigin();

		// Local-only ghost (not replicated). Editor preview APIs are editor-scoped; this stays player-safe.
		s_Ghost = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DeleteGhost()
	{
		if (!s_Ghost)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(s_Ghost);
		s_Ghost = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartOverheadCamera()
	{
		StopOverheadCamera();

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return;

		s_PlayerCamera = cameraManager.CurrentCamera();
		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
		{
			CameraBase playerCam = pc.GetPlayerCamera();
			if (playerCam)
				s_PlayerCamera = playerCam;
		}

		Resource resource = Resource.Load(OVERHEAD_CAMERA_PREFAB);
		if (!resource || !resource.IsValid())
			return;

		vector focus = s_vPlacePos;
		if (s_Anchor)
			focus = s_Anchor.GetOrigin();

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = focus + Vector(0, OVERHEAD_HEIGHT, 0);

		s_OverheadCamera = SCR_ManualCamera.Cast(GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params));
		if (!s_OverheadCamera)
			return;

		s_OverheadCamera.SetName("AROR_GaragePlaceCam");
		s_OverheadCamera.SetInputEnabled(false);
		s_OverheadCamera.SetVerticalFOV(OVERHEAD_FOV);
		UpdateOverheadCamera(focus);
		cameraManager.SetCamera(s_OverheadCamera);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopOverheadCamera()
	{
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (s_OverheadCamera)
		{
			if (cameraManager)
			{
				if (s_PlayerCamera)
					cameraManager.SetCamera(s_PlayerCamera);
				else
					s_OverheadCamera.SwitchToPreviousCamera();
			}

			s_OverheadCamera.Terminate();
			SCR_EntityHelper.DeleteEntityAndChildren(s_OverheadCamera);
			s_OverheadCamera = null;
		}

		s_PlayerCamera = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void UpdateOverheadCamera(vector focus)
	{
		if (!s_OverheadCamera)
			return;

		s_OverheadCamera.SetOrigin(Vector(focus[0], focus[1] + OVERHEAD_HEIGHT, focus[2]));
		s_OverheadCamera.SetAngles(Vector(OVERHEAD_PITCH, 0, 0));
		s_OverheadCamera.SetVerticalFOV(OVERHEAD_FOV);

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager && cameraManager.CurrentCamera() != s_OverheadCamera)
			cameraManager.SetCamera(s_OverheadCamera);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_bActive)
		{
			GetGame().GetCallqueue().Remove(Tick);
			return;
		}

		UpdateCursorTransform();
		HandleInput();
	}

	//------------------------------------------------------------------------------------------------
	protected static void UpdateCursorTransform()
	{
		ApplyMouseDeltaToPlacePos();
		UpdateOverheadCamera(s_vPlacePos);

		float radius = 50;
		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (garage)
			radius = garage.GetPlaceRadius();

		string reason;
		bool inRange = s_Anchor && vector.DistanceXZ(s_vPlacePos, s_Anchor.GetOrigin()) <= radius;
		bool clear = garage && garage.IsPlacementClear(s_vPlacePos, reason, s_Ghost);
		s_bValid = inRange && clear;

		if (s_bValid != s_bWasValid)
		{
			s_bWasValid = s_bValid;
			if (!s_bValid)
			{
				if (!inRange)
					AROR_NotificationManagerComponent.ShowHint("Too far from garage");
				else if (!reason.IsEmpty())
					AROR_NotificationManagerComponent.ShowHint(reason);
				else
					AROR_NotificationManagerComponent.ShowHint("Invalid placement");
			}
		}

		if (!s_Ghost)
			return;

		vector mat[4];
		Math3D.AnglesToMatrix(Vector(s_fYaw, 0, 0), mat);
		mat[3] = s_vPlacePos;
		s_Ghost.SetTransform(mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Relative mouse pan (not screen-ray) so sensitivity stays usable at bird's-eye height
	protected static void ApplyMouseDeltaToPlacePos()
	{
		ArmaReforgerScripted game = GetGame();
		if (!game)
			return;

		WorkspaceWidget workspace = game.GetWorkspace();
		BaseWorld world = game.GetWorld();
		if (!workspace || !world)
			return;

		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);
		float sx = workspace.DPIUnscale(mouseX);
		float sy = workspace.DPIUnscale(mouseY);

		if (!s_bMouseSeeded)
		{
			s_fLastMouseX = sx;
			s_fLastMouseY = sy;
			s_bMouseSeeded = true;
			return;
		}

		float dx = sx - s_fLastMouseX;
		float dy = sy - s_fLastMouseY;
		s_fLastMouseX = sx;
		s_fLastMouseY = sy;

		if (dx == 0 && dy == 0)
			return;

		// Top-down: screen +X → world +X; screen +Y (down) → world −Z (north is −screen Y)
		s_vPlacePos[0] = s_vPlacePos[0] + dx * MOUSE_METERS_PER_PIXEL;
		s_vPlacePos[2] = s_vPlacePos[2] - dy * MOUSE_METERS_PER_PIXEL;

		float radius = 50;
		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (garage)
			radius = garage.GetPlaceRadius();

		if (s_Anchor && radius > 0)
		{
			vector anchor = s_Anchor.GetOrigin();
			vector delta = Vector(s_vPlacePos[0] - anchor[0], 0, s_vPlacePos[2] - anchor[2]);
			float dist = delta.Length();
			if (dist > radius && dist > 0)
			{
				delta = delta * (radius / dist);
				s_vPlacePos[0] = anchor[0] + delta[0];
				s_vPlacePos[2] = anchor[2] + delta[2];
			}
		}

		s_vPlacePos[1] = world.GetSurfaceY(s_vPlacePos[0], s_vPlacePos[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected static void HandleInput()
	{
		bool shift = Debug.KeyState(KeyCode.KC_LSHIFT) || Debug.KeyState(KeyCode.KC_RSHIFT);
		InputManager input = GetGame().GetInputManager();
		if (input && shift)
		{
			float wheel = input.GetActionValue("MouseWheel");
			if (wheel != 0)
				s_fYaw = WrapYaw(s_fYaw + wheel * 15);
		}

		if (Debug.KeyState(KeyCode.KC_Q))
			s_fYaw = WrapYaw(s_fYaw - 2);
		if (Debug.KeyState(KeyCode.KC_E))
			s_fYaw = WrapYaw(s_fYaw + 2);

		bool placeDown = IsConfirmDown();
		bool cancelDown = IsCancelDown();

		if (placeDown && !s_bPlaceHeld)
		{
			s_bPlaceHeld = true;
			TryConfirmPlace();
		}
		else if (!placeDown)
		{
			s_bPlaceHeld = false;
		}

		if (cancelDown && !s_bCancelHeld)
		{
			s_bCancelHeld = true;
			Cancel();
		}
		else if (!cancelDown)
		{
			s_bCancelHeld = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static float WrapYaw(float yaw)
	{
		while (yaw < 0)
			yaw += 360;
		while (yaw >= 360)
			yaw -= 360;
		return yaw;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsConfirmDown()
	{
		if (Debug.KeyState(KeyCode.KC_SPACE))
		{
			Debug.ClearKey(KeyCode.KC_SPACE);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsCancelDown()
	{
		if (Debug.KeyState(KeyCode.KC_ESCAPE))
		{
			Debug.ClearKey(KeyCode.KC_ESCAPE);
			return true;
		}

		InputManager input = GetGame().GetInputManager();
		if (input && (input.GetActionTriggered("MouseRight") || input.GetActionTriggered("MenuBack")))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryConfirmPlace()
	{
		if (!s_bValid)
		{
			AROR_NotificationManagerComponent.ShowHint("Cannot place here (need clear ground within range).");
			return;
		}

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		int entryId = s_iEntryId;
		vector pos = s_vPlacePos;
		float yaw = s_fYaw;
		RplId anchorId = s_AnchorRplId;
		Stop();
		pc.AROR_RequestGarageRetrieve(entryId, pos, yaw, anchorId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cancel()
	{
		Stop();
		AROR_GarageRetrieveUI.OnPlacementCancelled();
	}
}
