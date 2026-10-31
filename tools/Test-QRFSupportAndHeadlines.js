// Execute the changed Enforce method bodies with UI/flight/navigation services mocked.
// These checks do not compile scripts or certify native aircraft handling or driving.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
const air=read('Operations/AROR_ReaperQRFExecutor.c'),ground=read('Operations/AROR_OperationExecutorComponent.c');
const ui=read('Notifications/AROR_NotificationDisplay.c');
let assertions=0;
const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
const distance=(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2]);
Object.defineProperty(String.prototype,'Split',{value(sep,out){out.push(...this.split(sep));},configurable:true});
const math={Max:Math.max,Min:Math.min,Ceil:Math.ceil,Atan2:Math.atan2,Sin:Math.sin,Cos:Math.cos};
const env={Math:math,Vector:(...v)=>v,vector:{DistanceXZ:distance},
  Print(){},LogLevel:{NORMAL:0,WARNING:1},string:{Format:(s,...a)=>s.replace(/%(\d+)/g,(_,n)=>a[n-1])},
  AROR_TextUtils:{CharCount:s=>Array.from(s).length},
  FrameSlot:{SetSizeToContent:(w,v)=>w.auto=v,SetSize:(w,width,height)=>Object.assign(w,{width,height})},
  AROR_ReaperAirForce:{Controller:v=>v.controller}};
