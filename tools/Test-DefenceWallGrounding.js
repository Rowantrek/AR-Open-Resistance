/* Actual grounding Run() with a mock that invalidates every live entity/source
 * after each write, reproducing the native prefab rebuild that broke traversal.
 */
function runDefenceWallGroundingTests(source,trialSource=null){
 let assertions=0,generation=0,edit=true,prefab=false,worldPath='AROR_Everon_Chicken.ent',allowBegin=true,missingId=-1;
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 let writes=[],logs=[],begin=0,end=0,sites=[],targetId='';
 function alive(version){if(version!==generation)throw Error('Stale live editor reference used after prefab rebuild');}
 function freshSource(id,name=''){const version=generation;return {GetID:()=>{alive(version);return id;},GetName:()=>{alive(version);return name;}};}
 const registry=new Map();
 function setup(){
  generation=0;writes=[];logs=[];begin=0;end=0;sites=[];registry.clear();
  for(let siteIndex=0;siteIndex<2;siteIndex++){
   const rootId=100+siteIndex,source=freshSource(rootId,'AROR_Location_everon_fixture_'+siteIndex),version=generation;
   const location={GetId:()=>{alive(version);return 'everon_fixture_'+siteIndex;},GetLocationType:()=>8};
   const root={FindComponent:()=>{alive(version);return location;},CoordToLocal:p=>{alive(version);return [...p];}};
   const walls=[];
   for(let i=0;i<3;i++){
    const id=siteIndex*10+i;
    const wall={id,terrain:10,groundValid:true,FindComponent:()=>{alive(version);return true;},GetParent:()=>{alive(version);return root;},
     GetWorldTransform:out=>{alive(version);out.push([1,0,0],[0,1,0],[0,0,1],[i,11,siteIndex]);},
     GetBounds:(mins,maxs)=>{alive(version);mins.push(-1,-0.8,-1);maxs.push(1,1,1);},source:freshSource(id)};
    registry.set(id,true);walls.push(wall);
   }
   walls.forEach((w,i)=>w.GetSibling=()=>{alive(version);return walls[i+1]||null;});
   root.GetChildren=()=>{alive(version);return walls[0]||null;};
   sites.push({source,root,walls});
  }
 }
 const api={IsEditMode:()=>edit,IsPrefabEditMode:()=>prefab,GetWorldPath:()=>worldPath,GetWorld:()=>({}),
  GetEditorEntityCount:()=>sites.length,GetEditorEntity:i=>sites[i].source,SourceToEntity:s=>sites.find(x=>x.source===s).root,
  EntityToSource:e=>e.source,FindEntityByID:id=>id===missingId?null:freshSource(id),
  BeginEntityAction:()=>{begin++;return allowBegin;},EndEntityAction:()=>end++,
  SetVariableValue:(s,parent,key,value)=>{const id=s.GetID();if(!registry.has(id))throw Error('Unknown source ID');writes.push({id,key,value});generation++;return true;}};
 const env={Workbench:{GetModule:()=>({GetApi:()=>api})},WorldEditor:{},AROR_LocationComponent:{Cast:x=>x},
  AROR_CaptureLocationAction:{IsCapturableType:()=>true},AROR_DefenceWallComponent:{},
  Print:message=>logs.push(message),LogLevel:{NORMAL:0,WARNING:1},
  ShouldGroundLocation:location=>!targetId||location.GetId()===targetId,
  string:{Format:(s,...values)=>s.replace(/%(\d+)/g,(_,i)=>String(values[i-1]))}};
 const match=source.match(/^\toverride void Run\(\)\r?\n\t\{([\s\S]*?)^\t\}/m);
 if(!match)throw Error('Missing actual grounding Run');
 let body=match[1]
  .replace(/array<(?:EntityID|string|vector|float)> (\w+) = \{\};/g,'let $1=[];')
  .replace(/vector (\w+)\[4\];/g,'let $1=[];').replace('vector mins, maxs;','let mins=[],maxs=[];')
  .replace(/\b(?:WorldEditorAPI|WorldEditor|IEntitySource|IEntity|BaseWorld|AROR_LocationComponent|vector|int|float|string) (\w+);/g,'let $1=0;')
  .replace(/\b(?:WorldEditorAPI|WorldEditor|IEntitySource|IEntity|BaseWorld|AROR_LocationComponent|vector|int|float|string) (\w+) =/g,'let $1 =')
  .replace(/\.StartsWith\(/g,'.startsWith(').replace(/\.Contains\(/g,'.includes(')
  .replace(/\.Insert\(/g,'.push(').replace(/\.Count\(\)/g,'.length')
  .replace('api.GetWorldPath(path);','path=api.GetWorldPath();')
  .replace('GroundHeight(world, child, transform[3], height)','(height=child.terrain,child.groundValid)')
  .replace('ModelBottom(transform, mins, maxs)','(transform[3][1]+mins[1])');
 const run=new Function(...Object.keys(env),'return function(){'+body+'}')( ...Object.values(env));
 setup();run();
 check(writes.length===6&&new Set(writes.map(w=>w.id)).size===6,'all walls are written despite a full hierarchy rebuild after every edit');
 check(writes.every(w=>w.key==='coords'&&Math.abs(Number(w.value.split(' ')[1])-10.82)<1e-8),'model-bottom clearance is applied to every wall');
 check(begin===1&&end===1,'all edits remain in one undo action');
 check(logs.some(l=>l.includes('found=6 grounded=6 warnings=0')),'summary reflects complete traversal');
 check(logs.filter(l=>l.includes('source=')).length===6,'each write logs a stable source identity');
 setup();missingId=1;run();
 check(writes.length===5&&end===1&&logs.some(l=>l.includes('warnings=1')),'a missing source warns without touching stale entities or skipping other walls');missingId=-1;
 setup();sites[0].walls[0].groundValid=false;run();
 check(writes.length===5&&logs.some(l=>l.includes('warnings=1')),'failed terrain measurement remains visible and does not invalidate traversal');
 setup();edit=false;run();check(!writes.length&&!begin,'Play mode refuses mutation');edit=true;
 setup();prefab=true;run();check(!writes.length&&!begin,'prefab editing refuses world grounding');prefab=false;
 setup();worldPath='Rowan.ent';run();check(!writes.length&&!begin,'other worlds remain untouched');worldPath='AROR_Everon_Chicken.ent';
 setup();allowBegin=false;run();check(!writes.length&&!end,'failed undo-action admission leaves measurements unapplied');allowBegin=true;
 setup();sites=[];run();check(!writes.length&&!begin&&logs.some(l=>l.includes('No attached tagged wall')),'zero walls cannot appear as a successful grounding pass');
 setup();targetId='everon_fixture_0';run();
 check(writes.length===3&&writes.every(w=>w.id<10),'targeted grounding does not write excluded sites after hierarchy rebuilds');targetId='';
 if(trialSource){
  const filter=trialSource.match(/^\toverride protected bool ShouldGroundLocation\([^\r\n]*\)\r?\n\t\{([\s\S]*?)^\t\}/m);
  if(!filter)throw Error('Missing fortified trial grounding filter');
  const types={OUTPOST:8,MILITARY_BASE:5,AIRPORT:2,FACTORY:4,RESOURCES:7,PORT:6,TOWN:0};
  const filterBody=filter[1].replace('AROR_ELocationType type =','let type =');
  const accepts=new Function('AROR_ELocationType','return function(location){'+filterBody+'}')(types);
  check(!accepts({GetId:()=> 'everon_military_outpost_chotain',GetLocationType:()=>5}),'manual Chotain castle is excluded from trial grounding');
  for(const type of Object.values(types))check(accepts({GetId:()=> 'everon_fixture',GetLocationType:()=>type})===[8,5].includes(type),'only ordinary/military outposts are grounded: '+type);
 }
 return {assertions,note:'Editor invalidation reproduced with mocks; native compile/grounding still required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path');
 const plugins=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/WorkbenchGame/Plugins');
 console.log(runDefenceWallGroundingTests(fs.readFileSync(path.join(plugins,'AROR_GroundDefenceWallsPlugin.c'),'utf8'),fs.readFileSync(path.join(plugins,'AROR_GroundFortifiedOutpostsPlugin.c'),'utf8')));
}
