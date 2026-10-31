/* Actual-source unloading checks with mocked native services; not an Enforce compiler.
 * Run: node tools/Test-QRFUnloading.js
 */
function runQRFUnloadingTests(source) {
  let assertions = 0;
  const check = (ok, message) => { assertions++; if (!ok) throw Error(message); };
  const types = {MOTORIZED:0, MECHANIZED:1, ARMOUR:2, AIRBORNE:3};
  let server = true;
  const game = {GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:p=>p.player?1:0}), GetWorld:()=>({GetSurfaceY:()=>0})};
  const env = {Replication:{IsServer:()=>server}, ChimeraCharacter:{Cast:x=>x}, CompartmentAccessComponent:{Cast:x=>x},
    SCR_AIVehicleUsageComponent:{Cast:x=>x},AROR_EForceType:types, EGetOutType:{ANIMATED:0}, ECloseDoorAfterActions:{LEAVE_OPEN:1},
    GetGame:()=>game, Math3D:{MatrixIdentity4:t=>{t[3]=[0,0,0];}},
    vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])}, Math:{AbsFloat:Math.abs},
    Print:()=>{}, string:{Format:()=>''}, LogLevel:{NORMAL:0}};
  function compile(name) {
    const match = source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if (!match) throw Error('Missing '+name);
    const args = match[1].replace(/\b(?:bool|IEntity|CompartmentAccessComponent)\s+/g,'');
    let body = match[2].replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for (const $1 of $2)')
      .replace(/array<\w+> (\w+) = \{\};/g,'let $1 = [];')
      .replace(/vector (\w+)\[4\];/g,'let $1 = [];')
      .replace(/\b(?:bool|int|float|vector|Physics|ChimeraCharacter|CompartmentAccessComponent|BaseCompartmentSlot|SCR_AIVehicleUsageComponent)\s+(\w+)/g,'let $1')
      .replace(/let (\w+);/g,'let $1 = 0;')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/(?<![.\w])RecoverPassengerExit_S\(/g,'this.RecoverPassengerExit_S(')
      .replace(/(?<![.\w])Seat\(/g,'this.Seat(');
    return new Function(...Object.keys(env), 'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  const mount = compile('RequestDismount_S'), recover = compile('RecoverPassengerExit_S');
  function fixture() {
    const vehicle = {speed:0, GetPhysics(){return {GetVelocity:()=>({Length:()=>this.speed})};}, GetOrigin:()=>[0,0,0]};
    const passengers = [];
    passengers.Contains = p=>passengers.includes(p);
    const force = {m_Vehicle:vehicle,m_Profile:{m_eType:types.MECHANIZED},m_aPassengers:passengers,
      m_iExitAttempts:0,m_sOperationId:'test',m_iPackageIndex:0,RecoverPassengerExit_S:recover};
    function add(doors=[0], accepted=true) {
      const access = {inside:true,exiting:false,switching:false,requests:[], recoveries:0,clear:true,position:[3,0,0],
        IsInCompartment(){return this.inside;},IsGettingOut(){return this.exiting;},IsSwitchingSeatsAnim(){return this.switching;},
        GetCompartment(){return {GetVehicle:()=>vehicle,GetCompartmentSlotID:()=>0,GetAvailableDoorIndices:a=>a.push(...doors)};},
        CanGetOutVehicleViaDoor:()=>true, GetOutVehicle(type,door,close){this.requests.push([door,close]);const ok=typeof accepted==='function'?accepted(door):accepted;if(ok)this.exiting=true;return ok;},
        FindSuitableTeleportLocation(t){t[3]=this.position;return this.clear;},
        GetOutVehicle_NoDoor(t,ragdoll,paused,teleport){this.recoveries++;this.inside=false;return !ragdoll&&!paused&&teleport;}};
      const p = {access,dead:false,FindComponent:()=>access,GetTransform:t=>{t[3]=[0,0,0];},GetCharacterController(){return {IsDead:()=>this.dead};}};
      passengers.push(p); return p;
    }
    return {force,vehicle,add};
  }
  let f=fixture(), a=f.add([0,1],door=>door===1), b=f.add();
  mount.call(f.force);check(a.access.requests.length===2,'try second door after native rejection');
  check(a.access.requests[1][1]===1,'leave unloading door open');check(b.access.requests.length===0,'one accepted request per tick');
  mount.call(f.force);check(b.access.requests.length===1,'next passenger admitted next tick');check(a.access.requests.length===2,'in-progress animation not restarted');
  f=fixture();a=f.add([0],false);b=f.add();mount.call(f.force);check(b.access.requests.length===1,'blocked passenger does not starve squad');
  check(a.access.recoveries===0,'recovery disabled during normal exit period');
  mount.call(f.force,true);check(a.access.recoveries===1,'delayed cleared native recovery');
  for (const [name,change] of [
    ['moving vehicle',(f,p)=>f.vehicle.speed=1],['aircraft',(f,p)=>f.force.m_Profile.m_eType=types.AIRBORNE],
    ['armour crew',(f,p)=>f.force.m_Profile.m_eType=types.ARMOUR],['player',(f,p)=>p.player=true],
    ['dead passenger',(f,p)=>p.dead=true],['active exit',(f,p)=>p.access.exiting=true],
    ['seat switch',(f,p)=>p.access.switching=true],['no clearance',(f,p)=>p.access.clear=false],
    ['distant exit',(f,p)=>p.access.position=[20,0,0]],['elevated exit',(f,p)=>p.access.position=[3,4,0]],
    ['unowned character',(f,p)=>f.force.m_aPassengers.length=0]]) {
    f=fixture();a=f.add([0],false);change(f,a);check(!recover.call(f.force,a,a.access),'reject '+name);check(a.access.recoveries===0,'no forced exit: '+name);
  }
  f=fixture();a=f.add();server=false;check(!mount.call(f.force,true),'client cannot unload');check(!recover.call(f.force,a,a.access),'client cannot recover');
  server=true;check(source.includes('GetInVehicle(seatEntity, slot, true,'),'initial seating remains instant');
  // A full infantry load exits in paced requests; neither retained crew member is an exit target.
  f=fixture();const fullInfantry=Array.from({length:13},()=>f.add());
  const driver={access:{requests:[]}},gunner={access:{requests:[]}};
  f.force.m_aMembers=[driver,gunner,...fullInfantry];f.force.m_CrewGroup={members:[driver,gunner]};
  for(let i=0;i<fullInfantry.length;i++)mount.call(f.force,false);
  check(fullInfantry.every(p=>p.access.requests.length===1),'every soldier in a full load receives one animated exit');
  check(driver.access.requests.length===0&&gunner.access.requests.length===0,'unloading infantry leaves driver and gunner untouched');
  const restore=compile('RestoreNearbyDriver_S');
  let seated=0,occupied=false,playerGroup=0,crewDistance=3;
  f=fixture();a=f.add();a.access.inside=false;a.GetOrigin=()=>[crewDistance,0,0];a.access.IsGettingIn=()=>false;
  const usage={CanBePiloted:()=>true,GetPilotCompartmentSlot:()=>({IsOccupied:()=>occupied})};
  f.vehicle.FindComponent=()=>usage;
  f.force.m_CrewGroup={GetPlayerCount:()=>playerGroup,GetAgents:xs=>xs.push({GetControlledEntity:()=>a})};
  f.force.Seat=()=>{seated++;return true;};
  check(restore.call(f.force)&&seated===1,'nearby crew can resume empty driver seat');
  occupied=true;check(!restore.call(f.force),'occupied driver never displaced');occupied=false;
  crewDistance=20;check(!restore.call(f.force),'distant crew never teleported');crewDistance=3;
  a.player=true;check(!restore.call(f.force),'player crew never reseated');a.player=false;
  playerGroup=1;check(!restore.call(f.force),'player-commanded crew group untouched');playerGroup=0;
  f.vehicle.speed=1;check(!restore.call(f.force),'moving hull not reseated');f.vehicle.speed=0;
  server=false;check(!restore.call(f.force),'client cannot reseat crew');server=true;
  return {assertions,note:'Native clearance, animation and replication mocked; compile and Play required.'};
}
if (typeof require === 'function' && typeof module !== 'undefined' && require.main === module) {
  const fs=require('fs'),path=require('path');
  const file=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c');
  console.log(JSON.stringify(runQRFUnloadingTests(fs.readFileSync(file,'utf8'))));
}