let viewport=1920;
env.GetGame=()=>({GetWorkspace:()=>({DPIUnscale:x=>x}),GetWorld:()=>({GetSurfaceY:()=>100})});
env.AROR_ReaperFlightGeometry={Direction:()=>[1,0,0]};
const names=['EstimateHeadlineLineWidth','HeadlineTextWidth','PlaceHeadlineText','Place','ReaperGunshipHeight','ReaperGunshipRunHeight','ReaperMovePrefab','ReaperOrder','FindTacticalApproach','ReplaceTacticalWaypoint','GunshipTrafficConflict','RetireReaperEscort'];
function compile(src,name){
  const m=src.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!m)throw Error('Missing '+name);
  const args=m[1].replace(/\b(?:string|TextWidget|int|float|bool|AROR_\w+)\s+/g,'');
  let body=m[2].replace(/array<string> (\w+) = \{\};/g,'let $1=[];')
    .replace(/foreach \(string (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
    .replace(/float screenWidth, screenHeight;/g,'let screenWidth=0,screenHeight=0;')
    .replace('m_wRoot.GetScreenSize(screenWidth, screenHeight);','screenWidth=viewport;')
    .replace(/\b(?:bool|float|int|vector|IEntity|REAPER_AiHelicopterControllerComponent)\s+(\w+)/g,'let $1')
    .replace(/let (\w+);/g,'let $1=0;')
    .replace(/force.m_vGunshipSearch ([+-]) force.m_vGunshipRunDirection \* (\w+)/g,'force.m_vGunshipSearch.map((v,i)=>v $1 force.m_vGunshipRunDirection[i]*$2)')
    .replace('position + away * 250','position.map((v,i)=>v+away[i]*250)')
    .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
    .replace(/(?<![.\w])HEADLINE_\w+/g,x=>'this.'+x);
  for(const n of names)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
  return new Function(...Object.keys(env),'viewport','return function('+args+'){'+body+'}')( ...Object.values(env),viewport);
}
// Exercise the real helper against UTF-8 byte adapters, including signed native-byte representations.
const countCharacters=compile(read('Common/AROR_TextUtils.c'),'CharCount');
const utf8=(text,signed)=>{const bytes=require('buffer').Buffer.from(text,'utf8');return {Length:()=>bytes.length,Get:i=>({ToAscii:()=>signed&&bytes[i]>=128?bytes[i]-256:bytes[i]})}};
for(const signed of [false,true])for(const [text,count]of [['HQ',2],['ГОТОВНОСТЬ',10],['A😀Я',3]])
 check(countCharacters(utf8(text,signed))===count,'actual CharCount counts UTF-8 code points for signed='+signed);
env.AROR_TextUtils.CharCount=text=>countCharacters(utf8(text,false));
const widget=text=>({text,GetText(){return this.text;},SetTextWrapping(v){this.wrap=v;}});
function banner(screen,title,extras={}){
  viewport=screen;
  const b={Place:(w,x,y,width,height)=>{if(w)Object.assign(w,{x,y,width,height});},
    m_bCurrentHasIcon:!!extras.icon,m_bHeadlineHasDetail:!!extras.detail,m_bHeadlineHasIntercept:!!extras.intercept,m_bHeadlineHasEffect:!!extras.effect};
  for(const [,key,value]of ui.matchAll(/const (?:float|int) (HEADLINE_\w+) = (\d+);/g))b[key]=Number(value);
  for(const n of ['Headline','HeadlineBand','HeadlineHeader','HeadlineIcon','HeadlineLineTop','HeadlineLineBottom','HeadlineLineLeft','HeadlineLineRight','HeadlineTimer'])b['m_w'+n]={};
  for(const [n,text]of Object.entries({Text:title,Detail:extras.detail||'',Intercept:extras.intercept||'',Effect:extras.effect||'',Source:extras.source||'HQ',Time:'12:09'}))b['m_wHeadline'+n]=widget(text);
  for(const n of ['EstimateHeadlineLineWidth','HeadlineTextWidth','PlaceHeadlineText'])b[n]=compile(ui,n);
  b.layout=compile(ui,'LayoutHeadline');b.layout(.5);return b;
}
for(const screen of [480,711,1280,1920]){
  const b=banner(screen,'TERRITORY UNDER ATTACK / NORTH RIDGE WORKS',{icon:true,detail:'Defend the occupied military installation.',intercept:'Enemy movement reported nearby.',effect:'Responding forces are approaching.'});
  const width=b.m_wHeadline.width;
  check(width<=Math.min(900,screen-48),'banner stays inside viewport '+screen);
  for(const n of ['Text','Detail','Intercept','Effect','Source','Time']){
    const w=b['m_wHeadline'+n];
    check(w.x>=0&&w.x+w.width<=width+.01&&w.y+w.height<=b.m_wHeadline.height,'row fits banner: '+screen+' '+n);
    check(w.wrap&&!w.auto,'headline row has explicit wrapping dimensions');
  }
  const height=b.m_wHeadline.height;b.layout(.2);
  check(b.m_wHeadline.width===width&&b.m_wHeadline.height===height,'repeated layout does not shrink or oscillate');
}
const long=banner(711,'TERRITORY UNDER ATTACK / '+'NORTH RIDGE '.repeat(20),{source:'ГОТОВНОСТЬ / SOVIET ARMY',detail:'First line\nSecond line'});
check(long.m_wHeadlineText.height>26*1.3,'long title reserves multiple rows');
check(long.m_wHeadlineDetail.y>=long.m_wHeadlineText.y+long.m_wHeadlineText.height,'detail follows the entire wrapped title');
check(long.m_wHeadlineDetail.height>=2*15*1.3,'explicit newlines retain separate rows');
const short=banner(1920,'MISSION COMPLETE');check(short.m_wHeadline.width===520,'short banner retains default width');
const cyrillic=banner(480,'ТЕРРИТОРИЯ ПОД АТАКОЙ / '+'СЕВЕРНЫЙ ХРЕБЕТ '.repeat(10),{source:'ГОТОВНОСТЬ / СОВЕТСКАЯ АРМИЯ'});
const sample='ГОТОВНОСТЬ';
const estimated=cyrillic.EstimateHeadlineLineWidth(sample,26);
check(cyrillic.HeadlineTextWidth(widget(sample),26)===estimated,'natural width uses the same estimate as wrapping');
const sampleWidget=widget(sample);
check(cyrillic.PlaceHeadlineText(sampleWidget,26,0,0,estimated)===26*1.3,'exact estimated line width fits one row');
check(cyrillic.m_wHeadlineSource.y+cyrillic.m_wHeadlineSource.height<=cyrillic.m_wHeadlineText.y,'long Cyrillic source reserves header space');
const oldHeight=short.m_wHeadline.height;
short.m_wHeadlineText.text='TERRITORY UNDER ATTACK / '+'NORTH RIDGE '.repeat(12);short.m_bHeadlineLayoutDirty=true;short.layout(.4);
check(short.m_wHeadline.height>oldHeight,'new headline invalidates cached geometry');
const oldWidth=short.m_wHeadline.width,oldLongHeight=short.m_wHeadline.height;
viewport=480;short.layout=compile(ui,'LayoutHeadline');short.layout(.3);
check(short.m_wHeadline.width<oldWidth&&short.m_wHeadline.height>oldLongHeight,'viewport change recomputes wrapping');
const timerWidth=short.m_wHeadlineTimer.width;short.layout(.1);
check(short.m_wHeadlineTimer.width<timerWidth,'cached geometry still animates the lifetime bar');

let orders=[];
const aircraft={GetOrigin:()=>[0,500,0],controller:{AROR_ConfigureQRF(...args){aircraft.config=args;}}};
const e={m_iExecutorClock:10,m_fReaperGunshipRunHeight:110,m_fReaperGunshipRunBand:20,m_fReaperGunshipRunRadius:450,
  m_fReaperFriendlyClearance:60,ReaperMovePrefab:()=> 'move',ReaperOrder:(...args)=>{orders.push(args);return true;}};
e.ReaperGunshipRunHeight=compile(air,'ReaperGunshipRunHeight');e.pass=compile(air,'IssueReaperPass');
const f={m_vHeading:[0,0,1],m_iPassOrdinal:0,m_iPackageIndex:0,m_iPackageCount:3,m_bGunshipRunIn:false,
  m_vGunshipSearch:[1000,50,2000],m_Escort:{m_Vehicle:aircraft},m_sFactionKey:'US',m_sOperationId:'test'};
check(e.pass(f),'run-in order accepted');const entry=[...f.m_vPassPoint];
check(f.m_bGunshipRunIn&&f.m_iPassOrdinal===0,'run-in does not count as crossing');
check(Math.abs(distance(entry,f.m_vGunshipSearch)-450)<.01,'run-in lies at configured radius');
check(orders.at(-1)[4]===110&&orders.at(-1)[1]==='move','search uses lower moving order, never hover');
check(e.pass(f),'crossing order accepted');const exit=[...f.m_vPassPoint];
check(!f.m_bGunshipRunIn&&f.m_iPassOrdinal===1,'crossing completes one run cycle');
check(entry.every((v,i)=>Math.abs((v+exit[i])/2-f.m_vGunshipSearch[i])<.01),'entry and exit cross the search centre');
e.pass(f);check(distance(entry,f.m_vPassPoint)>100,'next run changes observation axis');
f.m_iPackageIndex=2;e.pass(f);check(orders.at(-1)[4]===150,'three packages use 110/130/150 m support bands');
check(aircraft.config.at(-1)===true,'support retains native engagement filtering');
check(air.includes('<= 80')&&!air.includes('currentHeight + 40'),'pass endpoint gate tightened; repeated 40 m climbs removed');
e.m_fReaperGunshipHeight=210;e.m_fReaperGunshipBand=35;
e.ReaperGunshipHeight=compile(air,'ReaperGunshipHeight');
let conflict={GetOrigin:()=>[0,500,10]};
e.GunshipTrafficConflict=()=>conflict;e.RetireReaperEscort=()=>{throw Error('Unexpected retirement');};
const yieldTraffic=compile(air,'YieldGunshipTraffic');f.m_iPackageIndex=0;
check(yieldTraffic.call(e,f)&&orders.at(-1)[4]===210,'high aircraft yields laterally at cruise band instead of ratcheting higher');
check(aircraft.config.at(-1)===false,'yield disables engagement');
let count=orders.length;e.m_iExecutorClock=11;yieldTraffic.call(e,f);check(orders.length===count,'six-second yield does not reorder every tick');
conflict=null;e.m_iExecutorClock=16;
check(!yieldTraffic.call(e,f)&&!f.m_bGunshipRunIn&&!f.m_bTrafficYielding,'clear traffic resets run-in for resumption');
e.pass(f);check(orders.at(-1)[4]===110&&aircraft.config.at(-1)===true,'support resumes at lower run altitude with native engagement');

const recover=compile(ground,'RecoverStalledQRFRoute');let position=[1000,0,0],retries=[];
const force={m_iTacticalNextRouteCheck:0,m_iTacticalRouteRetries:0,m_vTacticalProgressPosition:[950,0,0],m_fTacticalBestDistance:-1,
  m_iTacticalLastAdvance:0,m_Vehicle:{GetOrigin:()=>position},RestoreNearbyDriver_S:()=>false,m_sOperationId:'test',m_iPackageIndex:0};
const ctx={m_iExecutorClock:0,m_iQRFDetourTimeout:90,m_fQRFObjectiveDeliveryRadius:100,m_sMoveWaypointPrefab:'move',
  FindTacticalApproach(...args){retries.push(args);return true;},ReplaceTacticalWaypoint:()=>true};
const commander={m_vTown:[0,0,0]};
recover.call(ctx,force,commander);check(retries.length===0,'moving route establishes a progress baseline');
for(let tick=30;tick<=60;tick+=30){ctx.m_iExecutorClock=tick;position=[1000+tick,0,0];recover.call(ctx,force,commander);}
check(retries.length===0,'normal detours get a grace period');
ctx.m_iExecutorClock=90;position=[1090,0,0];recover.call(ctx,force,commander);
check(retries.length===1&&retries[0].at(-1)===true,'moving without closing triggers a road/foot approach after 90 s');
ctx.m_iExecutorClock=91;recover.call(ctx,force,commander);check(retries.length===1,'route checks remain throttled');
force.m_iTacticalRouteRetries=4;ctx.m_iExecutorClock=1000;recover.call(ctx,force,commander);check(retries.length===1,'four-retry ceiling retained');
force.m_iTacticalRouteRetries=0;force.m_fTacticalBestDistance=1000;force.m_iTacticalLastAdvance=0;force.m_iTacticalNextRouteCheck=0;
ctx.m_iExecutorClock=200;position=[900,0,0];recover.call(ctx,force,commander);
check(retries.length===1&&force.m_iTacticalLastAdvance===200,'net objective progress resets detour clock');
console.log(JSON.stringify({assertions,note:'Native banner rendering, terrain flight, target spotting and road navigation require Workbench compile and Play.'}));
