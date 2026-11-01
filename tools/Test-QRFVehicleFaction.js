/* Native affiliation clearing versus actual capture; mocked seats, not a native test. */
function runQRFVehicleFactionTests(sources) {
  if(!String.prototype.IsEmpty)Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  const env={AROR_ForceSpawner:{IsAlive:x=>!!x&&!x.dead},BaseCompartmentManagerComponent:{Cast:x=>x},FactionAffiliationComponent:{Cast:x=>x},
    SCR_VehicleFactionAffiliationComponent:{Cast:x=>x&&x.native?x:null},
    GetGame:()=>({GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:e=>e.player||0})}),
    Print:()=>{},string:{Format:()=>''},LogLevel:{NORMAL:0,WARNING:1},AROR_EForceType:{ARMOUR:2}};
  function compile(source,name){
    const match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!match)throw Error('Missing '+name);
    const args=match[1].replace(/\b(?:AROR_\w+|FactionAffiliationComponent)\s+/g,'');
    let body=match[2].replace(/array<BaseCompartmentSlot> (\w+) = \{\};/g,'let $1=[];')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
      .replace(/\b(?:bool|string|IEntity|BaseCompartmentManagerComponent|FactionAffiliationComponent)\s+(\w+)/g,'let $1')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/\.Contains\(/g,'.includes(').replace(/slots.Count\(\)/g,'slots.length')
      .replace(/actual.IsEmpty\(\)/g,'!actual.length').replace(/force.m_sAssetFactionKey.IsEmpty\(\)/g,'!force.m_sAssetFactionKey.length')
      .replace(/(?<![.\w])(FailPackage|TacticalInfantryOrders)\(/g,'this.$1(');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  let key='AROR_OCCUPIERS',player=0,slotOccupied=false,occupant=null,missingManager=false,missingFaction=false;
  const own={FindComponent:()=>missingFaction?null:({GetAffiliatedFactionKey:()=>key}),get player(){return player;}};
  let slots=[{GetOccupant:()=>occupant,IsOccupied:()=>slotOccupied}];
  const manager={GetCompartments:a=>a.push(...slots)};
  const vehicle={FindComponent:()=>missingManager?null:manager,GetOrigin:()=>[0,0,0]};
  const force={m_Vehicle:vehicle,m_aMembers:[own],m_sFactionKey:'AROR_OCCUPIERS'};
  const audit=compile(sources.ground,'HasOnlyOwnedVehicleOccupants');
  const run=()=>audit.call(force);
  check(run(),'confirmed empty hull survives blank native affiliation');
  occupant=own;slotOccupied=true;check(run(),'own AI crew survives blank native affiliation');
  player=1;check(!run(),'human occupant cannot use own-AI exception');player=0;
  key='AROR_INVADERS';check(!run(),'hostile occupant rejected');key='AROR_OCCUPIERS';
  occupant={};check(!run(),'foreign occupant rejected even before faction lookup');occupant=own;
  missingFaction=true;check(!run(),'unknown occupant affiliation rejected');missingFaction=false;
  occupant=null;check(!run(),'unresolved occupied seat rejected');slotOccupied=false;
  missingManager=true;check(!run(),'missing seat manager is not proof of empty hull');missingManager=false;
  slots=[];check(!run(),'unavailable slot list rejected');slots=[{GetOccupant:()=>occupant,IsOccupied:()=>slotOccupied}];
  const validate=compile(sources.executor,'ValidateGroundVehicleFaction');
  let failures=0;
  const ctx={m_iExecutorClock:20,FailPackage:()=>failures++,TacticalInfantryOrders:()=>{}};
  Object.assign(force,{m_Profile:{m_eType:2,m_sId:'armour'},m_sAssetFactionKey:'US',m_bDismounted:false,m_bMounted:true,
    m_iPackageIndex:0,m_iVehicleFactionMismatchSince:-1,HasVehicleOccupants:()=>slotOccupied,
    CountInfantry:()=>3,ArePassengersOut:()=>true,HasOnlyOwnedVehicleOccupants:run});
  const operation={GetId:()=>'operation_1'};
  let actual='';const affiliation={native:true,GetAffiliatedFactionKey:()=>actual};
  check(validate.call(ctx,force,operation,affiliation)&&failures===0,'empty armour not incorrectly failed before delivery');
  occupant=own;slotOccupied=true;
  check(validate.call(ctx,force,operation,affiliation)&&failures===0,'own occupied armour not incorrectly failed during combat');
  actual='AROR_INVADERS';check(!validate.call(ctx,force,operation,affiliation)&&failures===1,'explicit hostile hull affiliation still fails');
  actual='';occupant={};check(!validate.call(ctx,force,operation,affiliation)&&failures===2,'blank hull with foreign occupant still fails');
  check(!validate.call(ctx,force,operation,null)&&failures===3,'missing affiliation component still fails');

  const airAudit=compile(sources.reaper,'ReaperVehicleFaction');
  let aircraftFaction=affiliation;
  const aircraft={m_Vehicle:{FindComponent:()=>aircraftFaction},m_sFactionKey:'AROR_OCCUPIERS',m_sAssetFactionKey:'US',
    HasOnlyOwnedVehicleOccupants:run,HasVehicleOccupants:()=>slotOccupied};
  actual='AROR_OCCUPIERS';check(airAudit(aircraft),'correct aircraft faction accepted');
  actual='';occupant=null;slotOccupied=false;check(airAudit(aircraft),'empty native aircraft survives pilot-loss affiliation clearing');
  occupant=own;slotOccupied=true;check(airAudit(aircraft),'verified friendly occupants survive native aircraft affiliation clearing');
  own.dead=true;key='';check(airAudit(aircraft),'retained own dead pilot is not mistaken for a faction takeover');own.dead=false;key='AROR_OCCUPIERS';
  actual='AROR_RESISTANCE';check(!airAudit(aircraft),'Resistance capture still rejects aircraft');
  actual='';player=1;check(!airAudit(aircraft),'human occupant cannot bypass aircraft affiliation mismatch');player=0;
  occupant={};check(!airAudit(aircraft),'unknown aircraft occupant fails closed');
  occupant=null;slotOccupied=false;actual='US';check(airAudit(aircraft),'empty aircraft may restore declared source affiliation');
  aircraftFaction=null;check(!airAudit(aircraft),'missing aircraft affiliation component rejected');
  return {assertions,note:'Compile and combat test required; no vehicle re-factioning or loss-price changes.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
  console.log(JSON.stringify(runQRFVehicleFactionTests({ground:fs.readFileSync(path.join(root,'AROR_GroundOperationForce.c'),'utf8'),executor:fs.readFileSync(path.join(root,'AROR_OperationExecutorComponent.c'),'utf8'),reaper:fs.readFileSync(path.join(root,'AROR_ReaperQRFExecutor.c'),'utf8')})));
}
