// Focused audit source-method probes with mocked Enfusion services. No game-source edits.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..'),scripts=path.join(root,'AROpenResistance/Open Resistance/Scripts/Game/Operations');
const src=name=>fs.readFileSync(path.join(scripts,name+'.c'),'utf8');
for(const [name,fn] of Object.entries({Count(){return this.length},Contains(x){return this.includes(x)},IsEmpty(){return !this.length}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
function compile(file,name,args,env){
 const match=src(file).match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\([^\\r\\n]*\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
 if(!match)throw Error('Missing source method '+name);
 let body=match[1].replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/\b(?:bool|int|float|AROR_\w+)\s+(\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1=0;')
 .replace(/let extras = ([^;]+);/,'let extras = Math.trunc($1);')
 .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
 .replace(/(?<![.\w])GetReserved(?:Groups|Members|Vehicles)\(/g,x=>'this.'+x);
 body=body.replace(/(?<![.\w])(?:IsReserve|ReserveExpired)\(/g,x=>'this.'+x);
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const output=[];
const math={Max:Math.max,Min:Math.min,trunc:Math.trunc};
const extra=compile('AROR_ReaperQRFExecutor','JointExtraTransports','operation,profile,elements,groups,members,vehicles',{Math:math});
const air={m_iEscortCount:1,m_iPersonnelBudget:32,GetGroupBudget:()=>3,GetVehicleBudget:()=>2,ReplacementQuote:()=>4400};
const ground={ReplacementQuote:()=>500};
const ctx={m_iJointTransportPercent:50,m_iQRFMaxLiveAircraft:6,m_iMaxGroups:24,m_iMaxMembers:220,m_iMaxVehicles:12,
 GetReservedGroups:()=>0,GetReservedMembers:()=>0,GetReservedVehicles:()=>0,
 m_Operations:{IsQRFStrategyEnabled:()=>true,GetQRFFinance:()=>({Available:()=>7900}),QRFOverhead:()=>100}};
const op={m_iJointExtraTransports:-1,m_bDebugRequest:false,m_bFundingWaived:false,m_iRequestedTier:1,GetRole:()=>1};
const observed=extra.call(ctx,op,air,[air,ground,ground],4,30,2);
if(observed!==0)throw Error('Unexpected source probe outcome');
output.push({finding:'Extra unescorted transport priced as escorted',sourceMethod:'JointExtraTransports',available:7900,baseAndOverhead:5500,remaining:2400,unescortedQuoteUsingSamePersonnelCeiling:2400,actualReturnedExtras:observed,requestedExtras:1});
const construct=compile('AROR_QRFTacticalCommander','CanConstruct','index,now,liveCost,limit,cost',{Math:math});
const tactical={m_iPackages:9,m_bWithdraw:false,m_iFirstDispatch:-1,IsReserve:()=>false};
const asProfile=construct.call(tactical,4,0,5,6,2),asPackage=construct.call(tactical,4,0,5,6,1);
if(asProfile!==false||asPackage!==true)throw Error('Unexpected construction probe outcome');
output.push({finding:'Single-hull extra uses two-hull construction cost',sourceMethod:'CanConstruct',scenario:'Supported 100% extra-transport setting: four ground elements plus five transports and one escort, with sufficient global budgets',liveAircraft:5,limit:6,withCurrentProfileCost:asProfile,withActualPackageCost:asPackage,nativeVerified:false});
const report={generatedAt:new Date().toISOString(),nativeVerified:false,probes:output};
fs.writeFileSync(path.join(root,'docs/validation/BUILD_AUDIT_2026-11-06_PROBES.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report,null,2));
