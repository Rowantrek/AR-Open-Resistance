// Mutation checks prove selected regressions are caught; source files are never edited.
const cases=[
 {name:'airborne share ignores running history',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_QRFResponseMix.c',suite:'Test-QRFResponseMix.js',
  mutate:s=>s.replace('return airborne < Math.Round((dispatched + 1) * (percent / 100.0));','return false;')},
 {name:'dispatch history counts duplicate ACTIVE callbacks',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationManagerComponent.c',suite:'Test-QRFResponseMix.js',
  mutate:s=>s.replace(' || operation.m_bResponseMixCounted','')},
 {name:'airborne construction loses its mix classification',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('operation.m_bResponseMixAirborne = true;','operation.m_bResponseMixAirborne = false;')},
 {name:'new campaigns start above tier one',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationManagerComponent.c',suite:'Test-QRFProgression.js',
  mutate:s=>s.replace('[Attribute("0", desc: "Initial crackdown percentage','[Attribute("20", desc: "Initial crackdown percentage')},
 {name:'capture escalation returns to early maximum intensity',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationManagerComponent.c',suite:'Test-QRFProgression.js',
  mutate:s=>s.replace('[Attribute("50", desc: "Crackdown points per newly recorded capture loss','[Attribute("150", desc: "Crackdown points per newly recorded capture loss')},
 {name:'linear recipe adds two packages in one tier',file:'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer',suite:'Test-QRFProgression.js',
  mutate:s=>s.replace(/(m_sId "occupier_light_l4"[\s\S]*?m_aProfileIds \{)/,'$1\n      "us_motorized"')},
 {name:'air passengers race for one unreserved seat',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperAirForce.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace('Seat(passenger, passengerSeats[passengerIndex]);','Seat(passenger, passengerSeats[0]);')},
 {name:'ground crew retry races for the first free seat',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c',suite:'Test-QRFBoarding.js',
  mutate:s=>s.replace('array<BaseCompartmentSlot> assignedSeats = {assigned};','array<BaseCompartmentSlot> assignedSeats = crewSeats;')},
 {name:'global spawn timeout cuts short air preparation',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace(' + level + m_iReaperPreparationTimeout',' + 60')},
 {name:'unsafe coastal seed bypasses safe road search',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFIngress.js',
  mutate:s=>s.replace('(!operation.m_TacticalCommander && !IsLaunchPositionSafe(launchPosition, target.GetCenter()))','!IsLaunchPositionSafe(launchPosition, target.GetCenter())')},
 {name:'full vehicle ignores native passenger capacity',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c',suite:'Test-QRFBoarding.js',
  mutate:s=>s.replace('int passengers = cargoSeats - (transportCrew - crewSeats);','int passengers = 1;')},
 {name:'full load overruns personnel reservation',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c',suite:'Test-QRFBoarding.js',
  mutate:s=>s.replace(' || passengers + transportCrew + escortCrew > m_iReservedMembers','')},
 {name:'asynchronous pilot requests treated as paused=false',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperAirForce.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace('access.GetInVehicle(seatEntity, slot, true, -1, ECloseDoorAfterActions.INVALID, true)','access.GetInVehicle(seatEntity, slot, true, -1, ECloseDoorAfterActions.INVALID, false)')},
 {name:'unready reserve dispatched',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace(' || !ready.m_bReady','')},
 {name:'ground staging bypassed',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace('if (force.m_bTacticalStaging)','if (false)')},
 {name:'town pacification disconnected',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace('if (target && commander.m_iTownHoldStarted >= 0 && !commander.m_bWithdraw)','if (false)')},
 {name:'capture activation recorded after subscribers',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_TerritoryAttackManager.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace(/RecordTerritoryOperationChange\(operation\);\r?\n\t\tsuper.NotifyChanged\(operation\);/,'super.NotifyChanged(operation);\n\t\tRecordTerritoryOperationChange(operation);')},
 {name:'debug damage escalates campaign',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationDamageObserver.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace('attacker.GetOrigin(), !operation.m_bDebugRequest','attacker.GetOrigin(), true')},
 {name:'admin mutation loses role gate',file:'AROpenResistance/Open Resistance/Scripts/Game/Admin/AROR_AdminMenuComponent.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replaceAll(' || !AROR_AdminAccess.IsAdministrator(playerId)','')},
 {name:'unbuilt collateral refunded twice',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFReleaseLifecycle.js',
  mutate:s=>s.replace(' && pending.m_iReservedMembers > 0','')},
 {name:'air group budget undercounted',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ForceProfile.c',suite:'Test-MarsAndAttacks.js',
  mutate:s=>s.replace('return 2 + m_iEscortCount;','return 1 + m_iEscortCount;')},
 {name:'unknown operation types admitted',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationManagerComponent.c',suite:'Test-MarsAndAttacks.js',
  mutate:s=>s.replace('if (type != AROR_EOperationType.ATTACK && type != AROR_EOperationType.QRF)','if (false)')},
 {name:'withdrawal dispatches ready reserves',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (tactical && tactical.m_bWithdraw)','if (false)')},
 {name:'empty gunner accepted while crew sit in cargo',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c',suite:'Test-QRFBoarding.js',
  mutate:s=>s.replace('if (!IsRequiredGunnerSeat(slot))','if (true)')},
 {name:'mounted cargo-style weapon omitted from crew seats',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c',suite:'Test-QRFBoarding.js',
  mutate:s=>s.replace(' || slot.GetAttachedTurret()','')},
 {name:'foreign driver accepted as ready',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c',suite:'Test-QRFBoarding.js',
  mutate:s=>s.replace('if (!ownDriver)','if (false)')},
 {name:'unissued infantry group skipped by retention',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (force.m_bOrdersIssued || force.m_Group)','if (force.m_bOrdersIssued)')},
 {name:'aborted materialized aircraft deleted immediately',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperAirForce.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (materialized && !m_bImmediateCleanup)','if (m_bOrdersIssued && !m_bImmediateCleanup)')},
 {name:'queued ground origin is not rechecked',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (!TacticalOriginAvailable(operation))','if (false)')},
 {name:'air construction ignores changed player clearance',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (!IsLaunchPositionSafe(force.m_vLaunch, force.m_vTown) || !AROR_ReaperFlightGeometry.IsAirspaceClear(force.m_vLaunch, m_fReaperInsertionHeight + force.m_iPackageIndex * 15, 14))','if (false)')},
 {name:'funding shortage becomes configuration failure',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('profile = fallback;','profile = null;')},
 {name:'player aircraft occupancy ignored',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperAirForce.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace(/(static bool HasPlayerOccupant[\s\S]*?)return true;/,'$1return false;')},
 {name:'activation overhead charged twice',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_QRFFinance.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace(' || account.m_bActivated','')},
 {name:'closed account keeps unbuilt collateral',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_QRFFunding.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (!m_bClosed)','if (true)')},
 {name:'fixed-task exits retry without a bound',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace(' && force.m_iTacticalExitRetries < m_iQRFExitRetryLimit','')},
 {name:'nested finance traversal removes an account twice',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_QRFFinance.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace(' || m_bReconciling','')},
 {name:'reserve window expires before gunship egress',file:'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('m_iQRFReserveWait 450','m_iQRFReserveWait 300')},
 {name:'partially validated aircraft have no boarding hold',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('force.m_bAircraftHeld = true;','force.m_bAircraftHeld = false;')},
 {name:'empty failed construction keeps its collateral',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (emptyOwnedMembers.IsEmpty())','if (false)')},
 {name:'unused partial-package collateral never released',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_QRFFunding.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('m_iQuote = Math.Max(0, m_iQuote - Math.Max(0, packageQuote - registered));','m_iQuote = m_iQuote;')},
 {name:'virtual proxy retains package finalization ownership',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('reservation.m_bOwnsPackageQuote = false;','reservation.m_bOwnsPackageQuote = true;')},
 {name:'escort child finalizes transport collateral',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('force.m_Escort.m_bOwnsPackageQuote = false;','force.m_Escort.m_bOwnsPackageQuote = true;')},
 {name:'player-owned pending spawn queue releases collateral early',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationForce.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace(' && !m_bPackageQueuesPreserved','')},
 {name:'escort player queue not propagated to package owner',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperAirForce.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (m_Escort.m_bPackageQueuesPreserved)','if (false)')},
 {name:'retirement orders unissued escort to fly',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperRetirement.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('if (force.m_bOrdersIssued && !force.m_bEscortLost','if (!force.m_bEscortLost')},
 {name:'active dispatch failure terminates healthy packages',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('FailPackage(ready, operation, "Ready package could not receive deployment orders");','FailOperation(operation, "Ready package could not receive deployment orders");')},
 {name:'active reserve boarding loses its preparation deadline',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('m_iExecutorClock >= force.m_iPreparationDeadline','false')},
 {name:'unobserved removal is labelled recovered',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_QRFFunding.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('m_iQuote = Math.Max(0, m_iQuote - asset.m_iValue);','m_iRecovered += asset.m_iValue;')},
 {name:'headline width and row estimates diverge',file:'AROpenResistance/Open Resistance/Scripts/Game/Notifications/AROR_NotificationDisplay.c',suite:'Test-QRFSupportAndHeadlines.js',
  mutate:s=>s.replace('Math.Max(width, EstimateHeadlineLineWidth(line, fontSize))','Math.Max(width, EstimateHeadlineLineWidth(line, fontSize) * 0.5)')},
 {name:'scripted coast is treated as safe friendly recovery',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ScriptedResponses.c',suite:'Test-StrongholdIntegration.js',
  mutate:s=>s.replace('operation.m_TacticalCommander.m_bSafeWithdrawalOrigin = false;','operation.m_TacticalCommander.m_bSafeWithdrawalOrigin = true;')},
 {name:'scripted admission overlaps an ordinary target operation',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ScriptedResponses.c',suite:'Test-StrongholdIntegration.js',
  mutate:s=>s.replace('!existing.IsTerminal() && existing.GetRole()','!existing.IsTerminal() && existing.m_bFundingWaived && existing.GetRole()')},
 {name:'stronghold loses dispatch receipt between polls',file:'AROpenResistance/Open Resistance/Scripts/Game/Strongholds/AROR_StrongholdComponent.c',suite:'Test-StrongholdIntegration.js',
  mutate:s=>s.replace('if (operation.m_TacticalCommander && operation.m_TacticalCommander.m_iFirstDispatch >= 0)','if (false)')},
 {name:'retired garrison defender fabricates a retaliation kill',file:'AROpenResistance/Open Resistance/Scripts/Game/Strongholds/AROR_StrongholdComponent.c',suite:'Test-StrongholdIntegration.js',
  mutate:s=>s.replace('if (!watch.m_Entity)','if (false)')},
 {name:'forced scripted waves disconnected from construction',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ScriptedResponses.c',suite:'Test-StrongholdIntegration.js',
  mutate:s=>s.replace('operation.m_TacticalCommander.ForceTwoWaves();','operation.m_TacticalCommander.m_bWithdraw = true;')},
 {name:'recipe script default overwrites authored response family',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundQRFRecipe.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('AROR_EResponseType m_eResponseType;','AROR_EResponseType m_eResponseType = AROR_EResponseType.COMBINED_ARMS;')},
 {name:'profile script default overwrites authored vehicle price',file:'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ForceProfile.c',suite:'Test-QRFInterruptions.js',
  mutate:s=>s.replace('int m_iVehicleLossValue;','int m_iVehicleLossValue = -1;')},
 {name:'retaliation banner announced before deployment',file:'AROpenResistance/Open Resistance/Scripts/Game/Strongholds/AROR_StrongholdComponent.c',suite:'Test-StrongholdIntegration.js',
  mutate:s=>s.replace('if (deployed && !m_bResponseDeployed)','if (!m_bResponseDeployed)')}
];
function runQRFMutationTests(read,execute){
 const results=[];
 // A broken harness must never count as successful detection of a source mutation.
 for(const suite of new Set(cases.map(test=>test.suite))){
  const baseline=execute(suite,{});
  if(!baseline.passed)throw Error('Mutation baseline failed: '+suite+'\n'+baseline.error);
 }
 for(const test of cases){
  const original=read(test.file),mutated=test.mutate(original);
  if(original===mutated)throw Error('Mutation did not match current source: '+test.name);
  const result=execute(test.suite,{[test.file]:mutated});
  if(result.passed)throw Error('Regression survived: '+test.name);
  const lines=String(result.error).split('\n');
  results.push({name:test.name,suite:test.suite,killed:true,error:lines.find(line=>/^(?:\w*Error):/.test(line))||lines[0]});
 }
 return {assertions:results.length,results,note:'Intentional mutations in isolated source snapshots; no native behavior or source-file edits.'};
}
if(typeof module!=='undefined')module.exports={runQRFMutationTests};
if(typeof require==='function'&&require.main===module){
 const fs=require('fs'),path=require('path'),vm=require('vm'),root=path.resolve(__dirname,'..');
 const read=p=>fs.readFileSync(path.join(root,p),'utf8');
 const execute=(suite,overrides)=>{
  const mapped=new Map(Object.entries(overrides).map(([p,s])=>[path.resolve(root,p),s]));
  const module={exports:{}},requireInTest=name=>{
   if(name==='fs')return {...fs,readFileSync:(p,...args)=>mapped.get(path.resolve(p))??fs.readFileSync(p,...args)};
   return require(name);
  };
  requireInTest.main=module;
  try{vm.runInNewContext(read('tools/'+suite),{require:requireInTest,module,__dirname,console:{log(){}}},{filename:suite,timeout:30000});return {passed:true}}
  catch(error){return {passed:false,error:error.stack}}
 };
 console.log(JSON.stringify(runQRFMutationTests(read,execute)));
}
