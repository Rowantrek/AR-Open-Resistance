// Actual-source landing planner/clearance checks with mocked terrain and collision.
const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
const geometry=fs.readFileSync(path.join(root,'AROR_ReaperFlightGeometry.c'),'utf8');
const search=fs.readFileSync(path.join(root,'AROR_ReaperLandingSearch.c'),'utf8');
let assertions=0,terrain=()=>5,ocean=()=>0,blocked=false,traces=0,adjustment=null,adjustmentCalls=0;
const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
for(const [name,fn]of Object.entries({Clear(){this.length=0;},Count(){return this.length;},Insert(x){this.push(x);}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
const world={GetSurfaceY:(x,z)=>terrain(x,z),GetOceanHeight:(x,z)=>ocean(x,z),TraceMove:()=>{traces++;return blocked?0:1;}};
const G={LANDING_CLEAR:0,LANDING_WATER:1,LANDING_SLOPE:2,LANDING_OBSTACLE:3};
const env={AROR_ReaperFlightGeometry:G,GetGame:()=>({GetWorld:()=>world}),Vector:(x,y,z)=>[x,y,z],
  SCR_WorldTools:{FindEmptyTerrainPosition:(out,seed,radius,clearance,attempts)=>{adjustmentCalls++;if(!adjustment)return false;out.splice(0,3,...adjustment);return true;}},
  TraceBox:class{},TraceFlags:{ENTS:1},Math:{Clamp:(v,a,b)=>Math.max(a,Math.min(b,v)),Max:Math.max,AbsFloat:Math.abs,Sin:Math.sin,Cos:Math.cos,Lerp:(a,b,t)=>a+(b-a)*t},
  vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
  string:{Format:(s,...a)=>s.replace(/%(\d+)/g,(_,i)=>a[i-1])},...G};
function compile(source,name){
  const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!m)throw Error('Missing '+name);
  const args=m[1].replace(/\bout\s+/g,'').replace(/array<\w+>\s+|\b(?:int|float|bool|vector)\s+/g,'');
  let body=m[2].replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
    .replace(/\b(?:int|float|bool|string|vector|BaseWorld|TraceBox)\s+(\w+)/g,'let $1')
    .replace(/let (\w+);/g,'let $1=0;')
    .replace('let adjusted=0;','let adjusted=[0,0,0];')
    .replace(/let (ring|outerRing) = (index|outer) \/ 12 \+ 1;/g,'let $1 = Math.floor($2 / 12) + 1;')
    .replace(/center \+ Vector\(Math.Sin\((\w+)\), 0, Math.Cos\(\1\)\) \* (\w+)/g,'center.map((v,i)=>v+[Math.Sin($1)*$2,0,Math.Cos($1)*$2][i])')
    .replace('center + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (maxRadius * ring / 6)','center.map((v,i)=>v+[Math.Sin(angle)*maxRadius*ring/6,0,Math.Cos(angle)*maxRadius*ring/6][i])')
    .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
    .replace(/(?<![.\w])(CandidateClear|CandidateHeight|AdjustCandidate)\(/g,'this.$1(');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
}
env.Math.floor=Math.floor;
for(const name of ['LandingCandidate','HasPadSpacing','LandingRejectionReason'])G[name]=compile(geometry,name);
const reason=G.LandingRejectionReason;
check(reason([0,0,0],18)===G.LANDING_CLEAR,'flat unobstructed land accepted');
terrain=()=>0;check(reason([0,0,0],18)===G.LANDING_WATER,'water rejected');
terrain=(x,z)=>x<-8?0:2;check(reason([0,0,0],18)===G.LANDING_WATER,'pad overlapping shoreline rejected even if centre is dry');
terrain=x=>100+x/5;check(reason([0,0,0],18)===G.LANDING_SLOPE,'steep pad rejected');
terrain=()=>5;blocked=true;check(reason([0,0,0],18)===G.LANDING_OBSTACLE,'blocked rotor corridor rejected');blocked=false;
const near=[],outer=[];
for(let i=0;i<121;i++){
  const p=G.LandingCandidate([0,0,0],400,i,1200),r=Math.hypot(p[0],p[2]);
  if(i<73){near.push(p);check(r<=400.001,'original near candidate remains inside 400 m');}
  else{outer.push(p);check(r>400&&r<=1200.001,'fallback stays between original search and 1.2 km');}
}
check(new Set([...near,...outer].map(p=>p.map(x=>x.toFixed(3)).join(','))).size===121,'all candidates unique');
const methods={};for(const name of ['Initialize','SetReservedPads','Advance','CandidateClear','CandidateHeight','AdjustCandidate','GetDiagnostics'])methods[name]=compile(search,name);
function planner(radius=400,fallback=1200,desired=1){const p={...methods,m_aResults:[],m_aReserved:[]};p.Initialize([0,0,0],radius,18,desired,90,fallback);return p;}
function run(p){for(let tick=0;tick<200&&!p.m_bComplete;tick++)p.Advance(tick);}
terrain=(x,z)=>Math.hypot(x,z)<450?0:5;
let p=planner();run(p);check(p.m_bFound&&p.m_aResults.length===1,'coastal inner rejection can find safe inland fallback');
check(p.m_iRejectedWater===73&&p.m_iCursor===74,'all original candidates tried before fallback');
check(Math.hypot(p.m_aResults[0][0],p.m_aResults[0][2])>400,'selected pad is outside failed search');
p=planner(400,1200,5);run(p);check(p.m_aResults.length===5,'fallback can supply full five-package force');
for(let i=0;i<p.m_aResults.length;i++)for(let j=0;j<i;j++)check(env.vector.DistanceXZ(p.m_aResults[i],p.m_aResults[j])>=90,'fleet pad spacing retained');
terrain=()=>0;p=planner();p.Advance(0);const count=p.m_iCursor;p.Advance(0);
check(count===2&&p.m_iCursor===2,'repeated callers cannot exceed two checks per tick');
run(p);check(p.m_bComplete&&!p.m_bFound&&p.m_iCursor===121,'impossible search stops after bounded 121 candidates');
check(p.m_iRejectedWater===121&&p.GetDiagnostics().includes('water=121'),'terminal diagnostics explain water rejection');
const done=p.m_iCursor;p.Advance(201);check(p.m_iCursor===done,'completed failures never rescan');
p=planner(400,0);run(p);check(p.m_iCursor===73,'legacy no-fallback caller retains original budget');
terrain=()=>5;p=planner();p.SetReservedPads([[0,5,0]]);p.Advance(0);
check(p.m_iRejectedSpacing>0,'reserved pads remain excluded');
p=planner();run(p);check(p.m_iCursor===1&&p.m_bFound,'clear centre avoids unnecessary outer queries');
terrain=x=>1000+x/5;p=planner();run(p);check(!p.m_bFound&&p.m_iRejectedSlope===121,'outer search never weakens slope safety');
terrain=()=>5;blocked=true;p=planner();run(p);check(!p.m_bFound&&p.m_iRejectedObstacle===121,'outer search never weakens obstacle safety');
check(traces>0,'native corridor trace exercised through mock');
// A native local result is only a suggestion: all safety and spacing tests still apply.
blocked=false;terrain=()=>5;p=planner();adjustment=[20,5,0];let out=[0,0,0];
check(p.AdjustCandidate([0,5,0],out)&&p.m_iAdjustedPads===1,'nearby clear dry pad can be accepted');
adjustment=[100,5,0];check(!p.AdjustCandidate([0,5,0],out),'local result outside adjustment radius rejected');
adjustment=[1220,5,0];check(!p.AdjustCandidate([1190,5,0],out),'local result cannot escape overall search radius');
adjustment=[20,5,0];p.SetReservedPads([[20,5,0]]);check(!p.AdjustCandidate([0,5,0],out),'local result cannot steal reserved landing pad');p.SetReservedPads([]);
blocked=true;check(!p.AdjustCandidate([0,5,0],out),'native suggestion cannot bypass obstacle recheck');
blocked=false;terrain=()=>0;check(!p.AdjustCandidate([0,5,0],out),'native suggestion cannot bypass water recheck');
terrain=x=>1000+x/5;check(!p.AdjustCandidate([0,5,0],out),'native suggestion cannot bypass slope recheck');
terrain=()=>5;p.m_fAdjustmentRadius=0;const before=adjustmentCalls;check(!p.AdjustCandidate([0,5,0],out)&&adjustmentCalls===before,'local correction can be disabled');
console.log(JSON.stringify({assertions,note:'Native Lamentin terrain and REAPER landings still require Workbench Play.'}));
