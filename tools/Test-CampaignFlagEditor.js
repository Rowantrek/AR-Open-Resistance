/* Tests actual editor-plugin methods with a mocked loaded terrain/editor source API.
 * Native Workbench compilation and audit still required.
 */
function runCampaignFlagEditorTests(source) {
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  let edit=true,prefab=false,path='Worlds/AROR_Everon/AROR_Everon_Chicken.ent',fraction=0.5;
  let begin=0,end=0,writes=[],roots=[],messages=[];
  const world={GetSurfaceY:()=>250,GetOceanHeight:()=>0,TraceMove:t=>t.Flags===1?fraction:1};
  const api={IsEditMode:()=>edit,IsPrefabEditMode:()=>prefab,GetEditorEntityCount:()=>roots.length,
    GetEditorEntity:i=>roots[i],SourceToEntity:s=>s.entity,EntityToSource:e=>e.source,
    BeginEntityAction:()=>{begin++;return true;},EndEntityAction:()=>end++,GetWorld:()=>world,
    SetVariableValue:(s,p,k,v)=>{writes.push({s,k,v});return true;}};
  const env={Workbench:{GetModule:()=>({GetApi:()=>api})},WorldEditor:{},
    Print:s=>messages.push(s),LogLevel:{NORMAL:0,WARNING:1,ERROR:2},
    string:{Format:(s,...args)=>s.replace(/%(\d+)/g,(_,n)=>String(args[n-1]))},
    TraceFlags:{WORLD:1,ENTS:2},Vector:(...v)=>v,Math:{Min:Math.min}};
  function compile(name){
    const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!m)throw Error('Missing '+name);
    const args=m[1].replace(/\bout\s+/g,'').replace(/\b(?:bool|float|vector|IEntity|WorldEditorAPI|BaseWorld)\s+/g,'').replace('[4]','');
    let body=m[2].replace(/array<IEntitySource> roots = \{\};/g,'let roots=[];')
      .replace(/new Trace(?:Param|Box)\(\)/g,'({})').replace(/vector (\w+)\[4\];/g,'let $1=[];')
      .replace(/vector mins, maxs;/g,'let mins=[],maxs=[];')
      .replace(/vector desired = before;/g,'let desired=[...before];')
      .replace(/vector ground = desired;/g,'let ground=[...desired];')
      .replace(/\b(?:bool|int|float|vector|IEntity|IEntitySource|IEntityComponentSource|WorldEditorAPI|WorldEditor|BaseWorld|TraceParam|TraceBox|string)\s+(\w+);/g,'let $1;')
      .replace(/\b(?:bool|int|float|vector|IEntity|IEntitySource|IEntityComponentSource|WorldEditorAPI|WorldEditor|BaseWorld|TraceParam|TraceBox|string)\s+(\w+) =/g,'let $1 =')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
      .replace(/\.StartsWith\(/g,'.startsWith(').replace(/\.Contains\(/g,'.includes(')
      .replace(/roots.Count\(\)/g,'roots.length').replace(/roots.Insert\(/g,'roots.push(')
      .replace(/api.GetWorldPath\(path\);/g,'path=api.worldPath();')
      .replace(/GroundHeight\(world, flag, before, height\)/g,'(height=this.groundHeight, this.groundValid)')
      .replace(/(?<![.\w])(FindFlag|IsClear|ModelBottom)\(/g,'this.$1(')
      .replace('position + Vector(0, 0.15, 0)','[position[0],position[1]+0.15,position[2]]')
      .replace('trace.Start + Vector(0, 0.05, 0)','[trace.Start[0],trace.Start[1]+0.05,trace.Start[2]]');
    if(name==='GroundHeight')body=body.replace('return true;','return {ok:true,height};').replace('return false;','return {ok:false,height};')
      .replace('return height > world.GetOceanHeight(position[0], position[2]) + 0.25;','return {ok:height > world.GetOceanHeight(position[0], position[2]) + 0.25,height};');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  api.worldPath=()=>path;
  const tool={groundHeight:250,groundValid:true};
  for(const name of ['FindFlag','IsClear','ModelBottom','RunAudit'])tool[name]=compile(name);
  for(let i=0;i<41;i++){
    const rootSource={GetName:()=>'AROR_Location_everon_site_'+i};
    const flagSource={GetComponentCount:()=>1,GetComponent:()=>({GetClassName:()=>'AROR_CaptureFlagPlacementComponent'})};
    const parent={CoordToLocal:p=>[p[2]-300,p[1]-200,100-p[0]]}; // rotated parent
    const flag={source:flagSource,GetParent:()=>parent,GetSibling:()=>null,
      GetWorldTransform:t=>{t[0]=[1,0,0];t[1]=[0,1,0];t[2]=[0,0,1];t[3]=[100,0.1,300];},
      GetBounds:(min,max)=>{min.splice(0,3,-0.1,0,-0.1);max.splice(0,3,0.1,7,0.1);}};
    rootSource.entity={GetChildren:()=>flag};flagSource.entity=flag;roots.push(rootSource);
  }
  tool.RunAudit(false);check(writes.length===0&&begin===0&&end===0,'read-only audit never edits or opens an undo action');
  check(messages.filter(m=>m.startsWith('[AROR EDITOR FLAG] name=')).length===41,'audit reports every campaign flag');
  tool.RunAudit(true);check(writes.length===41&&begin===1&&end===1,'all 41 source edits grouped into one undo action');
  check(writes.every(w=>w.k==='coords'&&Math.abs(Number(w.v.split(' ')[1])-50.1)<1e-8),'world terrain converted through rotated parent into local child source coords');
  check(writes.every(w=>w.s.GetComponentCount()===1),'writes target flag sources rather than parent anchors');
  const reset=()=>{writes=[];begin=0;end=0;messages=[];};
  reset();edit=false;tool.RunAudit(true);check(!writes.length&&!begin,'cannot edit while playing');edit=true;
  prefab=true;tool.RunAudit(true);check(!writes.length&&!begin,'cannot edit prefab resources');prefab=false;
  path='Worlds/AROR_Everon/AROR_Everon_Rowantrek.ent';tool.RunAudit(true);check(!writes.length&&!begin,'cannot edit Rowan scene');path='AROR_Everon_Chicken.ent';
  roots.pop();tool.RunAudit(true);check(!writes.length&&!begin,'incomplete catalogue aborts before mutation');
  const terrain=compile('GroundHeight');
  fraction=0.5;let result=terrain(world,null,[100,0,300],0);check(result.ok&&result.height===250,'physical terrain fraction yields actual world height');
  fraction=1;check(!terrain(world,null,[100,0,300],0).ok,'missing physical terrain does not count as valid grounding');
  fraction=0.7;check(!terrain(world,null,[100,0,300],0).ok,'underwater terrain rejected');
  const model=tool.ModelBottom;
  check(model([[1,0,0],[0,1,0],[0,0,1],[100,10,300]],[-1,-5,-1],[1,2,1])===5,'below-pivot mesh base measured rather than assuming pivot equals base');
  check(model([[0,1,0],[0,0,1],[1,0,0],[100,10,300]],[-2,-5,-1],[1,2,1])===8,'rotated model basis projected into world height');
  return {assertions,note:'Mocked editor API; native plugin compile, terrain collision and actual mesh placement still need Workbench.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path');
  console.log(JSON.stringify(runCampaignFlagEditorTests(fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/WorkbenchGame/Plugins/AROR_GroundCampaignFlagsPlugin.c'),'utf8'))));
}
