// Source/mock and installed-resource validation only. Workbench is the native compiler/runtime.
const fs=require('fs'),path=require('path'),cp=require('child_process');
const root=path.resolve(__dirname,'..'),date='2026-11-09';
const auditPath=`docs/validation/WCS_ASSET_AUDIT_${date}.json`;
const env={...process.env,ELECTRON_RUN_AS_NODE:'1',AROR_WCS_AUDIT_REPORT:auditPath};
function run(name,args=[],completion=/./){
 const ps=name.endsWith('.ps1');
 const result=cp.spawnSync(ps?'powershell.exe':process.execPath,
  ps?['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(__dirname,name),...args]:[path.join(__dirname,name),...args],
  {cwd:root,env,encoding:'utf8',timeout:180000,windowsHide:true});
 const output=((result.stdout||'')+(result.stderr||'')).replace(/^.*crashpad.*(?:\r?\n|$)/gm,'').trim();
 const passed=result.status===0&&!result.error&&completion.test(output)&&!/^\w*Error(?: \[[^\]]+\])?:/m.test(output);
 console.log(`${passed?'PASS':'FAIL'} ${name}`);
 return {name,passed,exitCode:result.status,error:result.error?.message,output};
}
const results=[];
// Refresh the resource evidence, and require its summary so a GUI launcher cannot hide a crashed audit.
results.push(run('Audit-WCSAssets.js',[auditPath],/"references":\s*\d+/));
results.push(run('Test-WCSIntegration.js',[],/"assertions":\s*\d+/));
results.push(run('Test-PresetVehicles.js',[],/Preset vehicle selection.*checks passed/));
results.push(run('Validate-QRFRelease.js',[],/\d+\/35 suites passed/));
results.push(run('Test-QRFSourceStructure.ps1',[],/Checked balanced delimiters/));
results.push(run('Test-ExecutorSettings.ps1',[],/Executor settings OK/));
results.push(run('Test-OperationUIResources.ps1',[],/Verified metadata/));
const addonRoot=path.join(process.env.USERPROFILE,'OneDrive/Documents/My Games/ArmaReforger/addons');
const reaper=fs.readdirSync(addonRoot).find(n=>n.endsWith('_69A4D664A6284E06'));
if(reaper)results.push(run('Test-ReaperAssets.ps1',['-ResourceDatabasePath',path.join(addonRoot,reaper,'resourceDatabase.rdb')],/Verified|Validated|OK|matched|references/i));
else results.push({name:'Test-ReaperAssets.ps1',passed:false,error:'Installed REAPER resource index not found'});
const qrf=JSON.parse(fs.readFileSync(path.join(root,'docs/validation/QRF_AUTOMATED_RESULTS.json'),'utf8'));
const audit=JSON.parse(fs.readFileSync(path.join(root,auditPath),'utf8'));
const report={generatedAt:new Date().toISOString(),nativeVerified:false,releaseApproved:false,
 scope:'Actual-source methods with mocked engine services, configuration overlays, installed dependency/resource indexes and lexical/UI binding checks. No Enforce compilation, flight, inventory loading or dedicated-server Play.',
 passed:results.every(r=>r.passed),results,
 qrf:{passedSuites:qrf.suites.filter(s=>s.passed).length,totalSuites:qrf.suites.length,assertions:qrf.assertions,
 failures:qrf.suites.filter(s=>!s.passed).map(s=>s.name)},
 assets:{summary:audit.summary,missingDependencies:audit.missingDependencies},
 workbench:'Compile Game scripts, then Play AROR_Everon_Chicken with combined WCS/RHS compatibility and REAPER; repeat on a dedicated server with two clients and a late joiner.'};
fs.writeFileSync(path.join(root,`docs/validation/TOWN_GAMEPLAY_RESULTS_${date}.json`),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({passed:report.passed,qrf:report.qrf,resources:audit.summary.references,nativeVerified:false}));
process.exitCode=report.passed?0:1;
