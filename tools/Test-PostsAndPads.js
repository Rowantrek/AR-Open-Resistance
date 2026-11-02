/* Actual garrison sentry-post, raised-platform, helipad and rectangular-footprint methods against a mocked world of
 * horizontal slabs. No native geometry, spawning or AI acceptance is implied. */
function runPostsAndPadsTests(source) {
 let assertions=0,printed=[],teleports=[];
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 for(const [name,fn]of Object.entries({IsEmpty(){return this.length===0;},Count(){return this.length;},Insert(x){this.push(x);},Contains(x){return this.includes(x);}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
 Object.defineProperty(String.prototype,'IsEmpty',{value:function(){return this.length===0;},configurable:true});
 // World: terrain at height 10, slabs {entity,x0,x1,z0,z1,bottom,top}, ocean at 0
 let slabs=[],ground=10,ocean=0;
 const inside=(s,x,z)=>x>=s.x0&&x<=s.x1&&z>=s.z0&&z<=s.z1;
 const excluded=(trace,s)=>(trace.ExcludeArray||[]).includes(s.entity)||trace.Exclude===s.entity;
 const world={GetSurfaceY:()=>ground,GetOceanHeight:()=>ocean,GetWorldTime:()=>0,
  TraceMove(trace){
   const [x,y0,z]=trace.Start,y1=trace.End[1];
   if(trace.Mins){ // Box sweep: any slab overlapping the box volume blocks it
    const box={x0:x+trace.Mins[0],x1:x+trace.Maxs[0],z0:z+trace.Mins[2],z1:z+trace.Maxs[2],bottom:y0+trace.Mins[1],top:y0+trace.Maxs[1]};
    return slabs.some(s=>!excluded(trace,s)&&s.x0<box.x1&&s.x1>box.x0&&s.z0<box.z1&&s.z1>box.z0&&s.bottom<box.top&&s.top>box.bottom)?0:1;
   }
   let best=1,hit=null;
   for(const s of slabs){
    if(excluded(trace,s)||!inside(s,x,z))continue;
    if(y1<y0&&y0>s.top&&s.top>=y1){const f=(y0-s.top)/(y0-y1);if(f<best){best=f;hit=s;}}   // Down: a top surface below the start
    if(y1>y0&&y0<s.bottom&&s.bottom<=y1){const f=(s.bottom-y0)/(y1-y0);if(f<best){best=f;hit=s;}} // Up: an underside above the start
   }
   trace.TraceEnt=hit?hit.entity:null;
   return best;
  }};
 const vadd=(a,b)=>a.map((v,i)=>v+b[i]),vsub=(a,b)=>a.map((v,i)=>v-b[i]);
 class TraceParam{constructor(){this.Start=[0,0,0];this.End=[0,0,0];}}
 class TraceBox extends TraceParam{}
 const env={Replication:{IsServer:()=>true},Math:{Max:Math.max,Min:Math.min,AbsFloat:Math.abs,Clamp:(v,a,b)=>Math.max(a,Math.min(b,v))},
  vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},Vector:(...v)=>v,vadd,vsub,TraceParam,TraceBox,TraceFlags:{ENTS:1,VISIBILITY:2,WORLD:4},
  GetGame:()=>({GetWorld:()=>world}),Print:message=>printed.push(message),LogLevel:{NORMAL:0},
  string:{Format:(s,...a)=>s.replace(/%(\d+)/g,(_,n)=>String(a[n-1]))},Math3D:{AnglesToMatrix:(angles,out)=>{out[0]=angles;}},
  BaseGameEntity:{Cast:x=>x},AROR_LandingPadComponent:{Cast:x=>x},AROR_SentryPostComponent:{Cast:x=>x},AROR_DefencePositionComponent:{Cast:x=>x},
  SENTRY_VARIANT:1000,ROTOR_CLEARANCE_BOTTOM:2.5,ROTOR_CLEARANCE_TOP:6,PLATFORM_HEADROOM:1.8};
 const methods=['FindPlatform','PadSurface','LandingPadAt','CollectLandingPads','CollectHierarchy','SentryPosts','IsSentry','DefenceMarker','FindDefencePosition',
  'PositionOrdinal','InfantryPrefab','IsClear','HasPlayerClearance','GetOwner','GetDefenceCenter','GetDefenceHalfExtent','GetDefenceHalfDepth','IsPlayerOrForeignAsset',
  'MaintainCrewSeats_S','HasMountedDefence','CrewAndOrder'];
 function compile(name){
  const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
  if(!m)throw Error('Missing actual method '+name);
  const args=m[1].replace(/\bnotnull /g,'').replace(/\bout /g,'').replace(/array<[^>]+>\s+/g,'').replace(/\b(?:vector|bool|float|int|IEntity|AROR_\w+)\s+/g,'');
  let body=m[2].replace(/vector (\w+)\[4\];/g,'let $1=[];')
   .replace(/array<\w+> (\w+) = \{\};/g,'let $1=[];').replace(/array<\w+> (\w+) =/g,'let $1 =')
   .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
   .replace(/([\w.]+) \+ Vector\(([^)]*)\)/g,'vadd($1,[$2])')
   .replace(/position - GetDefenceCenter\(\)/g,'vsub(position,GetDefenceCenter())')
   .replace(/\b(?:bool|int|float|string|vector|IEntity|BaseWorld|TraceParam|TraceBox|BaseGameEntity|ChimeraAIWorld|NavmeshWorldComponent|AROR_\w+) (\w+);/g,'let $1=0;')
   .replace(/\b(?:bool|int|float|string|vector|IEntity|BaseWorld|TraceParam|TraceBox|BaseGameEntity|ChimeraAIWorld|NavmeshWorldComponent|ResourceName|AROR_\w+) (\w+) =/g,'let $1 =')
   .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x).replace(/\.Count\(\)/g,'.length');
  for(const n of methods)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
 }
 const tower={name:'tower'},roof={name:'roof'};
 const garrison={};
 for(const n of ['FindPlatform','PadSurface','LandingPadAt','CollectLandingPads','CollectHierarchy','SentryPosts','IsSentry','DefenceMarker','InfantryPrefab','IsClear','GetDefenceHalfDepth','CrewAndOrder'])garrison[n]=compile(n);
 let children=[];
 const chain=list=>list.map((c,i)=>Object.assign(c,{GetSibling:()=>list[i+1]||null,GetChildren:()=>null}));
 const owner={GetChildren:()=>chain(children)[0]||null,VectorToLocal:v=>v};
 Object.assign(garrison,{GetOwner:()=>owner,GetDefenceCenter:()=>[0,ground,0],GetDefenceHalfExtent:()=>40,m_fDefenceHalfDepth:23,m_aSlots:[],
  m_Location:{IsPositionInside:()=>true,GetId:()=>'site'},HasPlayerClearance:()=>true,IsPlayerOrForeignAsset:()=>false,
  FindDefencePosition:(kind,ordinal)=>({marker:kind+':'+ordinal}),PositionOrdinal:()=>0});

 // Raised platforms: the floor under a roof, never the roof; nothing there means no platform
 slabs=[{entity:tower,x0:-1.5,x1:1.5,z0:-1.5,z1:1.5,bottom:ground+2.8,top:ground+3},{entity:roof,x0:-2,x1:2,z0:-2,z1:2,bottom:ground+5.4,top:ground+5.6}];
 let floor=null;const platform=(elevation)=>{const holder={};const found=compileOut(elevation,holder);floor=holder.value;return found;};
 function compileOut(elevation,holder){
  // FindPlatform writes its out parameter by assignment; run the real body with a wrapper that captures it
  const body=source.match(/protected bool FindPlatform\(vector position, float elevation, out float height\)\r?\n\t\{([\s\S]*?)^\t\}/m)[1];
  const js=body.replace(/\b(?:float|bool|int) (\w+);/g,'let $1=0;').replace(/\b(?:float|bool|int|BaseWorld|TraceParam) (\w+) =/g,'let $1 =')
   .replace(/(?<![.\w])height = /g,'holder.value = ');
  return new Function(...Object.keys(env),'holder','position','elevation',js)(...Object.values(env),holder,[0,ground,0],elevation);
 }
 check(platform(4)&&Math.abs(floor-(ground+3))<1e-6,'a raised post stands on the floor under the roof, not on the roof');
 slabs[1].bottom=ground+4;slabs[1].top=ground+4.2;
 check(platform(4)&&Math.abs(floor-(ground+4.2))<1e-6,'a floor without standing room under a low roof is skipped');
 slabs=[];check(!platform(3.5),'no structure means no platform, so nobody is left floating');
 slabs=[{entity:tower,x0:-1,x1:1,z0:-1,z1:1,bottom:ground+0.5,top:ground+1}];
 check(!platform(3.5),'surfaces below a metre and a half are never treated as a platform');

 // Helipad decks: a parked helicopter stands on its own pad's deck, not on other props
 const deck={name:'deck'},pole={name:'pole'};
 const padComponent={GetRotorRadius:()=>10.7,GetLength:()=>25.3};
 children=[{FindComponent:t=>t===env.AROR_LandingPadComponent?padComponent:null,GetOrigin:()=>[25,ground,8],entity:deck}];
 // CollectHierarchy inserts the child entities themselves; the deck slab belongs to the pad child
 slabs=[{entity:children[0],x0:20,x1:30,z0:3,z1:13,bottom:ground,top:ground+0.2}];
 check(Math.abs(garrison.PadSurface([25,ground,8])-(ground+0.2))<1e-6,'a parked helicopter stands on its pad deck');
 slabs.push({entity:pole,x0:24.5,x1:25.5,z0:7.5,z1:8.5,bottom:ground,top:ground+1.9});
 check(garrison.PadSurface([25,ground,8])===ground,'a non-pad prop over the spot never lifts the helicopter onto it');
 slabs=[];check(garrison.PadSurface([25,ground,8])===ground,'no deck under the spot keeps the terrain height');
 check(garrison.LandingPadAt([26,ground,9])===padComponent&&!garrison.LandingPadAt([35,ground,8]),'a position within three metres of a pad uses that pad');

 // Sentry posts: each sentry holds the nth tagged post; marksman prefab; room for one man
 const postA={FindComponent:t=>t===env.AROR_SentryPostComponent?{GetFloorHeight:()=>3.5}:null,GetOrigin:()=>[30,ground,15],name:'A'};
 const postB={FindComponent:t=>t===env.AROR_SentryPostComponent?{GetFloorHeight:()=>0}:null,GetOrigin:()=>[-30,ground,15],name:'B'};
 children=[{FindComponent:()=>null,GetOrigin:()=>[0,ground,0]},postA,postB];
 check(garrison.SentryPosts().map(p=>p.name).join()==='A,B','sentry posts are found in composition order');
 garrison.m_aSlots=[{m_iKind:0,m_iVariant:2},{m_iKind:0,m_iVariant:1001},{m_iKind:0,m_iVariant:1005},{m_iKind:1,m_iVariant:1000}];
 check(!garrison.IsSentry(garrison.m_aSlots[0])&&garrison.IsSentry(garrison.m_aSlots[1])&&!garrison.IsSentry(garrison.m_aSlots[3]),'only infantry with a sentry variant is a sentry');
 check(garrison.DefenceMarker(1)===postB,'the second sentry holds the second post');
 check(garrison.DefenceMarker(2)===null,'a sentry whose post was removed has no marker');
 check(garrison.DefenceMarker(0).marker==='0:0','team markers are unchanged');
 const faction={m_sMarksman:'sniper',m_sInfantry:'rifle',m_aInfantryVariants:['ar','gl','tl']};
 check(garrison.InfantryPrefab(faction,1000)==='sniper'&&garrison.InfantryPrefab(faction,2)==='gl','sentries use the marksman; fireteam variants are unchanged');
 faction.m_sMarksman='';check(garrison.InfantryPrefab(faction,1001)==='rifle','without a marksman a sentry is a rifleman');
 // Room for one man inside a bunker: the bunker walls around him do not block, something on the spot does
 slabs=[{entity:{},x0:-32,x1:-31,z0:13,z1:17,bottom:ground,top:ground+1.5},{entity:{},x0:-29,x1:-28,z0:13,z1:17,bottom:ground,top:ground+1.5}];
 check(garrison.IsClear([-30,ground,15],0,1,50),'a bunker sentry has room between its walls');
 slabs.push({entity:{},x0:-30.3,x1:-29.7,z0:14.7,z1:15.3,bottom:ground,top:ground+1});
 check(!garrison.IsClear([-30,ground,15],0,1,50),'something on the post spot blocks the sentry');
 slabs=[];check(garrison.IsClear([30,ground+3,15],0,1,50),'a raised sentry is checked on his platform, not against the terrain');
 check(!garrison.IsClear([30,ground,30],0,1,50),'a rectangular footprint keeps posts within its depth');
 ocean=ground+5;check(!garrison.IsClear([30,ground+3,15],0,1,50),'nothing is placed at or below the sea');ocean=0;

 // Rectangular footprints: depth defaults to the half-extent and never exceeds it
 garrison.m_fDefenceHalfDepth=0;check(garrison.GetDefenceHalfDepth()===40,'no depth keeps the footprint square');
 garrison.m_fDefenceHalfDepth=90;check(garrison.GetDefenceHalfDepth()===40,'depth is capped at the half-width');
 garrison.m_fDefenceHalfDepth=5;check(garrison.GetDefenceHalfDepth()===12,'depth has the same 12 m minimum as the half-width');
 garrison.m_fDefenceHalfDepth=23;

 // Holding the post: lifted onto the platform once, no movement orders, height logged
 const waypoints=[];
 const marksman={origin:[30,ground,15],GetOrigin(){return this.origin;},Teleport(t){teleports.push(t);this.origin=t[3];}};
 garrison.m_aSlots=[{m_iKind:0,m_iVariant:1000,m_Entity:marksman,m_vPosition:[30,ground+3,15],m_vAngles:[45,0,0]}];
 Object.assign(garrison,{MaintainCrewSeats_S:()=>true,HasMountedDefence:()=>false,m_bLocalDefencePatrols:true,m_iInfantryOrderInterval:60,m_Location:{GetId:()=>'site',IsPositionInside:()=>true}});
 const pending={m_iId:12,m_aSlots:[0],m_Group:{AddWaypoint:w=>waypoints.push(w),RemoveWaypoint:()=>{}},m_bOrdered:false,m_bMountedDefence:false,m_iNextFootOrder:0};
 printed=[];garrison.CrewAndOrder(pending);
 check(teleports.length===1&&teleports[0][3][1]===ground+3,'a sentry spawned at ground level is lifted onto his platform');
 check(!waypoints.length&&pending.m_bOrdered,'a sentry gets no movement orders');
 check(printed.some(p=>p.includes('sentry holding post 3m above ground')),'the sentry log reports his height above ground');
 pending.m_bOrdered=false;garrison.CrewAndOrder(pending);
 check(teleports.length===1,'a sentry already on his post is not moved again');
 const groundPost={origin:[-30,ground,15],GetOrigin(){return this.origin;},Teleport(t){teleports.push(t);}};
 garrison.m_aSlots=[{m_iKind:0,m_iVariant:1001,m_Entity:groundPost,m_vPosition:[-30,ground,15],m_vAngles:[270,0,0]}];
 pending.m_bOrdered=false;garrison.CrewAndOrder(pending);
 check(teleports.length===1&&!waypoints.length,'a bunker sentry stays where he spawned');
 garrison.m_aSlots=[{m_iKind:0,m_iVariant:1000,m_Entity:marksman,m_vPosition:[30,ground+3,15],m_vAngles:[45,0,0]}];
 marksman.origin=[30,ground,15];garrison.IsPlayerOrForeignAsset=()=>true;pending.m_bOrdered=false;garrison.CrewAndOrder(pending);
 check(teleports.length===1,'a player or foreign actor is never teleported');
 return {assertions,note:'Native geometry, spawning, teleport and AI behaviour mocked; Workbench Play acceptance required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path');
 console.log(runPostsAndPadsTests(fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Garrisons/AROR_GarrisonComponent.c'),'utf8')));
}
