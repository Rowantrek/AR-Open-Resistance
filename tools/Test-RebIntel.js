/* Actual-source RebIntel regressions. Native UI/replication mocked, not an Enforce compile.
 * Run: node tools/Test-RebIntel.js
 */
function runRebIntelTests(sources) {
  let assertions=0, server=true, bumps=0, now=0, currentManager=null, currentInvasion=null;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  Object.defineProperty(String.prototype,'Length',{value(){return this.length;},configurable:true});
  Object.defineProperty(Array.prototype,'Insert',{value(x){this.push(x);},configurable:true});
  const roles={RESISTANCE:0,OCCUPIERS:1,INVADERS:2};
  const apps={HOME:0,BANK:1,TEST:2,REBINTEL:3,HICOMM:4,OPS:5,MARS:6,BLACK_MARKET:7,BANK_TRANSFER:8};
  const env={AROR_EFactionRole:roles,AROR_EPhoneApp:apps,Replication:{IsServer:()=>server,BumpMe:()=>bumps++},
    Math:{Floor:Math.floor,Clamp:(x,a,b)=>Math.max(a,Math.min(b,x)),Round:Math.round,Min:Math.min,Max:Math.max},
    Print(){},LogLevel:{NORMAL:0},string:{Empty:'',Format:(s,...a)=>s.replace(/%([1-9])/g,(_,n)=>a[n-1])},
    FrameSlot:{SetPos:(w,x,y)=>Object.assign(w,{x,y}),SetSize:(w,width,height)=>Object.assign(w,{width,height})},
    Color:{FromRGBA:(...rgba)=>rgba},RichTextWidget:{Cast:x=>x},ImageWidget:{Cast:x=>x},
    AROR_OperationManagerComponent:{Cast:x=>x},AROR_InvaderCampaignComponent:{Cast:x=>x},AROR_FactionRolesComponent:{GetInstance:()=>null},
    GetGame:()=>({GetWorld:()=>({GetWorldTime:()=>now}),GetGameMode:()=>({FindComponent:type=>type===env.AROR_InvaderCampaignComponent?currentInvasion:currentManager})})};
  const names=['PressureForLevel','ReadinessLevel','GetReadinessPressure','GetReadinessLevel','HasThreatState','GetCrackdownPercent','IsEnemyRole','SetReadinessPressure_S','RestoreQRFStrategy_S',
    'IsQRFStrategyEnabled','AreInvaderOperationsEnabled','Style','Place','Label','ForceName','WarText','CrackdownText','ResponseText','Init','Refresh','SetVisible','Layout',
    'OnPhoneButton','OpenApp','IsLandscapeApp','IsSettled','OnBack','RequestClose'];
  names.push('ResponseLevel','GetCrackdown','GetCrackdownLevel','SetCrackdown_S');
  function compile(src,name){
    const m=src.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!m)throw Error('Missing method '+name);
    const args=m[1].replace(/\b\w+\s+(\w+)/g,'$1');
    let body=m[2].replace(/\b(?:bool|int|float|string|Widget|IEntity|Faction|Color|RichTextWidget|ImageWidget|AROR_\w+)\s+(\w+)/g,'let $1')
      .replace(/let (\w+);/g,'let $1=null;').replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
    for(const n of names)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  const policy={};env.AROR_QRFStrategicPolicy=policy;
  for(const n of ['PressureForLevel','ReadinessLevel','ResponseLevel'])policy[n]=compile(sources.policy,n);
  const manager={m_iClock:0,m_iOccupierPressure:0,m_iInvaderPressure:0,m_iOccupierCrackdown:0,m_iInvaderCrackdown:0,m_bThreatStateReady:false,m_bStrategyRestored:false,m_bQRFStrategicEconomy:true,m_bInvaderOperationsEnabled:false,
    m_QRFFinance:{SetDebt_S(){}}};
  for(const n of [...names.slice(2,11),'GetCrackdown','GetCrackdownLevel','SetCrackdown_S'])manager[n]=compile(sources.manager,n);
  check(!manager.HasThreatState(),'no invented readings before server initialization');
  manager.RestoreQRFStrategy_S(100,700,0,0);
  check(manager.HasThreatState()&&manager.GetReadinessLevel(1)===2&&manager.GetReadinessLevel(2)===5,'restore publishes separate role readings');
  check(manager.GetCrackdownPercent(1)===10&&manager.GetCrackdownPercent(2)===70,'legacy restore seeds crackdown once from old pressure');
  let before=bumps;manager.SetReadinessPressure_S(1,100);
  check(bumps===before,'unchanged pressure does not replicate again');
  manager.SetReadinessPressure_S(1,185);
  check(manager.GetReadinessLevel(1)===2&&manager.GetCrackdownPercent(1)===10&&bumps===before+1,'equipment progress changes do not mutate crackdown');
  manager.SetCrackdown_S(1,185);check(manager.GetReadinessLevel(1)===2&&manager.GetCrackdownPercent(1)===18&&manager.GetCrackdownLevel(1)===1,'crackdown changes independently of equipment progress');
  check(manager.GetReadinessPressure(2)===700,'Occupier change leaves Invader state untouched');
  manager.SetReadinessPressure_S(1,-40);manager.SetReadinessPressure_S(2,1500);
  manager.SetCrackdown_S(1,-40);manager.SetCrackdown_S(2,1500);
  check(manager.GetCrackdownPercent(1)===0&&manager.GetCrackdownPercent(2)===100,'crackdown bounded at both ends');
  before=bumps;manager.SetReadinessPressure_S(0,900);
  check(bumps===before&&manager.GetReadinessPressure(0)===0,'unsupported role cannot change summary');
  server=false;manager.SetReadinessPressure_S(1,700);manager.RestoreQRFStrategy_S(500,500,0,0);
  check(manager.GetReadinessPressure(1)===0&&manager.GetReadinessPressure(2)===1000&&bumps===before,'client cannot change or restore readiness pressure');
  server=true;
  // Labels: every built-in army names all five levels of both scales, with an English line for each.
  const labelArrays=[...sources.labels.matchAll(/m_a(?:Readiness|Crackdown)(?:Lines)? = \{("[^}]*)\}/g)];
  check(labelArrays.length===10&&labelArrays.every(m=>m[1].match(/"[^"]*"/g).length===5),'every label table names all five levels');
  check(sources.labels.includes('"USSR_ARMY"')&&sources.labels.includes('"US_ARMY"'),'labels keyed by army preset, not by role');
  // Parse the real layouts to validate hierarchy, names, fonts and local IDs used by the app/menu/HUD.
  function parse(src){
    const widgets={},ids=[],stack=[];
    for(const line of src.split(/\r?\n/)){
      const id=line.match(/"\{([A-F0-9]{16})\}"/);if(id)ids.push(id[1]);
      if(/WidgetClass.*\{\s*$/.test(line)){const node={type:line.match(/(\w+WidgetClass)/)[1],parent:stack.findLast(x=>x.widget)?.widget??null};stack.push({widget:node});continue;}
      if(/\{\s*$/.test(line)){stack.push({});continue;}
      const name=line.match(/^\s*Name "([^"]+)"/);if(name){const node=stack.findLast(x=>x.widget).widget;node.name=name[1];widgets[name[1]]=node;}
      const font=line.match(/^\s*Font "([^"]+)"/);if(font)stack.findLast(x=>x.widget).widget.font=font[1];
      if(/^\s*}\s*$/.test(line))stack.pop();
    }
    check(stack.length===0&&new Set(ids).size===ids.length,'layout balanced with unique local IDs');
    check(Object.values(widgets).every(w=>w.parent?.type!=='ImageWidgetClass'),'image widgets are leaves; frame containers own card content');
    check(Object.values(widgets).filter(w=>/TextWidgetClass/.test(w.type)).every(w=>/Roboto/.test(w.font??'')),'every text widget uses a Roboto font');
    return widgets;
  }
  const widgets=parse(sources.layout);
  const direct=[...sources.menu.matchAll(/(?:root|m_wBankApp)\.FindAnyWidget\("([^"]+)"\)/g),...sources.app.matchAll(/phoneRoot\.FindAnyWidget\("([^"]+)"\)/g)].map(m=>m[1]);
  check(direct.every(n=>widgets[n]),'every direct menu/app binding resolves in the layout');
  const market=parse(sources.marketTileLayout);
  check([...sources.menu.matchAll(/market\.FindAnyWidget\("([^"]+)"\)/g)].every(m=>market[m[1]]),'dynamic market tile bindings resolve in their separate layout');
  const appControls=['RebIntelBackground','RebIntelTitle','RebIntelSubtitle','RebIntelDivider','RebIntelHudHeading'];
  for(const p of ['TabOccupiers','TabInvaders','OptionCompact','OptionFull','OptionLeft','OptionRight'])for(const s of ['Border','Fill','Label','Button'])appControls.push('RebIntel'+p+s);
  for(const t of ['Hud','Alerts'])for(const s of ['Border','Fill','Label','Track','TrackFill','Knob','Button'])appControls.push('RebIntelToggle'+t+s);
  check(appControls.every(n=>widgets[n]?.parent.name==='RebIntelApp'),'app controls sit directly in the app frame');
  for(const card of ['Readiness','Crackdown']){
    const children=['Border','Fill','Heading','Count','Highlight','Value','Line','Effect','Meter1','Meter2','Meter3','Meter4','Meter5'];
        check(children.every(c=>widgets['RebIntel'+card+c]?.parent.name==='RebIntel'+card),'card children stay inside their own card: '+card);
  }
  check(widgets.RebIntelTileButton.parent.name==='Home'&&widgets.RebIntelApp.parent.name==='AROR_PhoneMenu','tile and app belong to correct screens');
  const overlay=parse(sources.notificationsLayout);
  for(const army of ['Occupiers','Invaders'])for(const row of ['Stripe','Flag','Name',...['Readiness','Crackdown'].flatMap(s=>['Flash','Key','Value','Meter1','Meter2','Meter3','Meter4','Meter5'].map(p=>s+p))])
    check(overlay['Status'+army+row]?.parent.name==='Status','HUD status part stays in the status widget: '+army+row);
  for(const s of ['Header','Source','Detail','Time','Intercept','Effect','LineLeft','LineRight'])check(overlay['Headline'+s]?.parent.name==='Headline','banner part stays in the headline: '+s);
  check(!sources.app.includes('AROR_OperationManagerComponent.GetInstance()'),'client does not use server-only scheduler singleton');
  check(!sources.app.includes('Rpc(')&&!/Set(?:ReadinessPressure|ReadinessLevel|Crackdown|CrackdownLevel)_S\(/.test(sources.app),'app has no authority to mutate campaign state');
  check(!sources.settings.includes('Rpc(')&&sources.settings.includes('$profile:'),'player settings stay local to the client profile');
  check(/RplSession\.Mode\(\) != RplMode\.Dedicated\)\s*GetGame\(\)\.GetCallqueue\(\)\.CallLater\(UpdateRebIntelHud/.test(sources.notifications),'HUD widget never runs on a dedicated server');
  const phone={m_bClosing:false,m_fTurn:0,m_fTurnTarget:0,m_eApp:apps.HOME,m_Mars:{},m_BlackMarket:{},m_RebIntel:null,m_BankTransfer:null};
  for(const n of ['OnPhoneButton','OpenApp','IsLandscapeApp','IsSettled','OnBack','RequestClose'])phone[n]=compile(sources.menu,n);
  phone.OnPhoneButton('RebIntelTileButton');check(phone.m_eApp===apps.REBINTEL&&phone.m_fTurnTarget===0,'RebIntel tile opens portrait app');
  phone.OnPhoneButton('HomeButton');check(phone.m_eApp===apps.HOME,'frame Home returns from RebIntel');
  phone.OnPhoneButton('BankTileButton');check(phone.m_eApp===apps.BANK,'Bank remains accessible');
  phone.OnPhoneButton('TestTileButton');check(phone.m_eApp===apps.MARS&&phone.m_fTurnTarget===1,'MARS tile opens the landscape debugger');
  phone.OnPhoneButton('RebIntelTileButton');check(phone.m_eApp===apps.MARS,'clicks ignored while phone turning');
  phone.m_fTurn=1;phone.OnPhoneButton('HomeButton');phone.m_fTurn=0;phone.OnPhoneButton('RebIntelTileButton');
  check(phone.m_eApp===apps.REBINTEL&&phone.m_fTurnTarget===0,'return from landscape can reopen RebIntel');
  phone.OnBack();check(phone.m_bClosing,'Escape closes phone from RebIntel');
  check(/\[RplProp\(\)\]\s*protected int m_iOccupierPressure/.test(sources.manager)&&/\[RplProp\(\)\]\s*protected int m_iInvaderPressure/.test(sources.manager)&&/\[RplProp\(\)\]\s*protected bool m_bThreatStateReady/.test(sources.manager),'summary fields included in engine replication');
  check(/\[RplProp\(\)\]\s*protected int m_iOccupierCrackdown/.test(sources.manager)&&/\[RplProp\(\)\]\s*protected int m_iInvaderCrackdown/.test(sources.manager),'independent crackdown is included in engine replication');
  return {assertions,note:'Native rendering, replication/JIP and Enforce compilation still require Workbench.'};
}
if(typeof module!=='undefined')module.exports={runRebIntelTests};
if(typeof require!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.resolve(__dirname,'../AROpenResistance/Open Resistance');
  const read=p=>fs.readFileSync(path.join(root,p),'utf8');
  console.log(runRebIntelTests({policy:read('Scripts/Game/Operations/AROR_QRFStrategicPolicy.c'),manager:read('Scripts/Game/Operations/AROR_OperationManagerComponent.c'),
    app:read('Scripts/Game/Phone/AROR_RebIntelApp.c'),menu:read('Scripts/Game/Phone/AROR_PhoneMenu.c'),layout:read('UI/Layouts/Phone/AROR_PhoneMenu.layout'),
    marketTileLayout:read('UI/Layouts/Phone/AROR_BlackMarket.layout'),labels:read('Scripts/Game/Operations/AROR_ThreatLabels.c'),admin:read('Scripts/Game/Admin/AROR_AdminMenuComponent.c'),settings:read('Scripts/Game/Phone/AROR_RebIntelSettings.c'),
    notifications:read('Scripts/Game/Notifications/AROR_NotificationManagerComponent.c'),notificationsLayout:read('UI/Layouts/Notifications/AROR_Notifications.layout')}));
}
