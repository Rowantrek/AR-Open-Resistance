// Reproducible Node entry point. Passing mocked policy tests is not native release approval.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),{spawnSync}=require('child_process');
const root=path.resolve(__dirname,'..');
const suites=[
 'Test-MarsAndAttacks.js','Test-OperationAlerts.js','Test-QRFReleaseLifecycle.js','Test-QRFInterruptions.js','Test-StrongholdIntegration.js','Test-QRFManagerSoak.js','Test-QRFMutations.js','Test-QRFRouteAudit.js',
 'Test-QRFStrategy.js','Test-QRFProgression.js','Test-QRFResponseMix.js','Test-ReaperLandingSearch.js',
 'Test-QRFBoarding.js','Test-SessionFixes.js','Test-QRFIngress.js','Test-QRFUnloading.js','Test-QRFRetention.js',
 'Test-QRFSupportAndHeadlines.js','Test-QRFVehicleFaction.js','Test-QRFTownHold.js',
 'Test-EveronQRFIntegration.js','Test-InvaderCampaign.js','Test-RebIntel.js','Test-MissionLifecycle.js',
 'Test-CapturePlacement.js','Test-GarrisonsAndSupport.js','Test-GarrisonCrew.js',
 'Test-OutpostEquipment.js','Test-PostsAndPads.js','Test-DefenceLayout.js','Test-FortifiedOutpostLayouts.js',
 'Test-DefenceSiteAudit.js','Test-DefenceWallGrounding.js','Test-CampaignFlagEditor.js'
];
const start=Date.now();
const results=suites.map(name=>{
 const began=Date.now(),result=spawnSync(process.execPath,[path.join(__dirname,name)],{cwd:root,encoding:'utf8',timeout:120000});
 const output=(result.stdout||'')+(result.stderr||'');
 const assertions=Number(output.match(/["']?assertions["']?\s*:\s*(\d+)/)?.[1]||0);
 // Some GUI-hosted Node launchers return zero even for an uncaught exception.
 // Every suite must finish its assertion report; a zero exit status alone is insufficient.
 const passed=result.status===0&&!result.error&&assertions>0&&!/\n(?:\w*Error):/.test(output);
 console.log(`${passed?'PASS':'FAIL'} ${name} (${assertions} assertions)`);
 return {name,passed,assertions,elapsedMs:Date.now()-began,exitCode:result.status,output,error:result.error?.message};
});
const sourceHashes={};
for(const folder of ['Scripts/Game','Scripts/WorkbenchGame','Prefabs','Worlds/AROR_Everon/AROR_Everon_Chicken_Layers','UI','Configs']){
 const dir=path.join(root,'AROpenResistance/Open Resistance',folder);
 function visit(directory){for(const entry of fs.readdirSync(directory,{withFileTypes:true})){
  const file=path.join(directory,entry.name);
  if(entry.isDirectory())visit(file);
  else if(/\.(c|et|layer|layout|conf|meta)$/.test(file))sourceHashes[path.relative(root,file).split(path.sep).join('/')]=crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
 }}
 visit(dir);
}
for(const name of fs.readdirSync(__dirname))if(/\.(js|ps1)$/.test(name))sourceHashes['tools/'+name]=crypto.createHash('sha256').update(fs.readFileSync(path.join(__dirname,name))).digest('hex');
for(const name of ['FORCE_ASSETS.json','MILITARY_DEFENCE_PRESETS.json','EVERON_CAMPAIGN_LOCATIONS.csv','QRF_PRICES.csv'])sourceHashes['docs/'+name]=crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'docs',name))).digest('hex');
const artifactHashes={};
for(const name of fs.readdirSync(path.join(root,'docs/validation')))if(name!=='QRF_AUTOMATED_RESULTS.json'&&/\.(csv|json)$/.test(name))artifactHashes[name]=crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'docs/validation',name))).digest('hex');
const report={schemaVersion:2,scope:'Chicken QRFs and attacks, with capture/garrison regressions',runtime:process.version,
 runtimeHost:process.versions.electron?'Electron '+process.versions.electron+' in Node mode':'Node.js',nodeRunnerExecuted:true,
 nativeCompilation:{status:'NOT_RUN_BY_RUNNER',evidence:'Compilation history and current native acceptance status are recorded in docs/QRF_RELEASE_VALIDATION.md.'},releaseApproved:false,
 generatedAt:new Date().toISOString(),elapsedMs:Date.now()-start,nativeVerified:false,
 passed:results.every(r=>r.passed),assertions:results.reduce((n,r)=>n+r.assertions,0),suites:results,sourceHashes,artifactHashes};
const output=path.join(root,'docs/validation/QRF_AUTOMATED_RESULTS.json');
fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(`${results.filter(r=>r.passed).length}/${results.length} suites passed. Native Workbench/dedicated-server acceptance remains required.`);
process.exitCode=report.passed?0:1;
